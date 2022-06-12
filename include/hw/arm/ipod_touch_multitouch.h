#ifndef IPOD_TOUCH_MULTITOUCH_H
#define IPOD_TOUCH_MULTITOUCH_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/ssi/ssi.h"

#define TYPE_IPOD_TOUCH_MULTITOUCH                "ipodtouch.multitouch"
OBJECT_DECLARE_SIMPLE_TYPE(IPodTouchMultitouchState, IPOD_TOUCH_MULTITOUCH)

typedef struct IPodTouchMultitouchState {
    SSIPeripheral ssidev;
    uint32_t cur_cmd;
} IPodTouchMultitouchState;

#endif