#include "iqs9151_params.h"

#include <zephyr/sys/util.h>

#include <stdio.h>
#include <string.h>

void iqs9151_summary_pack(const struct iqs9151_attempt_summary *s,
                          uint32_t words[IQS9151_SUMMARY_WORDS]) {
    words[0] = s->start_ms;
    words[1] = s->end_ms;
    words[2] = (uint32_t)s->contacts | ((uint32_t)s->fingers_max << 8) |
               ((uint32_t)s->mode2f << 16) | ((uint32_t)s->hold << 24);
    words[3] = MIN(s->down_ms, 65535U) | (MIN(s->gap_ms, 65535U) << 16);
    words[4] = s->move_sum;
    words[5] = s->centroid_move;
    words[6] = (uint32_t)s->dist_delta;
    words[7] = (uint32_t)s->btn_press_bits | ((uint32_t)s->btn_release_bits << 8) |
               ((uint32_t)s->wheel_count << 16);
    words[8] = (uint32_t)s->wheel_sum;
    words[9] = (uint32_t)s->rel_count | ((uint32_t)s->drops << 16);
}

void iqs9151_summary_unpack(const uint32_t words[IQS9151_SUMMARY_WORDS],
                            struct iqs9151_attempt_summary *s) {
    memset(s, 0, sizeof(*s));
    s->start_ms = words[0];
    s->end_ms = words[1];
    s->contacts = (uint8_t)(words[2] & 0xFF);
    s->fingers_max = (uint8_t)((words[2] >> 8) & 0xFF);
    s->mode2f = (uint8_t)((words[2] >> 16) & 0xFF);
    s->hold = (uint8_t)((words[2] >> 24) & 0xFF);
    s->down_ms = words[3] & 0xFFFF;
    s->gap_ms = (words[3] >> 16) & 0xFFFF;
    s->move_sum = words[4];
    s->centroid_move = words[5];
    s->dist_delta = (int32_t)words[6];
    s->btn_press_bits = (uint8_t)(words[7] & 0xFF);
    s->btn_release_bits = (uint8_t)((words[7] >> 8) & 0xFF);
    s->wheel_count = (uint16_t)((words[7] >> 16) & 0xFFFF);
    s->wheel_sum = (int32_t)words[8];
    s->rel_count = (uint16_t)(words[9] & 0xFFFF);
    s->drops = (uint16_t)((words[9] >> 16) & 0xFFFF);
}

int iqs9151_summary_format(const struct iqs9151_attempt_summary *s, char *buf, size_t len) {
    return snprintf(buf, len, "T S %u %u %u %u %u %u %u %u %d %u %u %u %u %d %u %u %u", s->start_ms,
                    s->end_ms, s->contacts, s->fingers_max, s->down_ms, s->gap_ms, s->move_sum,
                    s->centroid_move, s->dist_delta, s->mode2f, s->btn_press_bits,
                    s->btn_release_bits, s->wheel_count, s->wheel_sum, s->rel_count, s->drops,
                    s->hold);
}
