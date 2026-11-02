#include "dcc_avc.h"
#include <string.h>

static const uint8_t start_code[4] = {0,0,0,1};

void dcc_avc_init(struct dcc_avc *avc)
{
    if (avc) memset(avc, 0, sizeof(*avc));
}

static int append_parameter(struct dcc_avc *avc, const uint8_t *data, size_t length)
{
    if (!length || avc->config_length + sizeof(start_code) + length > sizeof(avc->config))
        return -1;
    memcpy(avc->config + avc->config_length, start_code, sizeof(start_code));
    avc->config_length += sizeof(start_code);
    memcpy(avc->config + avc->config_length, data, length);
    avc->config_length += length;
    return 0;
}

int dcc_avc_configure(struct dcc_avc *avc, const uint8_t *data, size_t length)
{
    size_t offset = 6;
    unsigned count, i;
    struct dcc_avc next;
    if (!avc || !data || length < 7 || data[0] != 1) return -1;
    dcc_avc_init(&next);
    next.length_bytes = (uint8_t)((data[4] & 3u) + 1u);
    count = data[5] & 0x1fu;
    if (!count) return -1;
    for (i = 0; i < count; ++i) {
        size_t n;
        if (offset + 2u > length) return -1;
        n = ((size_t)data[offset] << 8) | data[offset + 1u]; offset += 2u;
        if (offset + n > length || append_parameter(&next, data + offset, n)) return -1;
        offset += n;
    }
    if (offset >= length) return -1;
    count = data[offset++];
    if (!count) return -1;
    for (i = 0; i < count; ++i) {
        size_t n;
        if (offset + 2u > length) return -1;
        n = ((size_t)data[offset] << 8) | data[offset + 1u]; offset += 2u;
        if (offset + n > length || append_parameter(&next, data + offset, n)) return -1;
        offset += n;
    }
    if (offset != length) return -1;
    *avc = next;
    return 0;
}

static int inspect(const struct dcc_avc *avc, const uint8_t *input, size_t length,
                   size_t *converted, int *idr)
{
    size_t offset = 0, total = 0;
    int found_idr = 0;
    if (!avc || !avc->length_bytes || !input || !length) return -1;
    while (offset < length) {
        size_t n = 0;
        unsigned i;
        if (offset + avc->length_bytes > length) return -1;
        for (i = 0; i < avc->length_bytes; ++i) n = (n << 8) | input[offset++];
        if (!n || offset + n > length || total > (size_t)-1 - sizeof(start_code) - n) return -1;
        if ((input[offset] & 0x1fu) == 5u) found_idr = 1;
        total += sizeof(start_code) + n;
        offset += n;
    }
    if (found_idr) {
        if (total > (size_t)-1 - avc->config_length) return -1;
        total += avc->config_length;
    }
    *converted = total;
    *idr = found_idr;
    return 0;
}

size_t dcc_avc_to_annexb(const struct dcc_avc *avc, const uint8_t *input,
                         size_t input_length, uint8_t *output,
                         size_t output_capacity, int *is_idr)
{
    size_t needed, in_offset = 0, out_offset = 0;
    int idr;
    if (!output || !is_idr || inspect(avc,input,input_length,&needed,&idr) ||
        needed > output_capacity) return 0;
    if (idr && avc->config_length) {
        memcpy(output, avc->config, avc->config_length);
        out_offset = avc->config_length;
    }
    while (in_offset < input_length) {
        size_t n = 0;
        unsigned i;
        for (i = 0; i < avc->length_bytes; ++i) n = (n << 8) | input[in_offset++];
        memcpy(output + out_offset, start_code, sizeof(start_code));
        out_offset += sizeof(start_code);
        memcpy(output + out_offset, input + in_offset, n);
        out_offset += n; in_offset += n;
    }
    *is_idr = idr;
    return out_offset;
}
