#include "dcc_ts.h"
#include <string.h>

#define PSI_PAYLOAD 184u

static uint32_t crc32_mpeg(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xffffffffu;
    size_t i;
    unsigned bit;
    for (i = 0; i < n; ++i) {
        crc ^= (uint32_t)p[i] << 24;
        for (bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04c11db7u : crc << 1;
    }
    return crc;
}

static void put_crc(uint8_t *section, size_t n)
{
    uint32_t crc = crc32_mpeg(section, n);
    section[n] = (uint8_t)(crc >> 24);
    section[n + 1] = (uint8_t)(crc >> 16);
    section[n + 2] = (uint8_t)(crc >> 8);
    section[n + 3] = (uint8_t)crc;
}

static int psi(uint16_t pid, uint8_t *cc, const uint8_t *section, size_t length,
               dcc_ts_sink sink, void *opaque)
{
    uint8_t packet[DCC_TS_PACKET_SIZE];
    if (length + 1u > PSI_PAYLOAD) return -1;
    memset(packet, 0xff, sizeof(packet));
    packet[0] = 0x47;
    packet[1] = (uint8_t)(0x40u | ((pid >> 8) & 0x1fu));
    packet[2] = (uint8_t)pid;
    packet[3] = (uint8_t)(0x10u | (*cc & 0x0fu));
    *cc = (uint8_t)((*cc + 1u) & 0x0fu);
    packet[4] = 0;
    memcpy(packet + 5, section, length);
    return sink(packet, opaque);
}

static int tables(struct dcc_ts_mux *m, dcc_ts_sink sink, void *opaque)
{
    uint8_t pat[16] = {0x00,0xb0,0x0d,0x00,0x01,0xc1,0x00,0x00,
                       0x00,0x01,0xf0,0x00,0,0,0,0};
    uint8_t pmt[21] = {0x02,0xb0,0x12,0x00,0x01,0xc1,0x00,0x00,
                       0xe1,0x00,0xf0,0x00,0x1b,0xe1,0x00,0xf0,0x00,0,0,0,0};
    put_crc(pat, 12);
    put_crc(pmt, 17);
    if (psi(DCC_TS_PAT_PID, &m->pat_cc, pat, sizeof(pat), sink, opaque)) return -1;
    return psi(DCC_TS_PMT_PID, &m->pmt_cc, pmt, sizeof(pmt), sink, opaque);
}

static void encode_pts(uint8_t out[5], uint64_t value)
{
    value &= 0x1ffffffffull;
    out[0] = (uint8_t)(0x21u | (((value >> 30) & 7u) << 1));
    out[1] = (uint8_t)(value >> 22);
    out[2] = (uint8_t)(1u | (((value >> 15) & 0x7fu) << 1));
    out[3] = (uint8_t)(value >> 7);
    out[4] = (uint8_t)(1u | ((value & 0x7fu) << 1));
}

static void encode_pcr(uint8_t out[6], uint64_t pts)
{
    uint64_t base = pts > 9000u ? pts - 9000u : 0u;
    base &= 0x1ffffffffull;
    out[0] = (uint8_t)(base >> 25);
    out[1] = (uint8_t)(base >> 17);
    out[2] = (uint8_t)(base >> 9);
    out[3] = (uint8_t)(base >> 1);
    out[4] = (uint8_t)(((base & 1u) << 7) | 0x7eu);
    out[5] = 0;
}

void dcc_ts_init(struct dcc_ts_mux *mux)
{
    if (mux) memset(mux, 0, sizeof(*mux));
}

int dcc_ts_push_annexb(struct dcc_ts_mux *m, const uint8_t *au, size_t n,
                       uint64_t pts, dcc_ts_sink sink, void *opaque)
{
    uint8_t header[14] = {0x00,0x00,0x01,0xe0,0x00,0x00,0x80,0x80,0x05,0,0,0,0,0};
    size_t hpos = 0, apos = 0, remaining, payload, adaptation;
    int first = 1;
    if (!m || !au || !n || !sink || n > 0x00ffffffu) return -1;
    if ((m->access_units % 30u) == 0u && tables(m, sink, opaque)) return -1;
    if (n + 8u <= 0xffffu) {
        size_t pes_length = n + 8u;
        header[4] = (uint8_t)(pes_length >> 8);
        header[5] = (uint8_t)pes_length;
    }
    encode_pts(header + 9, pts);
    remaining = sizeof(header) + n;
    while (remaining) {
        uint8_t packet[DCC_TS_PACKET_SIZE];
        size_t capacity = 184u;
        memset(packet, 0xff, sizeof(packet));
        packet[0] = 0x47;
        packet[1] = (uint8_t)(((DCC_TS_VIDEO_PID >> 8) & 0x1fu) | (first ? 0x40u : 0u));
        packet[2] = (uint8_t)DCC_TS_VIDEO_PID;
        packet[3] = (uint8_t)(0x10u | (m->video_cc & 0x0fu));
        m->video_cc = (uint8_t)((m->video_cc + 1u) & 0x0fu);
        adaptation = first ? 8u : 0u;
        capacity -= adaptation;
        if (remaining < capacity) {
            adaptation += capacity - remaining;
            packet[3] |= 0x20u;
            packet[4] = (uint8_t)(adaptation - 1u);
            packet[5] = first ? 0x10u : 0u;
            if (first) encode_pcr(packet + 6, pts);
            capacity = remaining;
            payload = 4u + adaptation;
        } else if (adaptation) {
            packet[3] |= 0x20u;
            packet[4] = 7u;
            packet[5] = 0x10u;
            encode_pcr(packet + 6, pts);
            payload = 12u;
        } else payload = 4u;
        while (capacity && hpos < sizeof(header)) {
            packet[payload++] = header[hpos++];
            --capacity; --remaining;
        }
        while (capacity && apos < n) {
            packet[payload++] = au[apos++];
            --capacity; --remaining;
        }
        if (sink(packet, opaque)) return -1;
        first = 0;
    }
    ++m->access_units;
    return 0;
}
