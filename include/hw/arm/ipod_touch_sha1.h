#ifndef HW_ARM_IPOD_TOUCH_SHA1_H
#define HW_ARM_IPOD_TOUCH_SHA1_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/hw.h"
#include "exec/hwaddr.h"
#include "exec/memory.h"
#include <openssl/sha.h>

#define SHA_CONFIG  0x0
#define SHA_RESET   0x4
#define SHA_UNKSTAT 0x7c
#define SHA_HRESULT 0x84
#define SHA_INSIZE  0x8c

typedef struct S5L8900SHA1State {
    uint32_t config;
    uint32_t reset;
    uint32_t hresult;
    uint32_t insize;
    uint32_t unkstat;
    uint8_t hashout[0x14];
} S5L8900SHA1State;

static void sha1_reset(void *opaque)
{
    S5L8900SHA1State *s = (S5L8900SHA1State *)opaque;
	memset(s, 0, sizeof(S5L8900SHA1State));
}

static uint64_t s5l8900_sha1_read(void *opaque, hwaddr offset, unsigned size)
{
	S5L8900SHA1State *s = (S5L8900SHA1State *)opaque;

    //fprintf(stderr, "%s: offset 0x%08x\n", __FUNCTION__, offset);

	switch(offset) {
		case SHA_CONFIG:
			return s->config;
		case SHA_RESET:
			return 0;
		case SHA_HRESULT:
			return s->hresult;
		case SHA_INSIZE:
			return s->insize;
		/* Hash result ouput */
		case 0x20 ... 0x34:
			//fprintf(stderr, "Hash out %08x\n",  *(uint32_t *)&s->hashout[offset - 0x20]);
			return *(uint32_t *)&s->hashout[offset - 0x20];
	}

    return 0;
}

static void s5l8900_sha1_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    S5L8900SHA1State *s = (S5L8900SHA1State *)opaque;

    //fprintf(stderr, "%s: offset 0x%08x value 0x%08x\n", __FUNCTION__, offset, value);

	switch(offset) {
		case SHA_CONFIG:
			if((value & 0x2) && (s->config & 0x8))
			{	
				uint8_t *hptr;

				if(!s->hresult || !s->insize)
					return;
				
				/* Why do they give us incorrect size? */
				s->insize += 0x20;

                hptr = (uint8_t *) malloc(s->insize);
                cpu_physical_memory_read(s->hresult, (uint8_t *)hptr, s->insize);

    			SHA_CTX context;
    			SHA1_Init(&context);
    			SHA1_Update(&context, (uint8_t*) hptr, s->insize);
    			SHA1_Final(s->hashout, &context);
				
				free(hptr);
			} else {
				s->config = value;
			}
			break;
		case SHA_RESET:
			if(value & 1) 
				sha1_reset(s);
			break;
		case SHA_HRESULT:
			s->hresult = value;
			break;
		case SHA_INSIZE:
			s->insize = value;
			break;
		case SHA_UNKSTAT:
			s->unkstat = value;
			break;
	}
}

static const MemoryRegionOps sha1_ops = {
    .read = s5l8900_sha1_read,
    .write = s5l8900_sha1_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

#endif