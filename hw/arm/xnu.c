#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/arm/xnu.h"
#include "hw/arm/xnu_mem.h"

static void allocate_and_copy(MemoryRegion *mem, AddressSpace *as, const char *name, uint32_t pa, uint32_t size, void *buf)
{
    if (mem) {
        allocate_ram(mem, name, pa, align_64k_high(size));
    }
    address_space_rw(as, pa, MEMTXATTRS_UNSPECIFIED, (uint8_t *)buf, size, 1);
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

    printf("BYTES: %02x\n", rom_buf[0x607ec]);

    if (data) {
        g_free(data);
    }
    if (rom_buf) {
        g_free(rom_buf);
    }
}