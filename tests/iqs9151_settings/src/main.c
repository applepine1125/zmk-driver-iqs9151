#include <zephyr/input/input.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#include "iqs9151_params.h"
#include "iqs9151_test.h"

#include <errno.h>
#include <string.h>

#define IQS9151_TEST_CTX_BUF_SIZE 2048
#define MEM_STORE_VALUE_MAX 512

struct iqs9151_settings_fixture {
    uint8_t ctx[IQS9151_TEST_CTX_BUF_SIZE] __aligned(8);
};

struct mem_store {
    struct settings_store store;
    char name[SETTINGS_MAX_NAME_LEN + 1];
    uint8_t value[MEM_STORE_VALUE_MAX];
    size_t len;
    bool present;
    unsigned int save_count;
};

struct mem_read_arg {
    const uint8_t *data;
    size_t len;
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

static ssize_t mem_read_cb(void *cb_arg, void *data, size_t len) {
    struct mem_read_arg *arg = cb_arg;
    const size_t n = MIN(arg->len, len);

    memcpy(data, arg->data, n);
    return (ssize_t)n;
}

static int mem_load(struct settings_store *cs, const struct settings_load_arg *arg) {
    struct mem_store *m = CONTAINER_OF(cs, struct mem_store, store);
    struct mem_read_arg read_arg = {.data = m->value, .len = m->len};

    if (!m->present) {
        return 0;
    }
    return settings_call_set_handler(m->name, m->len, mem_read_cb, &read_arg, arg);
}

static int mem_save(struct settings_store *cs, const char *name, const char *value,
                    size_t val_len) {
    struct mem_store *m = CONTAINER_OF(cs, struct mem_store, store);

    m->save_count++;
    if (value == NULL || val_len == 0) {
        m->present = false;
        m->len = 0;
        return 0;
    }
    if (val_len > sizeof(m->value)) {
        return -ENOMEM;
    }
    strncpy(m->name, name, sizeof(m->name) - 1);
    m->name[sizeof(m->name) - 1] = '\0';
    memcpy(m->value, value, val_len);
    m->len = val_len;
    m->present = true;
    return 0;
}

static const struct settings_store_itf mem_itf = {
    .csi_load = mem_load,
    .csi_save = mem_save,
};

static struct mem_store mem = {
    .store = {.cs_itf = &mem_itf},
};

int settings_backend_init(void) {
    settings_src_register(&mem.store);
    settings_dst_register(&mem.store);
    return 0;
}

static void mem_store_reset(void) {
    mem.present = false;
    mem.len = 0;
    mem.save_count = 0;
    memset(mem.name, 0, sizeof(mem.name));
}

static void mem_store_put(const uint8_t *blob, size_t len) {
    strncpy(mem.name, "iqs9151/params", sizeof(mem.name) - 1);
    memcpy(mem.value, blob, len);
    mem.len = len;
    mem.present = true;
}

static void *iqs9151_settings_setup(void) {
    static struct iqs9151_settings_fixture fixture;

    zassert_true(iqs9151_test_context_size() <= sizeof(fixture.ctx), "ctx too small");
    zassert_equal(settings_subsys_init(), 0, "settings_subsys_init failed");
    /* テストスレッドは協調優先度なので、低優先度のシェルスレッドが ACTIVE になるまで譲る */
    k_msleep(10);
    return &fixture;
}

static void iqs9151_settings_before(void *fixture_ptr) {
    struct iqs9151_settings_fixture *fixture = fixture_ptr;

    mem_store_reset();
    iqs9151_test_cancel_pending_work(fixture->ctx);
    iqs9151_test_context_init(fixture->ctx, NULL);
    iqs9151_settings_test_set_device(iqs9151_test_fake_dev(fixture->ctx));
    iqs9151_settings_test_reset_loaded();
}

static size_t blob_size(void) {
    return 4U + 4U * iqs9151_param_count();
}

/* 既定値以外を入れた params を encode して decode すると、同じ値に戻る */
ZTEST(iqs9151_settings, test_encode_decode_round_trip) {
    struct iqs9151_params src;
    struct iqs9151_params dst;
    uint8_t buf[256];
    int ret;

    iqs9151_params_init(&src);
    src.f1_tap_max_ms = 321;
    src.touch_set_threshold = 77;
    src.cursor_inertia_enable = 1;
    src.f2_pinch_ratio_x10 = 42;

    ret = iqs9151_settings_encode(&src, buf, sizeof(buf));
    zassert_equal(ret, (int)blob_size(), "encoded size=%d", ret);
    zassert_equal(sys_get_le16(&buf[0]), 1U, "version");
    zassert_equal(sys_get_le16(&buf[2]), iqs9151_param_count(), "count");

    memset(&dst, 0, sizeof(dst));
    zassert_equal(iqs9151_settings_decode(buf, (size_t)ret, &dst), 0, NULL);
    zassert_mem_equal(&dst, &src, sizeof(src), "decoded params differ");
}

/* バッファがブロブより小さいとき、encode は EINVAL になる */
ZTEST(iqs9151_settings, test_encode_rejects_small_buffer) {
    struct iqs9151_params src;
    uint8_t buf[16];

    iqs9151_params_init(&src);
    zassert_equal(iqs9151_settings_encode(&src, buf, sizeof(buf)), -EINVAL, NULL);
}

/* count が合わないブロブを decode すると、EINVAL になり出力は変わらない */
ZTEST(iqs9151_settings, test_decode_rejects_count_mismatch) {
    struct iqs9151_params src;
    struct iqs9151_params dst;
    uint8_t buf[256];
    int len;

    iqs9151_params_init(&src);
    len = iqs9151_settings_encode(&src, buf, sizeof(buf));
    zassert_true(len > 0, NULL);
    sys_put_le16((uint16_t)(iqs9151_param_count() - 1U), &buf[2]);

    memset(&dst, 0xAA, sizeof(dst));
    zassert_equal(iqs9151_settings_decode(buf, (size_t)len, &dst), -EINVAL, NULL);
    zassert_equal((uint8_t)dst.f1_tap_max_ms, 0xAA, "出力が書き換わっている");
}

/* version が合わないブロブを decode すると、EINVAL になる */
ZTEST(iqs9151_settings, test_decode_rejects_version_mismatch) {
    struct iqs9151_params src;
    struct iqs9151_params dst;
    uint8_t buf[256];
    int len;

    iqs9151_params_init(&src);
    len = iqs9151_settings_encode(&src, buf, sizeof(buf));
    sys_put_le16(2U, &buf[0]);

    zassert_equal(iqs9151_settings_decode(buf, (size_t)len, &dst), -EINVAL, NULL);
}

/* 長さがヘッダと合わないブロブを decode すると、EINVAL になる */
ZTEST(iqs9151_settings, test_decode_rejects_length_mismatch) {
    struct iqs9151_params src;
    struct iqs9151_params dst;
    uint8_t buf[256];
    int len;

    iqs9151_params_init(&src);
    len = iqs9151_settings_encode(&src, buf, sizeof(buf));

    zassert_equal(iqs9151_settings_decode(buf, (size_t)len - 4U, &dst), -EINVAL, NULL);
    zassert_equal(iqs9151_settings_decode(buf, 3U, &dst), -EINVAL, NULL);
}

/* 値を変えて save した後、デバイスを初期化し直して settings_load すると値が戻り IC 系はすべて保留になる */
ZTEST_F(iqs9151_settings, test_save_then_load_restores_values) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    int32_t value = 0;

