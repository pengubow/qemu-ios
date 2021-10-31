#ifndef HW_ARM_IPOD_TOUCH_H
#define HW_ARM_IPOD_TOUCH_H

#include "qemu-common.h"
#include "exec/hwaddr.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/intc/pl192.h"
#include "cpu.h"

#define TYPE_IPOD_TOUCH "iPod-Touch"

#define TYPE_IPOD_TOUCH_MACHINE   MACHINE_TYPE_NAME(TYPE_IPOD_TOUCH)

#define IPOD_TOUCH_MACHINE(obj) \
    OBJECT_CHECK(IPodTouchMachineState, (obj), TYPE_IPOD_TOUCH_MACHINE)

typedef struct {
    MachineClass parent;
} IPodTouchMachineClass;

typedef struct {
	MachineState parent;
	qemu_irq **irq;
	PL192State vic0;
	PL192State vic1;
	uint32_t kpc_pa;
	uint32_t kbootargs_pa;
	uint32_t uart_mmio_pa;
	ARMCPU *cpu;
	char kernel_filename[1024];
	char dtb_filename[1024];
	char kern_args[256];
} IPodTouchMachineState;

#endif