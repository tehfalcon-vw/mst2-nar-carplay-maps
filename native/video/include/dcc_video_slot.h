#ifndef DCC_VIDEO_SLOT_H
#define DCC_VIDEO_SLOT_H

#include <stddef.h>
#include <stdint.h>

#define DCC_VIDEO_SLOT_MAGIC 0x31434344u
#define DCC_VIDEO_SLOT_VERSION 1u
#define DCC_VIDEO_SLOT_CAPACITY 250000u

#define DCC_VIDEO_FLAG_ACTIVE 0x00000001u
#define DCC_VIDEO_FLAG_CONFIG 0x00000002u
#define DCC_VIDEO_FLAG_IDR 0x00000004u
#define DCC_VIDEO_FLAG_STOP 0x00000008u

/* A seqlock-protected latest-frame slot. Dropping old phone frames is
 * intentional: the cluster should receive the newest complete access unit. */
struct dcc_video_slot {
    volatile uint32_t sequence;
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t length;
    uint32_t pts90k_low;
    uint32_t pts90k_high;
    uint8_t data[DCC_VIDEO_SLOT_CAPACITY];
};

void dcc_video_slot_init(struct dcc_video_slot *slot);
int dcc_video_slot_publish(struct dcc_video_slot *slot, const uint8_t *data,
                           size_t length, uint64_t pts90k, uint32_t flags);
int dcc_video_slot_read(const struct dcc_video_slot *slot, uint32_t *last_sequence,
                        uint8_t *data, size_t capacity, size_t *length,
                        uint64_t *pts90k, uint32_t *flags);

#endif
