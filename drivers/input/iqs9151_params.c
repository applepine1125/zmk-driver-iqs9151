#include "iqs9151_params.h"

#include <errno.h>
#include <string.h>

static const struct iqs9151_param_def iqs9151_param_table[] = {
#define IQS9151_PARAM_ENTRY(field, pname, pdef, pmin, pmax, pkind, preg)                      \
    {                                                                                       \
        .name = pname,                                                                      \
        .offset = offsetof(struct iqs9151_params, field),                                   \
        .min = pmin,                                                                        \
        .max = pmax,                                                                        \
        .def = pdef,                                                                        \
        .kind = pkind,                                                                      \
        .reg = preg,                                                                        \
    },
    IQS9151_PARAM_LIST(IQS9151_PARAM_ENTRY)
#undef IQS9151_PARAM_ENTRY
};

size_t iqs9151_param_count(void) {
    return ARRAY_SIZE(iqs9151_param_table);
}

const struct iqs9151_param_def *iqs9151_param_def_at(size_t idx) {
    if (idx >= ARRAY_SIZE(iqs9151_param_table)) {
        return NULL;
    }
    return &iqs9151_param_table[idx];
}

const struct iqs9151_param_def *iqs9151_param_find(const char *name) {
    for (size_t i = 0; i < ARRAY_SIZE(iqs9151_param_table); i++) {
        if (strcmp(iqs9151_param_table[i].name, name) == 0) {
            return &iqs9151_param_table[i];
        }
    }
    return NULL;
}

bool iqs9151_param_is_ic(const struct iqs9151_param_def *def) {
    return def->kind == IQS9151_PARAM_IC_U8 || def->kind == IQS9151_PARAM_IC_U16;
}

const char *iqs9151_param_kind_str(enum iqs9151_param_kind kind) {
    switch (kind) {
    case IQS9151_PARAM_IC_U8:
        return "ic_u8";
    case IQS9151_PARAM_IC_U16:
        return "ic_u16";
    case IQS9151_PARAM_DRIVER:
        return "driver";
    case IQS9151_PARAM_DRIVER_BOOL:
        return "driver_bool";
    default:
        return "unknown";
    }
}

static int32_t *iqs9151_param_slot(struct iqs9151_params *p,
                                   const struct iqs9151_param_def *def) {
    return (int32_t *)((uint8_t *)p + def->offset);
}

static const int32_t *iqs9151_param_slot_const(const struct iqs9151_params *p,
                                               const struct iqs9151_param_def *def) {
    return (const int32_t *)((const uint8_t *)p + def->offset);
}

void iqs9151_params_init(struct iqs9151_params *p) {
    for (size_t i = 0; i < ARRAY_SIZE(iqs9151_param_table); i++) {
        *iqs9151_param_slot(p, &iqs9151_param_table[i]) = iqs9151_param_table[i].def;
    }
}

int iqs9151_params_set(struct iqs9151_params *p, const struct iqs9151_param_def *def,
                       int32_t value) {
    if (value < def->min || value > def->max) {
        return -ERANGE;
    }
    *iqs9151_param_slot(p, def) = value;
    return 0;
}

int32_t iqs9151_params_get(const struct iqs9151_params *p,
                           const struct iqs9151_param_def *def) {
    return *iqs9151_param_slot_const(p, def);
}

/*
 * tp list に出さないパラメータ。挙動や保存形式(位置ベースのブロブ)は変えずに、
 * 調整ツールからは触れないようにする。get/set は名前で引けるので従来どおり使える。
 * 省電力・ALP・ATI 校正・慣性の細かい判定・2/3 本指タップドラッグは、一度決めたら
 * 動かさない値なので一覧から外している。
 */
static const char *const iqs9151_hidden_param_names[] = {
    "active_mode_sampling_period_ms",
    "idle_touch_mode_sampling_period_ms",
    "idle_mode_sampling_period_ms",
    "lp1_mode_sampling_period_ms",
    "lp2_mode_sampling_period_ms",
    "active_mode_timeout_ms",
    "idle_touch_mode_timeout_s",
    "idle_mode_timeout_s",
    "lp1_mode_timeout_s",
    "alp_set_debounce",
    "alp_clear_debounce",
    "ati_targetcount",
    "cursor_inertia_recent_window_ms",
    "cursor_inertia_stale_gap_ms",
    "cursor_inertia_min_samples",
    "scroll_inertia_recent_window_ms",
    "scroll_inertia_stale_gap_ms",
    "scroll_inertia_min_samples",
    "2f_presshold_enable",
    "2f_tapdrag_gap_max_ms",
    "3f_presshold_enable",
    "3f_tapdrag_gap_max_ms",
};

bool iqs9151_param_is_hidden(const struct iqs9151_param_def *def) {
    for (size_t i = 0; i < ARRAY_SIZE(iqs9151_hidden_param_names); i++) {
        if (strcmp(iqs9151_hidden_param_names[i], def->name) == 0) {
            return true;
        }
    }
    return false;
}

size_t iqs9151_param_hidden_count(void) {
    return ARRAY_SIZE(iqs9151_hidden_param_names);
}
