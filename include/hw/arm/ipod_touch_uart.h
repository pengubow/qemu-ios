#ifndef HW_ARM_IPOD_TOUCH_UART_H
#define HW_ARM_IPOD_TOUCH_UART_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "chardev/char.h"
#include "chardev/char-fe.h"
#include "hw/irq.h"

#define TYPE_S5L8900UART "s5l8900uart"
OBJECT_DECLARE_SIMPLE_TYPE(S5L8900UartState, S5L8900UART)

#define QUEUE_SIZE   257

#define INT_RXD     (1 << 0)
#define INT_ERROR   (1 << 1)
#define INT_TXD     (1 << 2)
#define INT_MODEM   (1 << 3)

#define TRSTATUS_TRANSMITTER_READY  (1 << 2)
#define TRSTATUS_BUFFER_EMPTY       (1 << 1)
#define TRSTATUS_DATA_READY         (1 << 0)

#define UFSTAT_RX_FIFO_FULL         (1 << 8)

#define UFCON_FIFO_ENABLED          (1 << 0)
#define UFCON_TX_LEVEL_SHIFT        8
#define UFCON_TX_LEVEL              (7 << UFCON_TX_LEVEL_SHIFT)

#define UFSTAT_TX_COUNT_SHIT        16
#define UFSTAT_TX_COUNT             (0xFF << UFSTAT_TX_COUNT_SHIT)

#define QI(x) ((x + 1) % QUEUE_SIZE)
#define QD(x) ((x - 1 + QUEUE_SIZE) % QUEUE_SIZE)

#define S5L8900_UART_REG_MEM_SIZE 0x3C

typedef struct UartQueue {
    uint8_t queue[QUEUE_SIZE];
    uint32_t s, t;
    uint32_t size;
} UartQueue;

typedef struct S5L8900UartState {
    SysBusDevice busdev;
    MemoryRegion iomem;

    UartQueue rx;

	uint32_t base;
    uint32_t ulcon;
    uint32_t ucon;
    uint32_t ufcon;
    uint32_t umcon;
    uint32_t utrstat;
    uint32_t uerstat;
    uint32_t ufstat;
    uint32_t umstat;
    uint32_t utxh;
    uint32_t urxh;
    uint32_t ubrdiv;
    uint32_t udivslot;
    uint32_t uintp;
    uint32_t uintsp;
    uint32_t uintm;

    CharBackend chr;
    qemu_irq irq;
    uint32_t instance;
} S5L8900UartState;

DeviceState *ipod_touch_init_uart(int instance, int queue_size, qemu_irq irq, Chardev *chr);

#endif