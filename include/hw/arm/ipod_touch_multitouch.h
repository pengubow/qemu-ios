#ifndef IPOD_TOUCH_MULTITOUCH_H
#define IPOD_TOUCH_MULTITOUCH_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/ssi/ssi.h"
#include "hw/arm/ipod_touch_spi.h"

#define TYPE_IPOD_TOUCH_MULTITOUCH                "ipodtouch.multitouch"
OBJECT_DECLARE_SIMPLE_TYPE(IPodTouchMultitouchState, IPOD_TOUCH_MULTITOUCH)

#define MT_INTERFACE_VERSION   0x1
#define MT_FAMILY_ID           81
#define MT_ENDIANNESS          0x1
#define MT_SENSOR_ROWS         15
#define MT_SENSOR_COLUMNS      10
#define MT_BCD_VERSION         51
#define MT_SENSOR_REGION_DESC  0x0
#define MT_SENSOR_REGION_PARAM 0x0
#define MT_MAX_PACKET_SIZE     0x294 // 660

// report IDs
#define MT_REPORT_UNKNOWN1            0x70
#define MT_REPORT_FAMILY_ID           0xD1
#define MT_REPORT_SENSOR_INFO         0xD3
#define MT_REPORT_SENSOR_REGION_DESC  0xD0
#define MT_REPORT_SENSOR_REGION_PARAM 0xA1
#define MT_REPORT_SENSOR_DIMENSIONS   0xD9

// report sizes
#define MT_REPORT_UNKNOWN1_SIZE            0x1
#define MT_REPORT_FAMILY_ID_SIZE           0x1
#define MT_REPORT_SENSOR_INFO_SIZE         0x5
#define MT_REPORT_SENSOR_REGION_DESC_SIZE  0x1
#define MT_REPORT_SENSOR_REGION_PARAM_SIZE 0x1
#define MT_REPORT_SENSOR_DIMENSIONS_SIZE   0x8

#define MT_CMD_HBPP_DATA_PACKET      0x30
#define MT_CMD_GET_CMD_STATUS        0xE1
#define MT_CMD_GET_INTERFACE_VERSION 0xE2
#define MT_CMD_GET_REPORT_INFO       0xE3
#define MT_CMD_SHORT_CONTROL_WRITE   0xE4
#define MT_CMD_SHORT_CONTROL_READ    0xE6

typedef struct MTFrameHeader
{
    uint8_t type;
    uint8_t frameNum;
    uint8_t headerLen;
    uint8_t unk_3;
    uint32_t timestamp;
    uint8_t unk_8;
    uint8_t unk_9;
    uint8_t unk_A;
    uint8_t unk_B;
    uint16_t unk_C;
    uint16_t isImage;

    uint8_t numFingers;
    uint8_t fingerDataLen;
    uint16_t unk_12;
    uint16_t unk_14;
    uint16_t unk_16;
} MTFrameHeader;

typedef struct FingerData
{
    uint8_t id;
    uint8_t event;
    uint8_t unk_2;
    uint8_t unk_3;
    int16_t x;
    int16_t y;
    int16_t velX;
    int16_t velY;
    uint16_t radius2;
    uint16_t radius3;
    uint16_t angle;
    uint16_t radius1;
    uint16_t contactDensity;
    uint16_t unk_16;
    uint16_t unk_18;
    uint16_t unk_1A;
} FingerData;

typedef struct IPodTouchMultitouchState {
    SSIPeripheral ssidev;
    uint8_t cur_cmd;
    uint8_t *out_buffer;
    uint8_t *in_buffer;
    uint32_t buf_size;
    uint32_t buf_ind;
    uint32_t in_buffer_ind;
    uint8_t hbpp_atn_ack_response[2];
} IPodTouchMultitouchState;

#endif