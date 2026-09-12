#include <zephyr/device.h>
#include <zephyr/ztest.h>

#include "iqs9151_cmd.h"
#include "iqs9151_params.h"
#include "iqs9151_test.h"

#include <errno.h>
#include <string.h>

#define IQS9151_TEST_CTX_BUF_SIZE 2048
#define CMD_LOG_MAX_LINES 64
#define CMD_LOG_LINE_MAX 160

struct cmd_log {
    char lines[CMD_LOG_MAX_LINES][CMD_LOG_LINE_MAX];
    size_t count;
};

struct iqs9151_cmd_fixture {
    uint8_t ctx[IQS9151_TEST_CTX_BUF_SIZE] __aligned(8);
    struct cmd_log log;
};

int input_report(const struct device *dev, uint8_t type, uint16_t code, int32_t value, bool sync,
                 k_timeout_t timeout) {
    ARG_UNUSED(dev);
    ARG_UNUSED(type);
    ARG_UNUSED(code);
    ARG_UNUSED(value);
    ARG_UNUSED(sync);
    ARG_UNUSED(timeout);
    return 0;
}

static void collect_out(void *ctx, const char *line) {
    struct cmd_log *log = ctx;

    if (log->count >= ARRAY_SIZE(log->lines)) {
        return;
    }
    strncpy(log->lines[log->count], line, CMD_LOG_LINE_MAX - 1);
    log->lines[log->count][CMD_LOG_LINE_MAX - 1] = '\0';
    log->count++;
}

/* iqs9151_test_fake_dev は state を設定しないため、device_is_ready が読めるように補う */
static struct device_state cmd_fake_dev_state = {
    .initialized = true,
    .init_res = 0,
};

static const struct device *cmd_fake_dev(void *ctx) {
    struct device *dev = (struct device *)iqs9151_test_fake_dev(ctx);

    dev->state = &cmd_fake_dev_state;
    return dev;
}

static int run(struct iqs9151_cmd_fixture *fixture, const char *line) {
    memset(&fixture->log, 0, sizeof(fixture->log));
    return iqs9151_cmd_exec(cmd_fake_dev(fixture->ctx), line, collect_out, &fixture->log);
}

static void *iqs9151_cmd_setup(void) {
    static struct iqs9151_cmd_fixture fixture;

    zassert_true(iqs9151_test_context_size() <= sizeof(fixture.ctx), "ctx too small");
    iqs9151_test_context_init(fixture.ctx, NULL);
    return &fixture;
}

static void iqs9151_cmd_before(void *fixture_ptr) {
    struct iqs9151_cmd_fixture *fixture = fixture_ptr;

    memset(&fixture->log, 0, sizeof(fixture->log));
    iqs9151_test_cancel_pending_work(fixture->ctx);
    iqs9151_test_context_init(fixture->ctx, NULL);
}

/* list は 61 個ぜんぶを 1 行ずつ返し、先頭行は touch_set_threshold の定義になる */
ZTEST_F(iqs9151_cmd, test_list_returns_all_params_with_expected_format) {
    int ret = run(fixture, "list");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 61U, "count=%u", (unsigned int)fixture->log.count);
    zassert_equal(strcmp(fixture->log.lines[0], "touch_set_threshold 30 0 255 ic_u8 30"), 0,
                  "line0=%s", fixture->log.lines[0]);
}

/* info は side/uptime/params/saved を 1 行で返す */
ZTEST_F(iqs9151_cmd, test_info_reports_side_and_param_count) {
    int ret = run(fixture, "info");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 1U, NULL);
    zassert_true(strncmp(fixture->log.lines[0], "side=peripheral uptime_ms=", 26) == 0, "line=%s",
                 fixture->log.lines[0]);
    zassert_not_null(strstr(fixture->log.lines[0], " params=61 saved=no"), "line=%s",
                     fixture->log.lines[0]);
}

/* set した値は get で読み戻せる */
ZTEST_F(iqs9151_cmd, test_set_then_get_round_trip) {
    int ret = run(fixture, "set 1f_tap_max_ms 200");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 1U, NULL);
    zassert_equal(strcmp(fixture->log.lines[0], "OK 1f_tap_max_ms=200"), 0, "line=%s",
                  fixture->log.lines[0]);

    ret = run(fixture, "get 1f_tap_max_ms");
    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "1f_tap_max_ms=200"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* 未知パラメータへの set は ENOENT になる */
