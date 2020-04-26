#include "fp_channel.h"
#include "fp_network.h"
#include "mylog.h"

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/statfs.h>
#include <unistd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "fpHAL_C"
#else
#define LOG_TAG "fpHAL_C"
#endif

typedef struct cfp_sensor_notify_data {
    int action;
    fingerprint_msg_type_t msg_type;
    int custom_fp_id;
    int cfp_fp_id;
    int arg1;
    int arg2;
    unsigned char *data;
} cfp_sensor_notify_data_t;

int is_use_network = 1;
worker_state_t gSensorState = STATE_IDLE;
emu_fingerprint_hal_device_t *g_dev = NULL;

cfp_sensor_notify_data_t gSensorNotifyData = {0, (fingerprint_msg_type_t)0, 0, 0, 0, 0, NULL};

int gCurrentActiveGroup = -1;
int gLastOperationGroup = -1;

int g_client_sock = 0;

static pthread_mutex_t notify_mutex = PTHREAD_MUTEX_INITIALIZER;
/////////////////////////////////////// Data Management /////////////////////////////////////////////////////

int checkout_memory() {
    struct statfs diskInfo;
    statfs("/data", &diskInfo);
    unsigned long long totalBlocks = diskInfo.f_bsize;
    unsigned long long freeDisk = diskInfo.f_bfree * totalBlocks;
    LOGD("checkout_memory freeDisk:%lld", freeDisk);
    if (freeDisk < 20 * 1024 * 1024)
        return -1;
    return 0;
}

static uint64_t get_64bit_rand() {
    uint64_t val = (((uint64_t)rand()) << 32) | ((uint64_t)rand());

    LOGD(" get_64bit_rand()=%lu", (long unsigned int)val);
    return val;
}

void setWorkState(worker_state_t state) {
    gSensorState = state;
}

int set_active_group_to_channel(emu_fingerprint_hal_device_t *dev, int gid) {
    if (gid == gCurrentActiveGroup) {
        return 0;
    }
    gCurrentActiveGroup = gid;
    LOGD("%s active gid = %d.", __func__, gid);
    return 0;
}

/////////////////////////////////////// Receiver's functions //////////////////////////////////////////////

static void save_fplist(emu_fingerprint_hal_device_t *dev) {
#ifdef CFP_ENV_TEE_QSEE
    char *fplist_path = "/persist/data/ifaa_fplist";
    if (remove(fplist_path) != 0)
        LOGE("[%s]FILE: %s remove failed! [%d]", __func__, fplist_path, errno);
    int fd = open(fplist_path, O_RDWR | O_CREAT, 0700);
    LOGD("save_fplist fplist_path-->[%s]", fplist_path);
    write(fd, (char *)&dev->num_fingers_enrolled, sizeof(uint32_t));
    cfp_sensor_fp_get_all_fingerids(dev->all_fingerids);
    int i;
    for (i = 0; i < MAX_REGISTER_FP_COUNT; ++i) {
        if (dev->all_fingerids[i] > 0) {
            write(fd, (char *)&dev->all_fingerids[i], sizeof(uint32_t));
        }
    }
    close(fd);
#endif
}

void set_network_sock_fd(int fd) {
    g_client_sock = fd;
}

int cfp_notify_data(const void *buf, size_t n) {
    int ret = 0;
#ifdef HIDL_FEATURE
    send_fp_factory_hidl(buf, n);
#endif
    pthread_mutex_lock(&notify_mutex);
    if (g_client_sock < 0) {
        LOGE("send fd < 0!!!");
        return -1;
    }
    ret = send(g_client_sock, buf, n, 0);
    pthread_mutex_unlock(&notify_mutex);
    return ret;
}

int notify_hal_to_app(fingerprint_msg_t *msg) {
    int ret = 0;
    int size = 12;
    uint32_t data[3] = {0, 0, 0};
    data[0] = msg->type;
    // pthread_mutex_lock(&_tcp_mutex);
    switch (msg->type) {
    case FINGERPRINT_ERROR:
        data[1] = msg->data.error;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    case FINGERPRINT_AUTHENTICATED:
        data[1] = msg->data.authenticated.finger.fid;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d)", ret, errno, data[1]);
        break;
    case FINGERPRINT_TEMPLATE_ENROLLING:
        data[1] = msg->data.enroll.finger.fid;
        data[2] = msg->data.enroll.samples_remaining;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    case FINGERPRINT_ACQUIRED:
        data[1] = msg->data.acquired.acquired_info;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    case FINGERPRINT_TEMPLATE_REMOVED:
        data[1] = msg->data.removed.finger.fid;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    default:
        LOGE("Unexpected message type : %d", msg->type);
        // pthread_mutex_unlock(&_tcp_mutex);
        return -1;
    }
    // pthread_cond_signal(&_tcp_cond);
    // pthread_mutex_unlock(&_tcp_mutex);
    return 0;
}

