#include "dcc_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
typedef struct{unsigned calls;uint16_t id;uint16_t fields;} Sink;
static void capture(void *context,const DccMessage *message){
    Sink *sink=(Sink *)context;sink->calls++;sink->id=message->id;sink->fields=message->count;
}
static void split_everywhere(void){
    const char text[]="noise --?-- [msgid=5201] CTRL[40 40 00 0b 52 01 00 05 00 01 01] CKSUM[aa]";
    size_t split;
    for(split=0;split<sizeof(text)-1;split++){
        DccLogReader reader;Sink sink={0};size_t count=99,second=99;
        dcc_log_init(&reader);
        CHECK(dcc_log_feed(&reader,text,split,capture,&sink,&count)==DCC_OK);
        CHECK(dcc_log_feed(&reader,text+split,sizeof(text)-1-split,capture,&sink,&second)==DCC_OK);
        CHECK(count+second==1&&sink.calls==1&&sink.id==0x5201&&sink.fields==1);
    }
    puts("PASS every split of stock CTRL diagnostic framing");
}
static void malformed_is_atomic(void){
    const char bad[]="CTRL[40 4x]",good[]="CTRL[40 40 00 06 52 01]";
    DccLogReader reader;Sink sink={0};size_t count=99;
    dcc_log_init(&reader);
    CHECK(dcc_log_feed(&reader,bad,sizeof(bad)-1,capture,&sink,&count)==DCC_FRAME);
    CHECK(count==0&&sink.calls==0&&reader.used==0&&!reader.capturing);
    CHECK(dcc_log_feed(&reader,good,sizeof(good)-1,capture,&sink,&count)==DCC_OK);
    CHECK(count==1&&sink.calls==1&&sink.id==0x5201);
    puts("PASS malformed capture reset and recovery");
}
int main(void){split_everywhere();malformed_is_atomic();puts("LOG READER TESTS PASSED");return 0;}
