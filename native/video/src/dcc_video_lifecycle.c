#include "dcc_video_lifecycle.h"

void dcc_video_lifecycle_init(struct dcc_video_lifecycle *life,
                              uint32_t stale_after_ms)
{
    if (!life) return;
    life->state = DCC_VIDEO_INACTIVE;
    life->last_frame_ms = 0;
    life->stale_after_ms = stale_after_ms;
}

void dcc_video_lifecycle_config(struct dcc_video_lifecycle *life)
{
    if (!life) return;
    life->state = DCC_VIDEO_WAIT_IDR;
    life->last_frame_ms = 0;
}

int dcc_video_lifecycle_frame(struct dcc_video_lifecycle *life,
                              int is_idr, uint64_t now_ms)
{
    if (!life || life->state == DCC_VIDEO_INACTIVE) return 0;
    if (life->state == DCC_VIDEO_WAIT_IDR && !is_idr) return 0;
    life->state = DCC_VIDEO_ACTIVE;
    life->last_frame_ms = now_ms;
    return 1;
}

int dcc_video_lifecycle_poll(struct dcc_video_lifecycle *life, uint64_t now_ms)
{
    if (!life || life->state != DCC_VIDEO_ACTIVE || !life->stale_after_ms) return 0;
    if (now_ms < life->last_frame_ms ||
        now_ms - life->last_frame_ms <= life->stale_after_ms) return 0;
    life->state = DCC_VIDEO_WAIT_IDR;
    life->last_frame_ms = 0;
    return 1;
}

void dcc_video_lifecycle_stop(struct dcc_video_lifecycle *life)
{
    if (!life) return;
    life->state = DCC_VIDEO_INACTIVE;
    life->last_frame_ms = 0;
}
