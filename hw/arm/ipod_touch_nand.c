#include "hw/arm/ipod_touch_nand.h"

static int nand_read_cnt = 0;

static uint64_t itnand_read(void *opaque, hwaddr addr, unsigned size)
{
    ITNandState *s = (ITNandState *) opaque;
    //fprintf(stderr, "%s: reading from 0x%08x\n", __func__, addr);

    switch (addr) {
        case NAND_FMCTRL0:
            return s->fmctrl0;
        case NAND_FMFIFO:
            if(s->cmd == NAND_CMD_ID) {
                return 0xA514D3AD;
            }
            else if(s->cmd == NAND_CMD_READSTATUS) {
                return (1 << 6);
            }
            else {
                nand_read_cnt++;
                return 0x43303032;
            }

        case NAND_FMCSTAT:
            return (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12); // this indicates that everything is ready, including our eight banks
        case NAND_RSCTRL:
            return s->rsctrl;
        default:
            break;
    }
    return 0;
}

static void itnand_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    ITNandState *s = (ITNandState *) opaque;

    switch(addr) {
        case NAND_FMCTRL0:
            s->fmctrl0 = val;
            break;
        case NAND_CMD:
            s->cmd = val;
            break;
        case NAND_RSCTRL:
            s->rsctrl = val;
            break;
        default:
            break;
    }
}

static const MemoryRegionOps nand_ops = {
    .read = itnand_read,
    .write = itnand_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void itnand_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    ITNandState *s = ITNAND(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &nand_ops, s, "nand", 0x1000);
    sysbus_init_irq(sbd, &s->irq);
}

static void itnand_reset(DeviceState *d)
{
    ITNandState *s = (ITNandState *) d;

    s->fmctrl0 = 0;
    s->rsctrl = 0;
    s->cmd = 0;
}

static void itnand_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->reset = itnand_reset;
}

static const TypeInfo itnand_info = {
    .name          = TYPE_ITNAND,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ITNandState),
    .instance_init = itnand_init,
    .class_init    = itnand_class_init,
};

static void itnand_register_types(void)
{
    type_register_static(&itnand_info);
}

type_init(itnand_register_types)
