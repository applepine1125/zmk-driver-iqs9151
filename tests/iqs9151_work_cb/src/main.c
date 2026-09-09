#include <zephyr/input/input.h>
#include <zephyr/ztest.h>

#include "iqs9151_params.h"
#include "iqs9151_regs.h"
#include "iqs9151_test.h"

#include <errno.h>
#include <string.h>

#define IQS9151_TEST_CTX_BUF_SIZE 2048
#define IQS9151_TEST_MAX_EVENTS 32
#define IQS9151_TEST_MAX_SUMMARIES 4
#define IQS9151_TEST_MAX_FRAMES 16

/* 試行要約のアイドル待ち時間。ドライバ側の計算(tapdrag_gap_max_ms の最大値+100ms)と
 * この Kconfig 値(1F 230 / 2F 200 / 3F 230)から導かれる値に追随させる。
 */
#define TEST_SUMMARY_TAPDRAG_GAP_MAX_MS                                                        \
    MAX(CONFIG_INPUT_IQS9151_1F_TAPDRAG_GAP_MAX_MS,                                            \
       MAX(CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS,                                         \
          CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS))
#define TEST_SUMMARY_IDLE_MS (TEST_SUMMARY_TAPDRAG_GAP_MAX_MS + 100)

struct event_log {
    struct iqs9151_test_event events[IQS9151_TEST_MAX_EVENTS];
    size_t count;
};

struct summary_log {
    struct iqs9151_attempt_summary items[IQS9151_TEST_MAX_SUMMARIES];
    size_t count;
};

struct frame_log {
    struct iqs9151_frame_info items[IQS9151_TEST_MAX_FRAMES];
    size_t count;
};

struct iqs9151_work_cb_fixture {
    uint8_t ctx[IQS9151_TEST_CTX_BUF_SIZE] __aligned(8);
    struct event_log log;
    struct summary_log summaries;
    struct frame_log frames;
};

int input_report(const struct device *dev,
                 uint8_t type, uint16_t code, int32_t value, bool sync,
                 k_timeout_t timeout) {
    ARG_UNUSED(dev);
    ARG_UNUSED(type);
    ARG_UNUSED(code);
    ARG_UNUSED(value);
    ARG_UNUSED(sync);
    ARG_UNUSED(timeout);
    return 0;
}

static void record_event(const struct iqs9151_test_event *event, void *user_data) {
    struct event_log *log = (struct event_log *)user_data;

    if (log->count >= IQS9151_TEST_MAX_EVENTS) {
        return;
    }

    log->events[log->count++] = *event;
}

static void record_summary(const struct iqs9151_attempt_summary *summary, void *user_data) {
    struct summary_log *log = (struct summary_log *)user_data;

    if (log->count >= IQS9151_TEST_MAX_SUMMARIES) {
        return;
    }

    log->items[log->count++] = *summary;
}

static void record_frame(const struct iqs9151_frame_info *info, void *user_data) {
    struct frame_log *log = (struct frame_log *)user_data;

    if (log->count >= IQS9151_TEST_MAX_FRAMES) {
        return;
    }

    log->items[log->count++] = *info;
}

static struct iqs9151_test_frame make_frame(uint8_t finger_count,
                                            uint16_t trackpad_flags,
                                            int16_t rel_x, int16_t rel_y,
                                            uint16_t info_flags,
                                            uint16_t finger1_x, uint16_t finger1_y,
                                            uint16_t finger2_x, uint16_t finger2_y) {
    return (struct iqs9151_test_frame){
        .rel_x = rel_x,
        .rel_y = rel_y,
        .info_flags = info_flags,
        .trackpad_flags = trackpad_flags,
        .finger_count = finger_count,
        .finger1_x = finger1_x,
        .finger1_y = finger1_y,
        .finger2_x = finger2_x,
        .finger2_y = finger2_y,
    };
}

static void *iqs9151_work_cb_setup(void) {
    static struct iqs9151_work_cb_fixture fixture;
    const size_t ctx_size = iqs9151_test_context_size();

    zassert_true(ctx_size <= sizeof(fixture.ctx),
                 "Context buffer too small: %u > %u",
                 (unsigned int)ctx_size,
                 (unsigned int)sizeof(fixture.ctx));
    memset(&fixture, 0, sizeof(fixture));
    iqs9151_test_context_init(fixture.ctx, NULL);
    iqs9151_test_set_event_hook(record_event, &fixture.log);
    iqs9151_test_set_summary_hook(record_summary, &fixture.summaries);
    iqs9151_test_set_frame_hook(record_frame, &fixture.frames);
    return &fixture;
}

static void iqs9151_work_cb_before(void *fixture_ptr) {
    struct iqs9151_work_cb_fixture *fixture =
        (struct iqs9151_work_cb_fixture *)fixture_ptr;

    memset(&fixture->log, 0, sizeof(fixture->log));
    memset(&fixture->summaries, 0, sizeof(fixture->summaries));
    memset(&fixture->frames, 0, sizeof(fixture->frames));
    iqs9151_test_cancel_pending_work(fixture->ctx);
    iqs9151_test_context_init(fixture->ctx, NULL);
    iqs9151_test_set_event_hook(record_event, &fixture->log);
    iqs9151_test_set_summary_hook(record_summary, &fixture->summaries);
    iqs9151_test_set_frame_hook(record_frame, &fixture->frames);
    (void)iqs9151_dev_live_enable(false, IQS9151_LIVE_HZ_DEFAULT);
}

ZTEST_F(iqs9151_work_cb, test_show_reset_releases_pinch_and_clears_state) {
    const struct iqs9151_test_frame show_reset_frame =
        make_frame(2U, 2U, 0, 0, IQS9151_INFO_SHOW_RESET, 0, 0, 0, 0);

    iqs9151_test_force_pinch_session(fixture->ctx, true);
    iqs9151_test_process_frame(fixture->ctx, &show_reset_frame, 10);

    zassert_equal(fixture->log.count, 1U, "Unexpected event count");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Not a key event");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_7, "Unexpected key code");
    zassert_equal(fixture->log.events[0].value, 0, "Expected pinch release");
    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx),
                  "Scroll inertia should be inactive");
    zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx),
                  "Cursor inertia should be inactive");
    zassert_equal(iqs9151_test_prev_finger_count(fixture->ctx), 0U,
                  "Previous frame should be reset");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared");
}

ZTEST_F(iqs9151_work_cb, test_one_finger_release_starts_cursor_inertia) {
    const struct iqs9151_test_frame one_start =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   24, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame one_move =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   20, 0, 0, 140, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &one_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &one_move, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 20);

    zassert_true(iqs9151_test_cursor_inertia_active(fixture->ctx),
                 "Cursor inertia should start on release");
    zassert_equal(iqs9151_test_prev_finger_count(fixture->ctx), 0U,
                  "Previous frame should track release");
    zassert_equal(fixture->log.count, 4U,
                  "Only REL events from movement frames are expected");
    for (size_t i = 0; i < fixture->log.count; i++) {
        zassert_equal(fixture->log.events[i].type, IQS9151_TEST_EVENT_REL,
                      "Unexpected non-REL event at idx %u", (unsigned int)i);
    }
}

ZTEST_F(iqs9151_work_cb, test_one_finger_release_after_stale_gap_does_not_start_cursor_inertia) {
    const struct iqs9151_test_frame one_start =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   24, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame one_move =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   20, 0, 0, 140, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &one_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &one_move, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 80);

    zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx),
                  "Cursor inertia should stay off after a stale release");
    zassert_equal(fixture->log.count, 4U,
                  "Only REL events from active movement frames are expected");
}

ZTEST_F(iqs9151_work_cb, test_one_finger_release_after_slowdown_does_not_start_cursor_inertia) {
    const struct iqs9151_test_frame fast_start =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   14, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame fast_move =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   12, 0, 0, 112, 100, 0, 0);
    const struct iqs9151_test_frame slow_1 =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   1, 0, 0, 113, 100, 0, 0);
    const struct iqs9151_test_frame slow_2 =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   1, 0, 0, 114, 100, 0, 0);
    const struct iqs9151_test_frame slow_3 =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   1, 0, 0, 115, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &fast_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &fast_move, 10);
    iqs9151_test_process_frame(fixture->ctx, &slow_1, 20);
    iqs9151_test_process_frame(fixture->ctx, &slow_2, 30);
    iqs9151_test_process_frame(fixture->ctx, &slow_3, 40);
    iqs9151_test_process_frame(fixture->ctx, &release, 50);

    zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx),
                  "Cursor inertia should stay off after slowing down before release");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_scroll_reports_and_starts_scroll_inertia) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame two_scroll =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 160, 100, 260, 100);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &two_scroll, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 20);

    zassert_true(iqs9151_test_scroll_inertia_active(fixture->ctx),
                 "Scroll inertia should start on 2F scroll release");
    zassert_equal(iqs9151_test_prev_finger_count(fixture->ctx), 0U,
                  "Previous frame should track release");
    zassert_equal(fixture->log.count, 1U,
                  "Expected a single horizontal wheel event during scroll");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_REL, "Not a REL event");
    zassert_equal(fixture->log.events[0].code, INPUT_REL_HWHEEL, "Unexpected REL code");
    zassert_equal(fixture->log.events[0].value, -60, "Unexpected HWHEEL delta");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_scroll_release_after_stale_gap_does_not_start_inertia) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame two_scroll =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 160, 100, 260, 100);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &two_scroll, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 80);

    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx),
                  "Scroll inertia should stay off after a stale release");
}

