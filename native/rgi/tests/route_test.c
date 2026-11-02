#include "dcc_route.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static void w16(uint8_t *p,unsigned value){p[0]=(uint8_t)(value>>8);p[1]=(uint8_t)value;}
static size_t begin(uint8_t *b,unsigned id){b[0]=b[1]=0x40;w16(b+4,id);return 6;}
static void add(uint8_t *b,size_t *n,unsigned id,const void *value,size_t bytes){w16(b+*n,(unsigned)bytes+4);w16(b+*n+2,id);if(bytes)memcpy(b+*n+4,value,bytes);*n+=bytes+4;}
static void done(uint8_t *b,size_t n){w16(b+2,(unsigned)n);}
static DccResult apply(DccRoute *r,uint8_t *b,size_t n){DccMessage m;DccResult x=dcc_decode(b,n,&m);return x?x:dcc_route_apply(r,&m,0x10);}
static void route_and_maneuver(void){
    uint8_t b[512],state=1,type=2,lht=0,indices[2]={0,7},distance[4]={0,0,1,244},remaining[8]={0,0,0,0,0,0,3,132};
    uint8_t destination[4]={0,0,0x2e,0xe0},index[2]={0,7},step[4]={0,0,3,0xe8},angle[2]={0,90},roads[4]={0xff,0xa6,0,0};
    DccRoute r,copy;const DccRouteManeuver *m;size_t n=begin(b,0x5202);dcc_route_init(&r);
    add(b,&n,1,index,2);add(b,&n,3,&type,1);add(b,&n,4,"Broadway",8);add(b,&n,5,step,4);add(b,&n,8,&lht,1);add(b,&n,10,roads,4);add(b,&n,11,angle,2);done(b,n);CHECK(apply(&r,b,n)==DCC_OK);
    n=begin(b,0x5201);add(b,&n,1,&state,1);add(b,&n,3,"Main St",7);add(b,&n,6,remaining,8);add(b,&n,7,destination,4);add(b,&n,10,distance,4);add(b,&n,13,indices,2);done(b,n);CHECK(apply(&r,b,n)==DCC_OK);
    m=dcc_route_current_maneuver(&r);CHECK(r.active&&r.state==1&&r.distance_m==500&&r.remaining_s==900&&r.destination_m==12000);CHECK(!strcmp(r.road,"Main St")&&m&&m->index==7&&m->type==2&&!strcmp(m->street,"Broadway")&&m->step_m==1000&&m->angle==90);CHECK(m->road_count==2&&m->roads[0]==-90&&m->roads[1]==0);
    n=begin(b,0x5201);add(b,&n,3,"",0);add(b,&n,13,"",0);done(b,n);CHECK(apply(&r,b,n)==DCC_OK&&r.current_index==7&&!strcmp(r.road,"Main St"));
    copy=r;n=begin(b,0x5202);type=99;add(b,&n,1,index,2);add(b,&n,3,&type,1);done(b,n);CHECK(apply(&r,b,n)==DCC_FIELD&&!memcmp(&r,&copy,sizeof(r)));
    {char text[1024];size_t used=0;CHECK(dcc_route_snapshot(&r,text,sizeof(text),&used)==DCC_OK);text[used]=0;CHECK(strstr(text,"active=1\n")&&strstr(text,"type=2\n")&&strstr(text,"roads=-90,0\n")&&strstr(text,"street=Main St\n"));}
    puts("PASS atomic route, maneuver cache and stable snapshot");
}
static void lanes_and_addressing(void){
    uint8_t b[512],inner[128],route_state=1,lane_index[2]={0,3},show=1,set_index[2]={0,3},position[2]={0,1},status=2,angles[4]={0xff,0xa6,0,0},highlight[2]={0,0};
    DccRoute r,copy;const DccRouteLaneSet *set;size_t n,p=0;dcc_route_init(&r);
    n=begin(b,0x5204);add(inner,&p,0,position,2);add(inner,&p,1,&status,1);add(inner,&p,2,angles,4);add(inner,&p,3,highlight,2);add(b,&n,1,set_index,2);add(b,&n,2,inner,p);done(b,n);CHECK(apply(&r,b,n)==DCC_OK);
    n=begin(b,0x5201);add(b,&n,1,&route_state,1);add(b,&n,16,lane_index,2);add(b,&n,18,&show,1);done(b,n);CHECK(apply(&r,b,n)==DCC_OK);set=dcc_route_current_lanes(&r);CHECK(set&&set->count==1&&set->lanes[0].index==1&&set->lanes[0].status==2&&set->lanes[0].angle_count==2&&set->lanes[0].angles[0]==-90);
    copy=r;n=begin(b,0x5201);{uint8_t other[2]={0,9};add(b,&n,0,other,2);}add(b,&n,10,(uint8_t[]){0,0,0,1},4);done(b,n);CHECK(apply(&r,b,n)==DCC_UNSUPPORTED&&!memcmp(&r,&copy,sizeof(r)));
    puts("PASS lane cache and component addressing");
}
static void reset_route(void){uint8_t b[32],state=1,stop=0;DccRoute r;size_t n;dcc_route_init(&r);n=begin(b,0x5201);add(b,&n,1,&state,1);done(b,n);CHECK(apply(&r,b,n)==DCC_OK&&r.active);n=begin(b,0x5201);add(b,&n,1,&stop,1);done(b,n);CHECK(apply(&r,b,n)==DCC_OK&&!r.active&&r.current_index==-1&&r.distance_m==-1);puts("PASS guidance stop clears every cache");}
int main(void){route_and_maneuver();lanes_and_addressing();reset_route();puts("ROUTE MODEL TESTS PASSED");return 0;}
