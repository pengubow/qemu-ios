/*
 *  IPod Touch I2C Bus Serial Interface Emulation
 *
 *  Copyright (C) 2012 Samsung Electronics Co Ltd.
 *    Maksim Kozlov, <m.kozlov@samsung.com>
 *    Igor Mitsyanko, <i.mitsyanko@samsung.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "hw/i2c/ipod_touch_i2c.h"

#ifndef IPOD_TOUCH_I2C_DEBUG
#define IPOD_TOUCH_I2C_DEBUG                 0
#endif

#if IPOD_TOUCH_I2C_DEBUG
#define DPRINT(fmt, args...)              \
    do { fprintf(stderr, "QEMU I2C: "fmt, ## args); } while (0)

static const char *ipod_touch_i2c_get_regname(unsigned offset)
{
    switch (offset) {
    case I2CCON_ADDR:
        return "I2CCON";
    case I2CSTAT_ADDR:
        return "I2CSTAT";
    case I2CADD_ADDR:
        return "I2CADD";
    case I2CDS_ADDR:
        return "I2CDS";
    case I2CLC_ADDR:
        return "I2CLC";
    case I2C_REG20:
        return "I2CREG20";
    default:
        return "[?]";
    }
}

#else
#define DPRINT(fmt, args...)              do { } while (0)
#endif

static inline void ipod_touch_i2c_raise_interrupt(IPodTouchI2CState *s)
{
    if (s->i2ccon & I2CCON_INTRS_EN) {
        s->i2ccon |= I2CCON_INT_PEND;
        qemu_irq_raise(s->irq);
    }
}

static void ipod_touch_i2c_data_receive(void *opaque)
{
    IPodTouchI2CState *s = (IPodTouchI2CState *)opaque;

    s->i2cstat &= ~I2CSTAT_LAST_BIT;
    s->scl_free = false;
    s->i2cds = i2c_recv(s->bus);
    ipod_touch_i2c_raise_interrupt(s);
}

static void ipod_touch_i2c_data_send(void *opaque)
{
    IPodTouchI2CState *s = (IPodTouchI2CState *)opaque;

    s->i2cstat &= ~I2CSTAT_LAST_BIT;
    s->scl_free = false;
    s->iicreg20 |= 0x100;
    if (i2c_send(s->bus, s->i2cds) < 0 && (s->i2ccon & I2CCON_ACK_GEN)) {
        s->i2cstat |= I2CSTAT_LAST_BIT;
    }
    ipod_touch_i2c_raise_interrupt(s);
}

static uint64_t ipod_touch_i2c_read(void *opaque, hwaddr offset,
                                 unsigned size)
{
    IPodTouchI2CState *s = (IPodTouchI2CState *)opaque;
    uint8_t value;

    switch (offset) {
    case I2CCON_ADDR:
        value = s->i2ccon;
        break;
    case I2CSTAT_ADDR:
        value = s->i2cstat;
        break;
    case I2CADD_ADDR:
        value = s->i2cadd;
        break;
    case I2CDS_ADDR:
        value = s->i2cds;
        s->scl_free = true;
        s->iicreg20 |= 0x100;
        if (IPOD_TOUCH_I2C_MODE(s->i2cstat) == I2CMODE_MASTER_Rx &&
               (s->i2cstat & I2CSTAT_START_BUSY) &&
               !(s->i2ccon & I2CCON_INT_PEND)) {
            ipod_touch_i2c_data_receive(s);
        }
        break;
    case I2CLC_ADDR:
        value = s->i2clc;
        break;
    case I2C_REG20:
        {
            uint32_t tmp_reg20 = s->iicreg20;
            s->iicreg20 &= ~0x100;
            s->iicreg20 &= ~0x2000;
            return tmp_reg20;
        }
    default:
        value = 0;
        DPRINT("ERROR: Bad read offset 0x%x\n", (unsigned int)offset);
        break;
    }

    DPRINT("read %s [0x%02x] -> 0x%02x\n", ipod_touch_i2c_get_regname(offset),
            (unsigned int)offset, value);
    return value;
}

static void ipod_touch_i2c_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    IPodTouchI2CState *s = (IPodTouchI2CState *)opaque;
    uint8_t v = value & 0xff;

    DPRINT("write %s [0x%02x] <- 0x%02x\n", ipod_touch_i2c_get_regname(offset),
            (unsigned int)offset, v);

    switch (offset) {
    case I2CCON_ADDR:
        if(value & ~(I2CCON_ACK_GEN)) {
            s->iicreg20 |= 0x100;
        }
        if((value & 0x10) && (s->i2cstat == 0x90))  {
            s->iicreg20 |= 0x2000;
        }

        s->i2ccon = (v & ~I2CCON_INT_PEND) | (s->i2ccon & I2CCON_INT_PEND);
        if ((s->i2ccon & I2CCON_INT_PEND) && !(v & I2CCON_INT_PEND)) {
            s->i2ccon &= ~I2CCON_INT_PEND;
            qemu_irq_lower(s->irq);
            if (!(s->i2ccon & I2CCON_INTRS_EN)) {
                s->i2cstat &= ~I2CSTAT_START_BUSY;
            }

            if (s->i2cstat & I2CSTAT_START_BUSY) {
                if (s->scl_free) {
                    if (IPOD_TOUCH_I2C_MODE(s->i2cstat) == I2CMODE_MASTER_Tx) {
                        ipod_touch_i2c_data_send(s);
                    } else if (IPOD_TOUCH_I2C_MODE(s->i2cstat) ==
                            I2CMODE_MASTER_Rx) {
                        ipod_touch_i2c_data_receive(s);
                    }
                } else {
                    s->i2ccon |= I2CCON_INT_PEND;
                    qemu_irq_raise(s->irq);
                }
            }
        }
        break;
    case I2CSTAT_ADDR:
        s->i2cstat =
                (s->i2cstat & I2CSTAT_START_BUSY) | (v & ~I2CSTAT_START_BUSY);

        if (!(s->i2cstat & I2CSTAT_OUTPUT_EN)) {
            s->i2cstat &= ~I2CSTAT_START_BUSY;
            s->scl_free = true;
            qemu_irq_lower(s->irq);
            break;
        }

        /* Nothing to do if in i2c slave mode */
        if (!I2C_IN_MASTER_MODE(s->i2cstat)) {
            break;
        }

        if (v & I2CSTAT_START_BUSY) {
            s->i2cstat &= ~I2CSTAT_LAST_BIT;
            s->i2cstat |= I2CSTAT_START_BUSY;    /* Line is busy */
            s->scl_free = false;

            /* Generate start bit and send slave address */
            s->iicreg20 |= 0x100;
            if (i2c_start_transfer(s->bus, s->i2cds >> 1, s->i2cds & 0x1) &&
                    (s->i2ccon & I2CCON_ACK_GEN)) {
                s->i2cstat |= I2CSTAT_LAST_BIT;
            } else if (IPOD_TOUCH_I2C_MODE(s->i2cstat) == I2CMODE_MASTER_Rx) {
                ipod_touch_i2c_data_receive(s);
            }
            ipod_touch_i2c_raise_interrupt(s);
        } else {
            i2c_end_transfer(s->bus);
            if (!(s->i2ccon & I2CCON_INT_PEND)) {
                s->i2cstat &= ~I2CSTAT_START_BUSY;
            }
            s->scl_free = true;
        }
        break;
    case I2CADD_ADDR:
        if ((s->i2cstat & I2CSTAT_OUTPUT_EN) == 0) {
            s->i2cadd = v;
        }
        break;
    case I2CDS_ADDR:
        if (s->i2cstat & I2CSTAT_OUTPUT_EN) {
            s->i2cds = v;
            s->scl_free = true;
            if (IPOD_TOUCH_I2C_MODE(s->i2cstat) == I2CMODE_MASTER_Tx &&
                    (s->i2cstat & I2CSTAT_START_BUSY) &&
                    !(s->i2ccon & I2CCON_INT_PEND)) {
                ipod_touch_i2c_data_send(s);
            }
        }
        break;
    case I2CLC_ADDR:
        s->i2clc = v;
        break;
    case I2C_REG20:
        break;
    default:
        DPRINT("ERROR: Bad write offset 0x%x\n", (unsigned int)offset);
        break;
    }
}

