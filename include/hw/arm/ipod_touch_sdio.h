#ifndef IPOD_TOUCH_SDIO_H
#define IPOD_TOUCH_SDIO_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"

#define TYPE_IPOD_TOUCH_SDIO                "ipodtouch.sdio"
OBJECT_DECLARE_SIMPLE_TYPE(IPodTouchSDIOState, IPOD_TOUCH_SDIO)

#define SDIO_CMD        0x8
#define SDIO_ARGU       0xC
#define SDIO_DSTA       0x18
#define SDIO_CSR        0x34
#define SDIO_IRQMASK    0x3C

typedef struct IPodTouchSDIOState
{
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint32_t cmd;
    uint32_t arg;
    uint32_t csr;
    uint32_t irq_mask;
} IPodTouchSDIOState;

#endif