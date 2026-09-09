#ifndef ZEPHYR_DRIVERS_INPUT_IQS9151_TEST_H_
#define ZEPHYR_DRIVERS_INPUT_IQS9151_TEST_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "iqs9151_params.h"

struct iqs9151_test_frame {
    int16_t rel_x;
    int16_t rel_y;
    uint16_t info_flags;
    uint16_t trackpad_flags;
    uint8_t finger_count;
    uint16_t finger1_x;
    uint16_t finger1_y;
    uint16_t finger2_x;
    uint16_t finger2_y;
};

enum iqs9151_test_event_type {
    IQS9151_TEST_EVENT_KEY = 0,
    IQS9151_TEST_EVENT_REL,
};

struct iqs9151_test_event {
    enum iqs9151_test_event_type type;
    const struct device *dev;
    uint16_t code;
    int32_t value;
    bool sync;
    k_timeout_t timeout;
};

typedef void (*iqs9151_test_event_hook_t)(const struct iqs9151_test_event *event,
                                          void *user_data);

#ifdef CONFIG_INPUT_IQS9151_TEST
size_t iqs9151_test_context_size(void);
void iqs9151_test_context_init(void *ctx, const struct device *dev);
void iqs9151_test_cancel_pending_work(void *ctx);
void iqs9151_test_process_frame(void *ctx,
                                const struct iqs9151_test_frame *frame,
                                int64_t now_ms);
void iqs9151_test_set_event_hook(iqs9151_test_event_hook_t hook, void *user_data);
void iqs9151_test_set_summary_hook(iqs9151_summary_cb_t hook, void *user_data);

uint16_t iqs9151_test_hold_button(const void *ctx);
void iqs9151_test_force_hold_button(void *ctx, uint16_t button);
uint8_t iqs9151_test_prev_finger_count(const void *ctx);
bool iqs9151_test_cursor_inertia_active(const void *ctx);
bool iqs9151_test_scroll_inertia_active(const void *ctx);
void iqs9151_test_force_pinch_session(void *ctx, bool active);
struct iqs9151_params *iqs9151_test_params(void *ctx);
void iqs9151_test_sync_params(void *ctx);
uint32_t iqs9151_test_ic_dirty(const void *ctx);
bool iqs9151_test_reati_pending(const void *ctx);
uint16_t iqs9151_test_scroll_inertia_decay(const void *ctx);
const struct device *iqs9151_test_fake_dev(void *ctx);

#ifdef CONFIG_INPUT_IQS9151_SETTINGS
void iqs9151_settings_test_set_device(const struct device *dev);
void iqs9151_settings_test_reset_loaded(void);
#endif
#endif

#endif /* ZEPHYR_DRIVERS_INPUT_IQS9151_TEST_H_ */
