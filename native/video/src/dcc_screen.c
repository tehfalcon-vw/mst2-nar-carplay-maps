#include "dcc_screen.h"
#include <string.h>

static uint32_t little_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void clear_packet(struct dcc_screen_parser *parser)
{
    parser->header_used = 0;
    parser->body_used = 0;
    parser->body_expected = 0;
    memset(&parser->current, 0, sizeof(parser->current));
}

void dcc_screen_parser_init(struct dcc_screen_parser *parser,
                            uint8_t *body_storage, size_t body_capacity,
                            dcc_screen_packet_fn on_packet, void *opaque)
{
    if (!parser) return;
    memset(parser, 0, sizeof(*parser));
    parser->body_storage = body_storage;
    parser->body_capacity = body_capacity;
    parser->on_packet = on_packet;
    parser->opaque = opaque;
}

void dcc_screen_parser_reset(struct dcc_screen_parser *parser)
{
    if (!parser) return;
    parser->failed = 0;
    clear_packet(parser);
}

static int emit(struct dcc_screen_parser *parser)
{
    int result = 0;
    if (parser->on_packet)
        result = parser->on_packet(&parser->current, parser->body_storage,
                                   parser->body_expected, parser->opaque);
    clear_packet(parser);
    if (result) parser->failed = 1;
    return result ? -1 : 0;
}

int dcc_screen_parser_feed(struct dcc_screen_parser *parser,
                           const uint8_t *data, size_t length)
{
    if (!parser || (!data && length) || parser->failed) return -1;
    while (length) {
        size_t take;
        if (parser->header_used < DCC_SCREEN_HEADER_SIZE) {
            take = DCC_SCREEN_HEADER_SIZE - parser->header_used;
            if (take > length) take = length;
            memcpy(parser->header_bytes + parser->header_used, data, take);
            parser->header_used += take;
            data += take;
            length -= take;
            if (parser->header_used != DCC_SCREEN_HEADER_SIZE) continue;
            parser->current.body_size = little_u32(parser->header_bytes);
            parser->current.opcode = parser->header_bytes[4];
            memcpy(parser->current.small_param, parser->header_bytes + 5, 3);
            memcpy(parser->current.params, parser->header_bytes + 8, 120);
            parser->body_expected = parser->current.body_size;
            if (parser->body_expected > parser->body_capacity ||
                (parser->body_expected && !parser->body_storage)) {
                parser->failed = 1;
                return -1;
            }
            if (!parser->body_expected && emit(parser)) return -1;
        }
        if (parser->header_used == DCC_SCREEN_HEADER_SIZE && parser->body_expected) {
            take = parser->body_expected - parser->body_used;
            if (take > length) take = length;
            memcpy(parser->body_storage + parser->body_used, data, take);
            parser->body_used += take;
            data += take;
            length -= take;
            if (parser->body_used == parser->body_expected && emit(parser)) return -1;
        }
    }
    return 0;
}