ZTEST_F(iqs9151_work_cb, test_new_one_finger_touch_cancels_scroll_inertia) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame two_scroll =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 160, 100, 260, 100);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame one_start =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   18, 0, 0, 120, 100, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &two_scroll, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 20);

    zassert_true(iqs9151_test_scroll_inertia_active(fixture->ctx),
                 "Scroll inertia should start on 2F scroll release");

    iqs9151_test_process_frame(fixture->ctx, &one_start, 30);

    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx),
                  "A new 1F touch should cancel scroll inertia");
    zassert_equal(fixture->log.count, 3U,
                  "Expected one scroll REL event plus one 1F cursor frame");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_REL, "Event[1] not REL");
    zassert_equal(fixture->log.events[1].code, INPUT_REL_X, "Event[1] should be REL_X");
    zassert_equal(fixture->log.events[1].value, 18, "Event[1] unexpected REL_X value");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_REL, "Event[2] not REL");
    zassert_equal(fixture->log.events[2].code, INPUT_REL_Y, "Event[2] should be REL_Y");
    zassert_equal(fixture->log.events[2].value, 0, "Event[2] unexpected REL_Y value");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_scroll_tail_two_to_one_to_zero_suppresses_cursor_path) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame two_scroll =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 160, 100, 260, 100);
    const struct iqs9151_test_frame one_tail_move =
        make_frame(1U,
                   IQS9151_TP_FINGER2_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   11, -9, 0, UINT16_MAX, UINT16_MAX, 260, 140);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &two_scroll, 10);
    iqs9151_test_process_frame(fixture->ctx, &one_tail_move, 20);
    iqs9151_test_process_frame(fixture->ctx, &release, 30);

    zassert_true(iqs9151_test_scroll_inertia_active(fixture->ctx),
                 "Scroll inertia should start on 2F scroll release");
    zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx),
                  "Cursor inertia must stay off for a 2F scroll tail");
    zassert_equal(fixture->log.count, 1U,
                  "Only the 2F scroll REL event should be reported");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_REL, "Not a REL event");
    zassert_equal(fixture->log.events[0].code, INPUT_REL_HWHEEL, "Unexpected REL code");
    zassert_equal(fixture->log.events[0].value, -60, "Unexpected HWHEEL delta");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_pinch_reports_btn7_and_wheel) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame two_pinch =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 60, 100, 240, 100);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &two_pinch, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 20);

    zassert_equal(fixture->log.count, 3U, "Expected BTN7 press, wheel, BTN7 release");

    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_7, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN7 press");

    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_REL, "Event[1] not rel");
    zassert_equal(fixture->log.events[1].code, INPUT_REL_WHEEL, "Event[1] unexpected code");
    const int32_t expected_wheel =
        (80 * CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10) / (12 * 10);
    zassert_equal(fixture->log.events[1].value, expected_wheel, "Event[1] unexpected wheel value");

    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_KEY, "Event[2] not key");
    zassert_equal(fixture->log.events[2].code, INPUT_BTN_7, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 0, "Event[2] should be BTN7 release");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_pinch_tail_two_to_one_to_zero_suppresses_cursor_path) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame two_pinch =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 60, 100, 240, 100);
    const struct iqs9151_test_frame one_tail_move =
        make_frame(1U,
                   IQS9151_TP_FINGER2_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   9, 7, 0, UINT16_MAX, UINT16_MAX, 245, 108);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &two_pinch, 10);
    iqs9151_test_process_frame(fixture->ctx, &one_tail_move, 20);
    iqs9151_test_process_frame(fixture->ctx, &release, 30);

    zassert_false(iqs9151_test_scroll_inertia_active(fixture->ctx),
                  "Scroll inertia should remain off for a pinch tail");
    zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx),
                  "Cursor inertia must stay off for a 2F pinch tail");
    zassert_equal(fixture->log.count, 3U,
                  "Expected pinch press/wheel/release only");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_7, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN7 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_REL, "Event[1] not rel");
    zassert_equal(fixture->log.events[1].code, INPUT_REL_WHEEL, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value,
                  (80 * CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10) / (12 * 10),
                  "Event[1] unexpected wheel value");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_KEY, "Event[2] not key");
    zassert_equal(fixture->log.events[2].code, INPUT_BTN_7, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 0, "Event[2] should be BTN7 release");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_pinch_to_three_finger_releases_btn7) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame two_pinch =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 60, 100, 240, 100);
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 100, 100, 200, 100);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &two_pinch, 10);
    iqs9151_test_process_frame(fixture->ctx, &three_start, 20);

    zassert_equal(fixture->log.count, 3U,
                  "Expected BTN7 press, wheel, BTN7 release on 2F->3F transition");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_KEY, "Event[2] not key");
    zassert_equal(fixture->log.events[2].code, INPUT_BTN_7, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 0, "Event[2] should be BTN7 release");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_tap_click_emits_btn1) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN1 press on first 2F tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should remain pressed while waiting second 2F touch");

    k_msleep(CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN1 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN1 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_tap_staggered_release_emits_btn1) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN1 press on first 2F tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should remain pressed while waiting second 2F touch");

    k_msleep(CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN1 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN1 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_tap_one_lead_finger_emits_btn1) {
    const struct iqs9151_test_frame one_lead =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &one_lead, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN1 press on one-lead 2F tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should remain pressed while waiting second 2F touch");

    k_msleep(CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN1 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN1 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_tap_moved_one_lead_does_not_click) {
    const struct iqs9151_test_frame one_lead =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame one_lead_move =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 140, 100, 0, 0);
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 140, 100, 240, 100);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 140, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &one_lead, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_lead_move, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 0U, "Moved one-finger lead should not become 2F tap");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_tap_jitter_no_pinch_emits_btn1) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 2230, 1577, 622, 2198);
    const struct iqs9151_test_frame two_jitter =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 2217, 1580, 630, 2198);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 2217, 1580, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_jitter, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U,
                  "Expected deferred BTN1 press for jittered 2F tap without pinch");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should remain pressed while waiting second 2F touch");

    k_msleep(CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN1 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN1 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_tap_releases_latched_hold) {
    const struct iqs9151_test_frame two_start =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_force_hold_button(fixture->ctx, INPUT_BTN_1);

    iqs9151_test_process_frame(fixture->ctx, &two_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U,
                  "Expected BTN1 release by 2F tap when hold is latched");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 0, "Event[0] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared by 2F tap");
}

ZTEST_F(iqs9151_work_cb, test_one_finger_tap_defers_release_until_timeout) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    zassert_equal(fixture->log.count, 1U, "Expected deferred press on first tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_0, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN0 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_0,
                  "BTN0 should remain pressed while waiting second touch");

    k_msleep(CONFIG_INPUT_IQS9151_1F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_0, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN0 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_one_finger_double_tap_emits_release_then_click) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_touch_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 101, 100, 0, 0);
    const struct iqs9151_test_frame second_touch_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_down, k_uptime_get());
    k_msleep(70);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_up, k_uptime_get());

    zassert_equal(fixture->log.count, 4U,
                  "Expected deferred press release + second click on double-tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_0, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN0 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_0, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should release first click");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_KEY, "Event[2] not key");
    zassert_equal(fixture->log.events[2].code, INPUT_BTN_0, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 1, "Event[2] should be second click press");
    zassert_equal(fixture->log.events[3].type, IQS9151_TEST_EVENT_KEY, "Event[3] not key");
    zassert_equal(fixture->log.events[3].code, INPUT_BTN_0, "Event[3] unexpected code");
    zassert_equal(fixture->log.events[3].value, 0, "Event[3] should be second click release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after double-tap");
}

ZTEST_F(iqs9151_work_cb, test_one_finger_second_touch_drag_releases_on_finger_up) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_touch_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame second_touch_move_far =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 140, 100, 0, 0);
    const struct iqs9151_test_frame second_touch_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_touch_move_far, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_touch_up, k_uptime_get());

    zassert_equal(fixture->log.count, 2U,
                  "Expected press on first tap and release on second-touch finger-up");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_0, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN0 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_0, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN0 release");
    zassert_false(iqs9151_test_cursor_inertia_active(fixture->ctx),
                  "Cursor inertia must not start after drag release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after drag release");
}

