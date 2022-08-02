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
#define MT_REPORT_FAMILY_ID           0xD1
#define MT_REPORT_SENSOR_INFO         0xD3
#define MT_REPORT_SENSOR_REGION_DESC  0xD0
#define MT_REPORT_SENSOR_REGION_PARAM 0xA1
#define MT_REPORT_SENSOR_DIMENSIONS   0xD9

// report sizes
#define MT_REPORT_FAMILY_ID_SIZE           0x1
#define MT_REPORT_SENSOR_INFO_SIZE         0x5
#define MT_REPORT_SENSOR_REGION_DESC_SIZE  0x1
#define MT_REPORT_SENSOR_REGION_PARAM_SIZE 0x1
#define MT_REPORT_SENSOR_DIMENSIONS_SIZE   0x8

#define MT_CMD_GET_INTERFACE_VERSION 0xE2
#define MT_CMD_GET_REPORT_INFO       0xE3
#define MT_CMD_SHORT_CONTROL_READ    0xE6

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