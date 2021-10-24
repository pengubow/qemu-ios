#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/misc/unimp.h"
#include "sysemu/sysemu.h"
#include "sysemu/reset.h"
#include "qemu/error-report.h"
#include "hw/platform-bus.h"

#include "hw/arm/iphone6splus.h"

#define N66_PHYS_BASE (0x40000000)

#define N66_CPREG_FUNCS(name) \
static uint64_t n66_cpreg_read_##name(CPUARMState *env, \
                                      const ARMCPRegInfo *ri) \
{ \
    N66MachineState *nms = (N66MachineState *)ri->opaque; \
    return nms->N66_CPREG_VAR_NAME(name); \
} \
static void n66_cpreg_write_##name(CPUARMState *env, const ARMCPRegInfo *ri, \
                                   uint64_t value) \
{ \
    N66MachineState *nms = (N66MachineState *)ri->opaque; \
    nms->N66_CPREG_VAR_NAME(name) = value; \
}

#define N66_CPREG_DEF(p_name, p_op0, p_op1, p_crn, p_crm, p_op2, p_access) \
    { .cp = CP_REG_ARM64_SYSREG_CP, \
      .name = #p_name, .opc0 = p_op0, .crn = p_crn, .crm = p_crm, \
      .opc1 = p_op1, .opc2 = p_op2, .access = p_access, .type = ARM_CP_IO, \
      .state = ARM_CP_STATE_AA64, .readfn = n66_cpreg_read_##p_name, \
      .writefn = n66_cpreg_write_##p_name }

N66_CPREG_FUNCS(ARM64_REG_HID11)
N66_CPREG_FUNCS(ARM64_REG_HID3)
N66_CPREG_FUNCS(ARM64_REG_HID5)
N66_CPREG_FUNCS(ARM64_REG_HID4)
N66_CPREG_FUNCS(ARM64_REG_HID8)
N66_CPREG_FUNCS(ARM64_REG_HID7)
N66_CPREG_FUNCS(ARM64_REG_LSU_ERR_STS)
N66_CPREG_FUNCS(PMC0)
N66_CPREG_FUNCS(PMC1)
N66_CPREG_FUNCS(PMCR1)
N66_CPREG_FUNCS(PMSR)

// This is the same as the array for kvm, but without
// the L2ACTLR_EL1, which is already defined in TCG.
// Duplicating this list isn't a perfect solution,
// but it's quick and reliable.
static const ARMCPRegInfo n66_cp_reginfo_tcg[] = {
    // Apple-specific registers
    N66_CPREG_DEF(ARM64_REG_HID11, 3, 0, 15, 13, 0, PL1_RW),
    N66_CPREG_DEF(ARM64_REG_HID3, 3, 0, 15, 3, 0, PL1_RW),
    N66_CPREG_DEF(ARM64_REG_HID5, 3, 0, 15, 5, 0, PL1_RW),
    N66_CPREG_DEF(ARM64_REG_HID4, 3, 0, 15, 4, 0, PL1_RW),
    N66_CPREG_DEF(ARM64_REG_HID8, 3, 0, 15, 8, 0, PL1_RW),
    N66_CPREG_DEF(ARM64_REG_HID7, 3, 0, 15, 7, 0, PL1_RW),
    N66_CPREG_DEF(ARM64_REG_LSU_ERR_STS, 3, 3, 15, 0, 0, PL1_RW),
    N66_CPREG_DEF(PMC0, 3, 2, 15, 0, 0, PL1_RW),
    N66_CPREG_DEF(PMC1, 3, 2, 15, 1, 0, PL1_RW),
    N66_CPREG_DEF(PMCR1, 3, 1, 15, 1, 0, PL1_RW),
    N66_CPREG_DEF(PMSR, 3, 1, 15, 13, 0, PL1_RW),
    REGINFO_SENTINEL,
};

static void n66_add_cpregs(N66MachineState *nms)
{
    ARMCPU *cpu = nms->cpu;

    nms->N66_CPREG_VAR_NAME(ARM64_REG_HID11) = 0;
    nms->N66_CPREG_VAR_NAME(ARM64_REG_HID3) = 0;
    nms->N66_CPREG_VAR_NAME(ARM64_REG_HID5) = 0;
    nms->N66_CPREG_VAR_NAME(ARM64_REG_HID8) = 0;
    nms->N66_CPREG_VAR_NAME(ARM64_REG_HID7) = 0;
    nms->N66_CPREG_VAR_NAME(ARM64_REG_LSU_ERR_STS) = 0;
    nms->N66_CPREG_VAR_NAME(PMC0) = 0;
    nms->N66_CPREG_VAR_NAME(PMC1) = 0;
    nms->N66_CPREG_VAR_NAME(PMCR1) = 0;
    nms->N66_CPREG_VAR_NAME(PMSR) = 0;
    nms->N66_CPREG_VAR_NAME(L2ACTLR_EL1) = 0;

    define_arm_cp_regs_with_opaque(cpu, n66_cp_reginfo_tcg, nms);
}

static void n66_ns_memory_setup(MachineState *machine, MemoryRegion *sysmem,
                                AddressSpace *nsas)
{
    uint64_t used_ram_for_blobs = 0;
    hwaddr kernel_low;
    hwaddr kernel_high;
    hwaddr virt_base;
    hwaddr dtb_va;
    uint64_t dtb_size;
    hwaddr kbootargs_pa;
    hwaddr top_of_kernel_data_pa;
    hwaddr mem_size;
    hwaddr remaining_mem_size;
    hwaddr allocated_ram_pa;
    hwaddr phys_ptr;
    hwaddr phys_pc;
    hwaddr ramfb_pa = 0;
    video_boot_args v_bootargs = {0};
    N66MachineState *nms = N66_MACHINE(machine);

    //setup the memory layout:

    //At the beginning of the non-secure ram we have the raw kernel file.
    //After that we have the static trust cache.
    //After that we have all the kernel sections.
    //After that we have ramdosk
    //After that we have the device tree
    //After that we have the kernel boot args
    //After that we have the rest of the RAM

    macho_file_highest_lowest_base(nms->kernel_filename, N66_PHYS_BASE,
                                   &virt_base, &kernel_low, &kernel_high);

    g_virt_base = virt_base;
    g_phys_base = N66_PHYS_BASE;
    phys_ptr = N66_PHYS_BASE;

    //now account for the loaded kernel
    arm_load_macho(nms->kernel_filename, nsas, sysmem, "kernel.n66",
                    N66_PHYS_BASE, virt_base, kernel_low,
                    kernel_high, &phys_pc);
    nms->kpc_pa = phys_pc;
    used_ram_for_blobs += (align_64k_high(kernel_high) - kernel_low);

    phys_ptr = align_64k_high(vtop_static(kernel_high));

    // TODO load other stuff

    //now account for kernel boot args
    used_ram_for_blobs += align_64k_high(sizeof(struct xnu_arm64_boot_args));
    kbootargs_pa = phys_ptr;
    nms->kbootargs_pa = kbootargs_pa;
    phys_ptr += align_64k_high(sizeof(struct xnu_arm64_boot_args));
    nms->extra_data_pa = phys_ptr;
    allocated_ram_pa = phys_ptr;

    // TODO ramfb

    //phys_ptr += align_64k_high(sizeof(AllocatedData));
    top_of_kernel_data_pa = phys_ptr;
    remaining_mem_size = machine->ram_size - used_ram_for_blobs;
    mem_size = allocated_ram_pa - N66_PHYS_BASE + remaining_mem_size;
    macho_setup_bootargs("k_bootargs.n66", nsas, sysmem, kbootargs_pa,
                         virt_base, N66_PHYS_BASE, mem_size,
                         top_of_kernel_data_pa, dtb_va, dtb_size,
                         v_bootargs, nms->kern_args);

    allocate_ram(sysmem, "n66.ram", allocated_ram_pa, remaining_mem_size);
}

static void n66_memory_setup(MachineState *machine,
                             MemoryRegion *sysmem,
                             MemoryRegion *secure_sysmem,
                             AddressSpace *nsas)
{
    n66_ns_memory_setup(machine, sysmem, nsas);
}

static void n66_cpu_setup(MachineState *machine, MemoryRegion **sysmem,
                          MemoryRegion **secure_sysmem, ARMCPU **cpu,
                          AddressSpace **nsas)
{
    Object *cpuobj = object_new(machine->cpu_type);
    *cpu = ARM_CPU(cpuobj);
    CPUState *cs = CPU(*cpu);

    *sysmem = get_system_memory();

    object_property_set_link(cpuobj, "memory", OBJECT(*sysmem), &error_abort);

    //set secure monitor to false
    object_property_set_bool(cpuobj, "has_el3", false, NULL);

    object_property_set_bool(cpuobj, "has_el2", false, NULL);

    object_property_set_bool(cpuobj, "realized", true, &error_fatal);

    *nsas = cpu_get_address_space(cs, ARMASIdx_NS);

    object_unref(cpuobj);
    //currently support only a single CPU and thus
    //use no interrupt controller and wire IRQs from devices directly to the CPU
}

static void n66_cpu_reset(void *opaque)
{
    N66MachineState *nms = N66_MACHINE((MachineState *)opaque);
    ARMCPU *cpu = nms->cpu;
    CPUState *cs = CPU(cpu);
    CPUARMState *env = &cpu->env;

    cpu_reset(cs);

    env->xregs[0] = nms->kbootargs_pa;
    env->pc = nms->kpc_pa;
}

static void n66_machine_init(MachineState *machine)
{
    N66MachineState *nms = N66_MACHINE(machine);
    MemoryRegion *sysmem;
    MemoryRegion *secure_sysmem;
    AddressSpace *nsas;
    ARMCPU *cpu;
    CPUState *cs;
    DeviceState *cpudev;

    n66_cpu_setup(machine, &sysmem, &secure_sysmem, &cpu, &nsas);

    nms->cpu = cpu;

    n66_memory_setup(machine, sysmem, secure_sysmem, nsas);

    // TODO finish

    n66_add_cpregs(nms);

    // TODO finish

    qemu_register_reset(n66_cpu_reset, nms);
}

static void n66_set_kernel_filename(Object *obj, const char *value,
                                     Error **errp)
{
    N66MachineState *nms = N66_MACHINE(obj);

    g_strlcpy(nms->kernel_filename, value, sizeof(nms->kernel_filename));
}

static char *n66_get_kernel_filename(Object *obj, Error **errp)
{
    N66MachineState *nms = N66_MACHINE(obj);
    return g_strdup(nms->kernel_filename);
}

static void n66_set_kern_args(Object *obj, const char *value,
                                     Error **errp)
{
    N66MachineState *nms = N66_MACHINE(obj);

    g_strlcpy(nms->kern_args, value, sizeof(nms->kern_args));
}

static char *n66_get_kern_args(Object *obj, Error **errp)
{
    N66MachineState *nms = N66_MACHINE(obj);
    return g_strdup(nms->kern_args);
}

static void n66_instance_init(Object *obj)
{
    object_property_add_str(obj, "kernel-filename", n66_get_kernel_filename, n66_set_kernel_filename);
    object_property_set_description(obj, "kernel-filename", "Set the kernel filename");

    object_property_add_str(obj, "kern-cmd-args", n66_get_kern_args, n66_set_kern_args);
    object_property_set_description(obj, "kern-cmd-args", "Set the XNU kernel cmd args");

    // TODO
}

static void n66_machine_class_init(ObjectClass *klass, void *data)
{
    MachineClass *mc = MACHINE_CLASS(klass);
    mc->desc = "iPhone 6s plus (n66 - s8000)";
    mc->init = n66_machine_init;
    mc->max_cpus = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a57");
    mc->minimum_page_bits = 12;
}

static const TypeInfo n66_machine_info = {
    .name          = TYPE_N66_MACHINE,
    .parent        = TYPE_MACHINE,
    .instance_size = sizeof(N66MachineState),
    .class_size    = sizeof(N66MachineClass),
    .class_init    = n66_machine_class_init,
    .instance_init = n66_instance_init,
};

static void n66_machine_types(void)
{
    type_register_static(&n66_machine_info);
}

type_init(n66_machine_types)