ZTEST_F(iqs9151_work_cb, test_one_finger_long_press_without_tapdrag_arm_does_not_emit_hold) {
    const struct iqs9151_test_frame one_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame one_hold_check =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &one_down, k_uptime_get());
    k_msleep(180);
    iqs9151_test_process_frame(fixture->ctx, &one_hold_check, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 0U,
                  "Long press without TapDrag arm must not emit BTN0 hold");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should stay clear without TapDrag arm");
}

ZTEST_F(iqs9151_work_cb, test_one_finger_tap_releases_latched_hold) {
    const struct iqs9151_test_frame tap_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 100, 100, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_force_hold_button(fixture->ctx, INPUT_BTN_0);

    iqs9151_test_process_frame(fixture->ctx, &tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U,
                  "Expected BTN0 release by tap when hold is latched");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_0, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 0, "Event[0] should be BTN0 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared by tap");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_double_tap_emits_release_then_click) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_tap_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 102, 100, 202, 100);
    const struct iqs9151_test_frame second_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_tap_up, k_uptime_get());

    zassert_equal(fixture->log.count, 4U,
                  "Expected deferred press release + second click on 2F double-tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should release first click");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_KEY, "Event[2] not key");
    zassert_equal(fixture->log.events[2].code, INPUT_BTN_1, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 1, "Event[2] should be second click press");
    zassert_equal(fixture->log.events[3].type, IQS9151_TEST_EVENT_KEY, "Event[3] not key");
    zassert_equal(fixture->log.events[3].code, INPUT_BTN_1, "Event[3] unexpected code");
    zassert_equal(fixture->log.events[3].value, 0, "Event[3] should be second click release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after 2F double-tap");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_double_tap_one_lead_second_touch_emits_release_then_click) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_one_lead_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 102, 100, 0, 0);
    const struct iqs9151_test_frame second_tap_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 102, 100, 202, 100);
    const struct iqs9151_test_frame second_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN1 press after first 2F tap");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should remain pressed while waiting for second 2F touch");

    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_one_lead_down, k_uptime_get());
    zassert_equal(fixture->log.count, 1U,
                  "Second-touch one-lead should not cancel pending 2F deferred click");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should stay pressed during second-touch one-lead");

    k_msleep(12);
    iqs9151_test_process_frame(fixture->ctx, &second_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_tap_up, k_uptime_get());

    zassert_equal(fixture->log.count, 4U,
                  "Expected deferred press release + second click on one-lead 2F double-tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should release first click");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_KEY, "Event[2] not key");
    zassert_equal(fixture->log.events[2].code, INPUT_BTN_1, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 1, "Event[2] should be second click press");
    zassert_equal(fixture->log.events[3].type, IQS9151_TEST_EVENT_KEY, "Event[3] not key");
    zassert_equal(fixture->log.events[3].code, INPUT_BTN_1, "Event[3] unexpected code");
    zassert_equal(fixture->log.events[3].value, 0, "Event[3] should be second click release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after 2F double-tap");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_second_touch_drag_releases_on_finger_up) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_touch_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame second_touch_move_far =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 150, 100, 250, 100);
    const struct iqs9151_test_frame one_drag_continue =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 150, 100, 0, 0);
    const struct iqs9151_test_frame second_touch_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_touch_move_far, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_drag_continue, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should stay pressed while second-touch drag continues");
    iqs9151_test_process_frame(fixture->ctx, &second_touch_up, k_uptime_get());

    zassert_equal(fixture->log.count, 2U,
                  "Expected press on first 2F tap and release on second-touch finger-up");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after second-touch drag release");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_second_touch_drag_one_lead_keeps_hold_until_finger_up) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_one_lead_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 101, 100, 0, 0);
    const struct iqs9151_test_frame second_touch_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 101, 100, 201, 100);
    const struct iqs9151_test_frame second_touch_move_far =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 150, 100, 250, 100);
    const struct iqs9151_test_frame one_drag_continue =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 150, 100, 0, 0);
    const struct iqs9151_test_frame second_touch_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN1 press on first 2F tap");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should remain pressed while waiting second 2F touch");

    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_one_lead_down, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should stay pressed during second-touch one-lead");

    k_msleep(12);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_touch_move_far, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_drag_continue, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should stay pressed while one-lead second-touch drag continues");
    iqs9151_test_process_frame(fixture->ctx, &second_touch_up, k_uptime_get());

    zassert_equal(fixture->log.count, 2U,
                  "Expected press on first 2F tap and release on one-lead second-touch drag end");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_1, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after second-touch drag release");
}

