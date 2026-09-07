#include <zephyr/input/input.h>
#include <zephyr/ztest.h>

#include "iqs9151_params.h"
#include "iqs9151_regs.h"
#include "iqs9151_test.h"

#include <string.h>

#define IQS9151_TEST_CTX_BUF_SIZE 1536
#define IQS9151_TEST_MAX_EVENTS 32

struct event_log {
    struct iqs9151_test_event events[IQS9151_TEST_MAX_EVENTS];
    size_t count;
};

struct iqs9151_work_cb_fixture {
    uint8_t ctx[IQS9151_TEST_CTX_BUF_SIZE];
    struct event_log log;
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
    return &fixture;
}

static void iqs9151_work_cb_before(void *fixture_ptr) {
    struct iqs9151_work_cb_fixture *fixture =
        (struct iqs9151_work_cb_fixture *)fixture_ptr;

    memset(&fixture->log, 0, sizeof(fixture->log));
    iqs9151_test_cancel_pending_work(fixture->ctx);
    iqs9151_test_context_init(fixture->ctx, NULL);
    iqs9151_test_set_event_hook(record_event, &fixture->log);
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

ZTEST_SUITE(iqs9151_work_cb, NULL, iqs9151_work_cb_setup, iqs9151_work_cb_before, NULL, NULL);
