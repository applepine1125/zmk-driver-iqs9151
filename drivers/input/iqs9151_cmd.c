#include "iqs9151_cmd.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iqs9151_params.h"

#define IQS9151_CMD_MAX_TOKENS 4

static void cmd_out(iqs9151_cmd_out_t out, void *ctx, const char *fmt, ...) {
    char buf[96];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    out(ctx, buf);
}

static const struct device *cmd_device(const struct device *dev, iqs9151_cmd_out_t out, void *ctx) {
    const struct device *device = dev != NULL ? dev : DEVICE_DT_GET_ANY(azoteq_iqs9151);

    if (device == NULL || !device_is_ready(device)) {
        cmd_out(out, ctx, "ERR device not ready");
        return NULL;
    }
    return device;
}

static int parse_i32(const char *text, int32_t *out_value) {
    char *end = NULL;
    long v = strtol(text, &end, 10);

    if (end == text || *end != '\0') {
        return -EINVAL;
    }
    *out_value = (int32_t)v;
    return 0;
}

static int cmd_info(size_t argc, iqs9151_cmd_out_t out, void *ctx) {
    if (argc != 1) {
        cmd_out(out, ctx, "ERR usage: info");
        return -EINVAL;
    }
    /* Non-split builds (CONFIG_ZMK_SPLIT_ROLE_CENTRAL unset) also report "peripheral". */
    cmd_out(out, ctx, "side=%s uptime_ms=%u params=%u saved=%s",
            IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) ? "central" : "peripheral",
            (uint32_t)k_uptime_get(), (unsigned int)iqs9151_param_count(),
            iqs9151_settings_loaded() ? "yes" : "no");
    return 0;
}

static int cmd_list(const struct device *dev, size_t argc, iqs9151_cmd_out_t out, void *ctx) {
    const struct device *device;

    if (argc != 1) {
        cmd_out(out, ctx, "ERR usage: list");
        return -EINVAL;
    }
    device = cmd_device(dev, out, ctx);
    if (device == NULL) {
        return -ENODEV;
    }
    for (size_t i = 0; i < iqs9151_param_count(); i++) {
        const struct iqs9151_param_def *def = iqs9151_param_def_at(i);
        int32_t value = 0;

        (void)iqs9151_dev_param_get(device, def->name, &value);
        cmd_out(out, ctx, "%s %d %d %d %s %d", def->name, value, def->min, def->max,
                iqs9151_param_kind_str(def->kind), def->def);
    }
    return 0;
}

static int cmd_get(const struct device *dev, size_t argc, char **argv, iqs9151_cmd_out_t out,
                   void *ctx) {
    const struct device *device;
    int32_t value = 0;

    if (argc != 2) {
        cmd_out(out, ctx, "ERR usage: get <name>");
        return -EINVAL;
    }
    device = cmd_device(dev, out, ctx);
    if (device == NULL) {
        return -ENODEV;
    }
    if (iqs9151_dev_param_get(device, argv[1], &value) != 0) {
        cmd_out(out, ctx, "ERR unknown param %s", argv[1]);
        return -ENOENT;
    }
    cmd_out(out, ctx, "%s=%d", argv[1], value);
    return 0;
}

static int cmd_set(const struct device *dev, size_t argc, char **argv, iqs9151_cmd_out_t out,
                   void *ctx) {
    const struct device *device;
    int32_t value = 0;
    int ret;

    if (argc != 3) {
        cmd_out(out, ctx, "ERR usage: set <name> <value>");
        return -EINVAL;
    }
    device = cmd_device(dev, out, ctx);
    if (device == NULL) {
        return -ENODEV;
    }
    if (parse_i32(argv[2], &value) != 0) {
        cmd_out(out, ctx, "ERR invalid value %s", argv[2]);
        return -EINVAL;
    }
    ret = iqs9151_dev_param_set(device, argv[1], value);
    if (ret == -ENOENT) {
        cmd_out(out, ctx, "ERR unknown param %s", argv[1]);
        return ret;
    }
    if (ret == -ERANGE) {
        const struct iqs9151_param_def *def = iqs9151_param_find(argv[1]);

        cmd_out(out, ctx, "ERR out of range %d..%d", def->min, def->max);
        return ret;
    }
    cmd_out(out, ctx, "OK %s=%d", argv[1], value);
    return 0;
}