ZTEST_F(iqs9151_work_cb, test_two_finger_second_touch_drag_two_to_one_reports_cursor_motion) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_touch_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 100, 100, 200, 100);
    const struct iqs9151_test_frame second_touch_move_far =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 150, 100, 250, 100);
    const struct iqs9151_test_frame one_drag_move =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   13, -8, 0, 190, 120, 0, 0);
    const struct iqs9151_test_frame second_touch_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_touch_move_far, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_drag_move, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_1,
                  "BTN1 should stay pressed after 2F->1F transition during TapDrag");
    iqs9151_test_process_frame(fixture->ctx, &second_touch_up, k_uptime_get());

    zassert_equal(fixture->log.count, 4U,
                  "Expected BTN1 press, cursor REL_X/Y on 2->1, and BTN1 release");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_1, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN1 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_REL, "Event[1] not rel");
    zassert_equal(fixture->log.events[1].code, INPUT_REL_X, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 13, "Event[1] should report cursor delta X");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_REL, "Event[2] not rel");
    zassert_equal(fixture->log.events[2].code, INPUT_REL_Y, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, -8, "Event[2] should report cursor delta Y");
    zassert_equal(fixture->log.events[3].type, IQS9151_TEST_EVENT_KEY, "Event[3] not key");
    zassert_equal(fixture->log.events[3].code, INPUT_BTN_1, "Event[3] unexpected code");
    zassert_equal(fixture->log.events[3].value, 0, "Event[3] should be BTN1 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after second-touch drag release");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_releases_latched_hold) {
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_force_hold_button(fixture->ctx, INPUT_BTN_2);

    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U,
                  "Expected BTN2 release by 3F tap when hold is latched");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 0, "Event[0] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared by 3F tap");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_double_tap_emits_release_then_click) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_tap_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 502, 500, 0, 0);
    const struct iqs9151_test_frame second_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_tap_up, k_uptime_get());

    zassert_equal(fixture->log.count, 4U,
                  "Expected deferred press release + second click on 3F double-tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should release first click");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_KEY, "Event[2] not key");
    zassert_equal(fixture->log.events[2].code, INPUT_BTN_2, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 1, "Event[2] should be second click press");
    zassert_equal(fixture->log.events[3].type, IQS9151_TEST_EVENT_KEY, "Event[3] not key");
    zassert_equal(fixture->log.events[3].code, INPUT_BTN_2, "Event[3] unexpected code");
    zassert_equal(fixture->log.events[3].value, 0, "Event[3] should be second click release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after 3F double-tap");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_second_touch_drag_releases_on_finger_up) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_touch_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame second_touch_move_far =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 560, 500, 0, 0);
    const struct iqs9151_test_frame second_touch_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_touch_move_far, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should stay pressed while second-touch drag continues");
    iqs9151_test_process_frame(fixture->ctx, &second_touch_up, k_uptime_get());

    zassert_equal(fixture->log.count, 2U,
                  "Expected press on first 3F tap and release on second-touch finger-up");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after second-touch drag release");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_second_touch_drag_staged_entry_keeps_hold_until_finger_up) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_one_down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame second_two_down =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 500, 500, 620, 500);
    const struct iqs9151_test_frame second_three_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 620, 500);
    const struct iqs9151_test_frame second_three_move_far =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 560, 500, 620, 500);
    const struct iqs9151_test_frame second_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN2 press on first 3F tap");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should remain pressed while waiting second 3F touch");

    k_msleep(60);

    iqs9151_test_process_frame(fixture->ctx, &second_one_down, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should stay pressed during staged second-touch 1F entry");
    zassert_equal(fixture->log.count, 1U,
                  "No extra events should be emitted during staged second-touch 1F entry");

    iqs9151_test_process_frame(fixture->ctx, &second_two_down, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should stay pressed during staged second-touch 2F entry");
    zassert_equal(fixture->log.count, 1U,
                  "No extra events should be emitted during staged second-touch 2F entry");

    iqs9151_test_process_frame(fixture->ctx, &second_three_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_three_move_far, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should stay pressed while staged second-touch drag continues");

    iqs9151_test_process_frame(fixture->ctx, &second_up, k_uptime_get());

    zassert_equal(fixture->log.count, 2U,
                  "Expected press on first 3F tap and release on staged second-touch finger-up");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after staged second-touch drag release");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_second_touch_drag_three_to_one_reports_cursor_motion) {
    const struct iqs9151_test_frame first_tap_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame first_tap_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame second_touch_down =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame second_touch_move_far =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 560, 500, 0, 0);
    const struct iqs9151_test_frame one_drag_move =
        make_frame(1U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                   -9, 15, 0, 610, 520, 0, 0);
    const struct iqs9151_test_frame second_touch_up =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &first_tap_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &first_tap_up, k_uptime_get());
    k_msleep(60);
    iqs9151_test_process_frame(fixture->ctx, &second_touch_down, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &second_touch_move_far, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_drag_move, k_uptime_get());
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should stay pressed after 3F->1F transition during TapDrag");
    iqs9151_test_process_frame(fixture->ctx, &second_touch_up, k_uptime_get());

    zassert_equal(fixture->log.count, 4U,
                  "Expected BTN2 press, cursor REL_X/Y on 3->1, and BTN2 release");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 deferred press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_REL, "Event[1] not rel");
    zassert_equal(fixture->log.events[1].code, INPUT_REL_X, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, -9, "Event[1] should report cursor delta X");
    zassert_equal(fixture->log.events[2].type, IQS9151_TEST_EVENT_REL, "Event[2] not rel");
    zassert_equal(fixture->log.events[2].code, INPUT_REL_Y, "Event[2] unexpected code");
    zassert_equal(fixture->log.events[2].value, 15, "Event[2] should report cursor delta Y");
    zassert_equal(fixture->log.events[3].type, IQS9151_TEST_EVENT_KEY, "Event[3] not key");
    zassert_equal(fixture->log.events[3].code, INPUT_BTN_2, "Event[3] unexpected code");
    zassert_equal(fixture->log.events[3].value, 0, "Event[3] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Hold button should be cleared after second-touch drag release");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_click_emits_btn2) {
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN2 press on first 3F tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should remain pressed while waiting second 3F touch");

    k_msleep(CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN2 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN2 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_staggered_release_emits_btn2) {
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame two_release =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 500, 500, 620, 500);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN2 press on first 3F tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should remain pressed while waiting second 3F touch");

    k_msleep(CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN2 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN2 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_one_lead_finger_emits_btn2) {
    const struct iqs9151_test_frame one_lead =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &one_lead, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN2 press on one-lead 3F tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should remain pressed while waiting second 3F touch");

    k_msleep(CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN2 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN2 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_moved_one_lead_does_not_click) {
    const struct iqs9151_test_frame one_lead =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame one_lead_move =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 540, 500, 0, 0);
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 540, 500, 0, 0);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 540, 500, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &one_lead, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_lead_move, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 0U, "Moved one-finger lead should not become 3F tap");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_two_lead_fingers_emits_btn2) {
    const struct iqs9151_test_frame two_lead =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 500, 500, 620, 500);
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame two_release =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 500, 500, 620, 500);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_lead, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN2 press on two-lead 3F tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should remain pressed while waiting second 3F touch");

    k_msleep(CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN2 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN2 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_moved_two_lead_does_not_click) {
    const struct iqs9151_test_frame two_lead =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 500, 500, 620, 500);
    const struct iqs9151_test_frame two_lead_move =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 540, 500, 660, 500);
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 540, 500, 0, 0);
    const struct iqs9151_test_frame two_release =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 540, 500, 660, 500);
    const struct iqs9151_test_frame one_release =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 540, 500, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_lead, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_lead_move, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &one_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 0U, "Moved two-finger lead should not become 3F tap");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_tap_step_to_two_then_zero_avoids_btn1) {
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 500, 500, 0, 0);
    const struct iqs9151_test_frame two_release =
        make_frame(2U,
                   IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                   0, 0, 0, 500, 500, 620, 500);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &two_release, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 1U, "Expected deferred BTN2 press on 3->2->0 tap");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_2, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN2 press");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), INPUT_BTN_2,
                  "BTN2 should remain pressed while waiting second 3F touch");

    k_msleep(CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS + 30);

    zassert_equal(fixture->log.count, 2U, "Expected timeout release after deferred BTN2 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_2, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN2 release");
    zassert_equal(iqs9151_test_hold_button(fixture->ctx), 0U,
                  "Deferred BTN2 press should be released on timeout");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_swipe_right_emits_btn3_click) {
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1000, 1000, 0, 0);
    const struct iqs9151_test_frame three_swipe =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1400, 1000, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &three_swipe, k_uptime_get());

    zassert_equal(fixture->log.count, 2U, "Expected BTN3 click (press + release)");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_3, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN3 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_3, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN3 release");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_swipe_continuous_touch_emits_once) {
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1000, 1000, 0, 0);
    const struct iqs9151_test_frame swipe_step_1 =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1400, 1000, 0, 0);
    const struct iqs9151_test_frame swipe_step_2 =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1800, 1000, 0, 0);
    const struct iqs9151_test_frame swipe_step_3 =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 2200, 1000, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &swipe_step_1, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &swipe_step_2, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &swipe_step_3, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 2U,
                  "Expected a single BTN3 click while 3 fingers stay on pad");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_3, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN3 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_3, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN3 release");
}

ZTEST_F(iqs9151_work_cb, test_three_finger_swipe_left_continuous_touch_emits_once) {
    const struct iqs9151_test_frame three_start =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 2200, 1000, 0, 0);
    const struct iqs9151_test_frame swipe_step_1 =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1800, 1000, 0, 0);
    const struct iqs9151_test_frame swipe_step_2 =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1400, 1000, 0, 0);
    const struct iqs9151_test_frame swipe_step_3 =
        make_frame(3U, IQS9151_TP_FINGER1_CONFIDENCE | 3U, 0, 0, 0, 1000, 1000, 0, 0);
    const struct iqs9151_test_frame release =
        make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &three_start, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &swipe_step_1, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &swipe_step_2, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &swipe_step_3, k_uptime_get());
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 2U,
                  "Expected a single BTN4 click while 3 fingers stay on pad");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, "Event[0] not key");
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_4, "Event[0] unexpected code");
    zassert_equal(fixture->log.events[0].value, 1, "Event[0] should be BTN4 press");
    zassert_equal(fixture->log.events[1].type, IQS9151_TEST_EVENT_KEY, "Event[1] not key");
    zassert_equal(fixture->log.events[1].code, INPUT_BTN_4, "Event[1] unexpected code");
    zassert_equal(fixture->log.events[1].value, 0, "Event[1] should be BTN4 release");
}

/* 1f_tap_max_ms を 50 に下げたとき、100ms のタップを処理するとタップ判定されない */
ZTEST_F(iqs9151_work_cb, test_lowered_1f_tap_max_ms_rejects_100ms_tap) {
    struct iqs9151_params *params = iqs9151_test_params(fixture->ctx);
    const struct iqs9151_param_def *def = iqs9151_param_find("1f_tap_max_ms");
    const struct iqs9151_test_frame down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 1000, 1000, 0, 0);
    const struct iqs9151_test_frame up = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    zassert_equal(iqs9151_params_set(params, def, 50), 0, NULL);
    iqs9151_test_sync_params(fixture->ctx);

    iqs9151_test_process_frame(fixture->ctx, &down, k_uptime_get());
    k_msleep(100);
    iqs9151_test_process_frame(fixture->ctx, &up, k_uptime_get());

    zassert_equal(fixture->log.count, 0U, "100ms のタップはタップ判定されない(events=%u)",
                  (unsigned int)fixture->log.count);
}

/* 既定値のままのとき、100ms のタップを処理すると BTN0 が押される */
ZTEST_F(iqs9151_work_cb, test_default_1f_tap_max_ms_accepts_100ms_tap) {
    const struct iqs9151_test_frame down =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 1000, 1000, 0, 0);
    const struct iqs9151_test_frame up = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &down, k_uptime_get());
    k_msleep(100);
    iqs9151_test_process_frame(fixture->ctx, &up, k_uptime_get());

    zassert_true(fixture->log.count >= 1U, "タップで press が出る");
    zassert_equal(fixture->log.events[0].type, IQS9151_TEST_EVENT_KEY, NULL);
    zassert_equal(fixture->log.events[0].code, INPUT_BTN_0, NULL);
    zassert_equal(fixture->log.events[0].value, 1, NULL);
}

/* driver 系を set すると、即座に値が変わり保留ビットは立たない */
ZTEST_F(iqs9151_work_cb, test_driver_param_set_applies_immediately_without_ic_dirty) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    int32_t value = 0;

    zassert_equal(iqs9151_dev_param_set(dev, "2f_scroll_start_move", 15), 0, NULL);
    zassert_equal(iqs9151_dev_param_get(dev, "2f_scroll_start_move", &value), 0, NULL);
    zassert_equal(value, 15, NULL);
    zassert_equal(iqs9151_test_ic_dirty(fixture->ctx), 0U, NULL);
}

/* 慣性系を set すると、慣性パラメータにも反映される */
ZTEST_F(iqs9151_work_cb, test_inertia_param_set_syncs_inertia_params) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);

    zassert_equal(iqs9151_dev_param_set(dev, "scroll_inertia_decay", 900), 0, NULL);
    zassert_equal(iqs9151_test_scroll_inertia_decay(fixture->ctx), 900, NULL);
}

