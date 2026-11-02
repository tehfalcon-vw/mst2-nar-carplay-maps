#ifndef DCC_ALT111_INGRESS_H
#define DCC_ALT111_INGRESS_H

#include <stddef.h>
#include <stdint.h>

/* One process-wide Type-111 ingress generation. begin() derives a fresh
 * continuous screen cipher for stream_connection_id and creates the shared
 * latest-frame slot. */
int dcc_alt111_ingress_begin(const uint8_t master_key[16],
                             uint64_t stream_connection_id);
int dcc_alt111_ingress_feed(const uint8_t *bytes, size_t length,
                            uint64_t now_ms, uint64_t first_pts90k);
int dcc_alt111_ingress_poll(uint64_t now_ms);
void dcc_alt111_ingress_stop(void);

#endif
