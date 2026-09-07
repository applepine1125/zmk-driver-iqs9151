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
    return *iqs9151_param_slot((struct iqs9151_params *)p, def);
}
