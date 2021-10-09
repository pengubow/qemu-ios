#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "sysemu/reset.h"
#include "hw/arm/ipod_touch.h"

#define WATCHDOG_MEM_BASE 0x3E300000
#define VIC0_MEM_BASE 0x38E00000
#define VIC1_MEM_BASE 0x38E01000
#define CLOCK0_MEM_BASE 0x38100000
#define EIC_MEM_BASE 0x39A00000
#define SYS_CONTROLLER_MEM_BASE 0x3C500000
#define SRAM1_MEM_BASE 0x22020000
#define HDMI_LINK_MEM_BASE 0x3E200000
#define IIC2_MEM_BASE 0x3E400000
#define SPI0_MEM_BASE 0x3C300000
#define CHIPID_MEM_BASE 0x3D100000
#define GPIO_MEM_BASE 0x3CF00000
#define GPIOIC_MEM_BASE 0x39700000
#define TIMER_MEM_BASE 0x3C700000
#define USBOTG_MEM_BASE 0x38400000
#define USBPHYS_MEM_BASE 0x3C400000

#define LLB_MEM_BASE 0x22000000

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

    cpu_set_pc(CPU(cpu), 0x22020000);
}

static uint32_t align_64k_high(uint32_t addr)
{
    return (addr + 0xffffull) & ~0xffffull;
}

static void allocate_ram(MemoryRegion *top, const char *name, uint32_t addr, uint32_t size)
{
        MemoryRegion *sec = g_new(MemoryRegion, 1);
        memory_region_init_ram(sec, NULL, name, size, &error_fatal);
        memory_region_add_subregion(top, addr, sec);
}

static void allocate_and_copy(MemoryRegion *mem, AddressSpace *as, const char *name, uint32_t pa, uint32_t size, void *buf)
{
    if (mem) {
        allocate_ram(mem, name, pa, align_64k_high(size));
    }
    address_space_rw(as, pa, MEMTXATTRS_UNSPECIFIED, (uint8_t *)buf, size, 1);
}

static void ipod_touch_memory_setup(MachineState *machine, MemoryRegion *sysmem, AddressSpace *nsas)
{
	// load SecureROM in memory
    uint8_t *file_data = NULL;
    unsigned long fsize;

    if (g_file_get_contents("/Users/martijndevos/Documents/ipod_touch_emulation/ipodtouch2/rom2.bin", (char **)&file_data, &fsize, NULL)) {
        allocate_and_copy(sysmem, nsas, "SecureROM", 0x22020000, fsize, file_data);
    }

    // allocate memory for controllers
    allocate_ram(sysmem, "testz", 0x0, align_64k_high(0x10));

    allocate_ram(sysmem, "watchdog", WATCHDOG_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "vic0", VIC0_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "vic1", VIC1_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "clock0", CLOCK0_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "eic", EIC_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "syscontrollers", SYS_CONTROLLER_MEM_BASE, align_64k_high(0x10));
    //allocate_ram(sysmem, "sram1", SRAM1_MEM_BASE, 0x20000);
    allocate_ram(sysmem, "hdmilink", HDMI_LINK_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "iic2", IIC2_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "spi0", SPI0_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "chipid", CHIPID_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "gpio", GPIO_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "gpioic", GPIOIC_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "timer", TIMER_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "usbotg", USBOTG_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "usbphys", USBPHYS_MEM_BASE, align_64k_high(0x10));
    allocate_ram(sysmem, "unknown1", 0xfffffffc, 0x1);

    // if (g_file_get_contents("/Users/martijndevos/Documents/ipod_touch_emulation/LLB.n45ap.RELEASE", (char **)&file_data, &fsize, NULL)) {
    //     allocate_ram(sysmem, "llb", LLB_MEM_BASE, 0x20000);
    //     address_space_rw(nsas, LLB_MEM_BASE, MEMTXATTRS_UNSPECIFIED, file_data, fsize, 1);
    // }

    // set "UCLKCON" (??) to 1
    uint8_t val = 0x1;
    address_space_rw(nsas, 0x3c500040, MEMTXATTRS_UNSPECIFIED, &val, sizeof(val), 1);
}

static void ipod_touch_instance_init(Object *obj)
{
	// TODO
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
