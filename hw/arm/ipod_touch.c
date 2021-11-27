#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/misc/unimp.h"
#include "hw/irq.h"
#include "sysemu/sysemu.h"
#include "sysemu/reset.h"
#include "hw/arm/xnu.h"
#include "hw/platform-bus.h"
#include "hw/block/flash.h"
#include "hw/arm/xnu_mem.h"
#include "hw/arm/ipod_touch.h"
#include "hw/arm/ipod_touch_uart.h"
#include "hw/arm/ipod_touch_spi.h"
#include "hw/arm/exynos4210.h"

#define IPOD_TOUCH_PHYS_BASE (0xc0000000)
#define IBOOT_BASE 0x18000000
#define LLB_BASE 0x22000000
#define SRAM1_MEM_BASE 0x22020000
#define CLOCK0_MEM_BASE 0x38100000
#define CLOCK1_MEM_BASE 0x3C500000
#define SYSIC_MEM_BASE 0x39A00000
#define VIC0_MEM_BASE 0x38E00000
#define VIC1_MEM_BASE 0x38E01000
#define UNKNOWN1_MEM_BASE 0x38E02000
#define IIS0_MEM_BASE 0x3D400000
#define IIS1_MEM_BASE 0x3CD00000
#define IIS2_MEM_BASE 0x3CA00000
#define NOR_MEM_BASE 0x24000000
#define TIMER1_MEM_BASE 0x3E200000
#define USBOTG_MEM_BASE 0x38400000
#define USBPHYS_MEM_BASE 0x3C400000
#define GPIO_MEM_BASE 0x3E400000
#define I2C1_MEM_BASE 0x3C900000
#define SPI0_MEM_BASE 0x3C300000
#define SPI1_MEM_BASE 0x3CE00000
#define SPI2_MEM_BASE 0x3D200000
#define WATCHDOG_MEM_BASE 0x3E300000
#define DISPLAY_MEM_BASE 0x38900000
#define CHIPID_MEM_BASE 0x3e500000
#define RAM_MEM_BASE 0x8000000
#define SHA1_MEM_BASE 0x38000000
#define AES_MEM_BASE 0x38C00000
#define VROM_MEM_BASE 0x20000000
#define NAND_MEM_BASE 0x38A00000
#define DMAC0_MEM_BASE 0x38200000
#define DMAC1_MEM_BASE 0x39900000
#define UART0_MEM_BASE 0x3CC00000
#define UART1_MEM_BASE 0x3CC04000
#define UART2_MEM_BASE 0x3CC08000
#define UART3_MEM_BASE 0x3CC0C000
#define UART4_MEM_BASE 0x3CC10000
#define KERNELCACHE_BASE 0x9000000

static void ipod_touch_cpu_setup(MachineState *machine, MemoryRegion **sysmem, ARMCPU **cpu, AddressSpace **nsas)
{
    Object *cpuobj = object_new(machine->cpu_type);
    *cpu = ARM_CPU(cpuobj);
    CPUState *cs = CPU(*cpu);

    *sysmem = get_system_memory();

    object_property_set_link(cpuobj, "memory", OBJECT(*sysmem), &error_abort);

    object_property_set_bool(cpuobj, "has_el3", false, NULL);

    object_property_set_bool(cpuobj, "has_el2", false, NULL);

    object_property_set_bool(cpuobj, "realized", true, &error_fatal);

    *nsas = cpu_get_address_space(cs, ARMASIdx_NS);

    object_unref(cpuobj);
}

static void ipod_touch_cpu_reset(void *opaque)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE((MachineState *)opaque);
    ARMCPU *cpu = nms->cpu;
    CPUState *cs = CPU(cpu);
    CPUARMState *env = &cpu->env;

    cpu_reset(cs);

    env->regs[0] = nms->kbootargs_pa;
    //cpu_set_pc(CPU(cpu), 0xc00607ec);
    cpu_set_pc(CPU(cpu), IBOOT_BASE);
    //cpu_set_pc(CPU(cpu), LLB_BASE);
    //cpu_set_pc(CPU(cpu), VROM_MEM_BASE);
}

static void s5l8900_st_update(s5l8900_timer_s *s)
{
    s->freq_out = 1000000000 / 100; 
    s->tick_interval = /* bcount1 * get_ticks / freq  + ((bcount2 * get_ticks / freq)*/
    muldiv64((s->bcount1 < 1000) ? 1000 : s->bcount1, NANOSECONDS_PER_SECOND, s->freq_out);
    s->next_planned_tick = 0;
}

