/*
 * S5L8900 SPI Emulation
 *
 * by cmw
 */

#include "hw/arm/ipod_touch_spi.h"

static uint64_t s5l8900_spi_read(void *opaque, hwaddr offset, unsigned size)
{
    S5L8900SPIState *s = (S5L8900SPIState *)opaque;

	//fprintf(stderr, "%s: base 0x%08x offset 0x%08x\n", __func__, s->base, offset);

    switch (offset) {
    case SPI_CONTROL:
		return s->ctrl;
    case SPI_SETUP:
        return s->setup;
    case SPI_STATUS:
        return s->status;
    case SPI_PIN:
        return s->pin;
    case SPI_TXDATA:
        return s->tx_data;
    case SPI_RXDATA:
		//fprintf(stderr, "%s: s->cmd 0x%08x\n", __func__, s->cmd);
		switch(s->cmd) {
			case 0x95:
				return 1;
			case 0xDA:
				return 0x71;
			case 0xDB:
				return 0xC2;
			case 0xDC:
				return 0x00;
		  default:
			return 0;
		}
        return s->rx_data;
    case SPI_CLKDIV:
        return s->clkdiv;
    case SPI_CNT:
        return s->cnt;
    case SPI_IDD:
        return s->idd;
    default:
        hw_error("s5l8900_spi: bad read offset 0x" TARGET_FMT_plx "\n", offset);
    }
}

static void s5l8900_spi_write(void *opaque, hwaddr offset, uint64_t val, unsigned size)
{
    S5L8900SPIState *s = (S5L8900SPIState *)opaque;

    //fprintf(stderr, "%s: base 0x%08x offset 0x%08x value 0x%08x\n", __func__, s->base, offset, val);

    switch (offset) {
    case SPI_CONTROL:
		if(val & 0x1) {
				s->status |= 0xff2;
				s->cmd = s->tx_data;
	    		qemu_irq_raise(s->irq);
		}
        break;
    case SPI_SETUP:
        s->setup = val;
        break;
    case SPI_STATUS:
		qemu_irq_lower(s->irq); 
        s->status = 0;
        break;
    case SPI_PIN:
        s->pin = val;
        break;
    case SPI_TXDATA:
        s->tx_data = val;
        break;
    case SPI_RXDATA:
        s->rx_data = val;
        break;
    case SPI_CLKDIV:
        s->clkdiv = val;
        break;
    case SPI_CNT:
        s->cnt = val;
        break;
    case SPI_IDD:
        s->idd = val;
        break;
    default:
        hw_error("s5l8900_spi: bad write offset 0x" TARGET_FMT_plx "\n", offset);
    }
}

static const MemoryRegionOps spi_ops = {
    .read = s5l8900_spi_read,
    .write = s5l8900_spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void s5l8900_spi_reset(DeviceState *d)
{
    S5L8900SPIState *s = (S5L8900SPIState *)d;

	s->cmd = 0;
	s->ctrl = 0;
	s->setup = 0;
	s->status = 0;
	s->pin = 0;
	s->tx_data = 0;
	s->rx_data = 0;
	s->clkdiv = 0;
	s->cnt = 0;
	s->idd = 0;
}

static uint32_t base_addr = 0;

void set_spi_base(uint32_t base)
{
	base_addr = base;
}

static void s5l8900_spi_init(Object *obj)
{
    S5L8900SPIState *s = S5L8900SPI(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(sbd, &s->irq);
    char name[5];
    snprintf(name, 5, "spi%d", base_addr);
    memory_region_init_io(&s->iomem, OBJECT(s), &spi_ops, s, name, 0x100);
    sysbus_init_mmio(sbd, &s->iomem);
	s->base = base_addr;
}

static void s5l8900_spi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->reset = s5l8900_spi_reset;
}

static const TypeInfo s5l8900_spi_info = {
    .name          = TYPE_S5L8900SPI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(S5L8900SPIState),
    .instance_init = s5l8900_spi_init,
    .class_init    = s5l8900_spi_class_init,
};

static void s5l8900_spi_register_types(void)
{
    type_register_static(&s5l8900_spi_info);
}

type_init(s5l8900_spi_register_types)