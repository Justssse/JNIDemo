#include "FingerprintTest.h"
#include "fingerprint_hidl_service.h"

#include <binder/IPCThreadState.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/StrongPointer.h>

void add_hidl_service(finger_hal_t *device){
    android::hardware::finger::V1_0::implementation::FingerprintTest::instantiate(device);
    ALOGE("add_factorytest_service");
}

namespace android {
namespace hardware {
namespace finger {
namespace V1_0 {
namespace implementation {

using :: std::vector;

FingerprintTest *FingerprintTest::sInstance = NULL;

void FingerprintTest::instantiate(finger_hal_t *device){
    if (sInstance == NULL){
        sInstance = new FingerprintTest(device);
        if (sInstance->registerAsService() != android::OK) {
            ALOGE("Failed to register FingerprintTest");
        }
    }
}

FingerprintTest::FingerprintTest(finger_hal_t *device)
    : mCallback(NULL), mDevice(device){};

void FingerprintTest::serviceDied(uint64_t cookie, const wp<IBase> &who){
    (void)cookie;
    if (who == mCallback){
        ALOGE("FingerprintTest serviceDied");
    }
}

void FingerprintTest::testOnResult(void *context, const unsigned char *buffer, uint32_t size){
    FingerprintTest *self = static_cast<FingerprintTest *>(context);
    if (self->mCallback != NULL){
        hidl_vec<uint8_t> vbuf;
        vbuf.resize(size);
        memcpy(vbuf.data(), buffer, size);
        if (!self->mCallback->onResult(vbuf).isOk()){
            ALOGE("%s callback failed", __func__);
        }
    }
}

// Methods from IFingerprintTest follow.
Return<void> FingerprintTest::sendMessage(const hidl_vec<uint8_t>& buffer, 
                                            const sp<IFingerprintCallback>& callback) {
    // TODO implement
    if (mCallback != NULL){
        mCallback->unlinkToDeath(this);
    }
    mCallback = callback;
    if (mDevice){
        mDevice->sendMessage(mDevice, buffer.data(), buffer.size(), this, FingerprintTest::testOnResult);
    }
    return Void();
}


// Methods from ::android::hidl::base::V1_0::IBase follow.

//IFingerprintTest* HIDL_FETCH_IFingerprintTest(const char* /* name */) {
//    return new FingerprintTest();
//}

}  // namespace implementation
}  // namespace V1_0
}  // namespace finger
}  // namespace hardware
}  // namespace android