static void s5l8900_st_set_timer(s5l8900_timer_s *s)
{
    uint64_t last = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->base_time;

    s->next_planned_tick = last + (s->tick_interval - last % s->tick_interval);
    timer_mod(s->st_timer, s->next_planned_tick + s->base_time);
    s->last_tick = last;
}

static void s5l8900_st_tick(void *opaque)
{
    s5l8900_timer_s *s = (s5l8900_timer_s *)opaque;

    if (s->status & TIMER_STATE_START) {
        //fprintf(stderr, "%s: Raising irq\n", __func__);
        qemu_irq_raise(s->irq);

        /* schedule next interrupt */
        s5l8900_st_set_timer(s);
    } else {
        s->next_planned_tick = 0;
        s->last_tick = 0;
        timer_del(s->st_timer);
    }
}

static void s5l8900_timer1_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    s5l8900_timer_s *s = (struct s5l8900_timer_s *) opaque;

    switch(addr){

        case TIMER_IRQSTAT:
            s->irqstat = value;
            return;
        case TIMER_IRQLATCH:
            //fprintf(stderr, "%s: lowering irq\n", __func__);
            qemu_irq_lower(s->irq);     
            return;
        case TIMER_4 + TIMER_CONFIG:
            s5l8900_st_update(s);
            s->config = value;
            break;
        case TIMER_4 + TIMER_STATE:
            if ((value & TIMER_STATE_START) > (s->status & TIMER_STATE_START)) {
                s->base_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
                s5l8900_st_update(s);
                s5l8900_st_set_timer(s);
            } else if ((value & TIMER_STATE_START) < (s->status & TIMER_STATE_START)) {
                timer_del(s->st_timer);
            }
            s->status = value;
            break;
        case TIMER_4 + TIMER_COUNT_BUFFER:
            s->bcount1 = s->bcreload = value;
            break;
        case TIMER_4 + TIMER_COUNT_BUFFER2:
            s->bcount2 = value;
            break;
      default:
        break;
    }
}

static uint64_t s5l8900_timer1_read(void *opaque, hwaddr addr, unsigned size)
{
    s5l8900_timer_s *s = (struct s5l8900_timer_s *) opaque;
    uint64_t ticks;

    switch (addr) {
        case TIMER_TICKSHIGH:    // needs to be fixed so that read from low first works as well
            ticks = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
            s->ticks_high = (ticks >> 32);
            s->ticks_low = (ticks & 0xFFFFFFFF);
            return s->ticks_high;
        case TIMER_TICKSLOW:
            return s->ticks_low;
        case TIMER_IRQSTAT:
            return ~0; // s->irqstat;
        case TIMER_IRQLATCH:
            return 0xffffffff;

      default:
        break;
    }
    return 0;
}

static const MemoryRegionOps timer1_ops = {
    .read = s5l8900_timer1_read,
    .write = s5l8900_timer1_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/*
CLOCK
*/

static void s5l8900_clock1_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    s5l8900_clk1_s *s = (struct s5l8900_clk1_s *) opaque;

    switch (addr) {
        case CLOCK1_CONFIG0:
            s->clk1_config0 = val;
        case CLOCK1_CONFIG1:
            s->clk1_config1 = val;
        case CLOCK1_CONFIG2:
            s->clk1_config2 = val;
        case CLOCK1_PLLLOCK:
            s->clk1_plllock = val;
        case CLOCK1_PLLMODE:
            s->clk1_pllmode = val;
    
      default:
            break;
    }
}

static uint64_t s5l8900_clock1_read(void *opaque, hwaddr addr, unsigned size)
{
    s5l8900_clk1_s *s = (struct s5l8900_clk1_s *) opaque;

    switch (addr) {
        case CLOCK1_CONFIG0:
            return s->clk1_config0;
        case CLOCK1_CONFIG1:
            return s->clk1_config1;
        case CLOCK1_CONFIG2:
            return s->clk1_config2;
        case CLOCK1_PLL0CON:
            return (1023 << 8) | (12 << 24); // TODO this gives a clock frequency that's slightly below the target value (412000000)!
        case CLOCK1_PLLLOCK:
            return s->clk1_plllock;
        case CLOCK1_PLLMODE:
            return s->clk1_pllmode;
    
      default:
            break;
    }
    return 0;
}