/* IC 系を set すると、そのパラメータの保留ビットが立つ */
ZTEST_F(iqs9151_work_cb, test_ic_param_set_marks_dirty_bit) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    const struct iqs9151_param_def *def = iqs9151_param_find("touch_set_threshold");
    size_t idx = 0;

    while (iqs9151_param_def_at(idx) != def) {
        idx++;
    }
    zassert_equal(iqs9151_dev_param_set(dev, "touch_set_threshold", 40), 0, NULL);
    zassert_equal(iqs9151_test_ic_dirty(fixture->ctx), BIT(idx), NULL);
}

/* 未知の名前と範囲外の値を set すると、エラーになる */
ZTEST_F(iqs9151_work_cb, test_unknown_name_and_out_of_range_return_errors) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);

    zassert_equal(iqs9151_dev_param_set(dev, "no_such", 1), -ENOENT, NULL);
    zassert_equal(iqs9151_dev_param_set(dev, "1f_tap_max_ms", 5000), -ERANGE, NULL);
}

/* 値を変更した後に reset すると、既定値に戻り IC 系がすべて保留になる */
ZTEST_F(iqs9151_work_cb, test_reset_restores_defaults_and_marks_all_ic_dirty) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    int32_t value = 0;

    zassert_equal(iqs9151_dev_param_set(dev, "1f_tap_max_ms", 500), 0, NULL);
    zassert_equal(iqs9151_dev_param_set(dev, "scroll_inertia_decay", 900), 0, NULL);
    zassert_equal(iqs9151_dev_param_reset(dev), 0, NULL);
    zassert_equal(iqs9151_dev_param_get(dev, "1f_tap_max_ms", &value), 0, NULL);
    zassert_equal(value, CONFIG_INPUT_IQS9151_1F_TAP_MAX_MS, NULL);
    zassert_equal(iqs9151_test_ic_dirty(fixture->ctx), BIT_MASK(IQS9151_PARAM_IC_COUNT), NULL);
    zassert_equal(iqs9151_test_scroll_inertia_decay(fixture->ctx),
                 CONFIG_INPUT_IQS9151_SCROLL_INERTIA_DECAY, NULL);
}

/* Re-ATI を要求すると、保留フラグが立つ */
ZTEST_F(iqs9151_work_cb, test_request_reati_sets_pending_flag) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);

    zassert_false(iqs9151_test_reati_pending(fixture->ctx), NULL);
    zassert_equal(iqs9151_dev_request_reati(dev), 0, NULL);
    zassert_true(iqs9151_test_reati_pending(fixture->ctx), NULL);
}

/* トレースは既定で無効で、有効化と無効化ができる */
ZTEST(iqs9151_work_cb, test_trace_disabled_by_default_and_can_be_enabled_and_disabled) {
    zassert_false(iqs9151_dev_trace_enabled(), NULL);
    iqs9151_dev_trace_enable(true);
    zassert_true(iqs9151_dev_trace_enabled(), NULL);
    iqs9151_dev_trace_enable(false);
    zassert_false(iqs9151_dev_trace_enabled(), NULL);
}

static struct iqs9151_test_frame make_cursor_move_frame(int16_t rel_x, int16_t rel_y,
                                                        uint16_t finger1_x) {
    return make_frame(1U,
                      IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_MOVEMENT_DETECTED | 1U,
                      rel_x, rel_y, 0, finger1_x, 100, 0, 0);
}

static struct iqs9151_test_frame make_two_finger_frame(uint16_t finger1_x) {
    return make_frame(2U,
                      IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                      0, 0, 0, finger1_x, 100, (uint16_t)(finger1_x + 100U), 100);
}

static void set_driver_param(struct iqs9151_work_cb_fixture *fixture, const char *name,
                             int32_t value) {
    zassert_equal(iqs9151_dev_param_set(iqs9151_test_fake_dev(fixture->ctx), name, value), 0,
                  "%s の set に失敗", name);
}

static void assert_rel_event(const struct iqs9151_test_event *event, uint16_t code,
                             int32_t value, bool sync) {
    zassert_equal(event->type, IQS9151_TEST_EVENT_REL, "REL ではない");
    zassert_equal(event->code, code, "code=%u", event->code);
    zassert_equal(event->value, value, "value=%d", event->value);
    zassert_equal(event->sync, sync, "sync=%d", (int)event->sync);
}

/* 送信間隔が 0 のとき、移動フレームを 2 回処理すると REL X/Y がフレームごとに送られる */
ZTEST_F(iqs9151_work_cb, test_report_interval_zero_sends_rel_every_frame) {
    const struct iqs9151_test_frame move_1 = make_cursor_move_frame(1, 2, 100);
    const struct iqs9151_test_frame move_2 = make_cursor_move_frame(3, 4, 140);

    iqs9151_test_process_frame(fixture->ctx, &move_1, k_uptime_get());
    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &move_2, k_uptime_get());

    zassert_equal(fixture->log.count, 4U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[0], INPUT_REL_X, 1, false);
    assert_rel_event(&fixture->log.events[1], INPUT_REL_Y, 2, true);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_X, 3, false);
    assert_rel_event(&fixture->log.events[3], INPUT_REL_Y, 4, true);
}

/* 送信間隔が 16 のとき、移動フレームを 5ms 間隔で入れると、最初の 1 回と 16ms 経過後の 1 回だけ合算して送られる */
ZTEST_F(iqs9151_work_cb, test_report_interval_16_coalesces_cursor_until_interval_elapses) {
    const struct iqs9151_test_frame first = make_cursor_move_frame(2, 3, 100);
    int64_t first_ms;
    uint16_t accumulated = 0U;

    set_driver_param(fixture, "cursor_report_interval_ms", 16);

    first_ms = k_uptime_get();
    iqs9151_test_process_frame(fixture->ctx, &first, first_ms);
    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);

    for (;;) {
        k_msleep(5);
        if ((k_uptime_get() - first_ms) >= 16) {
            break;
        }
        const struct iqs9151_test_frame move =
            make_cursor_move_frame(2, 3, (uint16_t)(100U + ((accumulated + 1U) * 10U)));

        iqs9151_test_process_frame(fixture->ctx, &move, k_uptime_get());
        accumulated++;
        zassert_equal(fixture->log.count, 2U, "frame %u: events=%u",
                      (unsigned int)accumulated, (unsigned int)fixture->log.count);
    }
    zassert_true(accumulated >= 1U, "間隔内に累積されたフレームがない");

    const struct iqs9151_test_frame last =
        make_cursor_move_frame(2, 3, (uint16_t)(100U + ((accumulated + 1U) * 10U)));

    iqs9151_test_process_frame(fixture->ctx, &last, k_uptime_get());

    zassert_equal(fixture->log.count, 4U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[0], INPUT_REL_X, 2, false);
    assert_rel_event(&fixture->log.events[1], INPUT_REL_Y, 3, true);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_X, 2 * (accumulated + 1), false);
    assert_rel_event(&fixture->log.events[3], INPUT_REL_Y, 3 * (accumulated + 1), true);
}

/* 送信間隔が 16 で指を置いたまま移動なしのフレームが挟まっても、累積は送られず 16ms 経過後に合算して送られる */
ZTEST_F(iqs9151_work_cb, test_report_interval_16_non_moving_frame_does_not_flush_cursor) {
    const struct iqs9151_test_frame move_1 = make_cursor_move_frame(2, 3, 100);
    const struct iqs9151_test_frame move_2 = make_cursor_move_frame(4, 5, 110);
    const struct iqs9151_test_frame still =
        make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, 120, 100, 0, 0);
    const struct iqs9151_test_frame move_3 = make_cursor_move_frame(1, 1, 130);
    int64_t first_ms;

    set_driver_param(fixture, "cursor_report_interval_ms", 16);

    first_ms = k_uptime_get();
    iqs9151_test_process_frame(fixture->ctx, &move_1, first_ms);
    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);

    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &move_2, k_uptime_get());
    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);

    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &still, k_uptime_get());
    zassert_equal(fixture->log.count, 2U, "移動なしフレームで送られた(events=%u)",
                  (unsigned int)fixture->log.count);

    while ((k_uptime_get() - first_ms) < 16) {
        k_msleep(1);
    }
    iqs9151_test_process_frame(fixture->ctx, &move_3, k_uptime_get());

    zassert_equal(fixture->log.count, 4U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_X, 5, false);
    assert_rel_event(&fixture->log.events[3], INPUT_REL_Y, 6, true);
}

/* 送信間隔が 16 のとき、移動 1 フレーム後に指を離すと、離しフレームで残りの累積が送られる */
ZTEST_F(iqs9151_work_cb, test_report_interval_16_flushes_cursor_on_release) {
    const struct iqs9151_test_frame move_1 = make_cursor_move_frame(2, 3, 100);
    const struct iqs9151_test_frame move_2 = make_cursor_move_frame(4, 5, 140);
    const struct iqs9151_test_frame release = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    set_driver_param(fixture, "cursor_report_interval_ms", 16);
    set_driver_param(fixture, "cursor_inertia_enable", 0);

    iqs9151_test_process_frame(fixture->ctx, &move_1, k_uptime_get());
    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &move_2, k_uptime_get());
    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);

    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 4U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_X, 4, false);
    assert_rel_event(&fixture->log.events[3], INPUT_REL_Y, 5, true);
}

