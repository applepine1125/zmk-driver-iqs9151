#ifndef ZEPHYR_DRIVERS_INPUT_IQS9151_PARAMS_H_
#define ZEPHYR_DRIVERS_INPUT_IQS9151_PARAMS_H_

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "iqs9151_regs.h"

enum iqs9151_param_kind {
    IQS9151_PARAM_IC_U8,
    IQS9151_PARAM_IC_U16,
    IQS9151_PARAM_DRIVER,
    IQS9151_PARAM_DRIVER_BOOL,
};

/*
 * X(field, name, default, min, max, kind, reg)
 * IC 系 17 個を先頭に置く。index を IC 書き込みの保留ビットに使う。
 */
#define IQS9151_PARAM_LIST(X)                                                                   \
    X(touch_set_threshold, "touch_set_threshold", CONFIG_INPUT_IQS9151_TOUCH_SET_THRESHOLD,     \
      0, 255, IQS9151_PARAM_IC_U8, IQS9151_ADDR_TOUCH_SET_THRESHOLD)                            \
    X(touch_clear_threshold, "touch_clear_threshold",                                           \
      CONFIG_INPUT_IQS9151_TOUCH_CLEAR_THRESHOLD, 0, 255, IQS9151_PARAM_IC_U8,                  \
      IQS9151_ADDR_TOUCH_CLEAR_THRESHOLD)                                                       \
    X(alp_set_debounce, "alp_set_debounce", CONFIG_INPUT_IQS9151_ALP_SET_DEBOUNCE, 0, 255,      \
      IQS9151_PARAM_IC_U8, IQS9151_ADDR_ALP_SET_DEBOUNCE)                                       \
    X(alp_clear_debounce, "alp_clear_debounce", CONFIG_INPUT_IQS9151_ALP_CLEAR_DEBOUNCE, 0,     \
      255, IQS9151_PARAM_IC_U8, IQS9151_ADDR_ALP_CLEAR_DEBOUNCE)                                \
    X(stationary_touch_mov_threshold, "stationary_touch_mov_threshold",                         \
      CONFIG_INPUT_IQS9151_STATIONARY_TOUCH_MOV_THRESHOLD, 0, 255, IQS9151_PARAM_IC_U8,         \
      IQS9151_ADDR_STATIONARY_TOUCH_MOV_THRESHOLD)                                              \
    X(jitter_filter_delta, "jitter_filter_delta", CONFIG_INPUT_IQS9151_JITTER_FILTER_DELTA, 0,  \
      255, IQS9151_PARAM_IC_U8, IQS9151_ADDR_JITTER_FILTER_DELTA)                               \
    X(finger_confidence_threshold, "finger_confidence_threshold",                               \
      CONFIG_INPUT_IQS9151_FINGER_CONFIDENCE_THRESHOLD, 0, 255, IQS9151_PARAM_IC_U8,            \
      IQS9151_ADDR_FINGER_CONFIDENCE_THRESHOLD)                                                 \
    X(dynamic_filter_bottom_beta, "dynamic_filter_bottom_beta",                                 \
      CONFIG_INPUT_IQS9151_DYNAMIC_FILTER_BOTTOM_BETA, 0, 255, IQS9151_PARAM_IC_U8,             \
      IQS9151_ADDR_XY_DYNAMIC_FILTER_BOTTOM_BETA)                                               \
    X(active_mode_sampling_period_ms, "active_mode_sampling_period_ms",                         \
      CONFIG_INPUT_IQS9151_ACTIVE_MODE_SAMPLING_PERIOD_MS, 1, 65535, IQS9151_PARAM_IC_U16,      \
      IQS9151_ADDR_ACTIVE_MODE_SAMPLING_PERIOD)                                                 \
    X(idle_touch_mode_sampling_period_ms, "idle_touch_mode_sampling_period_ms",                 \
      CONFIG_INPUT_IQS9151_IDLE_TOUCH_MODE_SAMPLING_PERIOD_MS, 1, 65535, IQS9151_PARAM_IC_U16,  \
      IQS9151_ADDR_IDLE_TOUCH_MODE_SAMPLING_PERIOD)                                             \
    X(idle_mode_sampling_period_ms, "idle_mode_sampling_period_ms",                             \
      CONFIG_INPUT_IQS9151_IDLE_MODE_SAMPLING_PERIOD_MS, 1, 65535, IQS9151_PARAM_IC_U16,        \
      IQS9151_ADDR_IDLE_MODE_SAMPLING_PERIOD)                                                   \
    X(lp1_mode_sampling_period_ms, "lp1_mode_sampling_period_ms",                               \
      CONFIG_INPUT_IQS9151_LP1_MODE_SAMPLING_PERIOD_MS, 1, 65535, IQS9151_PARAM_IC_U16,         \
      IQS9151_ADDR_LP1_MODE_SAMPLING_PERIOD)                                                    \
    X(lp2_mode_sampling_period_ms, "lp2_mode_sampling_period_ms",                               \
      CONFIG_INPUT_IQS9151_LP2_MODE_SAMPLING_PERIOD_MS, 1, 65535, IQS9151_PARAM_IC_U16,         \
      IQS9151_ADDR_LP2_MODE_SAMPLING_PERIOD)                                                    \
    X(active_mode_timeout_ms, "active_mode_timeout_ms",                                         \
      CONFIG_INPUT_IQS9151_ACTIVE_MODE_TIMEOUT_MS, 0, 65535, IQS9151_PARAM_IC_U16,              \
      IQS9151_ADDR_ACTIVE_MODE_TIMEOUT)                                                         \
    X(ati_targetcount, "ati_targetcount", CONFIG_INPUT_IQS9151_ATI_TARGETCOUNT, 0, 1000,        \
      IQS9151_PARAM_IC_U16, IQS9151_ADDR_TRACKPAD_ATI_TARGET)                                   \
    X(dynamic_filter_bottom_speed, "dynamic_filter_bottom_speed",                               \
      CONFIG_INPUT_IQS9151_DYNAMIC_FILTER_BOTTOM_SPEED, 0, 2047, IQS9151_PARAM_IC_U16,          \
      IQS9151_ADDR_XY_DYNAMIC_FILTER_BOTTOM_SPEED)                                              \
    X(dynamic_filter_top_speed, "dynamic_filter_top_speed",                                     \
      CONFIG_INPUT_IQS9151_DYNAMIC_FILTER_TOP_SPEED, 0, 2047, IQS9151_PARAM_IC_U16,             \
      IQS9151_ADDR_XY_DYNAMIC_FILTER_TOP_SPEED)                                                 \
    /* ここまで IC 系 17 個 */                                                                   \
    X(f1_tap_enable, "1f_tap_enable", IS_ENABLED(CONFIG_INPUT_IQS9151_1F_TAP_ENABLE), 0, 1,     \
      IQS9151_PARAM_DRIVER_BOOL, 0)                                                             \
    X(f1_tap_max_ms, "1f_tap_max_ms", CONFIG_INPUT_IQS9151_1F_TAP_MAX_MS, 1, 1000,              \
      IQS9151_PARAM_DRIVER, 0)                                                                  \
    X(f1_tap_move, "1f_tap_move", CONFIG_INPUT_IQS9151_1F_TAP_MOVE, 1, 1000,                    \
      IQS9151_PARAM_DRIVER, 0)                                                                  \
    X(f1_presshold_enable, "1f_presshold_enable",                                               \
      IS_ENABLED(CONFIG_INPUT_IQS9151_1F_PRESSHOLD_ENABLE), 0, 1, IQS9151_PARAM_DRIVER_BOOL, 0) \
    X(f1_tapdrag_gap_max_ms, "1f_tapdrag_gap_max_ms",                                           \
      CONFIG_INPUT_IQS9151_1F_TAPDRAG_GAP_MAX_MS, 1, 1000, IQS9151_PARAM_DRIVER, 0)             \
    X(f2_tap_enable, "2f_tap_enable", IS_ENABLED(CONFIG_INPUT_IQS9151_2F_TAP_ENABLE), 0, 1,     \
      IQS9151_PARAM_DRIVER_BOOL, 0)                                                             \
    X(f2_tap_max_ms, "2f_tap_max_ms", CONFIG_INPUT_IQS9151_2F_TAP_MAX_MS, 1, 1000,              \
      IQS9151_PARAM_DRIVER, 0)                                                                  \
    X(f2_tap_move, "2f_tap_move", CONFIG_INPUT_IQS9151_2F_TAP_MOVE, 1, 1000,                    \
      IQS9151_PARAM_DRIVER, 0)                                                                  \
    X(f2_presshold_enable, "2f_presshold_enable",                                               \
      IS_ENABLED(CONFIG_INPUT_IQS9151_2F_PRESSHOLD_ENABLE), 0, 1, IQS9151_PARAM_DRIVER_BOOL, 0) \
    X(f2_tapdrag_gap_max_ms, "2f_tapdrag_gap_max_ms",                                           \
      CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS, 1, 1000, IQS9151_PARAM_DRIVER, 0)             \
    X(scroll_x_enable, "scroll_x_enable", IS_ENABLED(CONFIG_INPUT_IQS9151_SCROLL_X_ENABLE), 0,  \
      1, IQS9151_PARAM_DRIVER_BOOL, 0)                                                          \
    X(scroll_y_enable, "scroll_y_enable", IS_ENABLED(CONFIG_INPUT_IQS9151_SCROLL_Y_ENABLE), 0,  \
      1, IQS9151_PARAM_DRIVER_BOOL, 0)                                                          \
    X(f2_scroll_start_move, "2f_scroll_start_move",                                             \
      CONFIG_INPUT_IQS9151_2F_SCROLL_START_MOVE, 1, 2000, IQS9151_PARAM_DRIVER, 0)              \
    X(f2_pinch_enable, "2f_pinch_enable", IS_ENABLED(CONFIG_INPUT_IQS9151_2F_PINCH_ENABLE), 0,  \
      1, IQS9151_PARAM_DRIVER_BOOL, 0)                                                          \
    X(f2_pinch_start_distance, "2f_pinch_start_distance",                                       \
      CONFIG_INPUT_IQS9151_2F_PINCH_START_DISTANCE, 1, 2000, IQS9151_PARAM_DRIVER, 0)           \
    X(f2_pinch_wheel_gain_x10, "2f_pinch_wheel_gain_x10",                                       \
      CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10, 1, 100, IQS9151_PARAM_DRIVER, 0)            \
    X(f3_tap_enable, "3f_tap_enable", IS_ENABLED(CONFIG_INPUT_IQS9151_3F_TAP_ENABLE), 0, 1,     \
      IQS9151_PARAM_DRIVER_BOOL, 0)                                                             \
    X(f3_tap_max_ms, "3f_tap_max_ms", CONFIG_INPUT_IQS9151_3F_TAP_MAX_MS, 1, 1000,              \
      IQS9151_PARAM_DRIVER, 0)                                                                  \
    X(f3_tap_move, "3f_tap_move", CONFIG_INPUT_IQS9151_3F_TAP_MOVE, 1, 1000,                    \
      IQS9151_PARAM_DRIVER, 0)                                                                  \
    X(f3_presshold_enable, "3f_presshold_enable",                                               \
      IS_ENABLED(CONFIG_INPUT_IQS9151_3F_PRESSHOLD_ENABLE), 0, 1, IQS9151_PARAM_DRIVER_BOOL, 0) \
    X(f3_tapdrag_gap_max_ms, "3f_tapdrag_gap_max_ms",                                           \
      CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS, 1, 1000, IQS9151_PARAM_DRIVER, 0)             \
    X(f3_swipe_threshold, "3f_swipe_threshold", CONFIG_INPUT_IQS9151_3F_SWIPE_THRESHOLD, 0,     \
      1000, IQS9151_PARAM_DRIVER, 0)                                                            \
    X(cursor_inertia_enable, "cursor_inertia_enable",                                           \
      IS_ENABLED(CONFIG_INPUT_IQS9151_CURSOR_INERTIA_ENABLE), 0, 1, IQS9151_PARAM_DRIVER_BOOL,  \
      0)                                                                                        \
    X(cursor_inertia_decay, "cursor_inertia_decay", CONFIG_INPUT_IQS9151_CURSOR_INERTIA_DECAY,  \
      0, 1000, IQS9151_PARAM_DRIVER, 0)                                                         \
    X(cursor_inertia_recent_window_ms, "cursor_inertia_recent_window_ms",                       \
      CONFIG_INPUT_IQS9151_CURSOR_INERTIA_RECENT_WINDOW_MS, 1, 500, IQS9151_PARAM_DRIVER, 0)    \
    X(cursor_inertia_stale_gap_ms, "cursor_inertia_stale_gap_ms",                               \
      CONFIG_INPUT_IQS9151_CURSOR_INERTIA_STALE_GAP_MS, 1, 500, IQS9151_PARAM_DRIVER, 0)        \
    X(cursor_inertia_min_samples, "cursor_inertia_min_samples",                                 \
      CONFIG_INPUT_IQS9151_CURSOR_INERTIA_MIN_SAMPLES, 1, 12, IQS9151_PARAM_DRIVER, 0)          \
    X(cursor_inertia_min_avg_speed, "cursor_inertia_min_avg_speed",                             \
      CONFIG_INPUT_IQS9151_CURSOR_INERTIA_MIN_AVG_SPEED, 1, 500, IQS9151_PARAM_DRIVER, 0)       \
    X(scroll_inertia_enable, "scroll_inertia_enable",                                           \
      IS_ENABLED(CONFIG_INPUT_IQS9151_SCROLL_INERTIA_ENABLE), 0, 1, IQS9151_PARAM_DRIVER_BOOL,  \
      0)                                                                                        \
    X(scroll_inertia_decay, "scroll_inertia_decay", CONFIG_INPUT_IQS9151_SCROLL_INERTIA_DECAY,  \
      0, 1000, IQS9151_PARAM_DRIVER, 0)                                                         \
    X(scroll_inertia_recent_window_ms, "scroll_inertia_recent_window_ms",                       \
      CONFIG_INPUT_IQS9151_SCROLL_INERTIA_RECENT_WINDOW_MS, 1, 500, IQS9151_PARAM_DRIVER, 0)    \
    X(scroll_inertia_stale_gap_ms, "scroll_inertia_stale_gap_ms",                               \
      CONFIG_INPUT_IQS9151_SCROLL_INERTIA_STALE_GAP_MS, 1, 500, IQS9151_PARAM_DRIVER, 0)        \
    X(scroll_inertia_min_samples, "scroll_inertia_min_samples",                                 \
      CONFIG_INPUT_IQS9151_SCROLL_INERTIA_MIN_SAMPLES, 1, 12, IQS9151_PARAM_DRIVER, 0)          \
    X(scroll_inertia_min_avg_speed, "scroll_inertia_min_avg_speed",                             \
      CONFIG_INPUT_IQS9151_SCROLL_INERTIA_MIN_AVG_SPEED, 1, 500, IQS9151_PARAM_DRIVER, 0)       \
    X(cursor_report_interval_ms, "cursor_report_interval_ms",                                   \
      CONFIG_INPUT_IQS9151_CURSOR_REPORT_INTERVAL_MS, 0, 100, IQS9151_PARAM_DRIVER, 0)          \
    X(scroll_report_interval_ms, "scroll_report_interval_ms",                                   \
      CONFIG_INPUT_IQS9151_SCROLL_REPORT_INTERVAL_MS, 0, 100, IQS9151_PARAM_DRIVER, 0)          \
    X(f2_pinch_ratio_x10, "2f_pinch_ratio_x10", CONFIG_INPUT_IQS9151_2F_PINCH_RATIO_X10, 5, 50, \
      IQS9151_PARAM_DRIVER, 0)

