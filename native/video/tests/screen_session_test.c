#include "dcc_screen_session.h"
#include <stdio.h>
#include <string.h>

struct decrypt_state { unsigned calls; };
static int decrypt(uint8_t *bytes, size_t length, void *opaque)
{
    struct decrypt_state *state = (struct decrypt_state *)opaque;
    size_t i;
    ++state->calls;
    for (i = 0; i < length; ++i) bytes[i] ^= 0xa5u;
    return 0;
}

static void header(struct dcc_screen_header *h, uint8_t opcode, size_t size)
{
    memset(h, 0, sizeof(*h));
    h->opcode = opcode;
    h->body_size = (uint32_t)size;
}

int main(void)
{
    static const uint8_t config[] = {
        1,0x42,0,0x1f,0xff,0xe1,0,4,0x67,0x42,0,0x1f,
        1,0,4,0x68,0xce,6,0xe2
    };
    static const uint8_t idr_plain[] = {0,0,0,3,0x65,0x88,0x84};
    static const uint8_t p_plain[] = {0,0,0,2,0x41,0x9a};
    struct dcc_video_slot slot;
    struct dcc_screen_session session;
    struct dcc_screen_header h;
    struct decrypt_state crypto;
    uint8_t annexb[256], body[64], readback[256];
    uint32_t sequence = 0, flags = 0;
    uint64_t pts = 0;
    size_t length = 0, i;
    memset(&crypto, 0, sizeof(crypto));
    dcc_video_slot_init(&slot);
    dcc_screen_session_init(&session, &slot, annexb, sizeof(annexb),
                            decrypt, &crypto, 2000);
    memcpy(body, p_plain, sizeof(p_plain));
    for (i = 0; i < sizeof(p_plain); ++i) body[i] ^= 0xa5u;
    header(&h, DCC_SCREEN_VIDEO_FRAME, sizeof(p_plain));
    if (dcc_screen_session_packet(&session, &h, body, sizeof(p_plain), 10, 900) != 0) return 1;
    if (crypto.calls != 1) return 2;
    memcpy(body, config, sizeof(config));
    header(&h, DCC_SCREEN_VIDEO_CONFIG, sizeof(config));
    if (dcc_screen_session_packet(&session, &h, body, sizeof(config), 20, 1800)) return 3;
    memcpy(body, p_plain, sizeof(p_plain));
    for (i = 0; i < sizeof(p_plain); ++i) body[i] ^= 0xa5u;
    header(&h, DCC_SCREEN_VIDEO_FRAME, sizeof(p_plain));
    if (dcc_screen_session_packet(&session, &h, body, sizeof(p_plain), 30, 2700) != 0) return 4;
    memcpy(body, idr_plain, sizeof(idr_plain));
    for (i = 0; i < sizeof(idr_plain); ++i) body[i] ^= 0xa5u;
    header(&h, DCC_SCREEN_VIDEO_FRAME, sizeof(idr_plain));
    if (dcc_screen_session_packet(&session, &h, body, sizeof(idr_plain), 40, 3600) != 1) return 5;
    if (dcc_video_slot_read(&slot, &sequence, readback, sizeof(readback),
                            &length, &pts, &flags) != 1) return 6;
    if (!(flags & DCC_VIDEO_FLAG_ACTIVE) || !(flags & DCC_VIDEO_FLAG_IDR) || pts != 3600) return 7;
    if (length < sizeof(idr_plain) || memcmp(readback + length - 7, "\0\0\0\1\x65\x88\x84", 7)) return 8;
    if (dcc_screen_session_poll(&session, 2040)) return 9;
    if (dcc_screen_session_poll(&session, 2041) != 1) return 10;
    if (dcc_video_slot_read(&slot, &sequence, readback, sizeof(readback),
                            &length, &pts, &flags) != 1 ||
        flags != DCC_VIDEO_FLAG_STOP || length != 0) return 11;
    if (crypto.calls != 3) return 12;
    puts("Screen protocol to Annex-B shared-slot pipeline checks passed");
    return 0;
}
