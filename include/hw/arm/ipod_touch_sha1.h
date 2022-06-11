#ifndef HW_ARM_IPOD_TOUCH_SHA1_H
#define HW_ARM_IPOD_TOUCH_SHA1_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/hw.h"
#include "exec/hwaddr.h"
#include "exec/memory.h"
#include <openssl/sha.h>

#define SHA1_BUFFER_SIZE 1024 * 1024

#define SHA_CONFIG         0x0
#define SHA_RESET          0x4
#define SHA_MEMORY_MODE    0x80 // whether we read from the memory
#define SHA_MEMORY_START   0x84
#define SHA_INSIZE         0x8c

typedef struct S5L8900SHA1State {
    uint32_t config;
    uint32_t reset;
    uint32_t memory_start;
    uint32_t memory_mode;
    uint32_t insize;
    uint8_t buffer[SHA1_BUFFER_SIZE];
    uint32_t hw_buffer[0x10]; // hardware buffer
    uint32_t buffer_ind;
    uint8_t hashout[0x14];
    bool hw_buffer_dirty;
    bool hash_computed;
    SHA_CTX ctx;
} S5L8900SHA1State;

static uint64_t swapLong(void *X) {
    uint64_t x = (uint64_t) X;
    x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
    x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
    x = (x & 0x00FF00FF00FF00FF) << 8  | (x & 0xFF00FF00FF00FF00) >> 8;
    return x;
}

static void flush_hw_buffer(S5L8900SHA1State *s) {
    // Flush the hardware buffer to the state buffer and clear the buffer.
    memcpy(s->buffer + s->buffer_ind, (uint8_t *)s->hw_buffer, 0x40);
    memset(s->hw_buffer, 0, 0x40);
    s->hw_buffer_dirty = false;
    s->buffer_ind += 0x40;
}

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
		case SHA_MEMORY_START:
			return s->memory_start;
		case SHA_MEMORY_MODE:
			return s->memory_mode;
		case SHA_INSIZE:
			return s->insize;
		/* Hash result ouput */
		case 0x20 ... 0x34:
			//fprintf(stderr, "Hash out %08x\n",  *(uint32_t *)&s->hashout[offset - 0x20]);
            if(!s->hash_computed) {
                // lazy compute the final hash by inspecting the last eight bytes of the buffer, which contains the length of the input data.
                uint64_t data_length = swapLong(((uint64_t *)s->buffer)[s->buffer_ind / 8 - 1]) / 8;

                SHA_CTX ctx;
                SHA1_Init(&ctx);
                SHA1_Update(&ctx, s->buffer, data_length);
                SHA1_Final(s->hashout, &ctx);
                s->hash_computed = true;
            }

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
			if(value == 0x2 || value == 0xa)
			{
                if(s->hw_buffer_dirty) {
                    flush_hw_buffer(s);
                }

				if(s->memory_mode)
				{
					// we are in memory mode - gradually add the memory to the buffer
					for(int i = 0; i < s->insize / 0x40; i++) {
						cpu_physical_memory_read(s->memory_start + i * 0x40, s->buffer + s->buffer_ind, 0x40);
						s->buffer_ind += 0x40;
					}
				}
			} else {
				s->config = value;
			}
			break;
		case SHA_RESET:
			sha1_reset(s);
			break;
		case SHA_MEMORY_START:
			s->memory_start = value;
			break;
		case SHA_MEMORY_MODE:
			s->memory_mode = value;
			break;
		case SHA_INSIZE:
            assert(value <= SHA1_BUFFER_SIZE);
			s->insize = value;
			break;
		case 0x40 ... 0x7c:
            // write to the hardware buffer
            s->hw_buffer[(offset - 0x40) / 4] |= value;
            s->hw_buffer_dirty = true;
			break;
	}
}

static const MemoryRegionOps sha1_ops = {
    .read = s5l8900_sha1_read,
    .write = s5l8900_sha1_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

#endif