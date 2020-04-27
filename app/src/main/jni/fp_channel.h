#ifndef CFP_FP_CHANNEL_H
#define CFP_FP_CHANNEL_H

#include <android/log.h>
#include "fingerprint.h"
#include "hardware.h"
#include <pthread.h>
#include <stdio.h>

// const uint8_t HW_AUTH_TOKEN_VERSION = 0;
#define HW_AUTH_TOKEN_VERSION 0

#define CFP_SENSOR_MOTION_KEY_INVALID 0
#define CFP_SENSOR_MOTION_TOUCH 1
#define CFP_SENSOR_MOTION_UNTOUCH 2
#define CFP_SENSOR_MOTION_ACTION_ON 3
#define CFP_SENSOR_MOTION_ACTION_OFF 4
#define CFP_SENSOR_MOTION_REMOVE_FP 5
#define CFP_SENSOR_MOTION_ERROR 6
#define CFP_SENSOR_MOTION_EXIT 7
#define CFP_SENSOR_MOTION_TEST 8
#define CFP_SENSOR_MOTION_FACTORY_TEST 9

typedef enum worker_state_t {
    STATE_ENROLL = 0,
    STATE_SCAN,
    STATE_IDLE,
    STATE_EXIT
} worker_state_t;

typedef struct emu_fingerprint_hal_device_t {
    fingerprint_device_t device; // inheritance
    int all_fingerids[5];
    volatile int num_fingers_enrolled;
    uint64_t challenge;
    uint64_t secure_user_id;
    uint64_t user_id;
    uint64_t authenticator_id;
} emu_fingerprint_hal_device_t;

extern worker_state_t gSensorState;
extern emu_fingerprint_hal_device_t *g_finger_dev;
extern int is_use_network;

#ifdef __cplusplus
extern "C" {
#endif

static uint64_t get_64bit_rand();
void setWorkState(worker_state_t state);

// Sender's functions
int send_fp_scan_on(fingerprint_msg_type_t type, int fingerId, int cfp_fpid, int arg1, int arg2, char *data);
int send_fp_error(int fingerId, int cfp_fpid, int arg1, int arg2);
int send_fp_removed(fingerprint_msg_type_t type, int fingerId, int cfp_fpid, int arg1, int arg2);
int send_fp_touch();
int send_fp_untouch();
int send_fp_factory_test(int result, int cmd);
void set_network_sock_fd(int fd);
void device_post_signal(int signo);
#ifdef HIDL_FEATURE
void send_fp_factory_hidl(const unsigned char *buffer, uint32_t size);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CFP_FP_CHANNEL_H
