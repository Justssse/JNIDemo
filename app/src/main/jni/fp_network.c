#include "fp_network.h"
#include "fp_channel.h"
#include "mylog.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/in.h>
#include <net/if.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define HAL_PATH "/system/lib64/cdfinger.fingerprint.default.so"
#define HAL_PATH_UPDATE HAL_PATH "_update"
// #define UDP_PORT 18930
#define TCP_PORT 18938 // 18928 + 10
#define BACKLOG 100
int _tcp_s_sock = -1;
int _client_sock = -1;
fingerprint_device_t *_sys_hal_device = NULL;

pthread_mutex_t _tcp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t _tcp_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t notify_mutex = PTHREAD_MUTEX_INITIALIZER;

int get_cmd_property_bytes(const uint32_t *data) {
    uint32_t _op_msg = data[0];
    switch (_op_msg) {
    case MSG_CMD_EXPORT_IMAGE:
    case MSG_CMD_DELETE_IMAGE:
    case MSG_CMD_SET_SAVE_IMAGE_PATH:
    case MSG_CMD_UPDATE_HAL:
    case MSG_CMD_UPDATE_TAC:
    case MSG_CMD_UPDATE_TEE:
    case MSG_CMD_SET_ALGO_PARAMETERS: {
        uint32_t buffer_size = data[1];
        LOGD("_op_msg[%d], buffer_size[%d]", _op_msg, buffer_size);
        return (int)(sizeof(int) * 2 + buffer_size);
    }
    default: {
        return (sizeof(int) * 2);
    }
    }
}

// Notice: some cases are directly return, don't need notify
void cmd_app_to_hal(const uint32_t *data) {
    uint32_t _op_msg = data[0];
    uint32_t parameter = data[1];
    LOGD("cmd_app_to_hal device: %p, _op_msg: %d, parameter: %d", g_finger_dev, _op_msg, parameter);
    fingerprint_msg_t report_msg = {0};
    report_msg.type = FINGERPRINT_CMD_ACK;
    report_msg.data.authenticated.finger.fid = _op_msg;
    notify_hal_to_app(&report_msg);
    int ret = 0;
    switch (_op_msg) {
    case MSG_ENROLL: {
        hw_auth_token_t hat;
        hat.version = 66;
        g_finger_dev->device.enroll(&g_finger_dev->device, &hat, 0, 60);
        break;
    }
    case MSG_TOUCH_SENSOR:
        LOGD("MSG_TOUCH_SENSOR匹配成功");
        g_finger_dev->device.touch_sensor(&g_finger_dev->device);
        break;
    case MSG_MATCH:
        g_finger_dev->device.authenticate(&g_finger_dev->device, 0, 0);
        break;
    case MSG_ENUMERATE:
        g_finger_dev->device.enumerate(&g_finger_dev->device);
        break;
    case MSG_CANCEL:
        g_finger_dev->device.cancel(&g_finger_dev->device);
        break;
    case MSG_DELETE: {
        LOGD("recv: errno(%d), fid(%d)", errno, parameter);
        g_finger_dev->device.remove(&g_finger_dev->device, 0, parameter);
        break;
    }
    default:
        LOGD("default Message: %d, parameter: %d", _op_msg, parameter);
        break;
    }
    report_msg.type = FINGERPRINT_CMD_RESULT;
    report_msg.data.enroll.finger.fid = _op_msg;
    report_msg.data.enroll.samples_remaining = (uint32_t)ret;
    notify_hal_to_app(&report_msg);

    if (_op_msg == MSG_CMD_KILL_FINGERPRINTD) {
        exit(1);
    }
}

