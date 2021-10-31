#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/arm/xnu.h"
#include "hw/arm/xnu_mem.h"
#include "hw/arm/xnu_dtb.h"

void allocate_and_copy(MemoryRegion *mem, AddressSpace *as, const char *name, uint32_t pa, uint32_t size, void *buf)
{
    if (mem) {
        allocate_ram(mem, name, pa, align_64k_high(size));
    }
    address_space_rw(as, pa, MEMTXATTRS_UNSPECIFIED, (uint8_t *)buf, size, 1);
}

void macho_setup_bootargs(const char *name, AddressSpace *as, MemoryRegion *mem, uint32_t bootargs_pa, uint32_t virt_base, uint32_t phys_base, uint32_t mem_size, uint32_t top_of_kernel_data_pa, uint32_t dtb_va, uint32_t dtb_size, char *kern_args)
{
    struct xnu_arm_boot_args boot_args;
    memset(&boot_args, 0, sizeof(boot_args));
    boot_args.Revision = xnu_arm_kBootArgsRevision2;
    boot_args.Version = xnu_arm_kBootArgsVersion3;
    boot_args.virtBase = virt_base;
    boot_args.physBase = phys_base;
    boot_args.memSize = mem_size;
    boot_args.topOfKernelData = top_of_kernel_data_pa;

    boot_args.Video.v_baseAddr = 1234;
    boot_args.Video.v_display = 1235;
    boot_args.Video.v_depth = 88;

    // TODO video stuff
    boot_args.deviceTreeP = dtb_va;
    boot_args.deviceTreeLength = dtb_size;
    if (kern_args) {
        g_strlcpy(boot_args.CommandLine, kern_args, sizeof(boot_args.CommandLine));
    }
    // TODO other args

    allocate_and_copy(mem, as, name, bootargs_pa, sizeof(boot_args), &boot_args);
}

static void macho_highest_lowest(struct mach_header* mh, uint32_t *lowaddr, uint32_t *highaddr)
{
    struct load_command* cmd = (struct load_command*)((uint8_t*)mh + sizeof(struct mach_header));
    // iterate all the segments once to find highest and lowest addresses
    uint32_t low_addr_temp = ~0;
    uint32_t high_addr_temp = 0;
    for (unsigned int index = 0; index < mh->ncmds; index++) {
        switch (cmd->cmd) {
            case LC_SEGMENT: {
                struct segment_command *segCmd = (struct segment_command *)cmd;
                if (segCmd->filesize > 0 && segCmd->vmaddr < low_addr_temp) {
                    low_addr_temp = segCmd->vmaddr;
                }
                if (segCmd->filesize > 0 && segCmd->vmaddr + segCmd->vmsize > high_addr_temp) {
                    high_addr_temp = segCmd->vmaddr + segCmd->vmsize;
                }
                break;
            }
        }
        cmd = (struct load_command*)((char*)cmd + cmd->cmdsize);
    }
    *lowaddr = low_addr_temp;
    *highaddr = high_addr_temp;
}

static void macho_file_highest_lowest(const char *filename, uint32_t *lowest, uint32_t *highest)
{
    gsize len;
    uint8_t *data = NULL;
    if (!g_file_get_contents(filename, (char **)&data, &len, NULL)) {
        fprintf(stderr, "macho_file_highest_lowest(): failed to load file\n");
        abort();
    }
    struct mach_header* mh = (struct mach_header*)data;
    macho_highest_lowest(mh, lowest, highest);
    g_free(data);
}

void macho_file_highest_lowest_base(const char *filename, uint32_t phys_base, uint32_t *virt_base, uint32_t *lowest, uint32_t *highest)
{
    uint8_t high_Low_dif_bit_index;
    uint8_t phys_base_non_zero_bit_index;
    uint32_t bit_mask_for_index;

    macho_file_highest_lowest(filename, lowest, highest);
    high_Low_dif_bit_index = get_highest_different_bit_index(align_64k_high(*highest), align_64k_low(*lowest));
    if (phys_base) {
        phys_base_non_zero_bit_index = get_lowest_non_zero_bit_index(phys_base);

        //make sure we have enough zero bits to have all the diffrent kernel
        //image addresses have the same non static bits in physical and in
        //virtual memory.
        if (high_Low_dif_bit_index > phys_base_non_zero_bit_index) {
            printf("not enough bits!\n");
            abort();
        }
        bit_mask_for_index = get_low_bits_mask_for_bit_index(phys_base_non_zero_bit_index);
        *virt_base = align_64k_low(*lowest) & (~bit_mask_for_index);
    }
}

