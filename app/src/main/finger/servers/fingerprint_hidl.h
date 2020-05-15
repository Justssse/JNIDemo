#ifndef FINGER_HIDL_H
#define FINGER_HIDL_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*finger_result_cb_t)(void *ctx, const unsigned char *buffer, uint32_t size);

typedef struct finger_hal finger_hal_t;

struct finger_hal {
    void (*sendMessage)(finger_hal_t *self, const unsigned char *buffer, 
                        uint32_t size, void *ctx,finger_result_cb_t result_cb);
};

finger_hal_t *finger_hal_new(void *dev);

void finger_destroy(finger_hal_t *self);

void *finger_set_callback(finger_hal_t *self, void *context,finger_result_cb_t result_cb);

void finger_notify(finger_hal_t *self, const unsigned char *buffer, uint32_t size);

#endif