static const MemoryRegionOps clock1_ops = {
    .read = s5l8900_clock1_read,
    .write = s5l8900_clock1_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/*
SYSIC
*/

static uint64_t s5l8900_sysic_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
        case POWER_ID:
            //return (3 << 24); //for older iboots
            return (2 << 0x18);
        case 0x7a:
        case 0x7c:
            return 1;
        case 0x14:
        case 0x8:
            return 0x12fc;
      default:
        break;
    }
    return 0;
}

static void s5l8900_sysic_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    // Do nothing
}

static const MemoryRegionOps sysic_ops = {
    .read = s5l8900_sysic_read,
    .write = s5l8900_sysic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/*
NAND
*/
int nand_cmd_status = 0;

static uint64_t s5l8900_nand_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
        case 0x80:
            if(nand_cmd_status == 0x90) {
                //return 0x2555D5EC;
                return 0;
            }
            else if(nand_cmd_status == 0x70) {
                return (1 << 6);
            }

        case 0x48:
            return (1 << 1) | (1 << 2) | (1 << 3); // set bit 2 and 4 to indicate that the FTM is ready
        default:
            break;
    }
    return 0;
}

static void s5l8900_nand_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    switch(addr) {
        case 0x8:
            nand_cmd_status = val;
        default:
            break; 
    }
}

static const MemoryRegionOps nand_ops = {
    .read = s5l8900_nand_read,
    .write = s5l8900_nand_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ipod_touch_init_clock(MachineState *machine, MemoryRegion *sysmem)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
    nms->clock1 = (s5l8900_clk1_s *) g_malloc0(sizeof(struct s5l8900_clk1_s));
    nms->clock1->clk1_plllock = 1;

    // TODO same as clock1 for now
    MemoryRegion *iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(nms), &clock1_ops, nms->clock1, "clock0", 0x1000);
    memory_region_add_subregion(sysmem, CLOCK0_MEM_BASE, iomem);

    iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(nms), &clock1_ops, nms->clock1, "clock1", 0x1000);
    memory_region_add_subregion(sysmem, CLOCK1_MEM_BASE, iomem);
}

static void ipod_touch_init_timer(MachineState *machine, MemoryRegion *sysmem)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
    nms->timer1 = (s5l8900_timer_s *) g_malloc0(sizeof(struct s5l8900_timer_s));
    nms->timer1->irq = nms->irq[0][7];
    nms->timer1->base_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    nms->timer1->st_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, s5l8900_st_tick, nms->timer1);

    MemoryRegion *iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(nms), &timer1_ops, nms->timer1, "timer1", 0x10001);
    memory_region_add_subregion(sysmem, TIMER1_MEM_BASE, iomem);
}

static void ipod_touch_init_sysic(MachineState *machine, MemoryRegion *sysmem)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
    MemoryRegion *iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(nms), &sysic_ops, NULL, "sysic", 0x1000);
    memory_region_add_subregion(sysmem, SYSIC_MEM_BASE, iomem);
}

