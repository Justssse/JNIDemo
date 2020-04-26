//
// Created by finger on 2020/4/21.
//

#ifndef JNIDEMO_MYLOG_H
#define JNIDEMO_MYLOG_H

#define LOG_TAG "FingerprintHal-dragon"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__);
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);

#endif //JNIDEMO_MYLOG_H