int CreateTCPSocket() {
    int tcp_sock = 0;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int sock_type = SOCK_STREAM; // SOCK_DGRAM

    if ((tcp_sock = socket(AF_INET, sock_type, 0)) < 0) {
        LOGE("create socket failed! %s(%d))", strerror(errno), errno);
        return -1;
    }
    int on = 1;
    setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (bind(tcp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("bind socket failed! %s(%d))", strerror(errno), errno);
        if (errno != EADDRINUSE) { // Address already in use(98))
            close(tcp_sock);
            return -1;
        }
    }
    if (listen(tcp_sock, BACKLOG) < 0) {
        close(tcp_sock);
        LOGE("listen socket failed! %s(%d))", strerror(errno), errno);
        return -1;
    }
    return tcp_sock;
}

static void *TCPClientWorker(void *arg) {
    // pthread_detach(pthread_self());
#define RECV_SIZE 256
    int local_client_sock = *((int *)arg);
    LOGD("Client[%d] worker start", local_client_sock);
    char data[RECV_SIZE] = {0};
    while (1) {
        memset(data, 0, sizeof(data));
        int rbytes = (int)recv(local_client_sock, &data, sizeof(data), 0);
        if (rbytes <= 0) {
            LOGE("recv failed: %s, rbytes(%d)", strerror(errno), rbytes);
            goto TCP_CLIENT_END;
        }
        is_use_network = 1;
        LOGD("Client[%d] received: %d", local_client_sock, rbytes);
        int offset = 0;
        char *ptr = (char *)data;
        while (offset < rbytes) {
            uint32_t *pMsg = (uint32_t *)(ptr + offset);
            pthread_mutex_lock(&_tcp_mutex);
            int cmdSize = get_cmd_property_bytes((const uint32_t *)pMsg);
            if (cmdSize > RECV_SIZE) {
                LOGD("current cmd size: %d", cmdSize);
                char *buffer = (void *)malloc((size_t)cmdSize);
                if (buffer == NULL) {
                    LOGE("%s buffer is NULL, malloc size: %d", __func__, cmdSize);
                    break;
                }
                int ofs = 0;
                memset(buffer, 0, (size_t)cmdSize);
                memcpy(buffer, data, RECV_SIZE);
                ofs += RECV_SIZE;
                char temp_buffer[2048] = {0};
                do {
                    int r = (int)recv(local_client_sock, temp_buffer, sizeof(temp_buffer), 0);
                    if (r <= 0) {
                        LOGE("Line %d, recv failed: %s, rbytes(%d)", __LINE__, strerror(errno), r);
                        free(buffer);
                        goto TCP_CLIENT_END;
                    }
                    memcpy(buffer + ofs, temp_buffer, (size_t)r);
                    ofs += r;
                } while (ofs < cmdSize);
                cmd_app_to_hal((const uint32_t *)buffer);
                free(buffer);
            } else {
                cmd_app_to_hal((const uint32_t *)pMsg);
            }
            offset += cmdSize;
            pthread_mutex_unlock(&_tcp_mutex);
            LOGD("Client[%d] offset: %d, total: %d", local_client_sock, offset, rbytes);
        }
    }
TCP_CLIENT_END:
    LOGD("Client[%d] worker end", local_client_sock);
    is_use_network = 0;
    close(local_client_sock);
    // cfp_file_close(_tcp_s_sock);
    // _tcp_s_sock = -1;

#ifdef _WIN32
    WSACleanup();
#endif
    return NULL;
}

static void *TCPListener(void *arg) {
    // pthread_detach(pthread_self());
    (void)arg;
    while (1) {
        if (_tcp_s_sock == -1) {
            _tcp_s_sock = CreateTCPSocket();
            if (_tcp_s_sock == -1) {
                sleep(1);
                continue;
            }
        }
        struct sockaddr_in client_addr;
        socklen_t length = sizeof(client_addr);

        int _client_sock_temp = accept(_tcp_s_sock, (struct sockaddr *)&client_addr, &length);
        if (_client_sock_temp < 0) {
            LOGE("accept failed: %s(%d))", strerror(errno), errno);
            close(_tcp_s_sock);
            _tcp_s_sock = -1;
            continue;
        }
        _client_sock = _client_sock_temp;
        LOGD("Client[%d] connected!", _client_sock_temp);
        pthread_t pid = 0;
        pthread_create(&pid, NULL, TCPClientWorker, &_client_sock_temp);
        usleep(1000 * 1000); // wait TCPClientWorker created
    }
    return NULL;
}

int cfp_notify_data(const void *buf, size_t n) {
    int ret = -1;
    if (is_use_network) {
        if (_client_sock < 0) {
            LOGE("send fd < 0!!!");
            return -1;
        }
        if (n < 12) {
            LOGE("%s buffer size must >= 12 !", __func__);
            return -1;
        }
        pthread_mutex_lock(&notify_mutex);
        int sbytes = 0;
        while (sbytes < n) {
            if ((n - sbytes) < 1024) {
                ret = (int)send(_client_sock, buf + sbytes, n - sbytes, 0);
            } else {
                ret = (int)send(_client_sock, buf + sbytes, 1024, 0);
            }
            if (ret >= 0) {
                sbytes += ret;
            } else {
                LOGE("socket send failed: %d, errno: %d", ret, errno);
                break;
            }
        }
        LOGD("%s %d, 0x%08x, 0x%08x, 0x%08x, %zu, %d, err: %d", __func__, _client_sock, ((int *)buf)[0],
             ((int *)buf)[1], ((int *)buf)[2], n, ret, errno);
        pthread_mutex_unlock(&notify_mutex);
    }
    return ret;
}

int notify_hal_to_app(const fingerprint_msg_t *msg) {
    int ret = 0;
    size_t size = 12;
    uint32_t data[3] = {0, 0, 0};
    data[0] = (uint32_t)msg->type;
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
    case FINGERPRINT_TEMPLATE_ENUMERATING:
        data[1] = msg->data.enumerated.finger.fid;
        data[2] = msg->data.enumerated.remaining_templates;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    case FINGERPRINT_CMD_ACK:
        data[1] = msg->data.authenticated.finger.fid;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    case FINGERPRINT_TEMPLATE_REMOVED:
        data[1] = msg->data.removed.finger.fid;
        ret = cfp_notify_data(data, size);
        LOGD("send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    case FINGERPRINT_CMD_RESULT:
        data[1] = msg->data.enroll.finger.fid;
        data[2] = msg->data.enroll.samples_remaining;
        ret = cfp_notify_data(data, size);
        LOGD("send:FINGERPRINT_CMD_RESULT ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        break;
    default:
        data[1] = msg->data.removed.finger.fid;
        ret = cfp_notify_data(data, size);
        LOGE("Error cmd - send: ret(%d), errno(%d), data(%d, %d)", ret, errno, data[0], data[1]);
        return -1;
    }
    return 0;
}

int cfp_network_enable() {
    pthread_t tcp_pid = 0;
    pthread_create(&tcp_pid, NULL, TCPListener, NULL);
    return 0;
}