static const MemoryRegionOps ipod_touch_i2c_ops = {
    .read = ipod_touch_i2c_read,
    .write = ipod_touch_i2c_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ipod_touch_i2c_reset(DeviceState *d)
{
    IPodTouchI2CState *s = IPOD_TOUCH_I2C(d);

    s->i2ccon  = 0x00;
    s->i2cstat = 0x00;
    s->i2cds   = 0xFF;
    s->i2clc   = 0x00;
    s->i2cadd  = 0xFF;
    s->iicreg20 = 0x0;
    s->scl_free = true;
}

static void ipod_touch_i2c_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    IPodTouchI2CState *s = IPOD_TOUCH_I2C(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &ipod_touch_i2c_ops, s, TYPE_IPOD_TOUCH_I2C, 0x100);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    s->bus = i2c_init_bus(dev, "i2c");
}

static void ipod_touch_i2c_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->reset = ipod_touch_i2c_reset;
}

static const TypeInfo ipod_touch_i2c_type_info = {
    .name = TYPE_IPOD_TOUCH_I2C,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IPodTouchI2CState),
    .instance_init = ipod_touch_i2c_init,
    .class_init = ipod_touch_i2c_class_init,
};

static void ipod_touch_i2c_register_types(void)
{
    type_register_static(&ipod_touch_i2c_type_info);
}

type_init(ipod_touch_i2c_register_types)
