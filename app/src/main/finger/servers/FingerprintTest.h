#ifndef ANDROID_HARDWARE_FINGER_V1_0_FINGERPRINTTEST_H
#define ANDROID_HARDWARE_FINGER_V1_0_FINGERPRINTTEST_H

#include "fingerprint_hidl.h"
#include <android/hardware/finger/1.0/IFingerprintTest.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace finger {
namespace V1_0 {
namespace implementation {

using ::android::sp;
using ::android::wp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::finger::V1_0::IFingerprintCallback;
using ::android::hardware::finger::V1_0::IFingerprintTest;
using ::android::hidl::base::V1_0::IBase;
using ::android::hidl::base::V1_0::DebugInfo;

struct FingerprintTest : public IFingerprintTest , public hidl_death_recipient{
    
    Return<void> sendMessage(const hidl_vec<uint8_t>& buffer, const sp<IFingerprintCallback>& callback) override;
    virtual void serviceDied(uint64_t cookie, const wp<IBase> &who) override;

    public:
        static void instantiate(finger_hal_t *device);

    private:
        FingerprintTest(finger_hal_t *device);
        static void testOnResult(void *context, const unsigned char *buffer, uint32_t size);
        static FingerprintTest *sInstance;
        sp<IFingerprintCallback> mCallback;
        finger_hal_t *mDevice;

};

// FIXME: most likely delete, this is only for passthrough implementations
// extern "C" IFingerprintTest* HIDL_FETCH_IFingerprintTest(const char* name);

}  // namespace implementation
}  // namespace V1_0
}  // namespace finger
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_FINGER_V1_0_FINGERPRINTTEST_H
