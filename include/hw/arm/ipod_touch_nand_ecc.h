#ifndef HW_ARM_IPOD_TOUCH_NAND_ECC_H
#define HW_ARM_IPOD_TOUCH_NAND_ECC_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/platform-bus.h"
#include "hw/hw.h"
#include "hw/irq.h"

#define NANDECC_DATA 0x4
#define NANDECC_ECC 0x8
#define NANDECC_START 0xC
#define NANDECC_STATUS 0x10
#define NANDECC_SETUP 0x14
#define NANDECC_CLEARINT 0x40

#define TYPE_ITNANDECC "itnand_ecc"
OBJECT_DECLARE_SIMPLE_TYPE(ITNandECCState, ITNANDECC)

typedef struct ITNandECCState {
    SysBusDevice busdev;
    MemoryRegion iomem;
    uint32_t data_addr;
    uint32_t ecc_addr;
    uint32_t status;
    uint32_t setup;
    qemu_irq irq;
} ITNandECCState;

#endif