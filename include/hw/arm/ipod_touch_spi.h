#ifndef HW_ARM_IPOD_TOUCH_SPI_H
#define HW_ARM_IPOD_TOUCH_SPI_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/platform-bus.h"
#include "hw/hw.h"
#include "hw/irq.h"

#define TYPE_S5L8900SPI "s5l8900spi"
OBJECT_DECLARE_SIMPLE_TYPE(S5L8900SPIState, S5L8900SPI)

#define S5L8900_WDT_REG_MEM_SIZE 0x30

#define SPI_CONTROL 0
#define SPI_SETUP 0x4 
#define SPI_STATUS 0x8
#define SPI_PIN 0xc 
#define SPI_TXDATA 0x10
#define SPI_RXDATA 0x20
#define SPI_CLKDIV 0x30
#define SPI_CNT 0x34
#define SPI_IDD 0x38


typedef struct S5L8900SPIState {
    SysBusDevice busdev;
    MemoryRegion iomem;

	uint32_t cmd;
	uint32_t base;
	uint32_t ctrl;
	uint32_t setup;
	uint32_t status;
	uint32_t pin;
	uint32_t tx_data;
	uint32_t rx_data;
	uint32_t clkdiv;
	uint32_t cnt;
	uint32_t idd;

    qemu_irq irq;
} S5L8900SPIState;

void set_spi_base(uint32_t base);

#endif