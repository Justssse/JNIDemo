/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_INCLUDE_HARDWARE_QEMUD_H
#define ANDROID_INCLUDE_HARDWARE_QEMUD_H

#define ANDROID_SOCKET_NAMESPACE_RESERVED 1

#include <sys/socket.h>
#include "qemu_pipe.h"
#include "socket_local_client_unix.c"
#include "mylog.h"

/* the following is helper code that is used by the QEMU-specific
 * hardware HAL modules to communicate with the emulator program
 * through the 'qemud' multiplexing daemon, or through the qemud
 * pipe.
 *
 * see the documentation comments for details in
 * development/emulator/qemud/qemud.c
 *
 * all definitions here are built into the HAL module to avoid
 * having to write a tiny shared library for this.
 */

/* we expect the D macro to be defined to a function macro
 * that sends its formatted string argument(s) to the log.
 * If not, ignore the traces.
 */
#ifndef D
#  define  D(...)   do{}while(0)
#endif


static __inline__ int
qemud_channel_open(const char*  name)
{
    int  fd;
    int  namelen = strlen(name);
    char answer[2];
    char pipe_name[256];

    /* First, try to connect to the pipe. */
    snprintf(pipe_name, sizeof(pipe_name), "qemud:%s", name);
    fd = qemu_pipe_open(pipe_name);
    LOGE("%s: pipe name %s (name %s) fd %d", __FUNCTION__, pipe_name, name, fd);
    if (fd < 0) {
        LOGE("QEMUD pipe is not available for %s: %s", name, strerror(errno));
        /* If pipe is not available, connect to qemud control socket */
        fd = socket_local_client( "qemud",
                                  ANDROID_SOCKET_NAMESPACE_RESERVED,
                                  SOCK_STREAM );
        if (fd < 0) {
            LOGE("no qemud control socket: %s", strerror(errno));
            return -1;
        }

        /* send service name to connect */
        if (!WriteFully(fd, name, namelen)) {
            LOGE("can't send service name to qemud: %s",
               strerror(errno));
            close(fd);
            return -1;
        }

        /* read answer from daemon */
        if (!ReadFully(fd, answer, 2) ||
            answer[0] != 'O' || answer[1] != 'K') {
            LOGE("cant' connect to %s service through qemud", name);
            close(fd);
            return -1;
        }
    }
    return fd;
}

static __inline__ int
qemud_channel_send(int  fd, const void*  msg, int  msglen)
{
    char  header[5];

    if (msglen < 0)
        msglen = strlen((const char*)msg);

    if (msglen == 0)
        return 0;

    snprintf(header, sizeof header, "%04x", msglen);
    if (!WriteFully(fd, header, 4)) {
        D("can't write qemud frame header: %s", strerror(errno));
        return -1;
    }

    if (!WriteFully(fd, msg, msglen)) {
        D("can4t write qemud frame payload: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static __inline__ int
qemud_channel_recv(int  fd, void*  msg, int  msgsize)
{
    char  header[5];
    int   size;

    if (!ReadFully(fd, header, 4)) {
        D("can't read qemud frame header: %s", strerror(errno));
        return -1;
    }
    header[4] = 0;
    if (sscanf(header, "%04x", &size) != 1) {
        D("malformed qemud frame header: '%.*s'", 4, header);
        return -1;
    }
    if (size > msgsize)
        return -1;

    if (!ReadFully(fd, msg, size)) {
        D("can't read qemud frame payload: %s", strerror(errno));
        return -1;
    }
    return size;
}

#endif /* ANDROID_INCLUDE_HARDWARE_QEMUD_H */
