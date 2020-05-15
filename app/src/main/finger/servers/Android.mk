
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := finger_hal

LOCAL_CFLAGS := -Wall -Werror \
    -DLOG_TAG='"finger_hal"' \

LOCAL_SRC_FILES  += FingerprintTest.cpp \
                    fingerprint_hidl.c

LOCAL_SHARED_LIBRARIES := liblog \
                          libutils \
                          libhidlbase \
                          libhidltransport \
                          android.hardware.finger@1.0

include $(BUILD_STATIC_LIBRARY)
