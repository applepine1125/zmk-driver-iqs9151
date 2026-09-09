#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "iqs9151_cmd.h"

static void shell_out(void *ctx, const char *line) {
    const struct shell *sh = ctx;

    shell_print(sh, "%s", line);
}

static int cmd_tp_info(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_list(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_get(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_set(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_reset(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_save(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_reati(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_trace(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
}

static int cmd_tp_summary(const struct shell *sh, size_t argc, char **argv) {
    return iqs9151_cmd_exec_argv(NULL, argc, argv, shell_out, (void *)sh);
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
    SHELL_CMD_ARG(summary, NULL, "summary on|off (T S line per attempt)", cmd_tp_summary, 2, 0),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(tp, &sub_tp, "IQS9151 trackpad tuning", NULL);
