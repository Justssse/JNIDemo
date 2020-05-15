#include "fingerprint_hidl.h"
#include <stdlib.h>

typedef struct {
    finger_hal_t f_hal;
    void *cb_ctx;
    finger_result_cb_t result_cb;
    void *dev;
} hal_module_t;

finger_hal_t *finger_hal_new(void *dev) {
    hal_module_t *module = malloc(sizeof(hal_module_t));
    if (!module) {
        return NULL;
    }
    module->dev = dev;
    return (finger_hal_t *)module;
}

void finger_destroy(finger_hal_t *self){
    free(self);
}

void *finger_set_callback(finger_hal_t *self, void *context,
                        finger_result_cb_t result_cb){
    if (self == NULL) {
        return NULL;
    }
    hal_module_t *module = (hal_module_t *)self;
    module->cb_ctx = context;
    module->result_cb = result_cb;
    return module->dev;
}

void finger_notify(finger_hal_t *self, const unsigned char *buffer, uint32_t size){
    if (self == NULL) {
        return;
    }
    hal_module_t *module = (hal_module_t *)self;
    if (module->result_cb == NULL) {
        return;
    }
    module->result_cb(module->cb_ctx, buffer, size);
}
