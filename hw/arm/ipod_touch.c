#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "sysemu/reset.h"
#include "hw/arm/xnu.h"
#include "hw/arm/xnu_mem.h"
#include "hw/arm/ipod_touch.h"

#define IPOD_TOUCH_PHYS_BASE (0xc0000000)
#define SRAM1_MEM_BASE 0x22020000

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
    macho_load_dtb(nms->dtb_filename, nsas, sysmem, "devicetree.45", phys_ptr, &dtb_size);
    uint32_t dtb_va = phys_ptr;

    // load boot args
    macho_setup_bootargs("k_bootargs.n45", nsas, sysmem, kbootargs_pa, virt_base, IPOD_TOUCH_PHYS_BASE, mem_size, top_of_kernel_data_pa, dtb_va, dtb_size, nms->kern_args);

    allocate_ram(sysmem, "sram1", SRAM1_MEM_BASE, 0x10000);

    // allocate UART ram
    allocate_ram(sysmem, "uart", 0xe0000000, 0x10000);
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
