#ifndef DCC_RGI_H
#define DCC_RGI_H
#include <stddef.h>
#include <stdint.h>

/* Local research module. Input is a decrypted iAP2 control message, not USB
 * packets or encrypted AirPlay data. No device transport is implemented. */
#define DCC_MAX_FRAME 65535u
#define DCC_MAX_FIELDS 128u
#define DCC_ROOT 65535u
typedef enum {
    DCC_OK=0, DCC_FRAME, DCC_TLV, DCC_FIELD, DCC_DUPLICATE, DCC_LIMIT,
    DCC_UNSUPPORTED, DCC_DISCONNECTED, DCC_STALE, DCC_TIME, DCC_ARGUMENT,
    DCC_CONFLICT
} DccResult;
typedef struct { uint16_t id, offset, length, parent; } DccField;
typedef struct {
    const uint8_t *bytes; /* Borrowed: valid only for the caller's buffer lifetime. */
    uint16_t length, id, count;
    DccField fields[DCC_MAX_FIELDS];
} DccMessage;

/* On failure, output is unchanged. Unknown TLVs are preserved as views. */
DccResult dcc_decode(const uint8_t *, size_t, DccMessage *);
const DccField *dcc_find(const DccMessage *, uint16_t parent, uint16_t id, size_t occurrence);
DccResult dcc_unsigned(const DccMessage *, const DccField *, uint64_t *);
DccResult dcc_signed16(const DccMessage *, const DccField *, int16_t *);
/* Returned text is UTF-8 bytes, not guaranteed NUL-terminated. No truncation. */
DccResult dcc_copy_text(const DccMessage *, const DccField *, char *, size_t, size_t *);

/* Buffer-only subscription encoders. Never send packets. Output unchanged on error. */
DccResult dcc_start(uint16_t component, const char *source, size_t source_bytes,
                    int supports_exit, uint8_t *out, size_t capacity, size_t *written);
DccResult dcc_stop(uint16_t component, uint8_t *out, size_t capacity, size_t *written);

typedef enum { DCC_EVENT_RESET=1, DCC_EVENT_MESSAGE=2 } DccEvent;
/* RESET requires the consumer to clear all route/maneuver/lane caches. Callbacks
 * are synchronous, must not reenter this session, and must copy any retained data. */
typedef void (*DccCallback)(void *, DccEvent, uint64_t, const DccMessage *);
typedef struct {
    uint64_t epoch, last_ms, clock_ms;
    int connected;
    size_t used, wanted;
    uint8_t frame[DCC_MAX_FRAME];
    DccCallback callback;
    void *context;
} DccSession;
void dcc_session_init(DccSession *, DccCallback, void *);
DccResult dcc_connect(DccSession *, uint64_t now_ms, uint64_t *token);
DccResult dcc_disconnect(DccSession *, uint64_t token);
/* Consumer supplies an already-established session token and monotonic time.
 * Each complete valid incoming message causes one callback. Invalid messages
 * cause no MESSAGE callback and reset the partial-frame buffer. No resync guess. */
DccResult dcc_feed(DccSession *, uint64_t token, uint64_t now_ms, const uint8_t *, size_t);
DccResult dcc_expire(DccSession *, uint64_t now_ms, uint64_t timeout_ms);
#endif
