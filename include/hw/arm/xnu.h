#ifndef HW_ARM_XNU_H
#define HW_ARM_XNU_H

#include "qemu-common.h"

#define UARTC_BASE_ADDR     0x3CC00000

#define xnu_arm_kBootArgsRevision2 2 /* added boot_args.bootFlags */
#define xnu_arm_kBootArgsVersion3 3
#define xnu_arm_BOOT_LINE_LENGTH 256

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

typedef struct xnu_arm_video_boot_args {
	uint32_t	v_baseAddr;	/* Base address of video memory */
	uint32_t	v_display;	/* Display Code (if Applicable */
	uint32_t	v_rowBytes;	/* Number of bytes per pixel row */
	uint32_t	v_width;	/* Width */
	uint32_t	v_height;	/* Height */
	uint32_t	v_depth;	/* Pixel Depth and other parameters */
} video_boot_args;

struct xnu_arm_boot_args {
    uint16_t           Revision;                                   /* Revision of boot_args structure */
    uint16_t           Version;                                    /* Version of boot_args structure */
    uint32_t           virtBase;                                   /* Virtual base of memory */
    uint32_t           physBase;                                   /* Physical base of memory */
    uint32_t           memSize;                                    /* Size of memory */
    uint32_t           topOfKernelData;                            /* Highest physical address used in kernel data area */
    video_boot_args		   Video;									   /* Video Information */
    uint32_t		machineType;		/* Machine Type */
	uint32_t			deviceTreeP;		/* Base of flattened device tree */
	uint32_t		deviceTreeLength;	/* Length of flattened tree */
	char               CommandLine[xnu_arm_BOOT_LINE_LENGTH];    /* Passed in command line */

};

void allocate_and_copy(MemoryRegion *mem, AddressSpace *as, const char *name, uint32_t pa, uint32_t size, void *buf);
void macho_setup_bootargs(const char *name, AddressSpace *as, MemoryRegion *mem, uint32_t bootargs_pa, uint32_t virt_base, uint32_t phys_base, uint32_t mem_size, uint32_t top_of_kernel_data_pa, uint32_t dtb_va, uint32_t dtb_size, char *kern_args, uint32_t framebuffer_addr);
void macho_file_highest_lowest_base(const char *filename, uint32_t phys_base, uint32_t *virt_base, uint32_t *lowest, uint32_t *highest);
void arm_load_macho(char *filename, AddressSpace *as, MemoryRegion *mem, const char *name, uint32_t phys_base, uint32_t virt_base, uint32_t low_virt_addr, uint32_t high_virt_addr, uint32_t *pc);
void macho_load_dtb(char *filename, AddressSpace *as, MemoryRegion *mem, const char *name, uint32_t dtb_pa, uint64_t *size, uint32_t *uart_mmio_pa);

#endif