static int cmd_reset(const struct device *dev, size_t argc, iqs9151_cmd_out_t out, void *ctx) {
    const struct device *device;

    if (argc != 1) {
        cmd_out(out, ctx, "ERR usage: reset");
        return -EINVAL;
    }
    device = cmd_device(dev, out, ctx);
    if (device == NULL) {
        return -ENODEV;
    }
    (void)iqs9151_dev_param_reset(device);
    (void)iqs9151_settings_clear();
    cmd_out(out, ctx, "OK reset");
    return 0;
}

static int cmd_save(const struct device *dev, size_t argc, iqs9151_cmd_out_t out, void *ctx) {
    const struct device *device;
    int ret;

    if (argc != 1) {
        cmd_out(out, ctx, "ERR usage: save");
        return -EINVAL;
    }
    device = cmd_device(dev, out, ctx);
    if (device == NULL) {
        return -ENODEV;
    }
    ret = iqs9151_settings_save(device);
    if (ret != 0) {
        cmd_out(out, ctx, "ERR %d", ret);
        return ret;
    }
    cmd_out(out, ctx, "OK saved");
    return 0;
}

static int cmd_reati(const struct device *dev, size_t argc, iqs9151_cmd_out_t out, void *ctx) {
    const struct device *device;

    if (argc != 1) {
        cmd_out(out, ctx, "ERR usage: reati");
        return -EINVAL;
    }
    device = cmd_device(dev, out, ctx);
    if (device == NULL) {
        return -ENODEV;
    }
    (void)iqs9151_dev_request_reati(device);
    cmd_out(out, ctx, "OK reati");
    return 0;
}

static int cmd_trace(size_t argc, char **argv, iqs9151_cmd_out_t out, void *ctx) {
    if (argc != 2) {
        cmd_out(out, ctx, "ERR usage: trace on|off");
        return -EINVAL;
    }
    if (strcmp(argv[1], "on") == 0) {
        iqs9151_dev_trace_enable(true);
    } else if (strcmp(argv[1], "off") == 0) {
        iqs9151_dev_trace_enable(false);
    } else {
        cmd_out(out, ctx, "ERR expected on|off");
        return -EINVAL;
    }
    cmd_out(out, ctx, "OK trace=%s", argv[1]);
    return 0;
}

static int cmd_summary(size_t argc, char **argv, iqs9151_cmd_out_t out, void *ctx) {
    if (argc != 2) {
        cmd_out(out, ctx, "ERR usage: summary on|off");
        return -EINVAL;
    }
    if (strcmp(argv[1], "on") == 0) {
        iqs9151_dev_summary_enable(true);
    } else if (strcmp(argv[1], "off") == 0) {
        iqs9151_dev_summary_enable(false);
    } else {
        cmd_out(out, ctx, "ERR expected on|off");
        return -EINVAL;
    }
    cmd_out(out, ctx, "OK summary=%s", argv[1]);
    return 0;
}

