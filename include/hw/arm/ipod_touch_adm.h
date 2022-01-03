#ifndef IPOD_TOUCH_ADM_H
#define IPOD_TOUCH_ADM_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "hw/irq.h"

#define TYPE_IPOD_TOUCH_ADM                "ipodtouch.adm"
OBJECT_DECLARE_SIMPLE_TYPE(IPodTouchADMState, IPOD_TOUCH_ADM)

typedef struct IPodTouchADMState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
} IPodTouchADMState;

#endif