static void ipod_touch_memory_setup(MachineState *machine, MemoryRegion *sysmem, AddressSpace *nsas)
{
    uint32_t kernel_low;
    uint32_t kernel_high;
    uint32_t virt_base;
    uint32_t phys_ptr;
    uint32_t phys_pc;
    uint64_t dtb_size;
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
    DriveInfo *dinfo;

    // macho_file_highest_lowest_base(nms->kernel_filename, IPOD_TOUCH_PHYS_BASE, &virt_base, &kernel_low, &kernel_high);

    // g_virt_base = virt_base;
    // g_phys_base = IPOD_TOUCH_PHYS_BASE;
    // phys_ptr = IPOD_TOUCH_PHYS_BASE;

    // printf("Virt base: %08x, kernel lowest: %08x, kernel highest: %08x\n", virt_base, kernel_low, kernel_high);

    // //now account for the loaded kernel
    // arm_load_macho(nms->kernel_filename, nsas, sysmem, "kernel.n45", IPOD_TOUCH_PHYS_BASE, virt_base, kernel_low, kernel_high, &phys_pc);
    // nms->kpc_pa = phys_pc; // TODO unused for now

    // phys_ptr = align_64k_high(kernel_high);
    // uint32_t kbootargs_pa = phys_ptr;
    // nms->kbootargs_pa = kbootargs_pa;
    // phys_ptr += align_64k_high(sizeof(struct xnu_arm_boot_args));
    // printf("Loading bootargs at memory location %08x\n", nms->kbootargs_pa);

    // // allocate free space for kernel management (e.g., VM tables)
    // uint32_t top_of_kernel_data_pa = phys_ptr;
    // printf("Top of kernel data: %08x\n", top_of_kernel_data_pa);
    // allocate_ram(sysmem, "n45.extra", phys_ptr, 0x300000);
    // phys_ptr += align_64k_high(0x300000);
    // uint32_t mem_size = 0x8000000; // TODO hard-coded

    // // load the device tree
    // macho_load_dtb(nms->dtb_filename, nsas, sysmem, "devicetree.45", phys_ptr, &dtb_size, &nms->uart_mmio_pa);
    // uint32_t dtb_va = phys_ptr;

    // phys_ptr += align_64k_high(dtb_size);

    // allocate framebuffer memory
    allocate_ram(sysmem, "framebuffer", phys_ptr, align_64k_high(3 * 320 * 480));

    // load boot args
    // macho_setup_bootargs("k_bootargs.n45", nsas, sysmem, kbootargs_pa, virt_base, IPOD_TOUCH_PHYS_BASE, mem_size, top_of_kernel_data_pa, dtb_va, dtb_size, nms->kern_args, phys_ptr);

    allocate_ram(sysmem, "sram1", SRAM1_MEM_BASE, 0x10000);

    // allocate UART ram
    allocate_ram(sysmem, "ram", RAM_MEM_BASE, 0x8000000);

    uint32_t start = 0xf000000;
    allocate_ram(sysmem, "unknown??", start, 0x10000000 - start);

    // load the bootrom (vrom)
    uint8_t *file_data = NULL;
    unsigned long fsize;
    if (g_file_get_contents("/Users/martijndevos/Documents/ipod_touch_emulation/bootrom_s5l8900", (char **)&file_data, &fsize, NULL)) {
        allocate_ram(sysmem, "vrom", VROM_MEM_BASE, 0x10000);
        address_space_rw(nsas, VROM_MEM_BASE, MEMTXATTRS_UNSPECIFIED, (uint8_t *)file_data, fsize, 1);
    }

    // load iBoot
    file_data = NULL;
    if (g_file_get_contents("/Users/martijndevos/Documents/ipod_touch_emulation/iboot.bin", (char **)&file_data, &fsize, NULL)) {
        allocate_ram(sysmem, "iboot", IBOOT_BASE, 0x400000);
        address_space_rw(nsas, IBOOT_BASE, MEMTXATTRS_UNSPECIFIED, (uint8_t *)file_data, fsize, 1);
     }

    // // load LLB
    // file_data = NULL;
    // if (g_file_get_contents("/Users/martijndevos/Documents/ipod_touch_emulation/LLB.n45ap.RELEASE", (char **)&file_data, &fsize, NULL)) {
    //     allocate_ram(sysmem, "llb", LLB_BASE, align_64k_high(fsize));
    //     address_space_rw(nsas, LLB_BASE, MEMTXATTRS_UNSPECIFIED, (uint8_t *)file_data, fsize, 1);
    //  }

     // load the kernelcache at 0x09000000
     file_data = NULL;
    if (g_file_get_contents(nms->kernel_filename, (char **)&file_data, &fsize, NULL)) {
        allocate_ram(sysmem, "kernel", KERNELCACHE_BASE, align_64k_high(fsize));
        address_space_rw(nsas, KERNELCACHE_BASE, MEMTXATTRS_UNSPECIFIED, (uint8_t *)file_data, fsize, 1);
     }

    allocate_ram(sysmem, "unknown1", UNKNOWN1_MEM_BASE, 0x1000);
    allocate_ram(sysmem, "chipid", CHIPID_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "usbphys", USBPHYS_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "gpio", GPIO_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "i2c1", I2C1_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "watchdog", WATCHDOG_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "display", DISPLAY_MEM_BASE, align_64k_high(0x1));

    allocate_ram(sysmem, "uart1", UART1_MEM_BASE, align_64k_high(0x4000));
    allocate_ram(sysmem, "uart2", UART2_MEM_BASE, align_64k_high(0x4000));
    allocate_ram(sysmem, "uart3", UART3_MEM_BASE, align_64k_high(0x4000));
    allocate_ram(sysmem, "uart4", UART4_MEM_BASE, align_64k_high(0x4000));

    allocate_ram(sysmem, "iis0", IIS0_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "iis1", IIS1_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "iis2", IIS2_MEM_BASE, align_64k_high(0x1));

    allocate_ram(sysmem, "iboot_framebuffer", 0xfe000000, align_64k_high(3 * 320 * 480));

    // intercept printf writes
    // MemoryRegion *iomem = g_new(MemoryRegion, 1);
    // memory_region_init_io(iomem, OBJECT(nms), &printf_ops, nms, "printf", 0x100);
    // memory_region_add_subregion(sysmem, 0x18022858, iomem);

    // setup 1MB NOR
    dinfo = drive_get(IF_PFLASH, 0, 0);
    if (!dinfo) {
        printf("A NOR image must be given with the -pflash parameter\n");
        abort();
    }

    if(!pflash_cfi02_register(NOR_MEM_BASE, "nor", 1024 * 1024, dinfo ? blk_by_legacy_dinfo(dinfo) : NULL, 4096, 1, 2, 0x00bf, 0x273f, 0x0, 0x0, 0x555, 0x2aa, 0)) {
        printf("Error registering NOR flash!\n");
        abort();
    }
}

