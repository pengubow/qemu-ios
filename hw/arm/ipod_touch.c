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
#include "hw/arm/xnu_mem.h"
#include "hw/arm/ipod_touch.h"
#include "hw/arm/exynos4210.h"

#define IPOD_TOUCH_PHYS_BASE (0xc0000000)
#define IBOOT_BASE 0x18000000
#define SRAM1_MEM_BASE 0x22020000
#define CLOCK0_MEM_BASE 0x38100000
#define CLOCK1_MEM_BASE 0x3C500000
#define EIC_MEM_BASE 0x39A00000
#define VIC0_MEM_BASE 0x38E00000
#define VIC1_MEM_BASE 0x38E01000
#define UNKNOWN1_MEM_BASE 0x38E02000
#define UNKNOWN2_MEM_BASE 0x3D400000
#define UNKNOWN3_MEM_BASE 0x24000000
#define UNKNOWN5_MEM_BASE 0xfe00000
#define TIMER1_MEM_BASE 0x3E200000
#define USBOTG_MEM_BASE 0x38400000
#define USBPHYS_MEM_BASE 0x3C400000
#define GPIO_MEM_BASE 0x3E400000
#define I2C1_MEM_BASE 0x3C900000
#define IIS_MEM_BASE 0x3CA00000
#define SPI_MEM_BASE 0x3CD00000
#define SDCI_MEM_BASE 0x3C300000
#define WATCHDOG_MEM_BASE 0x3E300000
#define ADC_MEM_BASE 0x3CE00000
#define RTC_MEM_BASE 0x3D200000
#define DISPLAY_MEM_BASE 0x38900000
#define CHIPID_MEM_BASE 0x3e500000

#define UART0_MEM_BASE 0x3CC00000
#define UART1_MEM_BASE 0x3CC04000
#define UART2_MEM_BASE 0x3CC08000
#define UART3_MEM_BASE 0x3CC0C000
#define UART4_MEM_BASE 0x3CC10000

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
    cpu_set_pc(CPU(cpu), 0xc00607ec);
    //cpu_set_pc(CPU(cpu), IBOOT_BASE);
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

    return;
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
            return s->irqstat;
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

static void s5l8900_clock1_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    // Do nothing
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
        case CLOCK1_PLLLOCK:
            return 1;
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

static void ipod_touch_init_clock(MachineState *machine, MemoryRegion *sysmem)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
    nms->clock1 = *(s5l8900_clk1_s *) g_malloc0(sizeof(struct s5l8900_clk1_s));

    MemoryRegion *iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(nms), &clock1_ops, nms, "clock1", 0x1000);
    memory_region_add_subregion(sysmem, CLOCK1_MEM_BASE, iomem);
}