    zassert_false(iqs9151_settings_loaded(), NULL);
    zassert_equal(iqs9151_dev_param_set(dev, "1f_tap_max_ms", 333), 0, NULL);
    zassert_equal(iqs9151_dev_param_set(dev, "touch_set_threshold", 40), 0, NULL);
    zassert_equal(iqs9151_settings_save(dev), 0, NULL);
    zassert_true(mem.present, "ブロブが保存されていない");
    zassert_equal(mem.len, blob_size(), "len=%u", (unsigned int)mem.len);
    zassert_true(strcmp(mem.name, "iqs9151/params") == 0, "name=%s", mem.name);
    zassert_true(iqs9151_settings_loaded(), NULL);

    iqs9151_test_context_init(fixture->ctx, NULL);
    iqs9151_settings_test_reset_loaded();
    zassert_equal(iqs9151_dev_param_get(dev, "1f_tap_max_ms", &value), 0, NULL);
    zassert_equal(value, CONFIG_INPUT_IQS9151_1F_TAP_MAX_MS, NULL);

    zassert_equal(settings_load(), 0, NULL);

    zassert_equal(iqs9151_dev_param_get(dev, "1f_tap_max_ms", &value), 0, NULL);
    zassert_equal(value, 333, NULL);
    zassert_equal(iqs9151_dev_param_get(dev, "touch_set_threshold", &value), 0, NULL);
    zassert_equal(value, 40, NULL);
    zassert_equal(iqs9151_test_ic_dirty(fixture->ctx), BIT_MASK(IQS9151_PARAM_IC_COUNT),
                  "dirty=%08x", iqs9151_test_ic_dirty(fixture->ctx));
    zassert_true(iqs9151_settings_loaded(), NULL);
}

