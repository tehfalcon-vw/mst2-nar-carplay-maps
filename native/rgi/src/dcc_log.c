#include "dcc_log.h"
#include <string.h>

static int nibble(char c){
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
static void clear_capture(DccLogReader *r){
    r->used=0;r->high_nibble=-1;r->capturing=0;
}
void dcc_log_init(DccLogReader *r){
    if(r){memset(r,0,sizeof(*r));r->high_nibble=-1;}
}
DccResult dcc_log_feed(DccLogReader *r,const char *text,size_t bytes,
    DccLogCallback callback,void *context,size_t *messages){
    static const char marker[]="CTRL[";size_t i,count=0;DccResult result=DCC_OK;
    if(!r||(!text&&bytes)||!messages)return DCC_ARGUMENT;
    for(i=0;i<bytes;i++){
        char c=text[i];int n;
        if(!r->capturing){
            if(c==marker[r->marker]){
                if(++r->marker==sizeof(marker)-1){r->marker=0;r->capturing=1;r->used=0;r->high_nibble=-1;}
            }else r->marker=c==marker[0]?1u:0u;
            continue;
        }
        if(c==']'){
            DccMessage message;DccResult decoded;
            if(r->high_nibble>=0||r->used<6){result=DCC_FRAME;clear_capture(r);continue;}
            decoded=dcc_decode(r->frame,r->used,&message);
            if(decoded==DCC_OK){
                if(callback)callback(context,&message);
                ++count;
            }else result=decoded;
            clear_capture(r);
            continue;
        }
        n=nibble(c);
        if(n>=0){
            if(r->high_nibble<0)r->high_nibble=n;
            else{
                if(r->used==sizeof(r->frame)){result=DCC_LIMIT;clear_capture(r);continue;}
                r->frame[r->used++]=(uint8_t)((r->high_nibble<<4)|n);r->high_nibble=-1;
            }
        }else if(c!=' '&&c!='\t'&&c!='\r'&&c!='\n'){
            result=DCC_FRAME;clear_capture(r);
        }
    }
    *messages=count;
    return result;
}