static void ipod_touch_set_kernel_filename(Object *obj, const char *value, Error **errp)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(obj);
    g_strlcpy(nms->kernel_filename, value, sizeof(nms->kernel_filename));
}

static char *ipod_touch_get_kernel_filename(Object *obj, Error **errp)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(obj);
    return g_strdup(nms->kernel_filename);
}

static void ipod_touch_set_dtb_filename(Object *obj, const char *value, Error **errp)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(obj);
    g_strlcpy(nms->dtb_filename, value, sizeof(nms->dtb_filename));
}

static char *ipod_touch_get_dtb_filename(Object *obj, Error **errp)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(obj);
    return g_strdup(nms->dtb_filename);
}

static void ipod_touch_set_kern_args(Object *obj, const char *value, Error **errp)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(obj);
    g_strlcpy(nms->kern_args, value, sizeof(nms->kern_args));
}

static char *ipod_touch_get_kern_args(Object *obj, Error **errp)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(obj);
    return g_strdup(nms->kern_args);
}

static void ipod_touch_instance_init(Object *obj)
{
	object_property_add_str(obj, "kernel-filename", ipod_touch_get_kernel_filename, ipod_touch_set_kernel_filename);
    object_property_set_description(obj, "kernel-filename", "Set the kernel filename to be loaded");

    object_property_add_str(obj, "dtb-filename", ipod_touch_get_dtb_filename, ipod_touch_set_dtb_filename);
    object_property_set_description(obj, "dtb-filename", "Set the dev tree filename to be loaded");

    object_property_add_str(obj, "kern-cmd-args", ipod_touch_get_kern_args, ipod_touch_set_kern_args);
    object_property_set_description(obj, "kern-cmd-args", "Set the XNU kernel cmd args");
}

static inline qemu_irq s5l8900_get_irq(IPodTouchMachineState *s, int n)
{
    return s->irq[n / S5L8900_VIC_SIZE][n % S5L8900_VIC_SIZE];
}

static uint32_t s5l8900_usb_hwcfg[] = {
    0,
    0x7a8f60d0,
    0x082000e8,
    0x01f08024
};

