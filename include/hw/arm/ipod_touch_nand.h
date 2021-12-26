#ifndef HW_ARM_IPOD_TOUCH_NAND_H
#define HW_ARM_IPOD_TOUCH_NAND_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/platform-bus.h"
#include "hw/hw.h"
#include "hw/irq.h"

#define NAND_FMCTRL0 0x0
#define NAND_CMD     0x8
#define NAND_FMCSTAT 0x48
#define NAND_FMFIFO  0x80
#define NAND_RSCTRL  0x100

#define NAND_CMD_ID  0x90
#define NAND_CMD_READSTATUS 0x70

#define TYPE_ITNAND "itnand"
OBJECT_DECLARE_SIMPLE_TYPE(ITNandState, ITNAND)

typedef struct ITNandState {
    SysBusDevice busdev;
    MemoryRegion iomem;
    uint32_t fmctrl0;
	uint32_t rsctrl;
	uint32_t cmd;
    qemu_irq irq;
} ITNandState;

#endif