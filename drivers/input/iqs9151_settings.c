#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "iqs9151_params.h"

LOG_MODULE_REGISTER(iqs9151_settings, CONFIG_INPUT_IQS9151_LOG_LEVEL);

#define IQS9151_SETTINGS_VERSION 1U
#define IQS9151_SETTINGS_KEY "params"
#define IQS9151_SETTINGS_PATH "iqs9151/" IQS9151_SETTINGS_KEY
#define IQS9151_SETTINGS_HEADER_SIZE 4U
#define IQS9151_SETTINGS_BLOB_SIZE                                                              \
    (IQS9151_SETTINGS_HEADER_SIZE + sizeof(struct iqs9151_params))

static const struct device *iqs9151_settings_dev = DEVICE_DT_GET_ANY(azoteq_iqs9151);
static bool iqs9151_settings_loaded_flag;

int iqs9151_settings_encode(const struct iqs9151_params *p, uint8_t *buf, size_t len) {
    const size_t count = iqs9151_param_count();
    const size_t needed = IQS9151_SETTINGS_HEADER_SIZE + 4U * count;

    if (len < needed) {
        return -EINVAL;
    }
    sys_put_le16(IQS9151_SETTINGS_VERSION, &buf[0]);
    sys_put_le16((uint16_t)count, &buf[2]);
    for (size_t i = 0; i < count; i++) {
        sys_put_le32((uint32_t)iqs9151_params_get(p, iqs9151_param_def_at(i)),
                     &buf[IQS9151_SETTINGS_HEADER_SIZE + 4U * i]);
    }
    return (int)needed;
}

int iqs9151_settings_decode(const uint8_t *buf, size_t len, struct iqs9151_params *p) {
    const size_t count = iqs9151_param_count();
    struct iqs9151_params tmp;

    if (len < IQS9151_SETTINGS_HEADER_SIZE) {
        return -EINVAL;
    }
    if (sys_get_le16(&buf[0]) != IQS9151_SETTINGS_VERSION ||
        sys_get_le16(&buf[2]) != count ||
        len != IQS9151_SETTINGS_HEADER_SIZE + 4U * count) {
        return -EINVAL;
    }
    for (size_t i = 0; i < count; i++) {
        *(int32_t *)((uint8_t *)&tmp + iqs9151_param_def_at(i)->offset) =
            (int32_t)sys_get_le32(&buf[IQS9151_SETTINGS_HEADER_SIZE + 4U * i]);
    }
    *p = tmp;
    return 0;
}

static void iqs9151_settings_apply(const struct device *dev, const struct iqs9151_params *p) {
    for (size_t i = 0; i < iqs9151_param_count(); i++) {
        const struct iqs9151_param_def *def = iqs9151_param_def_at(i);
        const int32_t value = iqs9151_params_get(p, def);
        const int ret = iqs9151_dev_param_set(dev, def->name, value);

        if (ret != 0) {
            LOG_WRN("saved %s=%d skipped (%d)", def->name, value, ret);
        }
    }
}

static int iqs9151_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                void *cb_arg) {
    const char *next;
    uint8_t buf[IQS9151_SETTINGS_BLOB_SIZE];
    struct iqs9151_params params;
    ssize_t read_len;

    if (!settings_name_steq(name, IQS9151_SETTINGS_KEY, &next) || next != NULL) {
        return -ENOENT;
    }
    if (len > sizeof(buf)) {
        LOG_WRN("saved params ignored: len %u > %u", (unsigned int)len,
                (unsigned int)sizeof(buf));
        return 0;
    }
    read_len = read_cb(cb_arg, buf, len);
    if (read_len < 0 || (size_t)read_len != len) {
        LOG_WRN("saved params read failed (%d)", (int)read_len);
        return 0;
    }
    if (iqs9151_settings_decode(buf, len, &params) != 0) {
        LOG_WRN("saved params ignored: version/count mismatch");
        return 0;
    }
    if (iqs9151_settings_dev == NULL) {
        LOG_WRN("saved params ignored: no device");
        return 0;
    }
    iqs9151_settings_apply(iqs9151_settings_dev, &params);
    iqs9151_settings_loaded_flag = true;
    LOG_INF("saved params applied");
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(iqs9151, "iqs9151", NULL, iqs9151_settings_set, NULL, NULL);

int iqs9151_settings_save(const struct device *dev) {
    uint8_t buf[IQS9151_SETTINGS_BLOB_SIZE];
    struct iqs9151_params params;
    int len;
    int ret;

    iqs9151_params_init(&params);
    for (size_t i = 0; i < iqs9151_param_count(); i++) {
        const struct iqs9151_param_def *def = iqs9151_param_def_at(i);
        int32_t value = 0;

        ret = iqs9151_dev_param_get(dev, def->name, &value);
        if (ret != 0) {
            return ret;
        }
        ret = iqs9151_params_set(&params, def, value);
        if (ret != 0) {
            return ret;
        }
    }
    len = iqs9151_settings_encode(&params, buf, sizeof(buf));
    if (len < 0) {
        return len;
    }
    ret = settings_save_one(IQS9151_SETTINGS_PATH, buf, (size_t)len);
    if (ret != 0) {
        return ret;
    }
    iqs9151_settings_loaded_flag = true;
    return 0;
}

int iqs9151_settings_clear(void) {
    const int ret = settings_delete(IQS9151_SETTINGS_PATH);

    if (ret != 0) {
        return ret;
    }
    iqs9151_settings_loaded_flag = false;
    return 0;
}

bool iqs9151_settings_loaded(void) {
    return iqs9151_settings_loaded_flag;
}

#ifdef CONFIG_INPUT_IQS9151_TEST
void iqs9151_settings_test_set_device(const struct device *dev) {
    iqs9151_settings_dev = dev;
}

void iqs9151_settings_test_reset_loaded(void) {
    iqs9151_settings_loaded_flag = false;
}
#endif