static int cmd_live(size_t argc, char **argv, iqs9151_cmd_out_t out, void *ctx) {
    if (argc < 2 || argc > 3) {
        cmd_out(out, ctx, "ERR usage: live on|off [hz]");
        return -EINVAL;
    }
    if (strcmp(argv[1], "on") == 0) {
        int32_t hz = IQS9151_LIVE_HZ_DEFAULT;
        int ret;

        if (argc == 3 && parse_i32(argv[2], &hz) != 0) {
            cmd_out(out, ctx, "ERR hz 1..100");
            return -ERANGE;
        }
        ret = iqs9151_dev_live_enable(true, (uint16_t)hz);
        if (ret != 0) {
            cmd_out(out, ctx, "ERR hz 1..100");
            return ret;
        }
        cmd_out(out, ctx, "OK live=on hz=%u", (unsigned int)iqs9151_dev_live_hz());
        return 0;
    }
    if (strcmp(argv[1], "off") == 0) {
        if (argc != 2) {
            cmd_out(out, ctx, "ERR usage: live on|off [hz]");
            return -EINVAL;
        }
        (void)iqs9151_dev_live_enable(false, iqs9151_dev_live_hz());
        cmd_out(out, ctx, "OK live=off");
        return 0;
    }
    cmd_out(out, ctx, "ERR expected on|off");
    return -EINVAL;
}

int iqs9151_cmd_exec_argv(const struct device *dev, size_t argc, char **argv, iqs9151_cmd_out_t out,
                          void *ctx) {
    const char *sub;

    if (argc == 0 || argv == NULL || argv[0] == NULL) {
        cmd_out(out, ctx, "ERR empty");
        return -EINVAL;
    }
    sub = argv[0];

    if (strcmp(sub, "info") == 0) {
        return cmd_info(argc, out, ctx);
    }
    if (strcmp(sub, "list") == 0) {
        return cmd_list(dev, argc, out, ctx);
    }
    if (strcmp(sub, "get") == 0) {
        return cmd_get(dev, argc, argv, out, ctx);
    }
    if (strcmp(sub, "set") == 0) {
        return cmd_set(dev, argc, argv, out, ctx);
    }
    if (strcmp(sub, "reset") == 0) {
        return cmd_reset(dev, argc, out, ctx);
    }
    if (strcmp(sub, "save") == 0) {
        return cmd_save(dev, argc, out, ctx);
    }
    if (strcmp(sub, "reati") == 0) {
        return cmd_reati(dev, argc, out, ctx);
    }
    if (strcmp(sub, "trace") == 0) {
        return cmd_trace(argc, argv, out, ctx);
    }
    if (strcmp(sub, "summary") == 0) {
        return cmd_summary(argc, argv, out, ctx);
    }
    if (strcmp(sub, "live") == 0) {
        return cmd_live(argc, argv, out, ctx);
    }
    cmd_out(out, ctx, "ERR unknown command %s", sub);
    return -ENOENT;
}

/* buf を空白/タブで最大 IQS9151_CMD_MAX_TOKENS 個までに分割する。超過分があれば -E2BIG */
static int tokenize_line(char *buf, char **argv) {
    int argc = 0;
    char *p = buf;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (argc >= IQS9151_CMD_MAX_TOKENS) {
            return -E2BIG;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    return argc;
}

int iqs9151_cmd_exec(const struct device *dev, const char *line, iqs9151_cmd_out_t out, void *ctx) {
    char buf[IQS9151_CMD_LINE_MAX];
    char *argv[IQS9151_CMD_MAX_TOKENS];
    size_t len;
    size_t start = 0;
    int argc;

    if (line == NULL) {
        cmd_out(out, ctx, "ERR empty");
        return -EINVAL;
    }
    len = strlen(line);
    if (len >= sizeof(buf)) {
        cmd_out(out, ctx, "ERR line too long");
        return -E2BIG;
    }
    memcpy(buf, line, len + 1);

    argc = tokenize_line(buf, argv);
    if (argc == -E2BIG) {
        cmd_out(out, ctx, "ERR too many args");
        return -E2BIG;
    }
    if (argc == 0) {
        cmd_out(out, ctx, "ERR empty");
        return -EINVAL;
    }
    if (strcmp(argv[0], "tp") == 0) {
        start = 1;
    }
    if ((size_t)argc - start == 0) {
        cmd_out(out, ctx, "ERR empty");
        return -EINVAL;
    }
    return iqs9151_cmd_exec_argv(dev, (size_t)argc - start, &argv[start], out, ctx);
}