int get_cmd_property_bytes(const uint32_t *data) {
    uint32_t _op_msg = ntohl(data[0]);
    LOGD("get_cmd_property_bytes: %d", _op_msg);
    switch (_op_msg) {
    case MSG_FINGER_DOWN:
    case MSG_FINGER_UP:
    case MSG_ENROLL:
    case MSG_MATCH:
    case MSG_ENUMERATE:
    case MSG_CANCEL:
    case MSG_DELETE:
    case MSG_CMD:
    case MSG_CMD_EXPOSURE_TIME:
        LOGD("_op_msg[%d], content[%d]", _op_msg, ntohl(data[1]));
        return (sizeof(int) + sizeof(uint32_t));
    case MSG_CMD_EXPORT_IMAGE: {
        char *path = (char *)(&data[1]);
        LOGD("_op_msg[%d], content[%s]", _op_msg, path);
        return (sizeof(int) + strlen(path));
    }
    default: {
        if (_op_msg >= 1050 && _op_msg <= 1100) { // ud factory test
            return (sizeof(int) + sizeof(uint32_t));
        }
        LOGW("Unknown Message: %d received", _op_msg);
        return 999;
    }
    }
}

void cmd_app_to_hal(const uint32_t *data) {
    int _op_msg = 0;
    _op_msg = ntohl(data[0]);
    LOGD("cmd_app_to_hal: %d", _op_msg);
    fingerprint_msg_t report_msg = {0};
    report_msg.type = FINGERPRINT_ACQUIRED;
    report_msg.data.authenticated.finger.fid = _op_msg;
    notify_hal_to_app(&report_msg);

    switch (_op_msg) {
    case MSG_ENROLL:
        g_dev->device.enroll(&g_dev->device, NULL, 0, 60);
        break;
    case MSG_TOUCH_SENSOR:
        g_dev->device.touch_sensor(&g_dev->device);
        break;
    case MSG_MATCH:
        g_dev->device.authenticate(&g_dev->device, 999, 0);
        break;
    case MSG_ENUMERATE:
        break;
    case MSG_CANCEL:
        g_dev->device.cancel(&g_dev->device);
        break;
    case MSG_DELETE: {
        int fid = ntohl(data[1]);
        LOGD("recv: errno(%d), fid(%d)", errno, fid);
        g_dev->device.remove(&g_dev->device, 999, fid);
        break;
    }

    case MSG_CMD: {
        int cmd = ntohl(data[1]);
        LOGD("recv: errno(%d), cmd(%d)", errno, cmd);
        break;
    }
    default:
        LOGW("Unknown Message: %d, content: %d", _op_msg, ntohl(data[1]));
        break;
    }
}

