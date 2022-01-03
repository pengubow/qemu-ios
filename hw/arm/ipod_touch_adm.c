#include "hw/arm/ipod_touch_adm.h"

static uint64_t ipod_touch_adm_read(void *opaque, hwaddr offset, unsigned size)
{
    IPodTouchADMState *s = (IPodTouchADMState *)opaque;

    fprintf(stderr, "s5l8900_adm_read(): offset = 0x%08x\n", offset);

    // TODO

    return 0;
}

static void ipod_touch_adm_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    IPodTouchADMState *s = (IPodTouchADMState *)opaque;

    fprintf(stderr, "s5l8900_adm_write: offset = 0x%08x, val = 0x%08x\n", offset, value);
}

static const MemoryRegionOps ipod_touch_adm_ops = {
    .read = ipod_touch_adm_read,
    .write = ipod_touch_adm_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ipod_touch_adm_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    IPodTouchADMState *s = IPOD_TOUCH_ADM(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &ipod_touch_adm_ops, s, TYPE_IPOD_TOUCH_ADM, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void ipod_touch_adm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
}

static const TypeInfo ipod_touch_adm_type_info = {
    .name = TYPE_IPOD_TOUCH_ADM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IPodTouchADMState),
    .instance_init = ipod_touch_adm_init,
    .class_init = ipod_touch_adm_class_init,
};

static void ipod_touch_adm_register_types(void)
{
    type_register_static(&ipod_touch_adm_type_info);
}

type_init(ipod_touch_adm_register_types)
