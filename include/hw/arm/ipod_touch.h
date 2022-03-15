#ifndef HW_ARM_IPOD_TOUCH_H
#define HW_ARM_IPOD_TOUCH_H

#include "qemu-common.h"
#include "exec/hwaddr.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/intc/pl192.h"
#include "hw/arm/ipod_touch_spi.h"
#include "hw/arm/ipod_touch_aes.h"
#include "hw/arm/ipod_touch_sha1.h"
#include "hw/arm/ipod_touch_8900_engine.h"
#include "hw/arm/ipod_touch_usb_otg.h"
#include "hw/arm/ipod_touch_nand.h"
#include "hw/arm/ipod_touch_nand_ecc.h"
#include "hw/arm/ipod_touch_pcf50633_pmu.h"
#include "hw/arm/ipod_touch_adm.h"
#include "hw/arm/ipod_touch_sysic.h"
#include "hw/arm/ipod_touch_chipid.h"
#include "hw/arm/ipod_touch_tvout.h"
#include "hw/i2c/ipod_touch_i2c.h"
#include "cpu.h"

#define TYPE_IPOD_TOUCH "iPod-Touch"
#define TYPE_IPOD_TOUCH_LCD "ipod_touch_lcd"
OBJECT_DECLARE_SIMPLE_TYPE(s5l8900_lcd_state, IPOD_TOUCH_LCD)

#define TYPE_IPOD_TOUCH_MACHINE   MACHINE_TYPE_NAME(TYPE_IPOD_TOUCH)
#define IPOD_TOUCH_MACHINE(obj) \
    OBJECT_CHECK(IPodTouchMachineState, (obj), TYPE_IPOD_TOUCH_MACHINE)

#define CLOCK1_CONFIG0 0x0
#define CLOCK1_CONFIG1 0x4
#define CLOCK1_CONFIG2 0x8
#define CLOCK1_PLL0CON 0x20
#define CLOCK1_PLL1CON 0x24
#define CLOCK1_PLL2CON 0x28
#define CLOCK1_PLL3CON 0x2C
#define CLOCK1_PLLLOCK 0x40
#define CLOCK1_PLLMODE 0x44

// timer stuff
#define TIMER_IRQSTAT 0x10000
#define TIMER_IRQLATCH 0xF8
#define TIMER_TICKSHIGH 0x80
#define TIMER_TICKSLOW 0x84
#define TIMER_STATE_START 1
#define TIMER_STATE_STOP 0
#define TIMER_STATE_MANUALUPDATE 2
#define NUM_TIMERS 7
#define TIMER_4 0xA0
#define TIMER_CONFIG 0 
#define TIMER_STATE 0x4
#define TIMER_COUNT_BUFFER 0x8
#define TIMER_COUNT_BUFFER2 0xC

// VIC
#define S5L8900_VIC_N	  2
#define S5L8900_VIC_SIZE  32

#define S5L8900_SPI0_IRQ 0x9
#define S5L8900_SPI1_IRQ 0xA
#define S5L8900_SPI2_IRQ 0xB

#define S5L8900_DMAC0_IRQ 0x10
#define S5L8900_DMAC1_IRQ 0x11

#define S5L8900_I2C0_IRQ 0x15
#define S5L8900_I2C1_IRQ 0x16

#define S5L8900_ADM_IRQ 0x25

#define S5L8900_NAND_ECC_IRQ 0x2B

#define S5L8900_TVOUT_SDO_IRQ 0x1E
#define S5L8900_TVOUT_MIXER_IRQ 0x26

typedef struct {
    MachineClass parent;
} IPodTouchMachineClass;

typedef struct s5l8900_clk1_s
{
	uint32_t	clk1_config0;
    uint32_t    clk1_config1;
    uint32_t    clk1_config2;
	uint32_t    clk1_pll1con;
    uint32_t    clk1_pll2con;
    uint32_t    clk1_pll3con;
	uint32_t    clk1_plllock;

} s5l8900_clk1_s;

typedef struct s5l8900_usb_phys_s
{
	uint32_t usb_ophypwr;
	uint32_t usb_ophyclk;
	uint32_t usb_orstcon;
	uint32_t usb_ophytune;

} s5l8900_usb_phys_s;

typedef struct s5l8900_timer_s
{
    uint32_t    ticks_high;
	uint32_t	ticks_low;
	uint32_t 	status;
	uint32_t    config;
	uint32_t    bcount1;
	uint32_t    bcount2;
	uint32_t    prescaler;
	uint32_t    irqstat;
    QEMUTimer *st_timer;
    Clock *sysclk;
	uint32_t bcreload;
    uint32_t freq_out;
    uint64_t tick_interval;
    uint64_t last_tick;
    uint64_t next_planned_tick;
    uint64_t base_time;
	qemu_irq	irq;

} s5l8900_timer_s;

typedef struct s5l8900_lcd_state
{
    SysBusDevice parent_obj;
    MemoryRegion *sysmem;
    MemoryRegion iomem;
    QemuConsole *con;
    int invalidate;
    MemoryRegionSection fbsection;
    uint32_t lcd_con;
    uint32_t lcd_con2;
    uint32_t framebuffer_base;
    uint32_t display_resolution_info;
    uint32_t display_depth_info;
} s5l8900_lcd_state;

typedef struct {
	MachineState parent;
	qemu_irq **irq;
	PL192State *vic0;
	PL192State *vic1;
	synopsys_usb_state *usb_otg;
	s5l8900_clk1_s *clock1;
	s5l8900_timer_s *timer1;
	IPodTouchSYSICState *sysic;
	s5l8900_usb_phys_s *usb_phys;
	S5L8900AESState *aes_state;
	S5L8900SHA1State *sha1_state;
	ITNandState *nand_state;
	ITNandECCState *nand_ecc_state;
	IPodTouchI2CState *i2c0_state;
	IPodTouchI2CState *i2c1_state;
	IPodTouchADMState *adm_state;
	IPodTouchChipIDState *chipid_state;
	IPodTouchTVOutState *tvout1_state;
	IPodTouchTVOutState *tvout2_state;
	IPodTouchTVOutState *tvout3_state;
	Clock *sysclk;
	uint32_t kpc_pa;
	uint32_t kbootargs_pa;
	uint32_t uart_mmio_pa;
	ARMCPU *cpu;
	char kernel_filename[1024];
	char dtb_filename[1024];
	char kern_args[256];
} IPodTouchMachineState;

#endif