ZTEST_F(iqs9151_cmd, test_set_unknown_param_reports_enoent) {
    int ret = run(fixture, "set nosuch 1");

    zassert_equal(ret, -ENOENT, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "ERR unknown param nosuch"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* 範囲外の値への set は ERANGE になる */
ZTEST_F(iqs9151_cmd, test_set_out_of_range_reports_erange) {
    int ret = run(fixture, "set 1f_tap_max_ms 5000");

    zassert_equal(ret, -ERANGE, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "ERR out of range 1..1000"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* reset は OK reset を返す */
ZTEST_F(iqs9151_cmd, test_reset_reports_ok) {
    int ret = run(fixture, "reset");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "OK reset"), 0, "line=%s", fixture->log.lines[0]);
}

/* 未知のサブコマンドは ENOENT になる */
ZTEST_F(iqs9151_cmd, test_unknown_command_reports_enoent) {
    int ret = run(fixture, "bogus");

    zassert_equal(ret, -ENOENT, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "ERR unknown command bogus"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* 先頭の tp トークンは読み飛ばされる */
ZTEST_F(iqs9151_cmd, test_leading_tp_token_is_skipped) {
    int ret = run(fixture, "tp info");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 1U, NULL);
    zassert_true(strncmp(fixture->log.lines[0], "side=peripheral uptime_ms=", 26) == 0, "line=%s",
                 fixture->log.lines[0]);
}

/* 64 バイトを超える行は E2BIG になる */
ZTEST_F(iqs9151_cmd, test_line_too_long_reports_e2big) {
    char line[IQS9151_CMD_LINE_MAX + 2];
    int ret;

    memset(line, 'a', IQS9151_CMD_LINE_MAX + 1);
    line[IQS9151_CMD_LINE_MAX + 1] = '\0';

    ret = run(fixture, line);

    zassert_equal(ret, -E2BIG, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "ERR line too long"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* 5 トークン以上は E2BIG になる */
ZTEST_F(iqs9151_cmd, test_too_many_tokens_reports_e2big) {
    int ret = run(fixture, "set a b c d");

    zassert_equal(ret, -E2BIG, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "ERR too many args"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* 要約を 10 語へパックしてから戻すと、down_ms は 65535 に丸まり他は元の値に戻る */
ZTEST(iqs9151_cmd, test_summary_pack_unpack_round_trip) {
    struct iqs9151_attempt_summary src;
    struct iqs9151_attempt_summary expected;
    struct iqs9151_attempt_summary dst;
    uint32_t words[IQS9151_SUMMARY_WORDS];

    memset(&src, 0, sizeof(src));
    src.start_ms = 1000;
    src.end_ms = 2500;
    src.contacts = 3;
    src.fingers_max = 2;
    src.down_ms = 70000;
    src.gap_ms = 400;
    src.move_sum = 12345;
    src.centroid_move = 678;
    src.dist_delta = -321;
    src.mode2f = 2;
    src.btn_press_bits = 0x01;
    src.btn_release_bits = 0x02;
    src.wheel_count = 7;
    src.wheel_sum = -55;
    src.rel_count = 9;
    src.drops = 4;
    src.hold = 1;

    memcpy(&expected, &src, sizeof(expected));
    expected.down_ms = 65535;

    iqs9151_summary_pack(&src, words);
    iqs9151_summary_unpack(words, &dst);

    zassert_mem_equal(&dst, &expected, sizeof(expected), "round trip differs");
}

/* format は現在の LOG 行と同じ書式の文字列を作る */
ZTEST(iqs9151_cmd, test_summary_format_matches_expected_string) {
    struct iqs9151_attempt_summary s;
    char buf[128];
    int ret;

    memset(&s, 0, sizeof(s));
    s.start_ms = 100;
    s.end_ms = 250;
    s.contacts = 2;
    s.fingers_max = 1;
    s.down_ms = 150;
    s.gap_ms = 0;
    s.move_sum = 42;
    s.centroid_move = 5;
    s.dist_delta = -7;
    s.mode2f = 1;
    s.btn_press_bits = 1;
    s.btn_release_bits = 2;
    s.wheel_count = 3;
    s.wheel_sum = -9;
    s.rel_count = 4;
    s.drops = 1;
    s.hold = 0;

    ret = iqs9151_summary_format(&s, buf, sizeof(buf));

    zassert_equal(strcmp(buf, "T S 100 250 2 1 150 0 42 5 -7 1 1 2 3 -9 4 1 0"), 0, "buf=%s", buf);
    zassert_equal(ret, (int)strlen(buf), "ret=%d", ret);
}

/* live on は hz 省略時に既定の 60 を使う */
ZTEST_F(iqs9151_cmd, test_live_on_defaults_to_60hz) {
    int ret = run(fixture, "live on");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "OK live=on hz=60"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* live on <hz> は指定した hz をそのまま使う */
ZTEST_F(iqs9151_cmd, test_live_on_with_hz_uses_given_value) {
    int ret = run(fixture, "live on 30");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "OK live=on hz=30"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* live off は OK live=off を返す */
ZTEST_F(iqs9151_cmd, test_live_off_reports_ok) {
    int ret = run(fixture, "live off");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "OK live=off"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* live on の hz が範囲外(0)だと ERANGE になる */
ZTEST_F(iqs9151_cmd, test_live_on_with_out_of_range_hz_reports_erange) {
    int ret = run(fixture, "live on 0");

    zassert_equal(ret, -ERANGE, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "ERR hz 1..100"), 0, "line=%s",
                  fixture->log.lines[0]);
}

/* stats はワークキュー処理がないこのテストでは常に 0 のまま、reset しても変わらない */
ZTEST_F(iqs9151_cmd, test_stats_reports_zero_counts_by_default) {
    int ret = run(fixture, "stats");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 2U, NULL);
    zassert_equal(strcmp(fixture->log.lines[0],
                         "stats frame_n=0 frame_max_us=0 frame_avg_us=0 frame_gap_max_ms=0 i2c_err=0"),
                  0, "line=%s", fixture->log.lines[0]);
    zassert_equal(strcmp(fixture->log.lines[1],
                         "stats i2c_max_us=0 i2c_avg_us=0 reset_n=0 rdy_miss=0 end_err=0"),
                  0, "line=%s", fixture->log.lines[1]);
}

/* stats noreset も同じ書式を返す(このテストでは reset の有無で見た目の差は出ない) */
ZTEST_F(iqs9151_cmd, test_stats_noreset_reports_same_format) {
    int ret = run(fixture, "stats noreset");

    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 2U, NULL);
    zassert_equal(strcmp(fixture->log.lines[0],
                         "stats frame_n=0 frame_max_us=0 frame_avg_us=0 frame_gap_max_ms=0 i2c_err=0"),
                  0, "line=%s", fixture->log.lines[0]);
    zassert_equal(strcmp(fixture->log.lines[1],
                         "stats i2c_max_us=0 i2c_avg_us=0 reset_n=0 rdy_miss=0 end_err=0"),
                  0, "line=%s", fixture->log.lines[1]);
}

/* stats の未知引数は ERR usage を返す */
ZTEST_F(iqs9151_cmd, test_stats_unknown_arg_reports_usage_error) {
    int ret = run(fixture, "stats bogus");

    zassert_equal(ret, -EINVAL, "ret=%d", ret);
    zassert_equal(strcmp(fixture->log.lines[0], "ERR usage: stats [noreset]"), 0, "line=%s",
                  fixture->log.lines[0]);
}

static void stats_hook_record(iqs9151_cmd_out_t out, void *ctx, bool reset) {
    out(ctx, reset ? "hook reset=1" : "hook reset=0");
}

/* 登録したフックは stats 行の後に呼ばれ、reset の有無をそのまま受け取る */
ZTEST_F(iqs9151_cmd, test_stats_calls_registered_hook_with_reset_flag) {
    int ret;

    iqs9151_cmd_set_stats_hook(stats_hook_record);

    ret = run(fixture, "stats");
    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 3U, NULL);
    zassert_equal(strcmp(fixture->log.lines[2], "hook reset=1"), 0, "line=%s",
                  fixture->log.lines[2]);

    ret = run(fixture, "stats noreset");
    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 3U, NULL);
    zassert_equal(strcmp(fixture->log.lines[2], "hook reset=0"), 0, "line=%s",
                  fixture->log.lines[2]);

    iqs9151_cmd_set_stats_hook(NULL);
}

/* フックを解除すると stats 行だけに戻る */
ZTEST_F(iqs9151_cmd, test_stats_without_hook_reports_driver_lines_only) {
    int ret;

    iqs9151_cmd_set_stats_hook(stats_hook_record);
    iqs9151_cmd_set_stats_hook(NULL);

    ret = run(fixture, "stats");
    zassert_equal(ret, 0, "ret=%d", ret);
    zassert_equal(fixture->log.count, 2U, NULL);
}

ZTEST_SUITE(iqs9151_cmd, NULL, iqs9151_cmd_setup, iqs9151_cmd_before, NULL, NULL);
