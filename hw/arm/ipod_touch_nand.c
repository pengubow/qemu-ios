#include "hw/arm/ipod_touch_nand.h"

static uint64_t itnand_read(void *opaque, hwaddr addr, unsigned size)
{
    ITNandState *s = (ITNandState *) opaque;
    //fprintf(stderr, "%s: reading from 0x%08x\n", __func__, addr);

    switch (addr) {
        case NAND_FMCTRL0:
            return s->fmctrl0;
        case NAND_FMFIFO:
            if(s->cmd == NAND_CMD_ID) {
                return NAND_CHIP_ID;
            }
            else if(s->cmd == NAND_CMD_READSTATUS) {
                return (1 << 6);
            }
            else {
                uint32_t page = (s->fmaddr1 << 16) | (s->fmaddr0 >> 16);
                if(page != s->buffered_page) {
                    // refresh the buffered page
                    FILE *f = fopen("/Users/martijndevos/Documents/generate_nand/nand/bank0", "rb");
                    if (f == NULL) { hw_error("Unable to read file!"); }
                    uint64_t page_begin_offset = page * NAND_BYTES_PER_PAGE;
                    fseek(f, page_begin_offset, SEEK_SET);
                    fread(s->page_buffer, sizeof(char), NAND_BYTES_PER_PAGE, f);
                    fclose(f);
                    
                    // cache the spare page
                    f = fopen("/Users/martijndevos/Documents/generate_nand/nand/bank0_spare", "rb");
                    if (f == NULL) { hw_error("Unable to read file!"); }
                    uint64_t spare_begin_offset = page * NAND_BYTES_PER_SPARE;
                    fseek(f, spare_begin_offset, SEEK_SET);
                    fread(s->page_spare_buffer, sizeof(char), NAND_BYTES_PER_SPARE, f);
                    fclose(f);

                    s->buffered_page = page;
                    //printf("Buffered page: %d\n", s->buffered_page);
                }

                uint32_t read_val = 0;
                if(s->reading_spare) {
                    read_val = ((uint32_t *)s->page_spare_buffer)[(NAND_BYTES_PER_SPARE - s->fmdnum - 1) / 4];
                } else {
                    read_val = ((uint32_t *)s->page_buffer)[(NAND_BYTES_PER_PAGE - s->fmdnum - 1) / 4];
                } 

                s->fmdnum -= 4;

                return read_val;
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
        case NAND_FMCTRL1:
            s->fmctrl1 = val;
            break;
        case NAND_FMADDR0:
            s->fmaddr0 = val;
            break;
        case NAND_FMADDR1:
            s->fmaddr1 = val;
            break;
        case NAND_FMANUM:
            s->fmanum = val;
            break;
        case NAND_CMD:
            s->cmd = val;
            break;
        case NAND_FMDNUM:
            if(val == NAND_BYTES_PER_SPARE - 1) {
                s->reading_spare = 1;
            } else {
                s->reading_spare = 0;
            }
            s->fmdnum = val;
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

    s->page_buffer = (uint8_t *)malloc(NAND_BYTES_PER_PAGE);
    s->page_spare_buffer = (uint8_t *)malloc(NAND_BYTES_PER_SPARE);
    s->buffered_page = -1;
}

static void itnand_reset(DeviceState *d)
{
    ITNandState *s = (ITNandState *) d;

    s->fmctrl0 = 0;
    s->fmctrl1 = 0;
    s->fmaddr0 = 0;
    s->fmaddr1 = 0;
    s->fmanum = 0;
    s->fmdnum = 0;
    s->rsctrl = 0;
    s->cmd = 0;
    s->reading_spare = 0;
    s->buffered_page = -1;
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
