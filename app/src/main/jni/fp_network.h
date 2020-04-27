#ifndef __FP_NETWORK_H_
#define __FP_NETWORK_H_

#include "fingerprint.h"

enum {
    MSG_FINGER_DOWN = 0x100,
    MSG_FINGER_UP = 0x101,
    MSG_TOUCH_SENSOR = 0x104,
    MSG_ENROLL = 0x200,
    MSG_MATCH = 0x201,
    MSG_ENUMERATE = 0x202,
    MSG_CANCEL = 0x203,
    MSG_DELETE = 0x204,
    MSG_CMD_EXPORT_IMAGE = 0x205,
    MSG_CMD_DELETE_CDFINGER_FOLDER = 0x206,
    MSG_CMD_DELETE_IMAGE_FORLDER = 0x207,
    MSG_CMD_SET_SAVE_IMAGE_PATH = 0x208,
    MSG_CMD_DELETE_IMAGE = 0x209,
    MSG_CMD_GET_VERSION = 0x210,
    MSG_CMD_UPDATE_HAL = 0x211,
    MSG_CMD_UPDATE_TAC = 0x212,
    MSG_CMD_UPDATE_TEE = 0x213,
    MSG_CMD_KILL_FINGERPRINTD = 0x214,
    MSG_CMD_SET_ALGO_PARAMETERS = 0x215,
    MSG_CMD_GET_ALGO_PARAMETERS = 0x216,
    MSG_CMD_RESET_ALGO_PARAMETERS = 0x217,
};

#ifdef __cplusplus
extern "C" {
#endif

void cmd_app_to_hal(const uint32_t *data);
int cfp_notify_data(const void *buf, size_t n);
int notify_hal_to_app(const fingerprint_msg_t *msg);
int cfp_network_enable();

#ifdef __cplusplus
}
#endif

#endif