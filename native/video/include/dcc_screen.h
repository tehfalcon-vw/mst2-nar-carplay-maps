#ifndef DCC_SCREEN_H
#define DCC_SCREEN_H

#include <stddef.h>
#include <stdint.h>

#define DCC_SCREEN_HEADER_SIZE 128u
#define DCC_SCREEN_VIDEO_FRAME 0u
#define DCC_SCREEN_VIDEO_CONFIG 1u
#define DCC_SCREEN_KEEPALIVE 2u
#define DCC_SCREEN_FORCE_KEYFRAME 3u
#define DCC_SCREEN_IGNORE 4u
#define DCC_SCREEN_KEEPALIVE_BODY 5u

struct dcc_screen_header {
    uint32_t body_size;
    uint8_t opcode;
    uint8_t small_param[3];
    uint8_t params[120];
};

typedef int (*dcc_screen_packet_fn)(const struct dcc_screen_header *header,
                                    uint8_t *body, size_t body_size,
                                    void *opaque);

/* Incremental parser for the TCP byte stream. The caller owns body_storage,
 * making the parser usable in both hosted tests and the freestanding QNX
 * payload without an allocator. */
struct dcc_screen_parser {
    uint8_t header_bytes[DCC_SCREEN_HEADER_SIZE];
    size_t header_used;
    uint8_t *body_storage;
    size_t body_capacity;
    size_t body_used;
    size_t body_expected;
    struct dcc_screen_header current;
    dcc_screen_packet_fn on_packet;
    void *opaque;
    int failed;
};

void dcc_screen_parser_init(struct dcc_screen_parser *parser,
                            uint8_t *body_storage, size_t body_capacity,
                            dcc_screen_packet_fn on_packet, void *opaque);
int dcc_screen_parser_feed(struct dcc_screen_parser *parser,
                           const uint8_t *data, size_t length);
void dcc_screen_parser_reset(struct dcc_screen_parser *parser);

#endif
