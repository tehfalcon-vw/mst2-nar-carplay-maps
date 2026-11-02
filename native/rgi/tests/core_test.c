#include "dcc_rgi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

static void w16(uint8_t *p,unsigned x){p[0]=(uint8_t)(x>>8);p[1]=(uint8_t)x;}
static size_t frame(uint8_t *b,unsigned msg){b[0]=b[1]=0x40;w16(b+4,msg);return 6;}
static void add(uint8_t *b,size_t *p,unsigned id,const uint8_t *v,size_t n){w16(b+*p,(unsigned)n+4);w16(b+*p+2,id);if(n)memcpy(b+*p+4,v,n);*p+=n+4;}
static void finish(uint8_t *b,size_t n){w16(b+2,(unsigned)n);}
static void fail_atomic(uint8_t *b,size_t n,DccResult expected){DccMessage a,c;memset(&a,0xa5,sizeof(a));c=a;CHECK(dcc_decode(b,n,&a)==expected);CHECK(!memcmp(&a,&c,sizeof(a)));}
typedef struct{unsigned messages,resets;uint64_t epoch;uint64_t distance;int has_distance;} Sink;
static void event(void *ctx,DccEvent kind,uint64_t epoch,const DccMessage *m){
    Sink *s=(Sink *)ctx;s->epoch=epoch;
    if(kind==DCC_EVENT_RESET){s->resets++;s->has_distance=0;s->distance=0;return;}
    s->messages++;
    if(m->id==0x5201){const DccField *f=dcc_find(m,DCC_ROOT,10,0);if(f){CHECK(dcc_unsigned(m,f,&s->distance)==DCC_OK);s->has_distance=1;}}
}
static void test_values(void){
    uint8_t b[256],d[4]={0,0,1,244},eta[8]={0,0x20,0,0,0,0,0,1},state=0;size_t n=frame(b,0x5201),used;
    DccMessage m;uint64_t x=0;char text[32];const uint8_t street[]={'V','i','a',' ',0xc3,0xa8};
    add(b,&n,1,&state,1);add(b,&n,10,d,4);add(b,&n,5,eta,8);add(b,&n,3,street,sizeof(street));add(b,&n,0x7777,d,4);finish(b,n);
    CHECK(dcc_decode(b,n,&m)==DCC_OK);CHECK(m.count==5);
    CHECK(dcc_unsigned(&m,dcc_find(&m,DCC_ROOT,10,0),&x)==DCC_OK&&x==500);
    CHECK(dcc_unsigned(&m,dcc_find(&m,DCC_ROOT,5,0),&x)==DCC_OK&&x==UINT64_C(9007199254740993));
    CHECK(dcc_find(&m,DCC_ROOT,4,0)==NULL); /* Missing is not a fabricated empty destination. */
    CHECK(dcc_find(&m,DCC_ROOT,1,0)!=NULL); /* Explicit zero is present. */
    CHECK(dcc_find(&m,DCC_ROOT,0x7777,0)!=NULL);
    CHECK(dcc_copy_text(&m,dcc_find(&m,DCC_ROOT,3,0),text,sizeof(text),&used)==DCC_OK&&used==sizeof(street));
    memset(text,'x',sizeof(text));used=999;
    CHECK(dcc_copy_text(&m,dcc_find(&m,DCC_ROOT,3,0),text,3,&used)==DCC_LIMIT&&text[0]=='x'&&used==999);
    puts("PASS numeric widths, presence, UTF-8, unknown fields and lossless text bounds");
}
static void test_rejection(void){
    uint8_t b[256],one=1,v[3]={0,0,1};size_t n=frame(b,0x5201);
    add(b,&n,1,&one,1);add(b,&n,10,v,3);finish(b,n);fail_atomic(b,n,DCC_FIELD);
    n=frame(b,0x5201);add(b,&n,1,&one,1);add(b,&n,1,&one,1);finish(b,n);fail_atomic(b,n,DCC_DUPLICATE);
    n=frame(b,0x5201);v[0]=0xc0;v[1]=0x80;add(b,&n,3,v,2);finish(b,n);fail_atomic(b,n,DCC_FIELD);
    n=frame(b,0x5201);add(b,&n,0x8888,&one,1);finish(b,n);b[6]=0xff;fail_atomic(b,n,DCC_TLV);
    n=frame(b,0x5201);add(b,&n,1,&one,1);finish(b,n);b[n++]=1;finish(b,n);fail_atomic(b,n,DCC_TLV);
    n=frame(b,0x5201);finish(b,n);b[0]=0;fail_atomic(b,n,DCC_FRAME);
    n=frame(b,0x9999);finish(b,n);fail_atomic(b,n,DCC_UNSUPPORTED);
    puts("PASS malformed frames are atomic; duplicates, UTF-8 and unsupported messages rejected");
}
static void test_lanes(void){
    uint8_t b[512],inner[128],id[2]={0,2},status=1,angles[4]={0xff,0xa6,0,90},highlight[2]={3,0xe8};
    size_t n=frame(b,0x5204),p=0;DccMessage m;int16_t angle=0;const DccField *container,*f;
    add(inner,&p,0,id,2);add(inner,&p,1,&status,1);add(inner,&p,2,angles,4);add(inner,&p,3,highlight,2);
    add(b,&n,1,id,2);add(b,&n,2,inner,p);finish(b,n);CHECK(dcc_decode(b,n,&m)==DCC_OK);
    container=dcc_find(&m,DCC_ROOT,2,0);CHECK(container);f=dcc_find(&m,(uint16_t)(container-m.fields),3,0);
    CHECK(dcc_signed16(&m,f,&angle)==DCC_OK&&angle==1000); /* Preserve unknown-angle sentinel. */
    n=frame(b,0x5202);add(b,&n,1,id,2);add(b,&n,11,angles,2);finish(b,n);CHECK(dcc_decode(b,n,&m)==DCC_OK);
    CHECK(dcc_signed16(&m,dcc_find(&m,DCC_ROOT,11,0),&angle)==DCC_OK&&angle==-90);
    p=0;add(inner,&p,0,id,2);n=frame(b,0x5204);add(b,&n,2,inner,p);finish(b,n);fail_atomic(b,n,DCC_FIELD);
    p=0;add(inner,&p,1,&status,1);n=frame(b,0x5204);add(b,&n,2,inner,p);finish(b,n);fail_atomic(b,n,DCC_FIELD);
    p=0;add(inner,&p,0,id,2);add(inner,&p,1,&status,1);inner[p++]=0;n=frame(b,0x5204);add(b,&n,2,inner,p);finish(b,n);fail_atomic(b,n,DCC_TLV);
    puts("PASS nested lanes, signed angles and sentinel preservation; missing lane identity/status rejected");
}
static void test_encoders(void){
    uint8_t b[512],copy[512];size_t n=0;DccMessage m;uint64_t c;
    CHECK(dcc_start(7,"Delphi RGI",10,1,b,sizeof(b),&n)==DCC_OK);
    CHECK(dcc_decode(b,n,&m)==DCC_OK&&m.id==0x5200);
    CHECK(dcc_unsigned(&m,dcc_find(&m,DCC_ROOT,0,0),&c)==DCC_OK&&c==7);
    memset(b,0xa5,sizeof(b));memcpy(copy,b,sizeof(b));n=123;
    CHECK(dcc_start(7,"Delphi RGI",10,1,b,5,&n)==DCC_LIMIT&&n==123&&!memcmp(b,copy,sizeof(b)));
    CHECK(dcc_stop(7,b,sizeof(b),&n)==DCC_OK&&n==12);CHECK(dcc_decode(b,n,&m)==DCC_OK&&m.id==0x5203);
    CHECK(dcc_unsigned(&m,dcc_find(&m,DCC_ROOT,0,0),&c)==DCC_OK&&c==7);
    puts("PASS start/stop buffer encoders; insufficient capacity leaves outputs unchanged");
}
static void test_session(void){
    static DccSession s;Sink sink={0};uint64_t token,old;uint8_t b[128],joined[256],d[4]={0,0,0,0};size_t n=frame(b,0x5201),i;
    add(b,&n,10,d,4);finish(b,n);dcc_session_init(&s,event,&sink);
    CHECK(dcc_connect(&s,100,&token)==DCC_OK&&sink.resets==1);old=token;
    for(i=0;i<n;i++)CHECK(dcc_feed(&s,token,100+i,b+i,1)==DCC_OK);
    CHECK(sink.messages==1&&sink.has_distance&&sink.distance==0);
    memcpy(joined,b,n);memcpy(joined+n,b,n);CHECK(dcc_feed(&s,token,200,joined,n*2)==DCC_OK&&sink.messages==3);
    CHECK(dcc_feed(&s,token,199,b,n)==DCC_TIME&&sink.messages==3);
    CHECK(dcc_feed(&s,token,201,b,3)==DCC_OK);
    CHECK(dcc_disconnect(&s,token)==DCC_OK&&!sink.has_distance&&s.used==0);
    CHECK(dcc_feed(&s,token,202,b,n)==DCC_DISCONNECTED);
    CHECK(dcc_connect(&s,300,&token)==DCC_OK&&token!=old);
    CHECK(dcc_feed(&s,old,301,b,n)==DCC_STALE&&sink.messages==3);
    CHECK(dcc_feed(&s,token,302,b,n)==DCC_OK&&sink.messages==4);
    CHECK(dcc_expire(&s,303,10)==DCC_OK&&s.connected);
    CHECK(dcc_expire(&s,312,10)==DCC_OK&&!s.connected&&!sink.has_distance);
    CHECK(dcc_connect(&s,400,&token)==DCC_OK);b[6]=0xff;
    CHECK(dcc_feed(&s,token,401,b,n)==DCC_TLV&&sink.messages==4&&s.used==0);
    b[6]=0;CHECK(dcc_connect(&s,500,&token)==DCC_OK);
    CHECK(dcc_feed(&s,token,500,b,n)==DCC_OK&&sink.messages==5);
    CHECK(dcc_feed(&s,token,505,b,1)==DCC_OK);
    CHECK(dcc_feed(&s,token,504,NULL,0)==DCC_TIME);
    CHECK(dcc_feed(&s,token,509,NULL,0)==DCC_OK);
    CHECK(dcc_expire(&s,510,10)==DCC_OK&&!s.connected&&s.used==0&&!sink.has_distance);
    puts("PASS fragmentation, coalescing, zero distances, disconnect, stale sessions, clock rollback and expiry");
}
static void test_limits_and_mutations(void){
    uint8_t b[2048],one=1;size_t n=frame(b,0x5201),i,j;DccMessage m,unchanged;uint32_t seed=0x319879;
    for(i=0;i<DCC_MAX_FIELDS+1;i++) {
        add(b,&n,0x8000+(unsigned)i,&one,1);
    }
    finish(b,n);
    fail_atomic(b,n,DCC_LIMIT);
    /* Synthetic mutation testing, not a real CarPlay capture corpus. */
    for(i=0;i<20000;i++) {
        size_t len;seed=seed*1664525u+1013904223u;len=seed%sizeof(b);
        for(j=0;j<len;j++){seed=seed*1664525u+1013904223u;b[j]=(uint8_t)(seed>>24);}
        if(len>=6){b[0]=b[1]=0x40;finish(b,len);w16(b+4,(unsigned)(0x5201+(i%3)));}
        memset(&m,0x5a,sizeof(m));unchanged=m;
        if(dcc_decode(b,len,&m)!=DCC_OK)CHECK(!memcmp(&m,&unchanged,sizeof(m)));
    }
    puts("PASS field limit and 20000 deterministic synthetic malformed-input mutations");
    n=frame(b,0x5201);{uint8_t dist[4]={0,0,1,44};add(b,&n,10,dist,4);add(b,&n,3,(const uint8_t *)"Via Roma",8);}finish(b,n);
    {uint8_t baseline[128];size_t k;memcpy(baseline,b,n);
     for(i=0;i<n;i++)for(k=0;k<8;k++){
        memcpy(b,baseline,n);b[i]^=(uint8_t)(1u<<k);memset(&m,0x5a,sizeof(m));unchanged=m;
        if(dcc_decode(b,n,&m)!=DCC_OK)CHECK(!memcmp(&m,&unchanged,sizeof(m)));
        else for(j=0;j<m.count;j++)CHECK((size_t)m.fields[j].offset+m.fields[j].length<=m.length);
     }}
    puts("PASS every single-bit mutation of a structured route frame: bounded views and atomic rejection");
}
int main(void){test_values();test_rejection();test_lanes();test_encoders();test_session();test_limits_and_mutations();puts("ALL CORE TESTS PASSED (host only; no Delphi integration)");return 0;}

