#ifndef IPOD_TOUCH_SYSIC_H
#define IPOD_TOUCH_SYSIC_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "hw/irq.h"

#define TYPE_IPOD_TOUCH_SYSIC                "ipodtouch.sysic"
OBJECT_DECLARE_SIMPLE_TYPE(IPodTouchSYSICState, IPOD_TOUCH_SYSIC)

#define POWER_ID 0x44
#define POWER_ONCTRL 0xC
#define POWER_OFFCTRL 0x10
#define POWER_SETSTATE 0x8
#define POWER_STATE 0x14 // seems to be toggled by writing a 1 to the right device ID - cleared to 0 when the device has started.

#define POWER_ID_ADM 0x10

typedef struct IPodTouchSYSICState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    uint32_t power_state;
} IPodTouchSYSICState;

#endif