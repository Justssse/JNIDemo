LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_LDLIBS := -llog
LOCAL_LDLIBS += -landroid -lz -lm

# 要生成的.so库名称。
LOCAL_MODULE := fingerprint.default

# C++文件
LOCAL_SRC_FILES := fingerprint.c \
					fp_network.c

MY_LIB_DIR := $(LOCAL_PATH)/../fingerLib

# 增加HIDL特性，定义宏
LOCAL_CFLAGS += -v -DHIDL_FEATURE

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../finger

LOCAL_LDFLAGS += $(MY_LIB_DIR)/finger_hal.a
LOCAL_LDFLAGS += $(MY_LIB_DIR)/libhidlbase.so
LOCAL_LDFLAGS += $(MY_LIB_DIR)/libutils.so
LOCAL_LDFLAGS += $(MY_LIB_DIR)/libhidltransport.so
LOCAL_LDFLAGS += $(MY_LIB_DIR)/android.hardware.finger@1.0.so

LOCAL_LDLIBS += -lm -l:libc++.a

include $(BUILD_SHARED_LIBRARY)