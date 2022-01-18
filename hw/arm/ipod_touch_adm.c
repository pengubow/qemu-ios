#include "hw/arm/ipod_touch_adm.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"

static uint64_t ipod_touch_adm_read(void *opaque, hwaddr offset, unsigned size)
{
    //IPodTouchADMState *s = (IPodTouchADMState *)opaque;

    fprintf(stderr, "s5l8900_adm_read(): offset = 0x%08x\n", offset);

    switch (offset) {
        case ADM_CTRL: // this seems to be the control register
            return 0x2; // this seems to indicate that the device is ready
        case ADM_CTRL2:
            return 0x10; // indicates that a upload event has been finished
        default:
            break;
    }

    return 0;
}

static void ipod_touch_adm_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    IPodTouchADMState *s = (IPodTouchADMState *)opaque;

    fprintf(stderr, "s5l8900_adm_write: offset = 0x%08x, val = 0x%08x\n", offset, value);

    switch(offset) {
        case ADM_CTRL:
            if(value == 0x3) {
                // some kind of start-up command?
                // write some bits to data2_sec_addr to indicate that the device is started
                uint32_t *data = (uint32_t *) malloc(sizeof(uint32_t));
                data[0] = 0x50;
                address_space_rw(&s->downstream_as, s->data2_sec_addr, MEMTXATTRS_UNSPECIFIED, (uint8_t *)data, 4, 1);

                // dunno, write some bytes to data4_sec_addr to indicate that the NAND banks are ready
                data = (uint32_t *) malloc(sizeof(uint32_t) * 8);
                for(int i = 0; i < 8; i++) {
                    data[i] = NAND_CHIP_ID;
                }
                
                address_space_rw(&s->downstream_as, s->data3_sec_addr, MEMTXATTRS_UNSPECIFIED, (uint8_t *)data, 8 * sizeof(uint32_t), 1);
            }
            break;
        case ADM_CTRL2:
            if(value == 0x2) {
                // TODO TESTING
                qemu_irq_raise(s->irq);
            }
            if((value & 0x2) == 0) {
                qemu_irq_lower(s->irq);
            }
            break;
        case ADM_CODE_SEC_ADDR:
            s->code_sec_addr = value;
            break;
        case ADM_DATA1_SEC_ADDR:
            s->data1_sec_addr = value;
            break;
        case ADM_DATA2_SEC_ADDR:
            s->data2_sec_addr = value;
            break;
        case ADM_DATA3_SEC_ADDR:
            s->data3_sec_addr = value;
            break;
        default:
            break;
    }
}

static const MemoryRegionOps ipod_touch_adm_ops = {
    .read = ipod_touch_adm_read,
    .write = ipod_touch_adm_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ipod_touch_adm_realize(DeviceState *dev, Error **errp)
{
    IPodTouchADMState *s = IPOD_TOUCH_ADM(dev);

    if (!s->downstream) {
        error_setg(errp, "ADM 'downstream' link not set");
        return;
    }

    address_space_init(&s->downstream_as, s->downstream, "adm-downstream");
}

static Property adm_properties[] = {
    DEFINE_PROP_LINK("downstream", IPodTouchADMState, downstream,
                     TYPE_MEMORY_REGION, MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
};

static void ipod_touch_adm_init(Object *obj)
{
    IPodTouchADMState *s = IPOD_TOUCH_ADM(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &ipod_touch_adm_ops, s, TYPE_IPOD_TOUCH_ADM, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void ipod_touch_adm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = ipod_touch_adm_realize;
    device_class_set_props(dc, adm_properties);
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
