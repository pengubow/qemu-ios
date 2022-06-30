#ifndef IPOD_TOUCH_LCD_H
#define IPOD_TOUCH_LCD_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "hw/irq.h"

#define TYPE_IPOD_TOUCH_LCD                "ipodtouch.lcd"
OBJECT_DECLARE_SIMPLE_TYPE(IPodTouchLCDState, IPOD_TOUCH_LCD)

typedef struct IPodTouchLCDState
{
    SysBusDevice parent_obj;
    MemoryRegion *sysmem;
    MemoryRegion iomem;
    QemuConsole *con;
    int invalidate;
    MemoryRegionSection fbsection;
    qemu_irq irq;
    uint32_t lcd_con;
    uint32_t lcd_con2;
    uint32_t unknown1;
    uint32_t unknown2;

    uint32_t wnd_con;

    uint32_t vid_con0;
    uint32_t vid_con1;

    uint32_t vidt_con0;
    uint32_t vidt_con1;
    uint32_t vidt_con2;
    uint32_t vidt_con3;

    uint32_t w1_hspan;
    uint32_t w1_framebuffer_base;
    uint32_t w1_display_resolution_info;
    uint32_t w1_display_depth_info;
    uint32_t w1_qlen;

    uint32_t w2_hspan;
    uint32_t w2_framebuffer_base;
    uint32_t w2_display_resolution_info;
    uint32_t w2_display_depth_info;
    uint32_t w2_qlen;
} IPodTouchLCDState;

#endif