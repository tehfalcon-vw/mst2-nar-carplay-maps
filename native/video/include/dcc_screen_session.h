#ifndef DCC_SCREEN_SESSION_H
#define DCC_SCREEN_SESSION_H

#include "dcc_avc.h"
#include "dcc_screen.h"
#include "dcc_video_lifecycle.h"
#include "dcc_video_slot.h"

typedef int (*dcc_screen_decrypt_fn)(uint8_t *bytes, size_t length, void *opaque);

struct dcc_screen_session {
    struct dcc_avc avc;
    struct dcc_video_lifecycle lifecycle;
    struct dcc_video_slot *slot;
    uint8_t *annexb;
    size_t annexb_capacity;
    dcc_screen_decrypt_fn decrypt;
    void *decrypt_opaque;
    uint64_t accepted_frames;
};

void dcc_screen_session_init(struct dcc_screen_session *session,
                             struct dcc_video_slot *slot,
                             uint8_t *annexb, size_t annexb_capacity,
                             dcc_screen_decrypt_fn decrypt,
                             void *decrypt_opaque,
                             uint32_t stale_after_ms);

/* body is mutable because VIDEO_FRAME is decrypted in place. Return values:
 * 1 = access unit published, 0 = valid control/config/drop, -1 = invalid. */
int dcc_screen_session_packet(struct dcc_screen_session *session,
                              const struct dcc_screen_header *header,
                              uint8_t *body, size_t body_size,
                              uint64_t now_ms, uint64_t pts90k);
int dcc_screen_session_poll(struct dcc_screen_session *session, uint64_t now_ms);
void dcc_screen_session_stop(struct dcc_screen_session *session);

#endif