static void ipod_touch_machine_init(MachineState *machine)
{
	IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
	MemoryRegion *sysmem;
    AddressSpace *nsas;
    ARMCPU *cpu;

    ipod_touch_cpu_setup(machine, &sysmem, &cpu, &nsas);

    nms->cpu = cpu;

    // setup VICs
    nms->irq = g_malloc0(sizeof(qemu_irq *) * 2);
    DeviceState *dev = pl192_manual_init("vic0", qdev_get_gpio_in(DEVICE(nms->cpu), ARM_CPU_IRQ), qdev_get_gpio_in(DEVICE(nms->cpu), ARM_CPU_FIQ), NULL);
    PL192State *s = PL192(dev);
    nms->vic0 = s;
    memory_region_add_subregion(sysmem, VIC0_MEM_BASE, &nms->vic0->iomem);
    nms->irq[0] = g_malloc0(sizeof(qemu_irq) * 32);
    for (int i = 0; i < 32; i++) { nms->irq[0][i] = qdev_get_gpio_in(dev, i); }

    dev = pl192_manual_init("vic1", NULL);
    s = PL192(dev);
    nms->vic1 = s;
    memory_region_add_subregion(sysmem, VIC1_MEM_BASE, &nms->vic1->iomem);
    nms->irq[1] = g_malloc0(sizeof(qemu_irq) * 32);
    for (int i = 0; i < 32; i++) { nms->irq[1][i] = qdev_get_gpio_in(dev, i); }

    // // chain VICs together
    nms->vic1->daisy = nms->vic0;

    // init clock
    ipod_touch_init_clock(machine, sysmem);

    // init timer
    ipod_touch_init_timer(machine, sysmem);

    // init sysic
    ipod_touch_init_sysic(machine, sysmem);

    // init uart
    // dev = ipod_touch_init_uart(0, 256, nms->irq[0][24], serial_hd(0));
    // S5L8900UartState *uart0 = S5L8900UART(dev);
    // nms->uart0 = uart0;
    // memory_region_add_subregion(sysmem, UART0_MEM_BASE, &nms->uart0->iomem);
    // SysBusDevice *sbd = SYS_BUS_DEVICE(uart0);
    // sysbus_realize_and_unref(sbd, &error_fatal);

    dev = exynos4210_uart_create(UART0_MEM_BASE, 256, 0, serial_hd(0), nms->irq[0][24]);
    if (!dev) {
        printf("Failed to create uart0 device!\n");
        abort();
    }

    // init spis
    set_spi_base(0);
    sysbus_create_simple("s5l8900spi", SPI0_MEM_BASE, s5l8900_get_irq(nms, S5L8900_SPI0_IRQ));

    set_spi_base(1);
    sysbus_create_simple("s5l8900spi", SPI1_MEM_BASE, s5l8900_get_irq(nms, S5L8900_SPI1_IRQ));

    set_spi_base(2);
    sysbus_create_simple("s5l8900spi", SPI2_MEM_BASE, s5l8900_get_irq(nms, S5L8900_SPI2_IRQ));

    ipod_touch_memory_setup(machine, sysmem, nsas);

    // init AES engine
    S5L8900AESState *aes_state = malloc(sizeof(S5L8900AESState));
    nms->aes_state = aes_state;
    MemoryRegion *iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(s), &aes_ops, s, "aes", 0x100);
    memory_region_add_subregion(sysmem, AES_MEM_BASE, iomem);

    // init SHA1 engine
    S5L8900SHA1State *sha1_state = malloc(sizeof(S5L8900SHA1State));
    nms->sha1_state = sha1_state;
    iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(s), &sha1_ops, s, "sha1", 0x100);
    memory_region_add_subregion(sysmem, SHA1_MEM_BASE, iomem);

    qemu_register_reset(ipod_touch_cpu_reset, nms);

    // init NAND flash
    iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(s), &nand_ops, s, "nand", 0x1000);
    memory_region_add_subregion(sysmem, NAND_MEM_BASE, iomem);

    // init USB OTG
    dev = ipod_touch_init_usb_otg(nms->irq[0][13], s5l8900_usb_hwcfg);
    synopsys_usb_state *usb_otg = S5L8900USBOTG(dev);
    nms->usb_otg = usb_otg;
    memory_region_add_subregion(sysmem, USBOTG_MEM_BASE, &nms->usb_otg->iomem);

    //register_synopsys_usb(S5L8900_USB_OTG_BASE, s5l8900_get_irq(s, S5L8900_IRQ_OTG), s5l8900_usb_hwcfg);
    //s5l8900_usb_phy_init(s);
}

static void ipod_touch_machine_class_init(ObjectClass *klass, void *data)
{
    MachineClass *mc = MACHINE_CLASS(klass);
    mc->desc = "iPod Touch";
    mc->init = ipod_touch_machine_init;
    mc->max_cpus = 1;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm1176");
}

static const TypeInfo ipod_touch_machine_info = {
    .name          = TYPE_IPOD_TOUCH_MACHINE,
    .parent        = TYPE_MACHINE,
    .instance_size = sizeof(IPodTouchMachineState),
    .class_size    = sizeof(IPodTouchMachineClass),
    .class_init    = ipod_touch_machine_class_init,
    .instance_init = ipod_touch_instance_init,
};

static void ipod_touch_machine_types(void)
{
    type_register_static(&ipod_touch_machine_info);
}

type_init(ipod_touch_machine_types)
