#include "dcc_screen.h"
#include <stdio.h>
#include <string.h>

struct capture { int calls; uint8_t opcode[4]; size_t length[4]; uint8_t first[4]; };

static int packet(const struct dcc_screen_header *header, uint8_t *body,
                  size_t body_size, void *opaque)
{
    struct capture *capture = (struct capture *)opaque;
    int i = capture->calls++;
    if (i >= 4) return -1;
    capture->opcode[i] = header->opcode;
    capture->length[i] = body_size;
    capture->first[i] = body_size ? body[0] : 0;
    return 0;
}

int main(void)
{
    uint8_t stream[128u * 3u + 7u];
    uint8_t body[16];
    struct dcc_screen_parser parser;
    struct capture capture;
    size_t offset = 0;
    memset(stream, 0, sizeof(stream));
    memset(&capture, 0, sizeof(capture));
    stream[0] = 3; stream[4] = DCC_SCREEN_VIDEO_CONFIG;
    stream[128] = 1; stream[129] = 2; stream[130] = 3;
    stream[131 + 4] = DCC_SCREEN_KEEPALIVE;
    stream[259] = 4; stream[263] = DCC_SCREEN_VIDEO_FRAME;
    stream[387] = 9; stream[388] = 8; stream[389] = 7; stream[390] = 6;
    dcc_screen_parser_init(&parser, body, sizeof(body), packet, &capture);
    while (offset < sizeof(stream)) {
        size_t n = (offset % 11u) + 1u;
        if (n > sizeof(stream) - offset) n = sizeof(stream) - offset;
        if (dcc_screen_parser_feed(&parser, stream + offset, n)) return 1;
        offset += n;
    }
    if (capture.calls != 3) return 2;
    if (capture.opcode[0] != DCC_SCREEN_VIDEO_CONFIG || capture.length[0] != 3 || capture.first[0] != 1) return 3;
    if (capture.opcode[1] != DCC_SCREEN_KEEPALIVE || capture.length[1] != 0) return 4;
    if (capture.opcode[2] != DCC_SCREEN_VIDEO_FRAME || capture.length[2] != 4 || capture.first[2] != 9) return 5;
    memset(stream, 0, 128); stream[0] = 17;
    dcc_screen_parser_reset(&parser);
    if (!dcc_screen_parser_feed(&parser, stream, 128)) return 6;
    puts("incremental Screen stream framing checks passed");
    return 0;
}
