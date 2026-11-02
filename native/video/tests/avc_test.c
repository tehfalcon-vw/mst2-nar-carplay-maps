#include "dcc_avc.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const uint8_t config[] = {
        1,0x42,0,0x1f,0xff,0xe1, 0,4,0x67,0x42,0,0x1f,
        1, 0,4,0x68,0xce,6,0xe2
    };
    static const uint8_t idr[] = {0,0,0,3,0x65,0x88,0x84, 0,0,0,2,0x06,5};
    static const uint8_t pframe[] = {0,0,0,2,0x41,0x9a};
    static const uint8_t expected_prefix[] = {0,0,0,1,0x67,0x42,0,0x1f,0,0,0,1,0x68,0xce,6,0xe2,0,0,0,1,0x65};
    struct dcc_avc avc;
    uint8_t output[128];
    size_t n;
    int is_idr = 0;
    dcc_avc_init(&avc);
    if (dcc_avc_configure(&avc,config,sizeof(config))) return 1;
    n = dcc_avc_to_annexb(&avc,idr,sizeof(idr),output,sizeof(output),&is_idr);
    if (!n || !is_idr || n != avc.config_length + 13u) return 2;
    if (memcmp(output,expected_prefix,sizeof(expected_prefix))) return 3;
    n = dcc_avc_to_annexb(&avc,pframe,sizeof(pframe),output,sizeof(output),&is_idr);
    if (n != 6u || is_idr || memcmp(output,"\0\0\0\1\x41\x9a",6)) return 4;
    if (dcc_avc_to_annexb(&avc,idr,sizeof(idr)-1u,output,sizeof(output),&is_idr)) return 5;
    if (dcc_avc_to_annexb(&avc,idr,sizeof(idr),output,4,&is_idr)) return 6;
    puts("avcC and AVCC-to-Annex-B checks passed");
    return 0;
}
