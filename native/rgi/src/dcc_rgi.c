#include "dcc_rgi.h"
#include <string.h>

static uint16_t be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }
static void put16(uint8_t *p, uint16_t n) { p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n; }
static int text_valid(const uint8_t *p, size_t n) {
    size_t i=0;
    while(i<n) {
        uint32_t c=p[i++], min=0; unsigned need=0;
        if(c<128) { if(!c && i!=n) return 0; continue; }
        if(c>=0xc2 && c<=0xdf) {need=1;min=0x80;c&=31;}
        else if(c>=0xe0 && c<=0xef) {need=2;min=0x800;c&=15;}
        else if(c>=0xf0 && c<=0xf4) {need=3;min=0x10000;c&=7;}
        else return 0;
        if(need>n-i) return 0;
        while(need--) {uint8_t b=p[i++];if((b&0xc0)!=0x80)return 0;c=(c<<6)|(b&63);}
        if(c<min || c>0x10ffff || (c>=0xd800&&c<=0xdfff))return 0;
    }
    return 1;
}
/* Strict initial schema profile. Widths/identifiers are protocol observations,
 * not a claim that every iOS version uses this exact profile. Unknown fields
 * remain opaque; no maneuver values or missing fields are invented. */
static DccResult shape(uint16_t msg, uint16_t parent, uint16_t id, const uint8_t *v, size_t n) {
    int width=0, text=0, array=0;
    if(parent!=DCC_ROOT) {
        switch(id) {case 0:width=2;break;case 1:width=1;break;
            case 2:array=1;if(n>32)return DCC_LIMIT;break;case 3:width=2;break;default:break;}
    } else if(id==0) array=1;
    else if(msg==0x5201) {
        switch(id) {
            case 1:case 2:case 9:case 12:case 15:case 18:case 20:width=1;break;
            case 5:case 6:width=8;break;case 7:case 10:width=4;break;
            case 14:case 16:case 17:width=2;break;case 13:array=1;break;
            case 3:case 4:case 8:case 11:case 19:text=1;break;default:break;
        }
    } else if(msg==0x5202) {
        switch(id) {
            case 1:case 11:case 12:width=2;break;case 3:case 7:case 8:case 9:width=1;break;
            case 5:width=4;break;case 10:array=1;break;
            case 2:case 4:case 6:text=1;break;default:break;
        }
    } else if(msg==0x5204) { if(id==1)width=2;else if(id==3)text=1; }
    else if(msg==0x5200) {if(id==1)text=1;else if(id==2||id==3)width=1;}
    if(width && n!=(size_t)width)return DCC_FIELD;
    if(array && n%2)return DCC_FIELD;
    if(text && !text_valid(v,n))return DCC_FIELD;
    return DCC_OK;
}
static int known(uint16_t msg, uint16_t id) {
    return id==0 || (msg==0x5201&&id<=20) || (msg==0x5202&&id<=13)
        || (msg==0x5204&&id<=3) || (msg==0x5200&&id<=3);
}
static DccResult sequence(DccMessage *m, size_t begin, size_t end, uint16_t parent) {
    uint32_t seen=0; int lane=0, status=0;unsigned lanes=0, angle_bytes=0;
    while(begin<end) {
        uint16_t size,id,index; DccResult r;
        if(end-begin<4)return DCC_TLV;
        size=be16(m->bytes+begin);id=be16(m->bytes+begin+2);
        if(size<4 || size>end-begin)return DCC_TLV;
        if(m->count==DCC_MAX_FIELDS)return DCC_LIMIT;
        if(parent==DCC_ROOT && known(m->id,id) && id<32) {
            int repeated=(m->id==0x5204&&id==2)||(m->id==0x5202&&id==10);
            if(!repeated && (seen&(1u<<id)))return DCC_DUPLICATE;
            seen|=1u<<id;
        }
        r=shape(m->id,parent,id,m->bytes+begin+4,size-4);if(r)return r;
        index=m->count++;
        m->fields[index].id=id;m->fields[index].offset=(uint16_t)(begin+4);
        m->fields[index].length=(uint16_t)(size-4);m->fields[index].parent=parent;
        if(parent!=DCC_ROOT) {
            if(id==0) {if(lane&&!status)return DCC_FIELD;if(++lanes>8)return DCC_LIMIT;lane=1;status=0;angle_bytes=0;}
            else if(id<=3) {if(!lane)return DCC_FIELD;if(id==1){if(status)return DCC_DUPLICATE;status=1;}
                if(id==2){angle_bytes+=size-4;if(angle_bytes>32)return DCC_LIMIT;}}
        }
        if(parent==DCC_ROOT && m->id==0x5204 && id==2) {
            r=sequence(m,begin+4,begin+size,index);if(r)return r;
        }
        begin+=size;
    }
    if(parent!=DCC_ROOT && (!lane||!status))return DCC_FIELD;
    return DCC_OK;
}
DccResult dcc_decode(const uint8_t *b,size_t n,DccMessage *out) {
    DccMessage m;DccResult r;
    if(!b||!out)return DCC_ARGUMENT;
    if(n<6||n>DCC_MAX_FRAME||b[0]!=0x40||b[1]!=0x40||be16(b+2)!=n)return DCC_FRAME;
    memset(&m,0,sizeof(m));m.bytes=b;m.length=(uint16_t)n;m.id=be16(b+4);
    if(m.id<0x5200||m.id>0x5204)return DCC_UNSUPPORTED;
    if(m.id==0x5203 && n!=12)return DCC_FIELD;
    r=sequence(&m,6,n,DCC_ROOT);if(r)return r;
    if(m.id==0x5203 && (m.count!=1||m.fields[0].id!=0||m.fields[0].length!=2))return DCC_FIELD;
    if(m.id==0x5204) {
        size_t i;unsigned lanes=0;
        for(i=0;i<m.count;i++)if(m.fields[i].parent!=DCC_ROOT&&m.fields[i].id==0)++lanes;
        if(lanes>8)return DCC_LIMIT;
    }
    *out=m;return DCC_OK;
}
const DccField *dcc_find(const DccMessage *m,uint16_t parent,uint16_t id,size_t occurrence) {
    size_t i;if(!m)return NULL;
    for(i=0;i<m->count;i++)if(m->fields[i].parent==parent&&m->fields[i].id==id) {
        if(!occurrence)return &m->fields[i];--occurrence;
    }
    return NULL;
}
static int field_bounds(const DccMessage *m,const DccField *f) {
    return m&&m->bytes&&f&&f->offset<=m->length&&f->length<=m->length-f->offset;
}
DccResult dcc_unsigned(const DccMessage *m,const DccField *f,uint64_t *out) {
    uint64_t value=0;size_t i;
    if(!out||!field_bounds(m,f))return DCC_ARGUMENT;
    if(f->length!=1&&f->length!=2&&f->length!=4&&f->length!=8)return DCC_FIELD;
    for(i=0;i<f->length;i++)value=(value<<8)|m->bytes[f->offset+i];
    *out=value;return DCC_OK;
}
DccResult dcc_signed16(const DccMessage *m,const DccField *f,int16_t *out) {
    uint16_t u;if(!out||!field_bounds(m,f))return DCC_ARGUMENT;
    if(f->length!=2)return DCC_FIELD;u=be16(m->bytes+f->offset);
    *out=(int16_t)(u<=32767?(int32_t)u:(int32_t)u-65536);return DCC_OK;
}
DccResult dcc_copy_text(const DccMessage *m,const DccField *f,char *out,size_t cap,size_t *written) {
    size_t n;if(!out||!written||!field_bounds(m,f))return DCC_ARGUMENT;
    n=f->length;if(!text_valid(m->bytes+f->offset,n))return DCC_FIELD;
    if(n&&m->bytes[f->offset+n-1]==0)--n;
    if(cap<=n)return DCC_LIMIT;
    memcpy(out,m->bytes+f->offset,n);out[n]=0;*written=n;return DCC_OK;
}
static void tlv(uint8_t *b,size_t *p,uint16_t id,const uint8_t *v,size_t n) {
    put16(b+*p,(uint16_t)(n+4));put16(b+*p+2,id);memcpy(b+*p+4,v,n);*p+=n+4;
}
DccResult dcc_start(uint16_t component,const char *source,size_t n,int exits,uint8_t *out,size_t cap,size_t *written) {
    uint8_t tmp[280],c[2],one=1,exit_flag=(uint8_t)(exits!=0);size_t p=6,total=26+n;
    if(!source||!out||!written)return DCC_ARGUMENT;
    if(n>254)return DCC_LIMIT;
    if(!text_valid((const uint8_t *)source,n))return DCC_FIELD;
    if(cap<total)return DCC_LIMIT;
    tmp[0]=tmp[1]=0x40;put16(tmp+2,(uint16_t)total);put16(tmp+4,0x5200);put16(c,component);
    tlv(tmp,&p,0,c,2);tlv(tmp,&p,1,(const uint8_t *)source,n);
    tlv(tmp,&p,2,&one,1);tlv(tmp,&p,3,&exit_flag,1);
    memcpy(out,tmp,p);*written=p;return DCC_OK;
}
DccResult dcc_stop(uint16_t component,uint8_t *out,size_t cap,size_t *written) {
    uint8_t b[12]={0x40,0x40,0,12,0x52,3,0,6,0,0,0,0};
    if(!out||!written)return DCC_ARGUMENT;if(cap<12)return DCC_LIMIT;
    put16(b+10,component);memcpy(out,b,12);*written=12;return DCC_OK;
}
void dcc_session_init(DccSession *s,DccCallback cb,void *context) {
    if(!s)return;memset(s,0,sizeof(*s));s->callback=cb;s->context=context;
}
static void reset(DccSession *s) {
    memset(s->frame,0,sizeof(s->frame));s->used=s->wanted=0;
    if(s->callback)s->callback(s->context,DCC_EVENT_RESET,s->epoch,NULL);
}
DccResult dcc_connect(DccSession *s,uint64_t now,uint64_t *token) {
    if(!s||!token)return DCC_ARGUMENT;if(s->epoch==UINT64_MAX)return DCC_LIMIT;
    ++s->epoch;s->connected=1;s->last_ms=s->clock_ms=now;reset(s);*token=s->epoch;return DCC_OK;
}
DccResult dcc_disconnect(DccSession *s,uint64_t token) {
    if(!s)return DCC_ARGUMENT;if(token!=s->epoch)return DCC_STALE;
    if(!s->connected)return DCC_DISCONNECTED;s->connected=0;reset(s);return DCC_OK;
}
DccResult dcc_feed(DccSession *s,uint64_t token,uint64_t now,const uint8_t *data,size_t n) {
    size_t p=0;if(!s||(!data&&n))return DCC_ARGUMENT;
    if(token!=s->epoch)return DCC_STALE;if(!s->connected)return DCC_DISCONNECTED;
    if(now<s->clock_ms)return DCC_TIME;
    s->clock_ms=now;
    while(p<n) {
        size_t target=s->wanted?s->wanted:4,need=target-s->used,take=n-p<need?n-p:need;
        memcpy(s->frame+s->used,data+p,take);p+=take;s->used+=take;
        if(!s->wanted&&s->used==4) {
            if(s->frame[0]!=0x40||s->frame[1]!=0x40||be16(s->frame+2)<6){reset(s);return DCC_FRAME;}
            s->wanted=be16(s->frame+2);
        }
        if(s->wanted&&s->used==s->wanted) {
            DccMessage msg;DccResult r=dcc_decode(s->frame,s->wanted,&msg);
            if(r){reset(s);return r;}
            if(msg.id!=0x5201&&msg.id!=0x5202&&msg.id!=0x5204){reset(s);return DCC_UNSUPPORTED;}
            if(s->callback)s->callback(s->context,DCC_EVENT_MESSAGE,s->epoch,&msg);
            s->last_ms=now;
            s->used=s->wanted=0;
        }
    }
    return DCC_OK;
}
DccResult dcc_expire(DccSession *s,uint64_t now,uint64_t timeout) {
    if(!s||!timeout)return DCC_ARGUMENT;if(!s->connected)return DCC_DISCONNECTED;
    if(now<s->clock_ms)return DCC_TIME;
    s->clock_ms=now;
    if(now-s->last_ms>=timeout){s->connected=0;reset(s);}
    return DCC_OK;
}
