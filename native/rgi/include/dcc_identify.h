#ifndef DCC_IDENTIFY_H
#define DCC_IDENTIFY_H
#include "dcc_rgi.h"
/* Explicit experimental capabilities. These are not auto-detected hardware
 * capabilities: the caller must not advertise storage its adapter cannot supply. */
typedef struct {
    uint16_t component_id;
    const char *name;
    size_t name_bytes; /* UTF-8, excluding terminating NUL */
    uint16_t current_road_chars, destination_chars, after_road_chars;
    uint16_t maneuver_description_chars, maneuver_slots;
    uint16_t lane_description_chars, lane_slots;
} DccIdentifyProfile;
typedef struct {
    size_t input_bytes, output_bytes;
    unsigned component_added, send_ids_added, receive_ids_added;
} DccIdentifyChanges;
/* Transform a complete 0x1D01 control frame in memory. No transmission or
 * firmware modification. Input/output must not overlap. On error, output,
 * written and changes are all unchanged. Existing conflicting RGI components
 * are rejected. Message lists are bounded to 128 bytes (observed driver bound).
 * Unknown top-level fields remain byte-identical. Known component-family ID
 * collisions are checked; unknown future families are not semantically decoded. */
DccResult dcc_identify_prepare(const uint8_t *input, size_t input_bytes,
    const DccIdentifyProfile *, uint8_t *output, size_t output_capacity,
    size_t *written, DccIdentifyChanges *changes);
#endif