static void ipod_touch_init_timer(MachineState *machine, MemoryRegion *sysmem)
{
    IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
    nms->timer1 = *(s5l8900_timer_s *) g_malloc0(sizeof(struct s5l8900_timer_s));
    nms->timer1.irq = nms->irq[0][7];
    nms->timer1.base_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    nms->timer1.st_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, s5l8900_st_tick, &nms->timer1);

    MemoryRegion *iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, OBJECT(nms), &timer1_ops, nms, "timer1", 0x1000);
    memory_region_add_subregion(sysmem, TIMER1_MEM_BASE, iomem);
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

    macho_file_highest_lowest_base(nms->kernel_filename, IPOD_TOUCH_PHYS_BASE, &virt_base, &kernel_low, &kernel_high);

    g_virt_base = virt_base;
    g_phys_base = IPOD_TOUCH_PHYS_BASE;
    phys_ptr = IPOD_TOUCH_PHYS_BASE;

    printf("Virt base: %08x, kernel lowest: %08x, kernel highest: %08x\n", virt_base, kernel_low, kernel_high);

    //now account for the loaded kernel
    arm_load_macho(nms->kernel_filename, nsas, sysmem, "kernel.n45", IPOD_TOUCH_PHYS_BASE, virt_base, kernel_low, kernel_high, &phys_pc);
    nms->kpc_pa = phys_pc; // TODO unused for now

    phys_ptr = align_64k_high(kernel_high);
    uint32_t kbootargs_pa = phys_ptr;
    nms->kbootargs_pa = kbootargs_pa;
    phys_ptr += align_64k_high(sizeof(struct xnu_arm_boot_args));
    printf("Loading bootargs at memory location %08x\n", nms->kbootargs_pa);

    // allocate free space for kernel management (e.g., VM tables)
    uint32_t top_of_kernel_data_pa = phys_ptr;
    printf("Top of kernel data: %08x\n", top_of_kernel_data_pa);
    allocate_ram(sysmem, "n45.extra", phys_ptr, 0x300000);
    phys_ptr += align_64k_high(0x300000);
    uint32_t mem_size = 0x8000000; // TODO hard-coded

    // load the device tree
    macho_load_dtb(nms->dtb_filename, nsas, sysmem, "devicetree.45", phys_ptr, &dtb_size, &nms->uart_mmio_pa);
    uint32_t dtb_va = phys_ptr;

    // load boot args
    macho_setup_bootargs("k_bootargs.n45", nsas, sysmem, kbootargs_pa, virt_base, IPOD_TOUCH_PHYS_BASE, mem_size, top_of_kernel_data_pa, dtb_va, dtb_size, nms->kern_args);

    allocate_ram(sysmem, "sram1", SRAM1_MEM_BASE, 0x10000);

    // allocate UART ram
    allocate_ram(sysmem, "uart", 0xe0000000, 0x10000);

    // load iBoot
    uint8_t *file_data = NULL;
    unsigned long fsize;
    if (g_file_get_contents("/Users/martijndevos/Documents/ipod_touch_emulation/iboot_patched_usb_timer.bin", (char **)&file_data, &fsize, NULL)) {
         allocate_ram(sysmem, "iboot", IBOOT_BASE, 0x400000);
         address_space_rw(nsas, IBOOT_BASE, MEMTXATTRS_UNSPECIFIED, (uint8_t *)file_data, fsize, 1);
     }

    allocate_ram(sysmem, "unknown1", UNKNOWN1_MEM_BASE, 0x1000);
    allocate_ram(sysmem, "unknown2", UNKNOWN2_MEM_BASE, 0x1000);
    allocate_ram(sysmem, "unknown3", UNKNOWN3_MEM_BASE, 0x10000);
    allocate_ram(sysmem, "chipid", CHIPID_MEM_BASE, align_64k_high(0x1));
    //allocate_ram(sysmem, "unknown5", UNKNOWN5_MEM_BASE, 0x100000);
    allocate_ram(sysmem, "clock0", CLOCK0_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "eic", EIC_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "usbotg", USBOTG_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "usbphys", USBPHYS_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "gpio", GPIO_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "i2c1", I2C1_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "iis", IIS_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "spi", SPI_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "sdci", SDCI_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "watchdog", WATCHDOG_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "adc", ADC_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "rtc", RTC_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "display", DISPLAY_MEM_BASE, align_64k_high(0x1));

    allocate_ram(sysmem, "uart0", UART0_MEM_BASE, align_64k_high(0x4000));
    allocate_ram(sysmem, "uart1", UART1_MEM_BASE, align_64k_high(0x4000));
    allocate_ram(sysmem, "uart2", UART2_MEM_BASE, align_64k_high(0x4000));
    allocate_ram(sysmem, "uart3", UART3_MEM_BASE, align_64k_high(0x4000));
    allocate_ram(sysmem, "uart4", UART4_MEM_BASE, align_64k_high(0x4000));

    // set the security epoch
    uint32_t security_epoch = 0x2000000;
    address_space_rw(nsas, EIC_MEM_BASE+0x44, MEMTXATTRS_UNSPECIFIED, (uint8_t *) &security_epoch, sizeof(security_epoch), 1);
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
    DeviceState *dev = pl192_manual_init("vic0");
    PL192State *s = PL192(dev);
    nms->vic0 = *s;
    memory_region_add_subregion(sysmem, VIC0_MEM_BASE, &nms->vic0.iomem);
    nms->irq[0] = g_malloc0(sizeof(qemu_irq) * 32);
    for (int i = 0; i < 32; i++) { nms->irq[0][i] = qdev_get_gpio_in(dev, i); }

    dev = pl192_manual_init("vic1");
    s = PL192(dev);
    nms->vic1 = *s;
    memory_region_add_subregion(sysmem, VIC1_MEM_BASE, &nms->vic1.iomem);
    nms->irq[1] = g_malloc0(sizeof(qemu_irq) * 32);
    for (int i = 0; i < 32; i++) { nms->irq[1][i] = qdev_get_gpio_in(dev, i); }

    // // chain VICs together
    nms->vic1.daisy = &nms->vic0;

    // init clock
    ipod_touch_init_clock(machine, sysmem);

    // init timer
    ipod_touch_init_timer(machine, sysmem);

    ipod_touch_memory_setup(machine, sysmem, nsas);

    qemu_register_reset(ipod_touch_cpu_reset, nms);
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
