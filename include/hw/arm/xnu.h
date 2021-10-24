#ifndef HW_ARM_XNU_H
#define HW_ARM_XNU_H

#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "hw/arm/xnu_mem.h"

// pexpert/pexpert/arm64/boot.h
#define xnu_arm64_kBootArgsRevision2 2 /* added boot_args.bootFlags */
#define xnu_arm64_kBootArgsVersion2 2
#define xnu_arm64_BOOT_LINE_LENGTH 256

#define LC_SEGMENT_64   0x19
#define LC_UNIXTHREAD   0x5

struct segment_command_64
{
    uint32_t cmd;
    uint32_t cmdsize;
    char segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t /*vm_prot_t*/ maxprot;
    uint32_t /*vm_prot_t*/ initprot;
    uint32_t nsects;
    uint32_t flags;
};

struct mach_header_64 {
    uint32_t    magic;      /* mach magic number identifier */
    uint32_t /*cpu_type_t*/  cputype;    /* cpu specifier */
    uint32_t /*cpu_subtype_t*/   cpusubtype; /* machine specifier */
    uint32_t    filetype;   /* type of file */
    uint32_t    ncmds;      /* number of load commands */
    uint32_t    sizeofcmds; /* the size of all the load commands */
    uint32_t    flags;      /* flags */
    uint32_t    reserved;   /* reserved */
};

struct load_command {
    uint32_t cmd;       /* type of load command */
    uint32_t cmdsize;   /* total size of command in bytes */
};

typedef struct xnu_arm64_video_boot_args {
    unsigned long v_baseAddr; /* Base address of video memory */
    unsigned long v_display;  /* Display Code (if Applicable */
    unsigned long v_rowBytes; /* Number of bytes per pixel row */
    unsigned long v_width;    /* Width */
    unsigned long v_height;   /* Height */
    unsigned long v_depth;    /* Pixel Depth and other parameters */
} video_boot_args;

struct xnu_arm64_boot_args {
    uint16_t           Revision;                                   /* Revision of boot_args structure */
    uint16_t           Version;                                    /* Version of boot_args structure */
    uint64_t           virtBase;                                   /* Virtual base of memory */
    uint64_t           physBase;                                   /* Physical base of memory */
    uint64_t           memSize;                                    /* Size of memory */
    uint64_t           topOfKernelData;                            /* Highest physical address used in kernel data area */
    video_boot_args    Video;                                      /* Video Information */
    uint32_t           machineType;                                /* Machine Type */
    uint64_t           deviceTreeP;                                /* Base of flattened device tree */
    uint32_t           deviceTreeLength;                           /* Length of flattened tree */
    char               CommandLine[xnu_arm64_BOOT_LINE_LENGTH];    /* Passed in command line */
    uint64_t           bootFlags;                                  /* Additional flags specified by the bootloader */
    uint64_t           memSizeActual;                              /* Actual size of memory */
};

void macho_file_highest_lowest_base(const char *filename, hwaddr phys_base,
                                    hwaddr *virt_base, hwaddr *lowest,
                                    hwaddr *highest);

void macho_setup_bootargs(const char *name, AddressSpace *as,
                          MemoryRegion *mem, hwaddr bootargs_pa,
                          hwaddr virt_base, hwaddr phys_base, hwaddr mem_size,
                          hwaddr top_of_kernel_data_pa, hwaddr dtb_va,
                          hwaddr dtb_size, video_boot_args v_bootargs,
                          char *kern_args);


void arm_load_macho(char *filename, AddressSpace *as, MemoryRegion *mem,
                    const char *name, hwaddr phys_base, hwaddr virt_base,
                    hwaddr low_virt_addr, hwaddr high_virt_addr, hwaddr *pc);

#endif