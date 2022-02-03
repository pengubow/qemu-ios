#ifndef HW_ARM_IPOD_TOUCH_AES_H
#define HW_ARM_IPOD_TOUCH_AES_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/hw.h"
#include "exec/hwaddr.h"
#include "exec/memory.h"
#include <openssl/aes.h>

#define key_uid ((uint8_t[]){0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF})

#define AES_128_CBC_BLOCK_SIZE 64
#define AES_CONTROL 0x0
#define AES_GO 0x4
#define AES_UNKREG0 0x8
#define AES_STATUS 0xC
#define AES_UNKREG1 0x10
#define AES_KEYLEN 0x14
#define AES_INSIZE 0x18
#define AES_INADDR 0x20
#define AES_OUTSIZE 0x24
#define AES_OUTADDR 0x28
#define AES_AUXSIZE 0x2C
#define AES_AUXADDR 0x30
#define AES_SIZE3 0x34
#define AES_KEY_REG 0x4C
#define AES_TYPE 0x6C
#define AES_IV_REG 0x74
#define AES_KEYSIZE 0x20
#define AES_IVSIZE 0x10

typedef enum AESKeyType {
    AESCustom = 0,
    AESGID = 1,
    AESUID = 2
} AESKeyType;

typedef enum AESKeyLen {
    AES128 = 0,
    AES192 = 1,
    AES256 = 2
} AESKeyLen;

typedef struct S5L8900AESState
{
    AES_KEY decryptKey;
	uint32_t ivec[4];
	uint32_t insize;
	uint32_t inaddr;
	uint32_t outsize;
	uint32_t outaddr;
	uint32_t auxaddr;
	uint32_t keytype;
	uint32_t status;
	uint32_t ctrl;
	uint32_t unkreg0;
	uint32_t unkreg1;
	uint32_t operation;
	uint32_t keylen;
	uint32_t custkey[8]; 
} S5L8900AESState;

static uint64_t s5l8900_aes_read(void *opaque, hwaddr offset, unsigned size)
{
    struct S5L8900AESState *aesop = (struct S5L8900AESState *)opaque;

    //fprintf(stderr, "%s: offset 0x%08x\n", __FUNCTION__, offset);

    switch(offset) {
        case AES_STATUS:
            return aesop->status;
      default:
            //fprintf(stderr, "%s: UNMAPPED AES_ADDR @ offset 0x%08x\n", __FUNCTION__, offset);
            break;
    }

    return 0;
}

static void s5l8900_aes_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    struct S5L8900AESState *aesop = (struct S5L8900AESState *)opaque;
    static uint8_t keylenop = 0;

    uint8_t inbuf[0x1000];
    uint8_t *buf;

    //fprintf(stderr, "%s: offset 0x%08x value 0x%08x\n", __FUNCTION__, offset, value);

    switch(offset) {
        case AES_GO:
            memset(inbuf, 0, sizeof(inbuf));
            cpu_physical_memory_read((aesop->inaddr - 0x80000000), (uint8_t *)inbuf, aesop->insize);

            switch(aesop->keytype) {
                    case AESGID:    
                        fprintf(stderr, "%s: No support for GID key\n", __func__);
                        return;         
                    case AESUID:
                        AES_set_decrypt_key(key_uid, sizeof(key_uid) * 8, &aesop->decryptKey);
                        break;
                    case AESCustom:
                        AES_set_decrypt_key((uint8_t *)aesop->custkey, 0x20 * 8, &aesop->decryptKey);
                        break;
            }

            buf = (uint8_t *) malloc(aesop->insize);

            AES_cbc_encrypt(inbuf, buf, aesop->insize, &aesop->decryptKey, (uint8_t *)aesop->ivec, aesop->operation);

            cpu_physical_memory_write((aesop->outaddr - 0x80000000), buf, aesop->insize);
            memset(aesop->custkey, 0, 0x20);
            memset(aesop->ivec, 0, 0x10);
            free(buf);
            keylenop = 0;
            aesop->outsize = aesop->insize;
            aesop->status = 0xf;
            break;
        case AES_KEYLEN:
            if(keylenop == 1) {
                aesop->operation = value;
            }
            keylenop++;
            aesop->keylen = value;
            break;
        case AES_INADDR:
            aesop->inaddr = value;
            break;
        case AES_INSIZE:
            aesop->insize = value;
            break;
        case AES_OUTSIZE:
            aesop->outsize = value;
            break;
        case AES_OUTADDR:
            aesop->outaddr = value;
            break;
        case AES_TYPE:
            aesop->keytype = value;
            break;
        case AES_KEY_REG ... ((AES_KEY_REG + AES_KEYSIZE) - 1):
            {
                uint8_t idx = (offset - AES_KEY_REG) / 4;
                aesop->custkey[idx] |= value;
                break;
            }
        case AES_IV_REG ... ((AES_IV_REG + AES_IVSIZE) -1 ):
            {
                uint8_t idx = (offset - AES_IV_REG) / 4;
                aesop->ivec[idx] |= value;
                break;
            }
        default:
            //fprintf(stderr, "%s: UNMAPPED AES_ADDR @ offset 0x%08x - 0x%08x\n", __FUNCTION__, offset, value);
            break;
    }
}

static const MemoryRegionOps aes_ops = {
    .read = s5l8900_aes_read,
    .write = s5l8900_aes_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

#endif
