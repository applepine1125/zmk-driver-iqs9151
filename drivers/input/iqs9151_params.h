#ifndef ZEPHYR_DRIVERS_INPUT_IQS9151_PARAMS_H_
#define ZEPHYR_DRIVERS_INPUT_IQS9151_PARAMS_H_

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include <errno.h>
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

/*
 * 実行時計測。iqs9151_work_cb の入口/出口を計測し、reset=true の読み出しで
 * カウンタを 0 に戻す(次の読み出し区間の計測を始める)。
 */
struct iqs9151_stats {
    uint32_t frame_count;
    uint32_t frame_max_us;
    uint32_t frame_avg_us;
    uint32_t frame_gap_max_ms;
    uint32_t i2c_errors;
    uint32_t i2c_max_us;       /* フレーム読み出し(I2C)単体の最大時間 */
    uint32_t show_reset_count; /* IC の SHOW_RESET を検出して状態を初期化した回数 */
    uint32_t frame_cpu_max_us; /* フレーム処理が実際に CPU を使った最大時間(THREAD_RUNTIME_STATS 有効時) */
    uint32_t rdy_miss;         /* フレーム読み出し開始時に RDY が非アクティブだった回数(通信窓を外すとクロックストレッチで待つ) */
};

void iqs9151_dev_stats_get(struct iqs9151_stats *out, bool reset);

/*
 * フレームのライブコールバック(第 5b 段)。tp live on|off [hz] で有効化する。
 * 指本数が 0 の間は「0 に変化した瞬間」だけ呼び、指が触れるまで呼ばない。
 * 指本数が 1 以上のときは、指本数/hold/2f モードが変わったフレームは即時、
 * それ以外は 1000/hz ms 間隔で間引いて呼ぶ。
 * コールバックはドライバの work キュー上で同期的に呼ばれるため、ブロックする処理や
 * 時間のかかる処理をしないこと(軽量に保つのは呼び出し側の責任)。
 */
struct iqs9151_frame_info {
    uint32_t ms;
    uint8_t fingers;
    int16_t rel_x, rel_y;
    uint16_t f1x, f1y, f2x, f2y;
    uint16_t flags;
    uint16_t hold;
    uint8_t mode2f;
    uint8_t pending;
};

int iqs9151_frame_format(const struct iqs9151_frame_info *f, char *buf, size_t len);

typedef void (*iqs9151_frame_cb_t)(const struct iqs9151_frame_info *f, void *user_data);

void iqs9151_dev_set_frame_callback(iqs9151_frame_cb_t cb, void *user_data);
int iqs9151_dev_live_enable(bool enable, uint16_t hz);
bool iqs9151_dev_live_enabled(void);
uint16_t iqs9151_dev_live_hz(void);

#define IQS9151_LIVE_HZ_DEFAULT 60

/*
 * 試行要約: 指の接触が始まってから、離してタップドラッグの許容間隔(1f/2f/3f_tapdrag_gap_max_ms
 * の最大値)+100ms(下限 200ms・上限 1000ms)何も触れないまでを 1 試行として集計する。
 * end_ms は最後に指を離した時刻。LOG 行は "T S" に続けて下の順で 17 値を出す。
 */
struct iqs9151_attempt_summary {
    uint32_t start_ms;
    uint32_t end_ms;
    uint8_t contacts;
    uint8_t fingers_max;
    uint32_t down_ms;
    uint32_t gap_ms;
    uint32_t move_sum;
    uint32_t centroid_move;
    int32_t dist_delta;
    uint8_t mode2f;
    uint8_t btn_press_bits;
    uint8_t btn_release_bits;
    uint16_t wheel_count;
    int32_t wheel_sum;
    uint16_t rel_count;
    uint16_t drops;
    uint8_t hold;
};

typedef void (*iqs9151_summary_cb_t)(const struct iqs9151_attempt_summary *summary,
                                     void *user_data);

void iqs9151_dev_set_summary_callback(iqs9151_summary_cb_t cb, void *user_data);
void iqs9151_dev_summary_enable(bool enable);
bool iqs9151_dev_summary_enabled(void);

/*
 * 要約コーデック(iqs9151_summary_codec.c)。GATT/split 転送用に 10 語の uint32 配列へパックする。
 */
#define IQS9151_SUMMARY_WORDS 10

void iqs9151_summary_pack(const struct iqs9151_attempt_summary *s,
                          uint32_t words[IQS9151_SUMMARY_WORDS]);
void iqs9151_summary_unpack(const uint32_t words[IQS9151_SUMMARY_WORDS],
                            struct iqs9151_attempt_summary *s);
int iqs9151_summary_format(const struct iqs9151_attempt_summary *s, char *buf, size_t len);

/*
 * 永続化(iqs9151_settings.c、CONFIG_INPUT_IQS9151_SETTINGS)。
 * ブロブ: uint16 version(=1), uint16 count, int32 values[count](テーブル順、LE)。
 */
#ifdef CONFIG_INPUT_IQS9151_SETTINGS
int iqs9151_settings_encode(const struct iqs9151_params *p, uint8_t *buf, size_t len);
int iqs9151_settings_decode(const uint8_t *buf, size_t len, struct iqs9151_params *p);
int iqs9151_settings_save(const struct device *dev);
int iqs9151_settings_clear(void);
bool iqs9151_settings_loaded(void);
#else
static inline int iqs9151_settings_save(const struct device *dev) {
    ARG_UNUSED(dev);
    return -ENOTSUP;
}

static inline int iqs9151_settings_clear(void) {
    return -ENOTSUP;
}

static inline bool iqs9151_settings_loaded(void) {
    return false;
}
#endif

#endif /* ZEPHYR_DRIVERS_INPUT_IQS9151_PARAMS_H_ */