#define IQS9151_PARAM_IC_COUNT 17

struct iqs9151_params {
#define IQS9151_PARAM_FIELD(field, name, def, min, max, kind, reg) int32_t field;
    IQS9151_PARAM_LIST(IQS9151_PARAM_FIELD)
#undef IQS9151_PARAM_FIELD
};

struct iqs9151_param_def {
    const char *name;
    uint16_t offset;
    int32_t min;
    int32_t max;
    int32_t def;
    enum iqs9151_param_kind kind;
    uint16_t reg;
};

/* デバイス非依存の純粋ロジック(iqs9151_params.c) */
size_t iqs9151_param_count(void);
const struct iqs9151_param_def *iqs9151_param_def_at(size_t idx);
const struct iqs9151_param_def *iqs9151_param_find(const char *name);
bool iqs9151_param_is_ic(const struct iqs9151_param_def *def);
const char *iqs9151_param_kind_str(enum iqs9151_param_kind kind);
void iqs9151_params_init(struct iqs9151_params *p);
int iqs9151_params_set(struct iqs9151_params *p, const struct iqs9151_param_def *def,
                       int32_t value);
int32_t iqs9151_params_get(const struct iqs9151_params *p,
                           const struct iqs9151_param_def *def);

/* デバイス API(iqs9151.c)。Task 3〜5 で実装する */
int iqs9151_dev_param_set(const struct device *dev, const char *name, int32_t value);
int iqs9151_dev_param_get(const struct device *dev, const char *name, int32_t *value);
int iqs9151_dev_param_reset(const struct device *dev);
int iqs9151_dev_request_reati(const struct device *dev);
void iqs9151_dev_trace_enable(bool enable);
bool iqs9151_dev_trace_enabled(void);

#endif /* ZEPHYR_DRIVERS_INPUT_IQS9151_PARAMS_H_ */
