#include "dcc_video_lifecycle.h"
#include <stdio.h>

int main(void)
{
    struct dcc_video_lifecycle life;
    dcc_video_lifecycle_init(&life, 2000);
    if (dcc_video_lifecycle_frame(&life, 1, 1)) return 1;
    dcc_video_lifecycle_config(&life);
    if (life.state != DCC_VIDEO_WAIT_IDR) return 2;
    if (dcc_video_lifecycle_frame(&life, 0, 100)) return 3;
    if (!dcc_video_lifecycle_frame(&life, 1, 200)) return 4;
    if (life.state != DCC_VIDEO_ACTIVE) return 5;
    if (dcc_video_lifecycle_poll(&life, 2200)) return 6;
    if (!dcc_video_lifecycle_poll(&life, 2201)) return 7;
    if (life.state != DCC_VIDEO_WAIT_IDR) return 8;
    if (dcc_video_lifecycle_frame(&life, 0, 2300)) return 9;
    if (!dcc_video_lifecycle_frame(&life, 1, 2400)) return 10;
    dcc_video_lifecycle_config(&life);
    if (life.state != DCC_VIDEO_WAIT_IDR) return 11;
    dcc_video_lifecycle_stop(&life);
    if (life.state != DCC_VIDEO_INACTIVE) return 12;
    puts("video lifecycle and stock-fallback checks passed");
    return 0;
}
