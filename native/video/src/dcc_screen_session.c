#include "dcc_screen_session.h"
#include <string.h>

static void publish_stop(struct dcc_screen_session *session)
{
    if (session && session->slot)
        (void)dcc_video_slot_publish(session->slot, NULL, 0, 0,
                                     DCC_VIDEO_FLAG_STOP);
}

void dcc_screen_session_init(struct dcc_screen_session *session,
                             struct dcc_video_slot *slot,
                             uint8_t *annexb, size_t annexb_capacity,
                             dcc_screen_decrypt_fn decrypt,
                             void *decrypt_opaque,
                             uint32_t stale_after_ms)
{
    if (!session) return;
    memset(session, 0, sizeof(*session));
    dcc_avc_init(&session->avc);
    dcc_video_lifecycle_init(&session->lifecycle, stale_after_ms);
    session->slot = slot;
    session->annexb = annexb;
    session->annexb_capacity = annexb_capacity;
    session->decrypt = decrypt;
    session->decrypt_opaque = decrypt_opaque;
}

int dcc_screen_session_packet(struct dcc_screen_session *session,
                              const struct dcc_screen_header *header,
                              uint8_t *body, size_t body_size,
                              uint64_t now_ms, uint64_t pts90k)
{
    size_t output_size;
    int is_idr = 0;
    uint32_t flags;
    if (!session || !header || body_size != header->body_size) return -1;
    switch (header->opcode) {
    case DCC_SCREEN_VIDEO_CONFIG:
        if (!body || !body_size || dcc_avc_configure(&session->avc, body, body_size)) {
            dcc_video_lifecycle_stop(&session->lifecycle);
            publish_stop(session);
            return -1;
        }
        dcc_video_lifecycle_config(&session->lifecycle);
        publish_stop(session);
        return 0;
    case DCC_SCREEN_VIDEO_FRAME:
        if (!body || !body_size || !session->decrypt ||
            session->decrypt(body, body_size, session->decrypt_opaque)) {
            dcc_video_lifecycle_stop(&session->lifecycle);
            publish_stop(session);
            return -1;
        }
        /* Decrypt before checking configuration: AES-CTR is continuous for the
         * connection and must advance even when an early AU is discarded. */
        if (session->lifecycle.state == DCC_VIDEO_INACTIVE) return 0;
        output_size = dcc_avc_to_annexb(&session->avc, body, body_size,
                                        session->annexb,
                                        session->annexb_capacity, &is_idr);
        if (!output_size) return -1;
        if (!dcc_video_lifecycle_frame(&session->lifecycle, is_idr, now_ms)) return 0;
        flags = DCC_VIDEO_FLAG_ACTIVE | (is_idr ? DCC_VIDEO_FLAG_IDR : 0u);
        if (!session->slot ||
            dcc_video_slot_publish(session->slot, session->annexb, output_size,
                                   pts90k, flags)) return -1;
        ++session->accepted_frames;
        return 1;
    case DCC_SCREEN_KEEPALIVE:
    case DCC_SCREEN_FORCE_KEYFRAME:
    case DCC_SCREEN_IGNORE:
    case DCC_SCREEN_KEEPALIVE_BODY:
        return 0;
    default:
        return -1;
    }
}

int dcc_screen_session_poll(struct dcc_screen_session *session, uint64_t now_ms)
{
    if (!session) return -1;
    if (!dcc_video_lifecycle_poll(&session->lifecycle, now_ms)) return 0;
    publish_stop(session);
    return 1;
}

void dcc_screen_session_stop(struct dcc_screen_session *session)
{
    if (!session) return;
    dcc_video_lifecycle_stop(&session->lifecycle);
    publish_stop(session);
}
