#include <zephyr/ztest.h>

#include "iqs9151_params.h"

#include <errno.h>
#include <string.h>

ZTEST_SUITE(iqs9151_params, NULL, NULL, NULL, NULL, NULL);

/* 定義テーブルは54個で、IC系17個が先頭にあるとき、種別が位置と一致する */
ZTEST(iqs9151_params, test_table_has_54_params_ic_first) {
    zassert_equal(iqs9151_param_count(), 57U, "count=%u",
                  (unsigned int)iqs9151_param_count());
    for (size_t i = 0; i < iqs9151_param_count(); i++) {
        const struct iqs9151_param_def *def = iqs9151_param_def_at(i);

        zassert_equal(iqs9151_param_is_ic(def), i < IQS9151_PARAM_IC_COUNT,
                      "%s の種別が位置と合わない", def->name);
    }
}

/* 初期化するとKconfigの値が入る */
ZTEST(iqs9151_params, test_init_fills_kconfig_defaults) {
    struct iqs9151_params p;

    memset(&p, 0xff, sizeof(p));
    iqs9151_params_init(&p);

    zassert_equal(p.f1_tap_max_ms, 120, NULL);
    zassert_equal(p.touch_set_threshold, 30, NULL);
    zassert_equal(p.scroll_inertia_enable, 1, NULL);
    zassert_equal(p.cursor_inertia_enable, 0, "未定義の bool は 0 になる");
}

/* 名前で検索できて、未知の名前はNULLになる */
ZTEST(iqs9151_params, test_find_by_name_returns_null_for_unknown) {
    const struct iqs9151_param_def *def = iqs9151_param_find("1f_tap_max_ms");

    zassert_not_null(def, NULL);
    zassert_equal(def->kind, IQS9151_PARAM_DRIVER, NULL);
    zassert_equal(def->min, 1, NULL);
    zassert_equal(def->max, 1000, NULL);
    zassert_equal(def->def, 120, NULL);
    zassert_is_null(iqs9151_param_find("no_such_param"), NULL);
}

/* 送信間隔パラメータは driver 種別で、範囲 0〜100・既定値 0 になる */
ZTEST(iqs9151_params, test_report_interval_params_are_driver_kind_with_range_0_100) {
    const char *const names[] = {"cursor_report_interval_ms", "scroll_report_interval_ms"};

    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        const struct iqs9151_param_def *def = iqs9151_param_find(names[i]);

        zassert_not_null(def, "%s が見つからない", names[i]);
        zassert_equal(def->kind, IQS9151_PARAM_DRIVER, "%s", names[i]);
        zassert_equal(def->min, 0, "%s", names[i]);
        zassert_equal(def->max, 100, "%s", names[i]);
        zassert_equal(def->def, 0, "%s", names[i]);
    }
}

/* ピンチ比率パラメータは driver 種別で、範囲 5〜50・既定値 15 になる */
ZTEST(iqs9151_params, test_pinch_ratio_param_is_driver_kind_with_range_5_50_default_15) {
    const struct iqs9151_param_def *def = iqs9151_param_find("2f_pinch_ratio_x10");

    zassert_not_null(def, "2f_pinch_ratio_x10 が見つからない");
    zassert_equal(def->kind, IQS9151_PARAM_DRIVER, NULL);
    zassert_equal(def->min, 5, NULL);
    zassert_equal(def->max, 50, NULL);
    zassert_equal(def->def, 15, NULL);
}

/* 範囲内の値をsetするとgetで返り、範囲外はERANGEになる */
ZTEST(iqs9151_params, test_set_get_range_check) {
    struct iqs9151_params p;
    const struct iqs9151_param_def *def = iqs9151_param_find("1f_tap_max_ms");

    iqs9151_params_init(&p);
    zassert_equal(iqs9151_params_set(&p, def, 1000), 0, NULL);
    zassert_equal(iqs9151_params_get(&p, def), 1000, NULL);
    zassert_equal(p.f1_tap_max_ms, 1000, NULL);
    zassert_equal(iqs9151_params_set(&p, def, 1001), -ERANGE, NULL);
    zassert_equal(iqs9151_params_set(&p, def, 0), -ERANGE, NULL);
    zassert_equal(p.f1_tap_max_ms, 1000, "範囲外のときは値が変わらない");
}

/* IC系の定義にはレジスタアドレスが入っている */
ZTEST(iqs9151_params, test_ic_params_have_register_address) {
    const struct iqs9151_param_def *def = iqs9151_param_find("touch_set_threshold");

    zassert_equal(def->kind, IQS9151_PARAM_IC_U8, NULL);
    zassert_equal(def->reg, 0x11CC, NULL);
    zassert_equal(iqs9151_param_find("ati_targetcount")->reg, 0x1196, NULL);
    zassert_equal(iqs9151_param_find("ati_targetcount")->kind, IQS9151_PARAM_IC_U16, NULL);
}

/* 種別文字列はシェル用の固定文字列になる */
ZTEST(iqs9151_params, test_kind_str_returns_fixed_strings) {
    zassert_true(strcmp(iqs9151_param_kind_str(IQS9151_PARAM_IC_U8), "ic_u8") == 0, NULL);
    zassert_true(strcmp(iqs9151_param_kind_str(IQS9151_PARAM_IC_U16), "ic_u16") == 0, NULL);
    zassert_true(strcmp(iqs9151_param_kind_str(IQS9151_PARAM_DRIVER), "driver") == 0, NULL);
    zassert_true(strcmp(iqs9151_param_kind_str(IQS9151_PARAM_DRIVER_BOOL), "driver_bool") == 0,
                 NULL);
}