void notify_message(cfp_sensor_notify_data_t *data) {
    emu_fingerprint_hal_device_t *dev = g_dev;
    LOGD(" notify_message(%p)", data);
    cfp_sensor_notify_data_t notifyData = {0};
    memcpy(&notifyData, data, sizeof(cfp_sensor_notify_data_t));

    LOGD("result:{action=%d, msg_type=%d, custom_fp_id=%d, cfp_fp_id=%d, arg1=%d, arg2=%d, data=%p}", notifyData.action,
         notifyData.msg_type, notifyData.custom_fp_id, notifyData.cfp_fp_id, notifyData.arg1, notifyData.arg2,
         notifyData.data);

    if (notifyData.action == CFP_SENSOR_MOTION_ACTION_ON) {
        int acquired_info = notifyData.arg2;
        switch (notifyData.msg_type) {
        case FINGERPRINT_ACQUIRED: {
            LOGD(" receivingListener -- FINGERPRINT_ACQUIRED, acquired_info=%d", acquired_info);
            fingerprint_msg_t acquired_message = {0};
            acquired_message.type = FINGERPRINT_ACQUIRED;
            acquired_message.data.acquired.acquired_info = acquired_info;
            if (!is_use_network) {
                dev->device.notify(&acquired_message);
            }
            notify_hal_to_app(&acquired_message);
            break;
        }
        case FINGERPRINT_AUTHENTICATED: {
            if (gSensorState != STATE_SCAN) {
                LOGW("Got Authentication Message while Not STATE_SCAN");
                break;
            }
            fingerprint_msg_t acquired_message = {0};
            acquired_message.type = FINGERPRINT_AUTHENTICATED;
            if (notifyData.custom_fp_id == 0) { // failed
                LOGD("Authenticate failed!");
                acquired_message.data.authenticated.finger.fid = 0;
                acquired_message.data.authenticated.finger.gid = 0;
                if (!is_use_network) {
                    dev->device.notify(&acquired_message);
                }
                notify_hal_to_app(&acquired_message);
            } else { // success
                acquired_message.data.authenticated.finger.gid = gCurrentActiveGroup;
                acquired_message.data.authenticated.finger.fid = notifyData.custom_fp_id;
                if (!is_use_network) {
                    dev->device.notify(&acquired_message);
                }
                notify_hal_to_app(&acquired_message);
            }

            break;
        }
        case FINGERPRINT_TEMPLATE_ENROLLING: {

            if (gSensorState != STATE_ENROLL) {
                LOGW("Got Enrollment Message while Not STATE_ENROLL");
                break;
            }
            int remainning = notifyData.arg1;

            if (remainning <= 0) {
                dev->authenticator_id = get_64bit_rand();
                LOGD(" FINGERPRINT_TEMPLATE_ENROLLING, new authenticator_id=%lu",
                     (long unsigned int)dev->authenticator_id);
                gSensorState = STATE_IDLE;
            }

            fingerprint_msg_t message = {0};
            message.type = FINGERPRINT_ACQUIRED;
            message.data.acquired.acquired_info = FINGERPRINT_ACQUIRED_GOOD;
            if (!is_use_network) {
                dev->device.notify(&message);
            }
            LOGD(" listener_send_notice, notified FINGERPRINT_ACQUIRED_GOOD");
            LOGD(" receivingListener -- FINGERPRINT_TEMPLATE_ENROLLING, acquired_info=%d", acquired_info);
            fingerprint_msg_t acquired_message = {0};
            acquired_message.type = FINGERPRINT_TEMPLATE_ENROLLING;
            acquired_message.data.enroll.finger.fid = notifyData.cfp_fp_id;
            if (gCurrentActiveGroup != gLastOperationGroup) {
                LOGW("User/Group changed after save,but before notify!!");
            }

            int ret = checkout_memory();
            if (ret != 0) {
                LOGE("checkout_memory failed !!");
                fingerprint_msg_t error_message = {0};
                error_message.type = FINGERPRINT_ERROR;
                error_message.data.error = FINGERPRINT_ERROR_NO_SPACE;
                LOGE("notify FINGERPRINT_ERROR_NO_SPACE !");
                if (!is_use_network) {
                    dev->device.notify(&error_message);
                }
                notify_hal_to_app(&error_message);
                setWorkState(STATE_IDLE);
                break;
            }
            acquired_message.data.enroll.finger.gid = gLastOperationGroup;
            acquired_message.data.enroll.samples_remaining = remainning;
            if (!is_use_network) {
                dev->device.notify(&acquired_message);
            }
            notify_hal_to_app(&acquired_message);

            if (remainning == 0) {
                LOGD("################## SAVE TEMPLATE ############################");
                dev->num_fingers_enrolled += 1;
                LOGD("save template, dev= %p, finger_num = [%d]", dev, dev->num_fingers_enrolled);
                save_fplist(dev);
            }

            break;
        }
        default:
            break;
        }
    } else if (notifyData.action == CFP_SENSOR_MOTION_ACTION_OFF) {
        LOGD("finger off %d", notifyData.custom_fp_id);
    } else if (notifyData.action == CFP_SENSOR_MOTION_REMOVE_FP) {
        if (notifyData.msg_type == FINGERPRINT_TEMPLATE_REMOVED) {
            LOGD(" cfp_fp_id=%d, FINGERPRINT_TEMPLATE_REMOVED", notifyData.cfp_fp_id);
            dev->num_fingers_enrolled -= 1;
            if (dev->num_fingers_enrolled <= 0) {
                dev->num_fingers_enrolled = 0;
                dev->authenticator_id = 0xdecefacedeceface;
            }
            save_fplist(dev);
        } else if (notifyData.msg_type == FINGERPRINT_ERROR) {
            LOGD(" cfp_fp_id=%d, fingerprint template removing error!", notifyData.cfp_fp_id);
            fingerprint_msg_t error_message = {0};
            error_message.type = FINGERPRINT_ERROR;
            error_message.data.error = notifyData.arg2;
            if (!is_use_network) {
                dev->device.notify(&error_message);
            }
        } else {
            LOGD(" cfp_fp_id=%d, fingerprint template removing state cannot be recognized!", notifyData.cfp_fp_id);
        }
    } else if (notifyData.action == CFP_SENSOR_MOTION_ERROR) {
        fingerprint_msg_t error_message = {0};
        error_message.type = FINGERPRINT_ERROR;
        error_message.data.error = notifyData.arg2;
        if (!is_use_network) {
            dev->device.notify(&error_message);
        }
        notify_hal_to_app(&error_message);
        // for java will not call stopEnrollment while onError(3)
        if (error_message.data.error == FINGERPRINT_ERROR_TIMEOUT) {
            setWorkState(STATE_IDLE);
        }
    } else if ((notifyData.action == CFP_SENSOR_MOTION_TOUCH || notifyData.action == CFP_SENSOR_MOTION_UNTOUCH) &&
               notifyData.msg_type == FINGERPRINT_ACQUIRED) {
        fingerprint_msg_t acquired_message = {0};
        acquired_message.type = FINGERPRINT_ACQUIRED;
        acquired_message.data.acquired.acquired_info = notifyData.arg1;
        if (!is_use_network) {
            dev->device.notify(&acquired_message);
        }
    } else if (notifyData.action == CFP_SENSOR_MOTION_TEST) {
        fingerprint_msg_t msg = {0};
        msg.type = notifyData.msg_type;
        if (!is_use_network) {
            dev->device.notify(&msg);
        }
    } else if (notifyData.action == CFP_SENSOR_MOTION_FACTORY_TEST) {
        fingerprint_msg_t message = {0};
        message.type = FINGERPRINT_ACQUIRED;
        // factory apk will deal with the acquired message.
        message.data.acquired.acquired_info = 5 + notifyData.arg1;
        if (!is_use_network) {
            dev->device.notify(&message);
        }
        int cmd = notifyData.arg2;
        if (cmd >= 3000) {
            fingerprint_msg_t acquired_message = {0};
            acquired_message.data.enroll.finger.fid = cmd;
            acquired_message.data.enroll.samples_remaining = notifyData.arg1;
            notify_hal_to_app(&acquired_message);
        }
    } else {
        LOGE("error: motion type:%d", notifyData.action);
    }
}

