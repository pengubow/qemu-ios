#ifndef IPOD_TOUCH_I2C_H
#define IPOD_TOUCH_I2C_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "hw/i2c/i2c.h"
#include "hw/irq.h"

#define TYPE_IPOD_TOUCH_I2C                  "ipodtouch.i2c"
OBJECT_DECLARE_SIMPLE_TYPE(IPodTouchI2CState, IPOD_TOUCH_I2C)

#define I2CCON_ADDR                       0x00  /* control register */
#define I2CSTAT_ADDR                      0x04  /* control/status register */
#define I2CADD_ADDR                       0x08  /* address register */
#define I2CDS_ADDR                        0x0c  /* data shift register */
#define I2CLC_ADDR                        0x10  /* line control register */
#define I2C_REG20						  0x20  /* unknown, some kind of control register */

#define I2CCON_ACK_GEN                    (1 << 7)
#define I2CCON_INTRS_EN                   (1 << 5)
#define I2CCON_INT_PEND                   (1 << 4)

#define IPOD_TOUCH_I2C_MODE(reg)             (((reg) >> 6) & 3)
#define I2C_IN_MASTER_MODE(reg)           (((reg) >> 6) & 2)
#define I2CMODE_MASTER_Rx                 0x2
#define I2CMODE_MASTER_Tx                 0x3
#define I2CSTAT_LAST_BIT                  (1 << 0)
#define I2CSTAT_OUTPUT_EN                 (1 << 4)
#define I2CSTAT_START_BUSY                (1 << 5)

typedef struct IPodTouchI2CState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    I2CBus *bus;
    qemu_irq irq;

    uint8_t i2ccon;
    uint8_t i2cstat;
    uint8_t i2cadd;
    uint8_t i2cds;
    uint8_t i2clc;
    uint32_t iicreg20;
    bool scl_free;
} IPodTouchI2CState;

#endif