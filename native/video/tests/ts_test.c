#include "dcc_ts.h"
#include <stdio.h>
#include <string.h>

struct capture { uint8_t bytes[188u * 16u]; size_t used; };

static int collect(const uint8_t packet[188], void *opaque)
{
    struct capture *c = (struct capture *)opaque;
    if (c->used + 188u > sizeof(c->bytes)) return -1;
    memcpy(c->bytes + c->used, packet, 188u); c->used += 188u;
    return 0;
}

static unsigned pid(const uint8_t *p) { return ((unsigned)(p[1] & 0x1fu) << 8) | p[2]; }

int main(void)
{
    static const uint8_t au[] = {0,0,0,1,0x67,0x42,0,0x1f,0,0,0,1,0x68,0xce,6,0xe2,
                                  0,0,0,1,0x65,0x88,0x84,0x21,0xa0};
    struct dcc_ts_mux mux;
    struct capture c;
    size_t packets, i, video = 0;
    memset(&c, 0, sizeof(c)); dcc_ts_init(&mux);
    if (dcc_ts_push_annexb(&mux, au, sizeof(au), 90000u, collect, &c)) return 1;
    packets = c.used / 188u;
    if (packets < 3u || pid(c.bytes) != 0 || pid(c.bytes + 188u) != 0x1000u) return 2;
    if (c.bytes[5] != 0x00 || c.bytes[188u + 5u] != 0x02) return 3;
    for (i = 0; i < packets; ++i) {
        const uint8_t *p = c.bytes + i * 188u;
        if (p[0] != 0x47) return 4;
        if (pid(p) == DCC_TS_VIDEO_PID) {
            size_t off = 4u;
            ++video;
            if (p[3] & 0x20u) off += 1u + p[4];
            if ((p[1] & 0x40u) && (!(p[3] & 0x20u) || p[4] < 7u ||
                !(p[5] & 0x10u) || memcmp(p + off, "\x00\x00\x01\xe0", 4))) return 5;
        }
    }
    if (!video || mux.access_units != 1u) return 6;
    memset(&c, 0, sizeof(c));
    if (dcc_ts_push_annexb(&mux, au, sizeof(au), 93000u, collect, &c)) return 7;
    if (pid(c.bytes) != DCC_TS_VIDEO_PID || (c.bytes[3] & 0x0fu) == 0u) return 8;
    puts("MPEG-TS packetizer checks passed");
    return 0;
}