/* 送信間隔が 16 のとき、移動後にフレームが止まると、間隔経過後に flush work が残りを送る */
ZTEST_F(iqs9151_work_cb, test_report_interval_16_flush_work_sends_remainder_when_frames_stop) {
    const struct iqs9151_test_frame move_1 = make_cursor_move_frame(2, 3, 100);
    const struct iqs9151_test_frame move_2 = make_cursor_move_frame(4, 5, 140);

    set_driver_param(fixture, "cursor_report_interval_ms", 16);

    iqs9151_test_process_frame(fixture->ctx, &move_1, k_uptime_get());
    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &move_2, k_uptime_get());
    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);

    k_msleep(30);

    zassert_equal(fixture->log.count, 4U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_X, 4, false);
    assert_rel_event(&fixture->log.events[3], INPUT_REL_Y, 5, true);
}

/* 送信間隔が 16 で累積が残っているとき、タップで BTN0 を送ると、その直前に累積カーソルが送られる */
ZTEST_F(iqs9151_work_cb, test_report_interval_16_flushes_cursor_before_button_event) {
    const struct iqs9151_test_frame move_1 = make_cursor_move_frame(2, 3, 100);
    const struct iqs9151_test_frame move_2 = make_cursor_move_frame(4, 5, 110);
    const struct iqs9151_test_frame release = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    set_driver_param(fixture, "cursor_report_interval_ms", 16);
    set_driver_param(fixture, "cursor_inertia_enable", 0);

    iqs9151_test_process_frame(fixture->ctx, &move_1, k_uptime_get());
    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &move_2, k_uptime_get());
    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);

    k_msleep(5);
    iqs9151_test_process_frame(fixture->ctx, &release, k_uptime_get());

    zassert_equal(fixture->log.count, 5U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_X, 4, false);
    assert_rel_event(&fixture->log.events[3], INPUT_REL_Y, 5, true);
    zassert_equal(fixture->log.events[4].type, IQS9151_TEST_EVENT_KEY, "Event[4] not key");
    zassert_equal(fixture->log.events[4].code, INPUT_BTN_0, "Event[4] unexpected code");
    zassert_equal(fixture->log.events[4].value, 1, "Event[4] should be BTN0 press");
}

/* スクロール送信間隔が 16 のとき、2F スクロールを 5ms 間隔で入れると、最初の 1 回と 16ms 経過後の 1 回だけ合算して送られる */
ZTEST_F(iqs9151_work_cb, test_scroll_report_interval_16_coalesces_wheel_until_interval_elapses) {
    const struct iqs9151_test_frame two_start = make_two_finger_frame(100);
    const struct iqs9151_test_frame scroll_first = make_two_finger_frame(160);
    int64_t first_ms;
    uint16_t accumulated = 0U;

    set_driver_param(fixture, "scroll_report_interval_ms", 16);

    iqs9151_test_process_frame(fixture->ctx, &two_start, k_uptime_get());
    k_msleep(5);
    first_ms = k_uptime_get();
    iqs9151_test_process_frame(fixture->ctx, &scroll_first, first_ms);
    zassert_equal(fixture->log.count, 1U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[0], INPUT_REL_HWHEEL, -60, true);

    for (;;) {
        k_msleep(5);
        if ((k_uptime_get() - first_ms) >= 16) {
            break;
        }
        const struct iqs9151_test_frame scroll =
            make_two_finger_frame((uint16_t)(160U + ((accumulated + 1U) * 10U)));

        iqs9151_test_process_frame(fixture->ctx, &scroll, k_uptime_get());
        accumulated++;
        zassert_equal(fixture->log.count, 1U, "frame %u: events=%u",
                      (unsigned int)accumulated, (unsigned int)fixture->log.count);
    }
    zassert_true(accumulated >= 1U, "間隔内に累積されたフレームがない");

    const struct iqs9151_test_frame scroll_last =
        make_two_finger_frame((uint16_t)(160U + ((accumulated + 1U) * 10U)));

    iqs9151_test_process_frame(fixture->ctx, &scroll_last, k_uptime_get());

    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[1], INPUT_REL_HWHEEL, -10 * (accumulated + 1), true);
}

static struct iqs9151_test_frame make_two_finger_xy_frame(uint16_t finger1_x, uint16_t finger1_y,
                                                          uint16_t finger2_x, uint16_t finger2_y) {
    return make_frame(2U,
                      IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U,
                      0, 0, 0, finger1_x, finger1_y, finger2_x, finger2_y);
}

static void assert_key_event(const struct iqs9151_test_event *event, uint16_t code,
                             int32_t value) {
    zassert_equal(event->type, IQS9151_TEST_EVENT_KEY, "KEY ではない");
    zassert_equal(event->code, code, "code=%u", event->code);
    zassert_equal(event->value, value, "value=%d", event->value);
}

static void assert_no_key_event(const struct event_log *log, uint16_t code) {
    for (size_t i = 0; i < log->count; i++) {
        zassert_false(log->events[i].type == IQS9151_TEST_EVENT_KEY &&
                          log->events[i].code == code,
                      "event[%u] に code=%u の KEY がある", (unsigned int)i, code);
    }
}

/* 2 本指が指間距離を ±30 揺らしながら平行に +150 動くと、ピンチにならず REL_WHEEL のスクロールになる */
ZTEST_F(iqs9151_work_cb, test_two_finger_parallel_move_with_distance_jitter_scrolls) {
    const struct iqs9151_test_frame two_start = make_two_finger_xy_frame(100, 100, 200, 100);
    const struct iqs9151_test_frame move_1 = make_two_finger_xy_frame(85, 150, 215, 150);
    const struct iqs9151_test_frame move_2 = make_two_finger_xy_frame(115, 200, 185, 200);
    const struct iqs9151_test_frame move_3 = make_two_finger_xy_frame(100, 250, 200, 250);
    const struct iqs9151_test_frame release = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &move_1, 10);
    iqs9151_test_process_frame(fixture->ctx, &move_2, 20);
    iqs9151_test_process_frame(fixture->ctx, &move_3, 30);
    iqs9151_test_process_frame(fixture->ctx, &release, 40);

    zassert_equal(fixture->log.count, 3U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[0], INPUT_REL_WHEEL, 50, true);
    assert_rel_event(&fixture->log.events[1], INPUT_REL_WHEEL, 50, true);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_WHEEL, 50, true);
    assert_no_key_event(&fixture->log, INPUT_BTN_7);
}

/* 重心が +60(スクロール開始しきい値 50 以上)ずれながら指間距離が -160 縮まると、既定比率 1.5 ではピンチになる */
ZTEST_F(iqs9151_work_cb, test_two_finger_pinch_with_centroid_drift_starts_pinch) {
    const struct iqs9151_test_frame two_start = make_two_finger_xy_frame(100, 100, 500, 100);
    const struct iqs9151_test_frame pinch = make_two_finger_xy_frame(240, 100, 480, 100);
    const struct iqs9151_test_frame release = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &pinch, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 20);

    zassert_equal(fixture->log.count, 3U, "events=%u", (unsigned int)fixture->log.count);
    assert_key_event(&fixture->log.events[0], INPUT_BTN_7, 1);
    assert_rel_event(&fixture->log.events[1], INPUT_REL_WHEEL,
                     (-160 * CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10) / (12 * 10), true);
    assert_key_event(&fixture->log.events[2], INPUT_BTN_7, 0);
}

/* 距離 -160 / 重心 +60 の同じ動きでも、2f_pinch_ratio_x10 を 40 にするとスクロール、15 に戻すとピンチになる */
ZTEST_F(iqs9151_work_cb, test_pinch_ratio_param_switches_same_gesture_between_scroll_and_pinch) {
    const struct iqs9151_test_frame two_start = make_two_finger_xy_frame(100, 100, 500, 100);
    const struct iqs9151_test_frame pinch = make_two_finger_xy_frame(240, 100, 480, 100);
    const struct iqs9151_test_frame release = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    set_driver_param(fixture, "2f_pinch_ratio_x10", 40);
    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &pinch, 10);
    iqs9151_test_process_frame(fixture->ctx, &release, 20);

    zassert_equal(fixture->log.count, 1U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[0], INPUT_REL_HWHEEL, -60, true);

    set_driver_param(fixture, "2f_pinch_ratio_x10", 15);
    iqs9151_test_process_frame(fixture->ctx, &two_start, 300);
    iqs9151_test_process_frame(fixture->ctx, &pinch, 310);
    iqs9151_test_process_frame(fixture->ctx, &release, 320);

    zassert_equal(fixture->log.count, 4U, "events=%u", (unsigned int)fixture->log.count);
    assert_key_event(&fixture->log.events[1], INPUT_BTN_7, 1);
    assert_rel_event(&fixture->log.events[2], INPUT_REL_WHEEL,
                     (-160 * CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10) / (12 * 10), true);
    assert_key_event(&fixture->log.events[3], INPUT_BTN_7, 0);
}

