#ifndef HW_ARM_XNU_MEM_H
#define HW_ARM_XNU_MEM_H

#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "target/arm/cpu.h"

extern hwaddr g_virt_base;
extern hwaddr g_phys_base;

hwaddr vtop_static(hwaddr va);
hwaddr ptov_static(hwaddr pa);
hwaddr vtop_mmu(hwaddr va, CPUState *cs);

hwaddr align_64k_low(hwaddr addr);
hwaddr align_64k_high(hwaddr addr);

hwaddr vtop_bases(hwaddr va, hwaddr phys_base, hwaddr virt_base);
hwaddr ptov_bases(hwaddr pa, hwaddr phys_base, hwaddr virt_base);

uint8_t get_highest_different_bit_index(hwaddr addr1, hwaddr addr2);
uint8_t get_lowest_non_zero_bit_index(hwaddr addr);
hwaddr get_low_bits_mask_for_bit_index(uint8_t bit_index);

void allocate_ram(MemoryRegion *top, const char *name, hwaddr addr,
                  hwaddr size);
#endif