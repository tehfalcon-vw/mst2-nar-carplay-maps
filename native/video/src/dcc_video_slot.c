#include "dcc_video_slot.h"
#include <string.h>

static void barrier(void) { __sync_synchronize(); }

void dcc_video_slot_init(struct dcc_video_slot *slot)
{
    if (!slot) return;
    memset(slot, 0, sizeof(*slot));
    slot->magic = DCC_VIDEO_SLOT_MAGIC;
    slot->version = DCC_VIDEO_SLOT_VERSION;
    barrier();
    slot->sequence = 2u;
}

int dcc_video_slot_publish(struct dcc_video_slot *slot, const uint8_t *data,
                           size_t length, uint64_t pts90k, uint32_t flags)
{
    uint32_t sequence;
    if (!slot || length > DCC_VIDEO_SLOT_CAPACITY ||
        (length && !data) || (!length && flags != DCC_VIDEO_FLAG_STOP)) return -1;
    if (slot->magic != DCC_VIDEO_SLOT_MAGIC || slot->version != DCC_VIDEO_SLOT_VERSION)
        return -1;
    sequence = slot->sequence;
    if (sequence & 1u) ++sequence;
    slot->sequence = sequence + 1u;
    barrier();
    slot->flags = flags;
    slot->length = (uint32_t)length;
    slot->pts90k_low = (uint32_t)pts90k;
    slot->pts90k_high = (uint32_t)(pts90k >> 32);
    if (length) memcpy(slot->data, data, length);
    barrier();
    slot->sequence = sequence + 2u;
    return 0;
}

int dcc_video_slot_read(const struct dcc_video_slot *slot, uint32_t *last_sequence,
                        uint8_t *data, size_t capacity, size_t *length,
                        uint64_t *pts90k, uint32_t *flags)
{
    uint32_t before, after, n, low, high, frame_flags;
    if (!slot || !last_sequence || !data || !length || !pts90k || !flags) return -1;
    if (slot->magic != DCC_VIDEO_SLOT_MAGIC || slot->version != DCC_VIDEO_SLOT_VERSION)
        return -1;
    before = slot->sequence;
    if ((before & 1u) || before == *last_sequence) return 0;
    barrier();
    n = slot->length;
    frame_flags = slot->flags;
    low = slot->pts90k_low;
    high = slot->pts90k_high;
    if ((!n && frame_flags != DCC_VIDEO_FLAG_STOP) ||
        n > DCC_VIDEO_SLOT_CAPACITY || n > capacity) return -1;
    if (n) memcpy(data, slot->data, n);
    barrier();
    after = slot->sequence;
    if (before != after || (after & 1u)) return 0;
    *last_sequence = after;
    *length = n;
    *pts90k = ((uint64_t)high << 32) | low;
    *flags = frame_flags;
    return 1;
}
