#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/misc/unimp.h"
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
#define EIC_MEM_BASE 0x39A00000
#define SYS_CONTROLLER_MEM_BASE 0x3C500000
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

static void s5l8900_timer1_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    // TODO
}

static uint64_t s5l8900_timer1_read(void *opaque, hwaddr addr, unsigned size)
{
    // TODO
    return 0;
}

static const MemoryRegionOps timer1_ops = {
    .read = s5l8900_timer1_read,
    .write = s5l8900_timer1_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

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

    allocate_ram(sysmem, "vic0", VIC0_MEM_BASE, 0x1000);
    allocate_ram(sysmem, "vic1", VIC1_MEM_BASE, 0x1000);
    allocate_ram(sysmem, "unknown1", UNKNOWN1_MEM_BASE, 0x1000);
    allocate_ram(sysmem, "unknown2", UNKNOWN2_MEM_BASE, 0x1000);
    allocate_ram(sysmem, "unknown3", UNKNOWN3_MEM_BASE, 0x10000);
    allocate_ram(sysmem, "chipid", CHIPID_MEM_BASE, align_64k_high(0x1));
    //allocate_ram(sysmem, "unknown5", UNKNOWN5_MEM_BASE, 0x100000);
    allocate_ram(sysmem, "clock0", CLOCK0_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "eic", EIC_MEM_BASE, align_64k_high(0x1));
    allocate_ram(sysmem, "syscontrollers", SYS_CONTROLLER_MEM_BASE, align_64k_high(0x1));
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

    // setup MMIO
    MemoryRegion *iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, NULL, &timer1_ops, NULL, "timer1", 0x10000);
    memory_region_add_subregion(sysmem, TIMER1_MEM_BASE, iomem);

    // set the security epoch
    uint32_t security_epoch = 0x2000000;
    address_space_rw(nsas, EIC_MEM_BASE+0x44, MEMTXATTRS_UNSPECIFIED, (uint8_t *) &security_epoch, sizeof(security_epoch), 1);

    // write the VIC identification registers
    uint8_t value = 0x92;
    address_space_rw(nsas, VIC0_MEM_BASE+0xFE0, MEMTXATTRS_UNSPECIFIED, (uint8_t *) &value, sizeof(value), 1);
    address_space_rw(nsas, VIC1_MEM_BASE+0xFE0, MEMTXATTRS_UNSPECIFIED, (uint8_t *) &value, sizeof(value), 1);
    value = 0x1;
    address_space_rw(nsas, VIC0_MEM_BASE+0xFE4, MEMTXATTRS_UNSPECIFIED, (uint8_t *) &value, sizeof(value), 1);
    address_space_rw(nsas, VIC1_MEM_BASE+0xFE4, MEMTXATTRS_UNSPECIFIED, (uint8_t *) &value, sizeof(value), 1);
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

static void ipod_touch_create_s3c_uart(const IPodTouchMachineState *nms, Chardev *chr)
{
    qemu_irq irq;
    DeviceState *d;
    SysBusDevice *s;
    uint32_t base = nms->uart_mmio_pa;

    //hack for now. create a device that is not used just to have a dummy
    //unused interrupt
    d = qdev_new(TYPE_PLATFORM_BUS_DEVICE);
    s = SYS_BUS_DEVICE(d);
    sysbus_init_irq(s, &irq);
    //pass a dummy irq as we don't need nor want interrupts for this UART
    DeviceState *dev = exynos4210_uart_create(base, 256, 0, chr, irq);
    if (!dev) {
        abort();
    }
}

static void ipod_touch_machine_init(MachineState *machine)
{
	IPodTouchMachineState *nms = IPOD_TOUCH_MACHINE(machine);
	MemoryRegion *sysmem;
    AddressSpace *nsas;
    ARMCPU *cpu;
    DeviceState *cpudev;

    ipod_touch_cpu_setup(machine, &sysmem, &cpu, &nsas);

    nms->cpu = cpu;

    ipod_touch_memory_setup(machine, sysmem, nsas);

    ipod_touch_create_s3c_uart(nms, serial_hd(0));

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
