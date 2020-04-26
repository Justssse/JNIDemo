#ifndef __FP_NETWORK_H_
#define __FP_NETWORK_H_

enum {
    MSG_FINGER_DOWN = 0x100,
    MSG_FINGER_UP = 0x101,
    MSG_TOUCH_SENSOR = 0x104,
    MSG_ENROLL = 0x200,
    MSG_MATCH = 0x201,
    MSG_ENUMERATE = 0x202,
    MSG_CANCEL = 0x203,
    MSG_DELETE = 0x204,
    MSG_CMD = 0x205,
    MSG_CMD_EXPOSURE_TIME = 0x206,
    MSG_CMD_EXPORT_IMAGE = 0x207,
    MSG_UNKNOWN = 0xFFF,
};

#ifdef __cplusplus
extern "C" {
#endif

int cfp_network_enable();

#ifdef __cplusplus
}
#endif

#endif