/* count が合わないブロブが保存されているとき、settings_load しても値は既定のままで loaded は false になる */
ZTEST_F(iqs9151_settings, test_load_ignores_count_mismatch_blob) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    struct iqs9151_params src;
    uint8_t buf[256];
    int32_t value = 0;
    int len;

    iqs9151_params_init(&src);
    src.f1_tap_max_ms = 333;
    len = iqs9151_settings_encode(&src, buf, sizeof(buf));
    sys_put_le16((uint16_t)(iqs9151_param_count() + 1U), &buf[2]);
    mem_store_put(buf, (size_t)len);

    zassert_equal(settings_load(), 0, NULL);

    zassert_equal(iqs9151_dev_param_get(dev, "1f_tap_max_ms", &value), 0, NULL);
    zassert_equal(value, CONFIG_INPUT_IQS9151_1F_TAP_MAX_MS, NULL);
    zassert_false(iqs9151_settings_loaded(), NULL);
}

/* 範囲外の値を含むブロブを load すると、その値だけ飛ばして他は適用される */
ZTEST_F(iqs9151_settings, test_load_skips_out_of_range_value) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    struct iqs9151_params src;
    uint8_t buf[256];
    int32_t value = 0;
    int len;

    iqs9151_params_init(&src);
    src.f1_tap_max_ms = 5000;
    src.f2_tap_max_ms = 222;
    len = iqs9151_settings_encode(&src, buf, sizeof(buf));
    mem_store_put(buf, (size_t)len);

    zassert_equal(settings_load(), 0, NULL);

    zassert_equal(iqs9151_dev_param_get(dev, "1f_tap_max_ms", &value), 0, NULL);
    zassert_equal(value, CONFIG_INPUT_IQS9151_1F_TAP_MAX_MS, "範囲外の値が適用された");
    zassert_equal(iqs9151_dev_param_get(dev, "2f_tap_max_ms", &value), 0, NULL);
    zassert_equal(value, 222, NULL);
    zassert_true(iqs9151_settings_loaded(), NULL);
}

/* save した後に clear すると、ブロブが消えて loaded は false になる */
ZTEST_F(iqs9151_settings, test_clear_deletes_blob) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);

    zassert_equal(iqs9151_settings_save(dev), 0, NULL);
    zassert_true(mem.present, NULL);

    zassert_equal(iqs9151_settings_clear(), 0, NULL);

    zassert_false(mem.present, "ブロブが残っている");
    zassert_false(iqs9151_settings_loaded(), NULL);
}

static const char *run_tp(const char *cmd) {
    const struct shell *sh = shell_backend_dummy_get_ptr();
    size_t size = 0;

    shell_backend_dummy_clear_output(sh);
    zassert_equal(shell_execute_cmd(sh, cmd), 0, "%s failed", cmd);
    return shell_backend_dummy_get_output(sh, &size);
}

/* tp info は保存ブロブがないとき saved=no、save した後は saved=yes を末尾に出す */
ZTEST_F(iqs9151_settings, test_tp_info_reports_saved_flag) {
    const struct device *dev = iqs9151_test_fake_dev(fixture->ctx);
    const char *out;

    out = run_tp("tp info");
    zassert_not_null(strstr(out, "params=54 saved=no"), "out=%s", out);

    zassert_equal(iqs9151_settings_save(dev), 0, NULL);

    out = run_tp("tp info");
    zassert_not_null(strstr(out, "params=54 saved=yes"), "out=%s", out);
}

ZTEST_SUITE(iqs9151_settings, NULL, iqs9151_settings_setup, iqs9151_settings_before, NULL, NULL);
