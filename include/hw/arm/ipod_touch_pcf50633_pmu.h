#ifndef HW_PCF50633_PMU_H
#define HW_PCF50633_PMU_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "hw/i2c/i2c.h"
#include "hw/irq.h"

#define TYPE_PCF50633                 "pcf50633"
OBJECT_DECLARE_SIMPLE_TYPE(Pcf50633State, PCF50633)

typedef struct Pcf50633State {
	I2CSlave i2c;
} Pcf50633State;

#endif