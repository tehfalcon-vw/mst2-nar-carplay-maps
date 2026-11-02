#include "dcc_qnx.h"
#include "dcc_log.h"
#include "dcc_route.h"

#define INPUT "/dev/shmem/iap2.log"
#define OUTPUT "/dev/shmem/mst2-nar-rgi.state"
#define TEMP "/dev/shmem/mst2-nar-rgi.state.tmp"
#define COMPONENT 0x0010u

typedef struct {DccRoute route;unsigned fresh,current_empty;} Relay;

static void publish(const DccRoute *route){
    char snapshot[4096];size_t bytes=0;int fd;
    if(dcc_route_snapshot(route,snapshot,sizeof(snapshot),&bytes)!=DCC_OK)return;
    fd=open(TEMP,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(fd<0)return;
    if(write(fd,snapshot,bytes)!=(ssize_t)bytes){close(fd);return;}
    close(fd);rename(TEMP,OUTPUT);
}
static void received(void *opaque,const DccMessage *message){
    Relay *relay=(Relay *)opaque;
    if(dcc_route_apply(&relay->route,message,COMPONENT)!=DCC_OK)return;
    if(message->id==0x5201){const DccField *current=dcc_find(message,DCC_ROOT,13,0);relay->fresh=1;
        if(!relay->route.active||!current||current->length)relay->current_empty=0;
        else if(!relay->current_empty)relay->current_empty=1;
    }
    publish(&relay->route);
}
static void *run(void *unused){
    Relay relay;DccLogReader reader;char chunk[2048];int fd=-1;off_t position=0;unsigned idle=0;(void)unused;
    dcc_route_init(&relay.route);relay.fresh=0;relay.current_empty=0;dcc_log_init(&reader);publish(&relay.route);
    for(;;){
        ssize_t bytes;size_t messages=0;
        if(fd<0){fd=open(INPUT,O_RDONLY);if(fd<0){delay(250);continue;}position=0;dcc_log_init(&reader);}
        bytes=read(fd,chunk,sizeof(chunk));
        if(bytes>0){position+=bytes;dcc_log_feed(&reader,chunk,(size_t)bytes,received,&relay,&messages);continue;}
        if(bytes<0){close(fd);fd=-1;delay(100);continue;}
        delay(100);
        if(relay.current_empty&&relay.current_empty<31){relay.current_empty++;if(relay.current_empty==31){relay.route.current_index=-1;publish(&relay.route);}}
        if(relay.fresh){relay.fresh=0;idle=0;}else if(idle<50)idle++;
        if(idle==50&&relay.route.active){dcc_route_init(&relay.route);publish(&relay.route);idle++;}
        {off_t end=lseek(fd,0,SEEK_END);if(end<0){close(fd);fd=-1;continue;}
         if(end<position){position=0;dcc_log_init(&reader);dcc_route_init(&relay.route);relay.current_empty=0;publish(&relay.route);}
         lseek(fd,position,SEEK_SET);}
    }
    return NULL;
}
int dcc_relay_start(void){pthread_t thread;if(pthread_create(&thread,NULL,run,NULL)!=0)return -1;return pthread_detach(thread);}