/* 距離が重心の 1.6 倍で伸びるがピンチ開始しきい値(200)に届かないとき、重心 +50 では待ち、+100(開始しきい値の 2 倍)でスクロールになる */
ZTEST_F(iqs9151_work_cb, test_centroid_twice_scroll_start_falls_back_to_scroll_when_pinch_not_met) {
    const struct iqs9151_test_frame two_start = make_two_finger_xy_frame(100, 100, 200, 100);
    const struct iqs9151_test_frame move_1 = make_two_finger_xy_frame(60, 150, 240, 150);
    const struct iqs9151_test_frame move_2 = make_two_finger_xy_frame(20, 200, 280, 200);

    set_driver_param(fixture, "2f_pinch_start_distance", 200);
    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &move_1, 10);
    zassert_equal(fixture->log.count, 0U, "events=%u", (unsigned int)fixture->log.count);

    iqs9151_test_process_frame(fixture->ctx, &move_2, 20);
    zassert_equal(fixture->log.count, 1U, "events=%u", (unsigned int)fixture->log.count);
    assert_rel_event(&fixture->log.events[0], INPUT_REL_WHEEL, 50, true);
    assert_no_key_event(&fixture->log, INPUT_BTN_7);
}

/* 比率 4.0 で重心が距離の 1/4 未満に届かず判定保留のとき、距離が開始しきい値の 2 倍(160)に達するとピンチになる */
ZTEST_F(iqs9151_work_cb, test_distance_twice_pinch_start_falls_back_to_pinch_when_ratio_not_met) {
    const struct iqs9151_test_frame two_start = make_two_finger_xy_frame(100, 100, 200, 100);
    const struct iqs9151_test_frame move_1 = make_two_finger_xy_frame(80, 100, 280, 100);
    const struct iqs9151_test_frame move_2 = make_two_finger_xy_frame(65, 100, 325, 100);

    set_driver_param(fixture, "2f_pinch_ratio_x10", 40);
    iqs9151_test_process_frame(fixture->ctx, &two_start, 0);
    iqs9151_test_process_frame(fixture->ctx, &move_1, 10);
    zassert_equal(fixture->log.count, 0U, "events=%u", (unsigned int)fixture->log.count);

    iqs9151_test_process_frame(fixture->ctx, &move_2, 20);
    zassert_equal(fixture->log.count, 2U, "events=%u", (unsigned int)fixture->log.count);
    assert_key_event(&fixture->log.events[0], INPUT_BTN_7, 1);
    assert_rel_event(&fixture->log.events[1], INPUT_REL_WHEEL,
                     (60 * CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10) / (12 * 10), true);
}

static struct iqs9151_test_frame make_one_finger_down_frame(uint16_t x, uint16_t y) {
    return make_frame(1U, IQS9151_TP_FINGER1_CONFIDENCE | 1U, 0, 0, 0, x, y, 0, 0);
}

static void process_now(struct iqs9151_work_cb_fixture *fixture,
                        const struct iqs9151_test_frame *frame) {
    iqs9151_test_process_frame(fixture->ctx, frame, k_uptime_get());
}

/* 要約出力は既定で有効で、無効化と有効化ができる */
ZTEST(iqs9151_work_cb, test_summary_enabled_by_default_and_can_be_toggled) {
    zassert_true(iqs9151_dev_summary_enabled(), NULL);
    iqs9151_dev_summary_enable(false);
    zassert_false(iqs9151_dev_summary_enabled(), NULL);
    iqs9151_dev_summary_enable(true);
    zassert_true(iqs9151_dev_summary_enabled(), NULL);
}

/* 1 本指で 50ms タップして離してからアイドル時間(TEST_SUMMARY_IDLE_MS=330ms)経つと、要約が 1 件出て
 * contacts 1・fingers_max 1・down_ms 50・BTN0 押しビットになる */
ZTEST_F(iqs9151_work_cb, test_summary_one_finger_tap_reports_single_contact) {
    const struct iqs9151_test_frame down = make_one_finger_down_frame(100, 100);
    const struct iqs9151_test_frame up = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const int64_t t_down = k_uptime_get();
    int64_t t_up;
    const struct iqs9151_attempt_summary *s;

    iqs9151_test_process_frame(fixture->ctx, &down, t_down);
    k_msleep(50);
    t_up = k_uptime_get();
    iqs9151_test_process_frame(fixture->ctx, &up, t_up);
    k_msleep(TEST_SUMMARY_IDLE_MS - 30);
    zassert_equal(fixture->summaries.count, 0U, "離してからアイドル時間未満では要約が出ない");

    k_msleep(60);

    zassert_equal(fixture->summaries.count, 1U, "count=%u",
                  (unsigned int)fixture->summaries.count);
    s = &fixture->summaries.items[0];
    zassert_equal(s->start_ms, (uint32_t)t_down, "start_ms=%u", s->start_ms);
    zassert_equal(s->end_ms, (uint32_t)t_up, "end_ms=%u", s->end_ms);
    zassert_equal(s->contacts, 1U, NULL);
    zassert_equal(s->fingers_max, 1U, NULL);
    zassert_equal(s->down_ms, (uint32_t)(t_up - t_down), "down_ms=%u", s->down_ms);
    zassert_equal(s->gap_ms, 0U, NULL);
    zassert_equal(s->move_sum, 0U, NULL);
    zassert_equal(s->mode2f, 0U, NULL);
    zassert_equal(s->btn_press_bits, BIT(0), "press_bits=%02x", s->btn_press_bits);
    zassert_equal(s->btn_release_bits, BIT(0), "deferred release も含まれる(%02x)",
                  s->btn_release_bits);
    zassert_equal(s->wheel_count, 0U, NULL);
    zassert_equal(s->rel_count, 0U, NULL);
    zassert_equal(s->drops, 0U, NULL);
    zassert_equal(s->hold, 1U, "deferred-click が pending になった");
}

/* タップの 100ms 後に再接触して移動すると、同じ試行として contacts 2・gap_ms 100・hold 1 になる */
ZTEST_F(iqs9151_work_cb, test_summary_tap_then_recontact_within_gap_counts_second_contact) {
    const struct iqs9151_test_frame down = make_one_finger_down_frame(100, 100);
    const struct iqs9151_test_frame up = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame move = make_cursor_move_frame(40, 0, 140);
    int64_t t_up;
    int64_t t_down2;
    const struct iqs9151_attempt_summary *s;

    process_now(fixture, &down);
    k_msleep(30);
    t_up = k_uptime_get();
    iqs9151_test_process_frame(fixture->ctx, &up, t_up);
    k_msleep(100);
    t_down2 = k_uptime_get();
    iqs9151_test_process_frame(fixture->ctx, &down, t_down2);
    k_msleep(10);
    process_now(fixture, &move);
    k_msleep(10);
    process_now(fixture, &up);
    k_msleep(450);

    zassert_equal(fixture->summaries.count, 1U, "count=%u",
                  (unsigned int)fixture->summaries.count);
    s = &fixture->summaries.items[0];
    zassert_equal(s->contacts, 2U, NULL);
    zassert_equal(s->gap_ms, (uint32_t)(t_down2 - t_up), "gap_ms=%u", s->gap_ms);
    zassert_equal(s->hold, 1U, NULL);
    zassert_equal(s->move_sum, 40U, "move_sum=%u", s->move_sum);
    zassert_equal(s->rel_count, 2U, "rel_count=%u", s->rel_count);
    zassert_equal(s->btn_press_bits, BIT(0), NULL);
    zassert_equal(s->btn_release_bits, BIT(0), NULL);
}

/* 2 本指でスクロールして離すと、mode2f 1・wheel_count 1 以上・centroid_move 60・fingers_max 2 になる */
ZTEST_F(iqs9151_work_cb, test_summary_two_finger_scroll_reports_scroll_mode_and_wheel) {
    const struct iqs9151_test_frame two_start = make_two_finger_xy_frame(100, 100, 200, 100);
    const struct iqs9151_test_frame two_scroll = make_two_finger_xy_frame(160, 100, 260, 100);
    const struct iqs9151_test_frame release = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_attempt_summary *s;

    process_now(fixture, &two_start);
    k_msleep(10);
    process_now(fixture, &two_scroll);
    k_msleep(10);
    process_now(fixture, &release);
    k_msleep(450);

    zassert_equal(fixture->summaries.count, 1U, "count=%u",
                  (unsigned int)fixture->summaries.count);
    s = &fixture->summaries.items[0];
    zassert_equal(s->contacts, 1U, NULL);
    zassert_equal(s->fingers_max, 2U, NULL);
    zassert_equal(s->mode2f, 1U, "mode2f=%u", s->mode2f);
    zassert_true(s->wheel_count >= 1U, "wheel_count=%u", s->wheel_count);
    zassert_equal(s->centroid_move, 60U, "centroid_move=%u", s->centroid_move);
    zassert_equal(s->dist_delta, 0, "dist_delta=%d", s->dist_delta);
    zassert_equal(s->btn_press_bits, 0U, NULL);
    zassert_equal(s->hold, 0U, NULL);
}

