#include "dcc_identify.h"
#include <string.h>
#include <stdint.h>

static uint16_t read16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void write16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static int valid_name(const char *p,size_t n){
    /* Reuse the core's UTF-8 validation via a bounded synthetic route-text
     * message. No source characters are normalized or truncated. */
    uint8_t frame[265];DccMessage message;size_t i;
    if(!p||!n||n>254)return 0;
    for(i=0;i<n;i++)if(!p[i])return 0;
    frame[0]=frame[1]=0x40;write16(frame+2,(unsigned)(n+10));write16(frame+4,0x5201);
    write16(frame+6,(unsigned)(n+4));write16(frame+8,3);memcpy(frame+10,p,n);
    return dcc_decode(frame,n+10,&message)==DCC_OK;
}
static size_t component(const DccIdentifyProfile *p,uint8_t *out){
    uint16_t values[9];unsigned id;size_t n=4;
    values[0]=p->component_id;values[1]=0;values[2]=p->current_road_chars;
    values[3]=p->destination_chars;values[4]=p->after_road_chars;
    values[5]=p->maneuver_description_chars;values[6]=p->maneuver_slots;
    values[7]=p->lane_description_chars;values[8]=p->lane_slots;
    for(id=0;id<9;id++){
        unsigned len=id==1?(unsigned)p->name_bytes+5:6;
        write16(out+n,len);write16(out+n+2,id);
        if(id==1){memcpy(out+n+4,p->name,p->name_bytes);out[n+4+p->name_bytes]=0;}
        else write16(out+n+4,values[id]);
        n+=len;
    }
    write16(out,(unsigned)n);write16(out+2,0x1e);return n;
}
static int known_component(uint16_t id){
    switch(id){case 0x0e:case 0x0f:case 0x10:case 0x11:case 0x12:
        case 0x14:case 0x15:case 0x16:case 0x17:case 0x18:return 1;default:return 0;}
}
static DccResult inspect_component(const uint8_t *v,size_t n,uint16_t wanted){
    size_t pos=0;int seen=0;
    while(pos<n){unsigned size,id;if(n-pos<4)return DCC_TLV;
        size=read16(v+pos);id=read16(v+pos+2);
        if(size<4||size>n-pos)return DCC_TLV;
        if(id==0){if(seen++)return DCC_DUPLICATE;if(size!=6)return DCC_FIELD;
            if(read16(v+pos+4)==wanted)return DCC_CONFLICT;}
        pos+=size;
    }
    /* Some known transport builders may omit their component; no invented ID. */
    return DCC_OK;
}
static int contains(const uint8_t *v,size_t n,uint16_t id){
    size_t k;for(k=0;k<n;k+=2)if(read16(v+k)==id)return 1;return 0;
}
static unsigned missing(const uint8_t *v,size_t n,const uint16_t *ids,unsigned count){
    unsigned i,extra=0;for(i=0;i<count;i++)if(!contains(v,n,ids[i]))++extra;return extra;
}
static size_t emit_list(uint8_t *out,uint16_t tag,const uint8_t *v,size_t n,
                       const uint16_t *ids,unsigned count){
    unsigned i;size_t end=4+n;write16(out+2,tag);if(n)memcpy(out+4,v,n);
    for(i=0;i<count;i++)if(!contains(v,n,ids[i])){write16(out+end,ids[i]);end+=2;}
    write16(out,(unsigned)end);return end;
}
DccResult dcc_identify_prepare(const uint8_t *in,size_t n,const DccIdentifyProfile *p,
    uint8_t *out,size_t capacity,size_t *written,DccIdentifyChanges *changes){
    static const uint16_t send_ids[2]={0x5200,0x5203},recv_ids[3]={0x5201,0x5202,0x5204};
    uint8_t comp[320];size_t comp_size,pos,need,at;int send_seen=0,recv_seen=0,comp_seen=0;
    unsigned send_extra=0,recv_extra=0;DccIdentifyChanges result;
    if(!in||!out||!p||!written||!changes)return DCC_ARGUMENT;
    if(n<6||n>DCC_MAX_FRAME||in[0]!=0x40||in[1]!=0x40||read16(in+2)!=n)return DCC_FRAME;
    if(read16(in+4)!=0x1d01)return DCC_UNSUPPORTED;
    if(!valid_name(p->name,p->name_bytes)||!p->current_road_chars||!p->destination_chars||
       !p->after_road_chars||!p->maneuver_description_chars||!p->maneuver_slots||
       !p->lane_description_chars||!p->lane_slots)return DCC_FIELD;
    if((uintptr_t)in>UINTPTR_MAX-n||(uintptr_t)out>UINTPTR_MAX-capacity)return DCC_ARGUMENT;
    if((uintptr_t)out<(uintptr_t)in+n&&(uintptr_t)in<(uintptr_t)out+capacity)return DCC_ARGUMENT;
    comp_size=component(p,comp);need=n;
    /* Preflight everything before the first write to any caller output. */
    for(pos=6;pos<n;){
        unsigned size,tag;const uint8_t *v;size_t len;DccResult r;
        if(n-pos<4)return DCC_TLV;size=read16(in+pos);tag=read16(in+pos+2);
        if(size<4||size>n-pos)return DCC_TLV;v=in+pos+4;len=size-4;
        if(tag==6||tag==7){
            unsigned extra;int *seen=tag==6?&send_seen:&recv_seen;
            if((*seen)++)return DCC_DUPLICATE;
            if(len%2)return DCC_FIELD;
            extra=missing(v,len,tag==6?send_ids:recv_ids,tag==6?2:3);
            if(len+extra*2>128)return DCC_LIMIT;
            if(tag==6)send_extra=extra;else recv_extra=extra;
            need+=extra*2;
        }else if(tag==0x1e){
            if(comp_seen++)return DCC_DUPLICATE;
            /* Strict conflict policy: preserve a matching canonical component;
             * do not silently change another integration's declaration. */
            if(size!=comp_size||memcmp(in+pos,comp,comp_size))return DCC_CONFLICT;
        }else if(known_component((uint16_t)tag)){
            r=inspect_component(v,len,p->component_id);if(r)return r;
        }
        pos+=size;
    }
    if(!send_seen){send_extra=2;need+=8;}
    if(!recv_seen){recv_extra=3;need+=10;}
    if(!comp_seen)need+=comp_size;
    if(need>DCC_MAX_FRAME||capacity<need)return DCC_LIMIT;
    result.input_bytes=n;result.output_bytes=need;result.component_added=(unsigned)!comp_seen;
    result.send_ids_added=send_extra;result.receive_ids_added=recv_extra;
    memcpy(out,in,6);at=6;
    for(pos=6;pos<n;){
        unsigned size=read16(in+pos),tag=read16(in+pos+2);
        if(tag==6||tag==7)at+=emit_list(out+at,(uint16_t)tag,in+pos+4,size-4,
            tag==6?send_ids:recv_ids,tag==6?2:3);
        else{memcpy(out+at,in+pos,size);at+=size;}
        pos+=size;
    }
    if(!send_seen)at+=emit_list(out+at,6,NULL,0,send_ids,2);
    if(!recv_seen)at+=emit_list(out+at,7,NULL,0,recv_ids,3);
    if(!comp_seen){memcpy(out+at,comp,comp_size);at+=comp_size;}
    write16(out+2,(unsigned)at);*written=at;*changes=result;return DCC_OK;
}
