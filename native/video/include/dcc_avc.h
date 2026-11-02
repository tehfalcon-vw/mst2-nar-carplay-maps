#ifndef DCC_AVC_H
#define DCC_AVC_H

#include <stddef.h>
#include <stdint.h>

#define DCC_AVC_CONFIG_CAPACITY 4096u

struct dcc_avc {
    uint8_t length_bytes;
    size_t config_length;
    uint8_t config[DCC_AVC_CONFIG_CAPACITY];
};

void dcc_avc_init(struct dcc_avc *avc);
int dcc_avc_configure(struct dcc_avc *avc, const uint8_t *avcc, size_t length);

/* Converts one complete AVCC access unit. SPS/PPS from the avcC record are
 * prepended to IDR units so a consumer can join at every accepted key frame. */
size_t dcc_avc_to_annexb(const struct dcc_avc *avc, const uint8_t *input,
                         size_t input_length, uint8_t *output,
                         size_t output_capacity, int *is_idr);

#endif
