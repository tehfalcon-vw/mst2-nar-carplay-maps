#ifndef DCC_TS_H
#define DCC_TS_H

#include <stddef.h>
#include <stdint.h>

#define DCC_TS_PACKET_SIZE 188u
#define DCC_TS_PAT_PID 0x0000u
#define DCC_TS_PMT_PID 0x1000u
#define DCC_TS_VIDEO_PID 0x0011u

typedef int (*dcc_ts_sink)(const uint8_t packet[DCC_TS_PACKET_SIZE], void *opaque);

struct dcc_ts_mux {
    uint8_t pat_cc;
    uint8_t pmt_cc;
    uint8_t video_cc;
    uint32_t access_units;
};

void dcc_ts_init(struct dcc_ts_mux *mux);

/* Packetize one complete Annex-B H.264 access unit. pts90k is the presentation
 * timestamp in the MPEG 90 kHz clock. The first packet of every access unit
 * carries PCR on the video PID. PAT and PMT are emitted before the first access
 * unit and then every 30 access units. The callback receives complete 188-byte
 * packets and nonzero aborts the operation. */
int dcc_ts_push_annexb(struct dcc_ts_mux *mux, const uint8_t *au, size_t length,
                       uint64_t pts90k, dcc_ts_sink sink, void *opaque);

#endif
