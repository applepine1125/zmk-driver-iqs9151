#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "iqs9151_params.h"

static const struct device *tp_device(const struct shell *sh) {
    const struct device *dev = DEVICE_DT_GET_ANY(azoteq_iqs9151);

    if (dev == NULL || !device_is_ready(dev)) {
        shell_print(sh, "ERR device not ready");
        return NULL;
    }
    return dev;
}

static int parse_i32(const char *text, int32_t *out) {
    char *end = NULL;
    long v = strtol(text, &end, 10);

    if (end == text || *end != '\0') {
        return -EINVAL;
    }
    *out = (int32_t)v;
    return 0;
}

static int cmd_tp_info(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    /* Non-split builds (CONFIG_ZMK_SPLIT_ROLE_CENTRAL unset) also report "peripheral". */
    shell_print(sh, "side=%s uptime_ms=%u params=%u saved=%s",
                IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) ? "central" : "peripheral",
                (uint32_t)k_uptime_get(), (unsigned int)iqs9151_param_count(),
                iqs9151_settings_loaded() ? "yes" : "no");
    return 0;
}

static int cmd_tp_list(const struct shell *sh, size_t argc, char **argv) {
    const struct device *dev = tp_device(sh);

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    if (dev == NULL) {
        return -ENODEV;
    }
    for (size_t i = 0; i < iqs9151_param_count(); i++) {
        const struct iqs9151_param_def *def = iqs9151_param_def_at(i);
        int32_t value = 0;

        (void)iqs9151_dev_param_get(dev, def->name, &value);
        shell_print(sh, "%s %d %d %d %s %d", def->name, value, def->min, def->max,
                    iqs9151_param_kind_str(def->kind), def->def);
    }
    return 0;
}

static int cmd_tp_get(const struct shell *sh, size_t argc, char **argv) {
    const struct device *dev = tp_device(sh);
    int32_t value = 0;

    ARG_UNUSED(argc);
    if (dev == NULL) {
        return -ENODEV;
    }
    if (iqs9151_dev_param_get(dev, argv[1], &value) != 0) {
        shell_print(sh, "ERR unknown param %s", argv[1]);
        return -ENOENT;
    }
    shell_print(sh, "%s=%d", argv[1], value);
    return 0;
}

static int cmd_tp_set(const struct shell *sh, size_t argc, char **argv) {
    const struct device *dev = tp_device(sh);
    int32_t value = 0;
    int ret;

    ARG_UNUSED(argc);
    if (dev == NULL) {
        return -ENODEV;
    }
    if (parse_i32(argv[2], &value) != 0) {
        shell_print(sh, "ERR invalid value %s", argv[2]);
        return -EINVAL;
    }
    ret = iqs9151_dev_param_set(dev, argv[1], value);
    if (ret == -ENOENT) {
        shell_print(sh, "ERR unknown param %s", argv[1]);
        return ret;
    }
    if (ret == -ERANGE) {
        const struct iqs9151_param_def *def = iqs9151_param_find(argv[1]);

        shell_print(sh, "ERR out of range %d..%d", def->min, def->max);
        return ret;
    }
    shell_print(sh, "OK %s=%d", argv[1], value);
    return 0;
}

static int cmd_tp_reset(const struct shell *sh, size_t argc, char **argv) {
    const struct device *dev = tp_device(sh);

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    if (dev == NULL) {
        return -ENODEV;
    }
    (void)iqs9151_dev_param_reset(dev);
    (void)iqs9151_settings_clear();
    shell_print(sh, "OK reset");
    return 0;
}

static int cmd_tp_save(const struct shell *sh, size_t argc, char **argv) {
    const struct device *dev = tp_device(sh);
    int ret;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    if (dev == NULL) {
        return -ENODEV;
    }
    ret = iqs9151_settings_save(dev);
    if (ret != 0) {
        shell_print(sh, "ERR %d", ret);
        return ret;
    }
    shell_print(sh, "OK saved");
    return 0;
}

static int cmd_tp_reati(const struct shell *sh, size_t argc, char **argv) {
    const struct device *dev = tp_device(sh);

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    if (dev == NULL) {
        return -ENODEV;
    }
    (void)iqs9151_dev_request_reati(dev);
    shell_print(sh, "OK reati");
    return 0;
}

static int cmd_tp_trace(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    if (strcmp(argv[1], "on") == 0) {
        iqs9151_dev_trace_enable(true);
    } else if (strcmp(argv[1], "off") == 0) {
        iqs9151_dev_trace_enable(false);
    } else {
        shell_print(sh, "ERR expected on|off");
        return -EINVAL;
    }
    shell_print(sh, "OK trace=%s", argv[1]);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_tp,
    SHELL_CMD_ARG(info, NULL, "Show side and uptime", cmd_tp_info, 1, 0),
    SHELL_CMD_ARG(list, NULL, "List params: name value min max kind default", cmd_tp_list, 1, 0),
    SHELL_CMD_ARG(get, NULL, "get <name>", cmd_tp_get, 2, 0),
    SHELL_CMD_ARG(set, NULL, "set <name> <value>", cmd_tp_set, 3, 0),
    SHELL_CMD_ARG(reset, NULL, "Reset all params to Kconfig defaults and delete saved values",
                  cmd_tp_reset, 1, 0),
    SHELL_CMD_ARG(save, NULL, "Save current params to settings", cmd_tp_save, 1, 0),
    SHELL_CMD_ARG(reati, NULL, "Request Re-ATI on next frame", cmd_tp_reati, 1, 0),
    SHELL_CMD_ARG(trace, NULL, "trace on|off", cmd_tp_trace, 2, 0),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(tp, &sub_tp, "IQS9151 trackpad tuning", NULL);
