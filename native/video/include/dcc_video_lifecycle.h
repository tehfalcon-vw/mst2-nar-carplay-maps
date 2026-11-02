#ifndef DCC_VIDEO_LIFECYCLE_H
#define DCC_VIDEO_LIFECYCLE_H

#include <stdint.h>

enum dcc_video_state {
    DCC_VIDEO_INACTIVE = 0,
    DCC_VIDEO_WAIT_IDR = 1,
    DCC_VIDEO_ACTIVE = 2
};

struct dcc_video_lifecycle {
    enum dcc_video_state state;
    uint64_t last_frame_ms;
    uint32_t stale_after_ms;
};

void dcc_video_lifecycle_init(struct dcc_video_lifecycle *life,
                              uint32_t stale_after_ms);
void dcc_video_lifecycle_config(struct dcc_video_lifecycle *life);
int dcc_video_lifecycle_frame(struct dcc_video_lifecycle *life,
                              int is_idr, uint64_t now_ms);
int dcc_video_lifecycle_poll(struct dcc_video_lifecycle *life, uint64_t now_ms);
void dcc_video_lifecycle_stop(struct dcc_video_lifecycle *life);

#endif
