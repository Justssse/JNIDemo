#include "fp_network.h"
#include "fp_channel.h"
#include "fingerprint.h"
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


#define TCP_PORT 18938 // 18928 + 10
#define BACKLOG 100
int _tcp_s_sock = -1;
int _client_sock = -1;
fingerprint_device_t *_sys_hal_device = NULL;

pthread_mutex_t _tcp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t _tcp_cond = PTHREAD_COND_INITIALIZER;

extern unsigned long long cfp_get_uptime();

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
        close(tcp_sock);
        return -1;
    }
    if (listen(tcp_sock, BACKLOG) < 0) {
        close(tcp_sock);
        LOGE("listen socket failed! %s(%d))", strerror(errno), errno);
        return -1;
    }
    return tcp_sock;
}

#if 0
int CreateBroadcastUDPSocket()
{
    int udp_sock = 0;
    int sock_type = SOCK_DGRAM;
    if ((udp_sock = socket(AF_INET, sock_type, 0)) < 0)
    {
        LOGE("create socket failed! %s(%d))", strerror(errno), errno);
        return -1;
    }
    int opt = -1;
    int ret =
        setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
    if (ret == -1)
    {
        LOGE("setsockopt SO_BROADCAST failed! %s", strerror(errno));
        return -1;
    }
    return udp_sock;
}

int get_local_ip(char *ifname, char *ip)
{
    char *temp = NULL;
    int inet_sock;
    struct ifreq ifr;

    inet_sock = socket(AF_INET, SOCK_DGRAM, 0);

    memset(ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    memcpy(ifr.ifr_name, ifname, strlen(ifname));

    if (0 != ioctl(inet_sock, SIOCGIFADDR, &ifr))
    {
        perror("ioctl error");
        return -1;
    }

    temp = inet_ntoa(((struct sockaddr_in *)(&(ifr.ifr_addr)))->sin_addr);
    memcpy(ip, temp, strlen(temp));

    close(inet_sock);

    return 0;
}
#endif

static void *TCPClientWorker(void *arg) {
    LOGD("Client[%d] worker start", _client_sock);
    uint32_t data[30] = {0};
    while (1) {
        // pthread_mutex_lock(&_tcp_mutex);
        memset(data, 0, 30);
        int ret = recv(_client_sock, &data, sizeof(data), 0);
        if (ret <= 0) {
            LOGE("recv failed: %s, ret(%d)", strerror(errno), ret);
            goto TCP_CLIENT_END;
        }
        LOGD("Client[%d] received: %d", _client_sock, ret);
        for (int i = 0; i < ret / sizeof(int); ++i) {
            LOGD("Client[%d] dump: %d", _client_sock, ntohl(data[i]));
        }
        int offset = 0;
        char *ptr = (char *)data;
        while (offset < ret) {
            uint32_t *pMsg = (uint32_t *)(ptr + offset);
            cmd_app_to_hal((const uint32_t *)pMsg);
            offset += get_cmd_property_bytes((const uint32_t *)pMsg);
            LOGD("Client[%d] offset: %d, total: %d", _client_sock, offset, ret);
        }
    }
TCP_CLIENT_END:
    LOGD("Client[%d] worker end", _client_sock);
    close(_client_sock);
    _client_sock = -1;
    close(_tcp_s_sock);
    _tcp_s_sock = -1;
    return NULL;
}

static void *TCPListener(void *arg) {
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
        if (_client_sock != -1) {
            close(_client_sock);
        }
        _client_sock = _client_sock_temp;
        LOGD("Client[%d] connected!", _client_sock);
        set_network_sock_fd(_client_sock);
        pthread_t pid = 0;
        pthread_create(&pid, NULL, TCPClientWorker, &_client_sock);
        usleep(1000 * 1000); // wait TCPClientWorker created
    }
    return NULL;
}

#if 0
static void *UDPBroadcaster(void *arg)
{
    char *msg = (char *)malloc(sizeof(char) * 128);
    sprintf(msg, "%s:%d", "cdfinger", TCP_PORT);
    while (1)
    {
        if (_udp_s_sock < 0)
        {
            _udp_s_sock = CreateBroadcastUDPSocket();
        }
        char ip[32] = {0};
        get_local_ip("wlan0", ip);
        // LOGD("%s", ip);
        in_addr_t _addr = inet_addr(ip) | 0xFF000000;
        // LOGD("0x%08x", _addr);
        struct sockaddr_in addr;
        bzero(&addr, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(UDP_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        socklen_t len = sizeof(addr);

        int ret =
            sendto(_udp_s_sock, msg, strlen(msg), 0, (struct sockaddr *)&addr, len);
        __android_log_print(ANDROID_LOG_DEBUG, "UDPBroadcaster",
                            "ret(%d), msg(%s), errno(%d) %s", ret, msg, errno,
                            strerror(errno));
        if (ret <= 0)
        {
            close(_udp_s_sock);
            _udp_s_sock = -1;
        }
        usleep(2 * 1000 * 1000); // 2 sec
    }
    if (msg != NULL)
        free(msg);
    return NULL;
}
#endif

int cfp_network_enable() {
    // pthread_t udp_pid = 0;
    // pthread_create(&udp_pid, NULL, UDPBroadcaster, NULL);
    pthread_t tcp_pid = 0;
    pthread_create(&tcp_pid, NULL, TCPListener, NULL);
    return 0;
}
