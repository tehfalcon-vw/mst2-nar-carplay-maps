#include "dcc_identify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static void w16(uint8_t *b,unsigned n){b[0]=(uint8_t)(n>>8);b[1]=(uint8_t)n;}
static unsigned r16(const uint8_t *b){return ((unsigned)b[0]<<8)|b[1];}
static void add(uint8_t *b,size_t *n,unsigned id,const void *v,size_t len){w16(b+*n,(unsigned)len+4);w16(b+*n+2,id);if(len)memcpy(b+*n+4,v,len);*n+=len+4;}
static const uint8_t *find(const uint8_t *b,size_t n,unsigned id,size_t *len){size_t p=6;while(p<n){unsigned size=r16(b+p);CHECK(size>=4&&size<=n-p);if(r16(b+p+2)==id){*len=size-4;return b+p+4;}p+=size;}return NULL;}
static int list_has(const uint8_t *v,size_t n,unsigned id){size_t p;for(p=0;p<n;p+=2)if(r16(v+p)==id)return 1;return 0;}
static DccIdentifyProfile profile(void){DccIdentifyProfile p={0x2222,"Delphi RGI",10,63,62,61,60,6,59,6};return p;}
static size_t fixture(uint8_t *b){
    size_t n=6,k=0;uint8_t tx[]={0x50,0,0x50,2},rx[]={0x50,1},opaque[]={0xff,0,0x40,0x40},inner[16],id[]={0,8};
    b[0]=b[1]=0x40;w16(b+4,0x1d01);add(b,&n,0,"VW",3);add(b,&n,6,tx,4);add(b,&n,7,rx,2);
    add(b,&n,0x7777,opaque,sizeof(opaque));add(inner,&k,0,id,2);add(b,&n,0x16,inner,k);w16(b+2,(unsigned)n);return n;
}
static void test_happy(void){
    uint8_t in[1024],out[1024],again[1024],original[1024];size_t n=fixture(in),used=0,used2=0,len=0;
    DccIdentifyProfile p=profile();DccIdentifyChanges c,c2;const uint8_t *v;unsigned id,seen=0;size_t x;
    memcpy(original,in,n);CHECK(dcc_identify_prepare(in,n,&p,out,sizeof(out),&used,&c)==DCC_OK);
    CHECK(!memcmp(original,in,n));CHECK(r16(out+2)==used&&r16(out+4)==0x1d01);
    CHECK(c.component_added==1&&c.send_ids_added==2&&c.receive_ids_added==3&&c.input_bytes==n&&c.output_bytes==used);
    v=find(out,used,6,&len);CHECK(v&&len==8&&list_has(v,len,0x5000)&&list_has(v,len,0x5002)&&list_has(v,len,0x5200)&&list_has(v,len,0x5203));
    v=find(out,used,7,&len);CHECK(v&&len==8&&list_has(v,len,0x5001)&&list_has(v,len,0x5201)&&list_has(v,len,0x5202)&&list_has(v,len,0x5204));
    for(id=0;id<3;id++){
        unsigned tag=id==0?0:id==1?0x7777:0x16;size_t before_len=0,after_len=0;
        const uint8_t *before=find(in,n,tag,&before_len),*after=find(out,used,tag,&after_len);
        CHECK(before&&after&&before_len==after_len&&!memcmp(before-4,after-4,before_len+4));
    }
    v=find(out,used,0x1e,&len);CHECK(v&&len==63);
    for(x=0;x<len;){unsigned size=r16(v+x),tag=r16(v+x+2);CHECK(tag<9&&!(seen&(1u<<tag)));seen|=1u<<tag;
        if(tag==1)CHECK(size==15&&!memcmp(v+x+4,"Delphi RGI\0",11));
        else{unsigned wanted=tag==0?0x2222:tag==2?63:tag==3?62:tag==4?61:tag==5?60:tag==6?6:tag==7?59:6;CHECK(size==6&&r16(v+x+4)==wanted);}
        x+=size;
    }
    CHECK(seen==511);
    CHECK(dcc_identify_prepare(out,used,&p,again,sizeof(again),&used2,&c2)==DCC_OK);
    CHECK(used2==used&&!memcmp(out,again,used)&&!c2.component_added&&!c2.send_ids_added&&!c2.receive_ids_added);
    puts("PASS nine component fields, message directions, unknown-field preservation, input immutability and idempotence");
}
static void rejected(uint8_t *in,size_t n,DccIdentifyProfile *p,size_t cap,DccResult expected){
    static uint8_t out[DCC_MAX_FRAME],copy[DCC_MAX_FRAME];DccIdentifyChanges changes,old;size_t written=0x1234;
    memset(out,0xa5,sizeof(out));memcpy(copy,out,sizeof(out));memset(&changes,0x5a,sizeof(changes));old=changes;
    CHECK(dcc_identify_prepare(in,n,p,out,cap,&written,&changes)==expected);
    CHECK(!memcmp(out,copy,sizeof(out))&&written==0x1234&&!memcmp(&changes,&old,sizeof(changes)));
}
static void test_errors(void){
    uint8_t in[2048],ids[128],bad=0,nested[8],zero_id[]={0x22,0x22};size_t n,k,len;DccIdentifyProfile p=profile();
    n=fixture(in);rejected(in,n,&p,4,DCC_LIMIT);
    p.name="\xc0\x80";p.name_bytes=2;rejected(in,n,&p,2048,DCC_FIELD);p=profile();
    p.maneuver_slots=0;rejected(in,n,&p,2048,DCC_FIELD);p=profile();
    n=fixture(in);p.component_id=8;rejected(in,n,&p,2048,DCC_CONFLICT);p=profile();
    n=fixture(in);add(in,&n,6,NULL,0);w16(in+2,(unsigned)n);rejected(in,n,&p,2048,DCC_DUPLICATE);
    n=6;in[0]=in[1]=0x40;w16(in+4,0x1d01);add(in,&n,6,&bad,1);w16(in+2,(unsigned)n);rejected(in,n,&p,2048,DCC_FIELD);
    n=fixture(in);in[n++]=0;w16(in+2,(unsigned)n);rejected(in,n,&p,2048,DCC_TLV);
    n=fixture(in);k=0;add(nested,&k,0,zero_id,2);add(in,&n,0x1e,nested,k);w16(in+2,(unsigned)n);rejected(in,n,&p,2048,DCC_CONFLICT);
    n=fixture(in);w16(in+4,0x5201);rejected(in,n,&p,2048,DCC_UNSUPPORTED);
    n=6;in[0]=in[1]=0x40;w16(in+4,0x1d01);for(k=0;k<64;k++)w16(ids+k*2,(unsigned)k);add(in,&n,6,ids,128);w16(in+2,(unsigned)n);rejected(in,n,&p,2048,DCC_LIMIT);
    n=fixture(in);{DccIdentifyChanges c;len=0;CHECK(dcc_identify_prepare(in,n,&p,in,sizeof(in),&len,&c)==DCC_ARGUMENT);}
    puts("PASS atomic rejection: conflicts, duplicates, malformed TLVs, odd lists, capacity, UTF-8 and overlapping buffers");
}
static void test_missing_and_edges(void){
    static uint8_t in[DCC_MAX_FRAME],out[DCC_MAX_FRAME];DccIdentifyProfile p=profile();DccIdentifyChanges c;size_t n=6,used=0,len;const uint8_t *v;
    in[0]=in[1]=0x40;w16(in+2,6);w16(in+4,0x1d01);
    CHECK(dcc_identify_prepare(in,n,&p,out,sizeof(out),&used,&c)==DCC_OK&&c.send_ids_added==2&&c.receive_ids_added==3);
    v=find(out,used,6,&len);CHECK(v&&len==4);v=find(out,used,7,&len);CHECK(v&&len==6);
    {uint8_t partial[64],tx[]={0x52,0},rx[]={0x52,1,0x52,4};size_t q=6;
     partial[0]=partial[1]=0x40;w16(partial+4,0x1d01);add(partial,&q,6,tx,2);add(partial,&q,7,rx,4);w16(partial+2,(unsigned)q);
     CHECK(dcc_identify_prepare(partial,q,&p,out,sizeof(out),&used,&c)==DCC_OK&&c.send_ids_added==1&&c.receive_ids_added==1);
     v=find(out,used,6,&len);CHECK(v&&len==4&&r16(v)==0x5200&&r16(v+2)==0x5203);
     v=find(out,used,7,&len);CHECK(v&&len==6&&r16(v)==0x5201&&r16(v+2)==0x5204&&r16(v+4)==0x5202);}
    memset(in+6,0,sizeof(in)-6);w16(in+6,65529);w16(in+8,0x7777);w16(in+2,65535);
    rejected(in,sizeof(in),&p,sizeof(out),DCC_LIMIT);
    puts("PASS missing/partial message lists, no duplicate new IDs, original order preserved; frame-size overflow refused");
}
static void test_mutations(void){
    uint8_t base[1024],in[1024],out[2048],old[2048];DccIdentifyProfile p=profile();size_t n=fixture(base),i,k,used;DccIdentifyChanges c;
    for(i=0;i<n;i++)for(k=0;k<8;k++){
        memcpy(in,base,n);in[i]^=(uint8_t)(1u<<k);memset(out,0xa5,sizeof(out));memcpy(old,out,sizeof(out));used=0;
        if(dcc_identify_prepare(in,n,&p,out,sizeof(out),&used,&c)!=DCC_OK)CHECK(!memcmp(out,old,sizeof(out))&&used==0);
        else CHECK(used<=sizeof(out)&&r16(out+2)==used);
    }
    puts("PASS all single-bit mutations of synthetic Identify fixture: bounded output and atomic failure");
}
int main(void){test_happy();test_errors();test_missing_and_edges();test_mutations();puts("IDENTIFY TESTS PASSED (buffer-only; no phone negotiation tested)");return 0;}