/* 2 本指ピンチをすると、mode2f 2・dist_delta -160・BTN7 の押し/離しビットになる */
ZTEST_F(iqs9151_work_cb, test_summary_two_finger_pinch_reports_pinch_mode_and_distance) {
    const struct iqs9151_test_frame two_start = make_two_finger_xy_frame(100, 100, 500, 100);
    const struct iqs9151_test_frame pinch = make_two_finger_xy_frame(180, 100, 420, 100);
    const struct iqs9151_test_frame release = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_attempt_summary *s;

    process_now(fixture, &two_start);
    k_msleep(10);
    process_now(fixture, &pinch);
    k_msleep(10);
    process_now(fixture, &release);
    k_msleep(450);

    zassert_equal(fixture->summaries.count, 1U, "count=%u",
                  (unsigned int)fixture->summaries.count);
    s = &fixture->summaries.items[0];
    zassert_equal(s->mode2f, 2U, "mode2f=%u", s->mode2f);
    zassert_equal(s->dist_delta, -160, "dist_delta=%d", s->dist_delta);
    zassert_equal(s->btn_press_bits, BIT(7), "press_bits=%02x", s->btn_press_bits);
    zassert_equal(s->btn_release_bits, BIT(7), "release_bits=%02x", s->btn_release_bits);
    zassert_true(s->wheel_count >= 1U, "wheel_count=%u", s->wheel_count);
}

/* 離してからアイドル時間(330ms)未満の再接触は同じ試行になり、アイドル時間が経って試行が閉じた後の
 * 接触は別の試行として 2 件目の要約になる */
ZTEST_F(iqs9151_work_cb, test_summary_recontact_within_idle_is_same_attempt_and_after_is_new) {
    const struct iqs9151_test_frame down = make_one_finger_down_frame(100, 100);
    const struct iqs9151_test_frame up = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    process_now(fixture, &down);
    k_msleep(20);
    process_now(fixture, &up);
    k_msleep(TEST_SUMMARY_IDLE_MS - 30);
    process_now(fixture, &down);
    k_msleep(20);
    process_now(fixture, &up);
    k_msleep(TEST_SUMMARY_IDLE_MS + 120);

    zassert_equal(fixture->summaries.count, 1U, "count=%u",
                  (unsigned int)fixture->summaries.count);
    zassert_equal(fixture->summaries.items[0].contacts, 2U, NULL);

    process_now(fixture, &down);
    k_msleep(20);
    process_now(fixture, &up);
    k_msleep(TEST_SUMMARY_IDLE_MS + 120);

    zassert_equal(fixture->summaries.count, 2U, "count=%u",
                  (unsigned int)fixture->summaries.count);
    zassert_equal(fixture->summaries.items[1].contacts, 1U, NULL);
    zassert_true(fixture->summaries.items[1].start_ms > fixture->summaries.items[0].end_ms,
                 "2 件目は 1 件目の終了より後に始まる");
}

/* 1f_tapdrag_gap_max_ms を 600 に上げるとアイドル時間が 700ms になり、650ms 後の再接触も
 * 同じ試行としてまとめられる */
ZTEST_F(iqs9151_work_cb, test_summary_idle_follows_widened_tapdrag_gap_max_ms) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    const struct iqs9151_test_frame down = make_one_finger_down_frame(100, 100);
    const struct iqs9151_test_frame up = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);

    zassert_equal(iqs9151_dev_param_set(dev, "1f_tapdrag_gap_max_ms", 600), 0, NULL);

    process_now(fixture, &down);
    k_msleep(20);
    process_now(fixture, &up);
    k_msleep(650);
    process_now(fixture, &down);
    k_msleep(20);
    process_now(fixture, &up);
    k_msleep(750);

    zassert_equal(fixture->summaries.count, 1U, "count=%u",
                  (unsigned int)fixture->summaries.count);
    zassert_equal(fixture->summaries.items[0].contacts, 2U, "contacts=%u",
                  fixture->summaries.items[0].contacts);
}

/* SHOW_RESET が来ると、進行中の試行は破棄され要約は出ない */
ZTEST_F(iqs9151_work_cb, test_summary_show_reset_discards_attempt) {
    const struct iqs9151_test_frame down = make_one_finger_down_frame(100, 100);
    const struct iqs9151_test_frame up = make_frame(0U, 0U, 0, 0, 0, 0, 0, 0, 0);
    const struct iqs9151_test_frame show_reset =
        make_frame(0U, 0U, 0, 0, IQS9151_INFO_SHOW_RESET, 0, 0, 0, 0);

    process_now(fixture, &down);
    k_msleep(20);
    process_now(fixture, &up);
    process_now(fixture, &show_reset);
    k_msleep(450);

    zassert_equal(fixture->summaries.count, 0U, "count=%u",
                  (unsigned int)fixture->summaries.count);
}

/* live on hz=50 の 1 本指フレームを 5ms 間隔で 10 回入れると、最初と 20ms ごとの計 3 回だけ呼ばれる */
ZTEST_F(iqs9151_work_cb, test_live_throttles_same_finger_count_by_hz) {
    const struct iqs9151_test_frame frame = make_one_finger_down_frame(100, 100);
    int ret = iqs9151_dev_live_enable(true, 50);

    zassert_equal(ret, 0, "ret=%d", ret);

    for (int i = 0; i < 10; i++) {
        iqs9151_test_process_frame(fixture->ctx, &frame, (int64_t)(i * 5));
    }

    zassert_equal(fixture->frames.count, 3U, "count=%u",
                  (unsigned int)fixture->frames.count);
    zassert_equal(fixture->frames.items[0].ms, 0U, NULL);
    zassert_equal(fixture->frames.items[1].ms, 20U, NULL);
    zassert_equal(fixture->frames.items[2].ms, 40U, NULL);
}

/* 指の本数が変わったフレームは間引き間隔に関係なく呼ばれる */
ZTEST_F(iqs9151_work_cb, test_live_always_calls_on_finger_count_change) {
    const struct iqs9151_test_frame one = make_one_finger_down_frame(100, 100);
    const struct iqs9151_test_frame two =
        make_frame(2U, IQS9151_TP_FINGER1_CONFIDENCE | IQS9151_TP_FINGER2_CONFIDENCE | 2U, 0, 0, 0,
                  100, 100, 200, 200);
    int ret = iqs9151_dev_live_enable(true, 50);

    zassert_equal(ret, 0, "ret=%d", ret);

    iqs9151_test_process_frame(fixture->ctx, &one, 0);
    iqs9151_test_process_frame(fixture->ctx, &two, 1);

    zassert_equal(fixture->frames.count, 2U, "count=%u",
                  (unsigned int)fixture->frames.count);
    zassert_equal(fixture->frames.items[0].fingers, 1U, NULL);
    zassert_equal(fixture->frames.items[1].fingers, 2U, NULL);
    zassert_equal(fixture->frames.items[1].ms, 1U, NULL);
}

/* live off ではフレームコールバックが呼ばれない */
ZTEST_F(iqs9151_work_cb, test_live_off_does_not_call_frame_hook) {
    const struct iqs9151_test_frame frame = make_one_finger_down_frame(100, 100);

    (void)iqs9151_dev_live_enable(false, IQS9151_LIVE_HZ_DEFAULT);
    iqs9151_test_process_frame(fixture->ctx, &frame, 0);
    iqs9151_test_process_frame(fixture->ctx, &frame, 100);

    zassert_equal(fixture->frames.count, 0U, "count=%u",
                  (unsigned int)fixture->frames.count);
}

/* iqs9151_frame_format は T F 行と同じ書式の文字列を作る */
ZTEST(iqs9151_work_cb, test_frame_format_matches_expected_string) {
    const struct iqs9151_frame_info f = {
        .ms = 1234,
        .fingers = 2,
        .rel_x = -3,
        .rel_y = 5,
        .f1x = 100,
        .f1y = 200,
        .f2x = 300,
        .f2y = 400,
        .flags = 0x12,
        .hold = 7,
        .mode2f = 1,
        .pending = 2,
    };
    char buf[96];
    int ret = iqs9151_frame_format(&f, buf, sizeof(buf));

    zassert_equal(strcmp(buf, "T F 1234 2 -3 5 100 200 300 400 0012 7 1 2"), 0, "buf=%s", buf);
    zassert_equal(ret, (int)strlen(buf), "ret=%d", ret);
}

ZTEST_SUITE(iqs9151_work_cb, NULL, iqs9151_work_cb_setup, iqs9151_work_cb_before, NULL, NULL);