void arm_load_macho(char *filename, AddressSpace *as, MemoryRegion *mem,
                    const char *name, uint32_t phys_base, uint32_t virt_base,
                    uint32_t low_virt_addr, uint32_t high_virt_addr, uint32_t *pc)
{
    uint8_t *data = NULL;
    gsize len;
    uint8_t* rom_buf = NULL;

    if (!g_file_get_contents(filename, (char **)&data, &len, NULL)) {
        printf("Could not get content of macho %s\n", filename);
        abort();
    }
    struct mach_header* mh = (struct mach_header*)data;
    struct load_command* cmd = (struct load_command*)(data + sizeof(struct mach_header));

    uint64_t rom_buf_size = align_64k_high(high_virt_addr) - low_virt_addr;
    rom_buf = g_malloc0(rom_buf_size);
    for (unsigned int index = 0; index < mh->ncmds; index++) {
        switch (cmd->cmd) {
            case LC_SEGMENT: {
                struct segment_command *segCmd = (struct segment_command *)cmd;
                memcpy(rom_buf + (segCmd->vmaddr - low_virt_addr), data + segCmd->fileoff, segCmd->filesize);
                break;
            }
            case LC_UNIXTHREAD: {
                // grab just the entry point PC
                // TODO
                break;
            }
        }
        cmd = (struct load_command*)((char*)cmd + cmd->cmdsize);
    }

    uint32_t low_phys_addr = vtop_bases(low_virt_addr, phys_base, virt_base);
    printf("Copying %s to address %08x (size: %llu bytes)\n", name, low_phys_addr, rom_buf_size);
    allocate_and_copy(mem, as, name, low_phys_addr, rom_buf_size, rom_buf);

    if (data) {
        g_free(data);
    }
    if (rom_buf) {
        g_free(rom_buf);
    }
}

void macho_load_dtb(char *filename, AddressSpace *as, MemoryRegion *mem, const char *name, uint32_t dtb_pa, uint64_t *size, uint32_t *uart_mmio_pa)
{
    uint8_t *file_data = NULL;
    unsigned long fsize;

    if (g_file_get_contents(filename, (char **)&file_data, &fsize, NULL)) {
        DTBNode *root = load_dtb(file_data);

        //first fetch the uart mmio address
        DTBNode *child = get_dtb_child_node_by_name(root, "arm-io");
        if (NULL == child) {
            abort();
        }
        DTBProp *prop = get_dtb_prop(child, "ranges");
        if (NULL == prop) {
            abort();
        }
        hwaddr *ranges = (hwaddr *)prop->value;
        hwaddr soc_base_pa = ranges[1];
        child = get_dtb_child_node_by_name(child, "uart0");
        if (NULL == child) {
            abort();
        }
        //make sure this node has the boot-console prop
        prop = get_dtb_prop(child, "boot-console");
        if (NULL == prop) {
            abort();
        }
        prop = get_dtb_prop(child, "reg");
        if (NULL == prop) {
            abort();
        }
        hwaddr *uart_offset = (hwaddr *)prop->value;
        if (NULL != uart_mmio_pa) {
            *uart_mmio_pa = soc_base_pa + uart_offset[0];
        }

        // setup cpu frequencies
        child = get_dtb_child_node_by_name(root, "cpus");
        if (NULL == child) {
            abort();
        }
        child = get_dtb_child_node_by_name(child, "cpu0");
        if (NULL == child) {
            abort();
        }

        // timebase
        uint32_t timebase_freq = 6000000;
        prop = get_dtb_prop(child, "timebase-frequency");
        if (NULL == prop) {
            abort();
        }
        remove_dtb_prop(child, prop);
        add_dtb_prop(child, "timebase-frequency", sizeof(uint32_t), (uint8_t *)&timebase_freq);
        
        // fixed
        uint32_t fixed_freq = 24000000;
        prop = get_dtb_prop(child, "fixed-frequency");
        if (NULL == prop) {
            abort();
        }
        remove_dtb_prop(child, prop);
        add_dtb_prop(child, "fixed-frequency", sizeof(uint32_t), (uint8_t *)&fixed_freq);

        // clock
        uint32_t clock_freq = 412000000;
        prop = get_dtb_prop(child, "clock-frequency");
        if (NULL == prop) {
            abort();
        }
        remove_dtb_prop(child, prop);
        add_dtb_prop(child, "clock-frequency", sizeof(uint32_t), (uint8_t *)&clock_freq);

        // bus
        uint32_t bus_freq = 103000000;
        prop = get_dtb_prop(child, "bus-frequency");
        if (NULL == prop) {
            abort();
        }
        remove_dtb_prop(child, prop);
        add_dtb_prop(child, "bus-frequency", sizeof(uint32_t), (uint8_t *)&bus_freq);

        // peripheral
        uint32_t peripheral_freq = 51500000;
        prop = get_dtb_prop(child, "peripheral-frequency");
        if (NULL == prop) {
            abort();
        }
        remove_dtb_prop(child, prop);
        add_dtb_prop(child, "peripheral-frequency", sizeof(uint32_t), (uint8_t *)&peripheral_freq);

        // memory
        uint32_t memory_freq = 137333333;
        prop = get_dtb_prop(child, "memory-frequency");
        if (NULL == prop) {
            abort();
        }
        remove_dtb_prop(child, prop);
        add_dtb_prop(child, "memory-frequency", sizeof(uint32_t), (uint8_t *)&memory_freq);

        uint64_t size_n = get_dtb_node_buffer_size(root);

        uint8_t *buf = g_malloc0(size_n);
        save_dtb(buf, root);

        printf("Loading device tree at address %08x (size: %llu bytes)\n", dtb_pa, size_n);
        allocate_and_copy(mem, as, name, dtb_pa, size_n, buf);
        g_free(file_data);
        delete_dtb_node(root);
        g_free(buf);
        *size = size_n;
    }
    else {
        printf("Could not get content of device tree %s\n", filename);
        abort();
    }
}