#include "dcc_video_slot.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    static struct dcc_video_slot slot;
    static const uint8_t first[] = {0,0,0,1,0x65,1,2,3};
    uint8_t output[32];
    uint32_t sequence = 0, flags = 0;
    uint64_t pts = 0;
    size_t length = 0;
    dcc_video_slot_init(&slot);
    if (dcc_video_slot_read(&slot,&sequence,output,sizeof(output),&length,&pts,&flags) != -1) return 1;
    if (dcc_video_slot_publish(&slot,first,sizeof(first),0x123456789ull,
                               DCC_VIDEO_FLAG_ACTIVE|DCC_VIDEO_FLAG_IDR)) return 2;
    if (dcc_video_slot_read(&slot,&sequence,output,sizeof(output),&length,&pts,&flags) != 1) return 3;
    if (length != sizeof(first) || memcmp(first,output,length) || pts != 0x123456789ull) return 4;
    if ((flags & (DCC_VIDEO_FLAG_ACTIVE|DCC_VIDEO_FLAG_IDR)) !=
        (DCC_VIDEO_FLAG_ACTIVE|DCC_VIDEO_FLAG_IDR)) return 5;
    if (dcc_video_slot_read(&slot,&sequence,output,sizeof(output),&length,&pts,&flags) != 0) return 6;
    slot.sequence |= 1u;
    if (dcc_video_slot_read(&slot,&sequence,output,sizeof(output),&length,&pts,&flags) != 0) return 7;
    puts("video latest-frame slot checks passed");
    return 0;
}
