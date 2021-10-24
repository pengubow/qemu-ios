#ifndef HW_ARM_N66_H
#define HW_ARM_N66_H

#include "qemu-common.h"
#include "exec/hwaddr.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/arm/xnu.h"
#include "exec/memory.h"
#include "cpu.h"
#include "sysemu/kvm.h"

#define TYPE_N66 "iPhone6splus-n66-s8000"

#define TYPE_N66_MACHINE   MACHINE_TYPE_NAME(TYPE_N66)

#define N66_MACHINE(obj) \
    OBJECT_CHECK(N66MachineState, (obj), TYPE_N66_MACHINE)

#define N66_CPREG_VAR_NAME(name) cpreg_##name
#define N66_CPREG_VAR_DEF(name) uint64_t N66_CPREG_VAR_NAME(name)

typedef struct {
    MachineClass parent;
} N66MachineClass;

typedef struct {
    MachineState parent;
    hwaddr extra_data_pa;
    hwaddr kpc_pa;
    hwaddr kbootargs_pa;
    ARMCPU *cpu;
    char kernel_filename[1024];
    char kern_args[1024];
    N66_CPREG_VAR_DEF(ARM64_REG_HID11);
    N66_CPREG_VAR_DEF(ARM64_REG_HID3);
    N66_CPREG_VAR_DEF(ARM64_REG_HID5);
    N66_CPREG_VAR_DEF(ARM64_REG_HID4);
    N66_CPREG_VAR_DEF(ARM64_REG_HID8);
    N66_CPREG_VAR_DEF(ARM64_REG_HID7);
    N66_CPREG_VAR_DEF(ARM64_REG_LSU_ERR_STS);
    N66_CPREG_VAR_DEF(PMC0);
    N66_CPREG_VAR_DEF(PMC1);
    N66_CPREG_VAR_DEF(PMCR1);
    N66_CPREG_VAR_DEF(PMSR);
    N66_CPREG_VAR_DEF(L2ACTLR_EL1);
} N66MachineState;

#endif