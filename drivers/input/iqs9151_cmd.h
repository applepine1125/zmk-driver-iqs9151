#ifndef ZEPHYR_DRIVERS_INPUT_IQS9151_CMD_H_
#define ZEPHYR_DRIVERS_INPUT_IQS9151_CMD_H_

#include <zephyr/device.h>

#include <stddef.h>

#define IQS9151_CMD_LINE_MAX 64

typedef void (*iqs9151_cmd_out_t)(void *ctx, const char *line);

/* dev が NULL なら DEVICE_DT_GET_ANY(azoteq_iqs9151) を使う */
int iqs9151_cmd_exec_argv(const struct device *dev, size_t argc, char **argv, iqs9151_cmd_out_t out,
                          void *ctx);
int iqs9151_cmd_exec(const struct device *dev, const char *line, iqs9151_cmd_out_t out, void *ctx);

#endif /* ZEPHYR_DRIVERS_INPUT_IQS9151_CMD_H_ */
