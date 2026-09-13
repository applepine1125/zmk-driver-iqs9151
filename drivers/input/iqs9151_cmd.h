#ifndef ZEPHYR_DRIVERS_INPUT_IQS9151_CMD_H_
#define ZEPHYR_DRIVERS_INPUT_IQS9151_CMD_H_

#include <zephyr/device.h>

#include <stdbool.h>
#include <stddef.h>

#define IQS9151_CMD_LINE_MAX 64

typedef void (*iqs9151_cmd_out_t)(void *ctx, const char *line);

/* dev が NULL なら DEVICE_DT_GET_ANY(azoteq_iqs9151) を使う */
int iqs9151_cmd_exec_argv(const struct device *dev, size_t argc, char **argv, iqs9151_cmd_out_t out,
                          void *ctx);
int iqs9151_cmd_exec(const struct device *dev, const char *line, iqs9151_cmd_out_t out, void *ctx);

typedef void (*iqs9151_cmd_stats_hook_t)(iqs9151_cmd_out_t out, void *ctx, bool reset);

/* `tp stats` の出力に行を追加するフック(config 側の syswq 遅延・リンク情報など)。NULL で解除 */
void iqs9151_cmd_set_stats_hook(iqs9151_cmd_stats_hook_t hook);

#endif /* ZEPHYR_DRIVERS_INPUT_IQS9151_CMD_H_ */
