#include "hw/arm/ipod_touch_pcf50633_pmu.h"

static int pcf50633_event(I2CSlave *i2c, enum i2c_event event)
{
    // TODO
    return 0;
}

static uint8_t pcf50633_recv(I2CSlave *i2c)
{
    Pcf50633State *s = PCF50633(i2c);

    switch(s->cmd) {
        case PMU_MBCS1:
            return 0; // battery power source
        case PMU_ADCC1:
            return 0; // battery charge voltage
        case 0x67:
            return 1; // whether we should enable debug UARTS
        case 0x69:
            return 0; // boot count error/panic
        case 0x76:
            return 0; // unknown register
        default:
            printf("AT RECV %d\n", s->cmd);
            return 0;
    }
}

static int pcf50633_send(I2CSlave *i2c, uint8_t data)
{
    Pcf50633State *s = PCF50633(i2c);
    s->cmd = data;
    return 0;
}

static void pcf50633_init(Object *obj)
{
    // TODO
}

static void pcf50633_class_init(ObjectClass *klass, void *data)
{
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    k->event = pcf50633_event;
    k->recv = pcf50633_recv;
    k->send = pcf50633_send;
}

static const TypeInfo pcf50633_info = {
    .name          = TYPE_PCF50633,
    .parent        = TYPE_I2C_SLAVE,
    .instance_init = pcf50633_init,
    .instance_size = sizeof(Pcf50633State),
    .class_init    = pcf50633_class_init,
};

static void pcf50633_register_types(void)
{
    type_register_static(&pcf50633_info);
}

type_init(pcf50633_register_types)