/////////////////////////////////////// Sender's functions //////////////////////////////////////////////

int send_fp_scan_on(fingerprint_msg_type_t type, int fingerId, int cfp_fpid, int arg1, int arg2, char *data) {
    gSensorNotifyData.action = CFP_SENSOR_MOTION_ACTION_ON;
    gSensorNotifyData.msg_type = type;
    gSensorNotifyData.custom_fp_id = fingerId;
    gSensorNotifyData.cfp_fp_id = cfp_fpid;
    gSensorNotifyData.arg1 = arg1;
    gSensorNotifyData.arg2 = arg2;
    gSensorNotifyData.data = (unsigned char *)data;
    notify_message(&gSensorNotifyData);
    return 0;
}

int send_fp_touch() {
    LOGD(" send_fp_touch()");
    gSensorNotifyData.action = CFP_SENSOR_MOTION_TOUCH;
    gSensorNotifyData.msg_type = FINGERPRINT_ACQUIRED;
    gSensorNotifyData.arg1 = FINGERPRINT_ACQUIRED_GOOD;
    notify_message(&gSensorNotifyData);
    return 0;
}

int send_fp_untouch() {
    LOGD(" send_fp_untouch()");
    gSensorNotifyData.action = CFP_SENSOR_MOTION_UNTOUCH;
    gSensorNotifyData.msg_type = FINGERPRINT_ACQUIRED;
    gSensorNotifyData.arg1 = FINGERPRINT_ACQUIRED;
    notify_message(&gSensorNotifyData);
    return 0;
}

int send_fp_error(int fingerId, int cfp_fpid, int arg1, int arg2) {
    LOGD(" send_fp_error(%d, %d, %d, %d...)", fingerId, cfp_fpid, arg1, arg2);
    gSensorNotifyData.action = CFP_SENSOR_MOTION_ERROR;
    gSensorNotifyData.msg_type = FINGERPRINT_ERROR;
    gSensorNotifyData.custom_fp_id = fingerId;
    gSensorNotifyData.cfp_fp_id = cfp_fpid;
    gSensorNotifyData.arg1 = arg1;
    gSensorNotifyData.arg2 = arg2;
    gSensorNotifyData.data = NULL;
    notify_message(&gSensorNotifyData);
    return 0;
}

int send_fp_removed(fingerprint_msg_type_t type, int fingerId, int cfp_fpid, int arg1, int arg2) {
    LOGD(" send_fp_removed(%d, %d, %d, %d, %d...)", type, fingerId, cfp_fpid, arg1, arg2);
    gSensorNotifyData.action = CFP_SENSOR_MOTION_REMOVE_FP;
    gSensorNotifyData.msg_type = type;
    gSensorNotifyData.custom_fp_id = fingerId;
    gSensorNotifyData.cfp_fp_id = cfp_fpid;
    gSensorNotifyData.arg1 = arg1;
    gSensorNotifyData.arg2 = arg2;
    gSensorNotifyData.data = NULL;
    notify_message(&gSensorNotifyData);
    return 0;
}

int send_fp_factory_test(int result, int cmd) {
    LOGD(" send_fp_factory_test() result = [%d]", result);
    gSensorNotifyData.action = CFP_SENSOR_MOTION_FACTORY_TEST;
    gSensorNotifyData.arg1 = result;
    gSensorNotifyData.arg2 = cmd;
    notify_message(&gSensorNotifyData);
    return 0;
}
