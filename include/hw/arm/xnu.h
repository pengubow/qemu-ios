#ifndef HW_ARM_XNU_H
#define HW_ARM_XNU_H

#include "qemu-common.h"

#define xnu_arm_kBootArgsRevision2 2 /* added boot_args.bootFlags */
#define xnu_arm_kBootArgsVersion3 3

#define	LC_SEGMENT	0x1	/* segment of this file to be mapped */
#define LC_UNIXTHREAD   0x5

struct segment_command { /* for 32-bit architectures */
	uint32_t	cmd;		/* LC_SEGMENT */
	uint32_t	cmdsize;	/* includes sizeof section structs */
	char		segname[16];	/* segment name */
	uint32_t	vmaddr;		/* memory address of this segment */
	uint32_t	vmsize;		/* memory size of this segment */
	uint32_t	fileoff;	/* file offset of this segment */
	uint32_t	filesize;	/* amount to map from the file */
	uint32_t	maxprot;	/* maximum VM protection */
	uint32_t	initprot;	/* initial VM protection */
	uint32_t	nsects;		/* number of sections in segment */
	uint32_t	flags;		/* flags */
};

struct load_command {
	uint32_t cmd;		/* type of load command */
	uint32_t cmdsize;	/* total size of command in bytes */
};

struct mach_header {
	uint32_t	magic;		/* mach magic number identifier */
	uint32_t	cputype;	/* cpu specifier */
	uint32_t	cpusubtype;	/* machine specifier */
	uint32_t	filetype;	/* type of file */
	uint32_t	ncmds;		/* number of load commands */
	uint32_t	sizeofcmds;	/* the size of all the load commands */
	uint32_t	flags;		/* flags */
};

struct xnu_arm_boot_args {
    uint16_t           Revision;                                   /* Revision of boot_args structure */
    uint16_t           Version;                                    /* Version of boot_args structure */
    uint32_t           virtBase;                                   /* Virtual base of memory */
    uint32_t           physBase;                                   /* Physical base of memory */
    uint32_t           memSize;                                    /* Size of memory */
    uint32_t           topOfKernelData;                            /* Highest physical address used in kernel data area */
};

void macho_setup_bootargs(const char *name, AddressSpace *as, MemoryRegion *mem, uint32_t bootargs_pa, uint32_t virt_base, uint32_t phys_base, uint32_t mem_size, uint32_t top_of_kernel_data_pa);
void macho_file_highest_lowest_base(const char *filename, uint32_t phys_base, uint32_t *virt_base, uint32_t *lowest, uint32_t *highest);
void arm_load_macho(char *filename, AddressSpace *as, MemoryRegion *mem, const char *name, uint32_t phys_base, uint32_t virt_base, uint32_t low_virt_addr, uint32_t high_virt_addr, uint32_t *pc);

#endif