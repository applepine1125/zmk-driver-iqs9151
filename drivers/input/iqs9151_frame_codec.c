#include "iqs9151_params.h"

#include <stdio.h>

int iqs9151_frame_format(const struct iqs9151_frame_info *f, char *buf, size_t len) {
    return snprintf(buf, len, "T F %u %u %d %d %u %u %u %u %04x %u %u %u", f->ms, f->fingers,
                    f->rel_x, f->rel_y, f->f1x, f->f1y, f->f2x, f->f2y, f->flags, f->hold,
                    f->mode2f, f->pending);
}
