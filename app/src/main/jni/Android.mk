LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_LDLIBS := -llog

# 要生成的.so库名称。
LOCAL_MODULE := fingerprint.default

# C++文件
LOCAL_SRC_FILES := fingerprint.c fp_network.c

include $(BUILD_SHARED_LIBRARY)