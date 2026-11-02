#include "dcc_route.h"
#include <limits.h>
#include <string.h>

static int16_t read_signed(const uint8_t *p){
    uint16_t v=(uint16_t)(((uint16_t)p[0]<<8)|p[1]);
    return (int16_t)(v<=32767?(int32_t)v:(int32_t)v-65536);
}
static DccResult number(const DccMessage *m,uint16_t parent,uint16_t id,uint64_t *v,int *seen){
    const DccField *f=dcc_find(m,parent,id,0);DccResult r;
    *seen=f!=NULL;if(!f)return DCC_OK;r=dcc_unsigned(m,f,v);return r;
}
static DccResult integer(const DccMessage *m,uint16_t parent,uint16_t id,int *v){
    uint64_t n=0;int seen=0;DccResult r=number(m,parent,id,&n,&seen);
    if(r||!seen)return r;
    if(n>INT_MAX)return DCC_LIMIT;
    *v=(int)n;
    return DCC_OK;
}
static DccResult text(const DccMessage *m,uint16_t id,char *out){
    const DccField *f=dcc_find(m,DCC_ROOT,id,0);size_t used=0;
    if(!f)return DCC_OK;
    return dcc_copy_text(m,f,out,DCC_ROUTE_TEXT,&used);
}
static int addressed(const DccMessage *m,uint16_t component){
    size_t occurrence=0;int any=0;
    for(;;){const DccField *f=dcc_find(m,DCC_ROOT,0,occurrence++);uint64_t value;
        if(!f)break;
        any=1;
        if(dcc_unsigned(m,f,&value)==DCC_OK&&value==component)return 1;
    }
    return !any;
}
void dcc_route_init(DccRoute *r){
    if(!r)return;
    memset(r,0,sizeof(*r));r->current_index=-1;r->lane_index=-1;
    r->distance_m=-1;r->destination_m=-1;r->remaining_s=-1;
}
static DccRouteManeuver *maneuver(DccRoute *r,int index){
    unsigned i;DccRouteManeuver *m;
    for(i=0;i<DCC_ROUTE_CACHE;i++)if(r->maneuvers[i].valid&&r->maneuvers[i].index==index)return &r->maneuvers[i];
    m=&r->maneuvers[r->maneuver_next++%DCC_ROUTE_CACHE];memset(m,0,sizeof(*m));
    m->valid=1;m->index=index;m->type=-1;m->angle=1000;return m;
}
static DccRouteLaneSet *lane_set(DccRoute *r,int index){
    unsigned i;DccRouteLaneSet *set;
    for(i=0;i<DCC_ROUTE_CACHE;i++)if(r->lane_sets[i].valid&&r->lane_sets[i].index==index)return &r->lane_sets[i];
    set=&r->lane_sets[r->lane_next++%DCC_ROUTE_CACHE];memset(set,0,sizeof(*set));set->valid=1;set->index=index;return set;
}
static DccResult apply_route(DccRoute *r,const DccMessage *m){
    const DccField *f;uint64_t n;int seen;DccResult result;char road[DCC_ROUTE_TEXT];
    result=number(m,DCC_ROOT,1,&n,&seen);if(result)return result;
    if(seen){if(n>6)return DCC_FIELD;if(!n){dcc_route_init(r);return DCC_OK;}r->state=(int)n;r->active=1;}
    result=number(m,DCC_ROOT,10,&n,&seen);if(result)return result;
    if(seen){if(n>INT_MAX)return DCC_LIMIT;r->distance_m=(int)n;}
    result=number(m,DCC_ROOT,6,&n,&seen);if(result)return result;
    if(seen){if(n>INT64_MAX)return DCC_LIMIT;r->remaining_s=(int64_t)n;}
    result=number(m,DCC_ROOT,7,&n,&seen);if(result)return result;
    if(seen){if(n>INT_MAX)return DCC_LIMIT;r->destination_m=(int)n;}
    memset(road,0,sizeof(road));result=text(m,3,road);if(result)return result;
    if(road[0])memcpy(r->road,road,sizeof(r->road));
    f=dcc_find(m,DCC_ROOT,13,0);
    if(f){if(f->length%2)return DCC_FIELD;if(f->length)r->current_index=((int)m->bytes[f->offset]<<8)|m->bytes[f->offset+1];}
    result=integer(m,DCC_ROOT,16,&r->lane_index);if(result)return result;
    result=number(m,DCC_ROOT,18,&n,&seen);if(result)return result;
    if(seen){if(n>1)return DCC_FIELD;r->show_lanes=(int)n;}
    return DCC_OK;
}
static DccResult apply_maneuver(DccRoute *r,const DccMessage *msg){
    const DccField *f;DccRouteManeuver *m;uint64_t n;int seen,index;size_t occurrence;DccResult result;
    f=dcc_find(msg,DCC_ROOT,1,0);if(!f)return DCC_FIELD;
    result=dcc_unsigned(msg,f,&n);if(result||n>INT_MAX)return result?result:DCC_LIMIT;index=(int)n;m=maneuver(r,index);
    result=number(msg,DCC_ROOT,3,&n,&seen);if(result)return result;if(seen){if(n>53)return DCC_FIELD;m->type=(int)n;}
    result=number(msg,DCC_ROOT,8,&n,&seen);if(result)return result;if(seen){if(n>1)return DCC_FIELD;m->left_hand=(int)n;}
    result=number(msg,DCC_ROOT,5,&n,&seen);if(result)return result;if(seen){if(n>INT_MAX)return DCC_LIMIT;m->step_m=(int)n;}
    f=dcc_find(msg,DCC_ROOT,11,0);if(f){int16_t angle;result=dcc_signed16(msg,f,&angle);if(result)return result;m->angle=(int)angle;}
    result=text(msg,4,m->street);if(result)return result;result=text(msg,2,m->description);if(result)return result;
    if(dcc_find(msg,DCC_ROOT,10,0)){m->road_count=0;for(occurrence=0;;occurrence++){
        f=dcc_find(msg,DCC_ROOT,10,occurrence);if(!f)break;
        if(f->length%2)return DCC_FIELD;
        {size_t at;for(at=0;at<f->length;at+=2){if(m->road_count==DCC_ROUTE_ROADS)return DCC_LIMIT;
            m->roads[m->road_count++]=read_signed(msg->bytes+f->offset+at);}}
    }}
    return DCC_OK;
}
static DccResult apply_lanes(DccRoute *r,const DccMessage *msg){
    const DccField *f;DccRouteLaneSet *set;uint64_t n;int index;size_t occurrence;DccResult result;
    f=dcc_find(msg,DCC_ROOT,1,0);if(!f)return DCC_FIELD;
    result=dcc_unsigned(msg,f,&n);if(result||n>INT_MAX)return result?result:DCC_LIMIT;index=(int)n;set=lane_set(r,index);set->count=0;
    for(occurrence=0;;occurrence++){
        const DccField *container=dcc_find(msg,DCC_ROOT,2,occurrence),*nested;DccRouteLane *lane;uint16_t parent;size_t angle;
        if(!container)break;
        if(set->count==DCC_ROUTE_LANES)return DCC_LIMIT;
        parent=(uint16_t)(container-msg->fields);lane=&set->lanes[set->count];memset(lane,0,sizeof(*lane));lane->highlight=1000;
        nested=dcc_find(msg,parent,0,0);if(!nested)return DCC_FIELD;result=dcc_unsigned(msg,nested,&n);if(result||n>INT_MAX)return result?result:DCC_LIMIT;lane->index=(int)n;
        nested=dcc_find(msg,parent,1,0);if(!nested)return DCC_FIELD;result=dcc_unsigned(msg,nested,&n);if(result||n>2)return result?result:DCC_FIELD;lane->status=(int)n;
        nested=dcc_find(msg,parent,3,0);if(nested){int16_t value;result=dcc_signed16(msg,nested,&value);if(result)return result;lane->highlight=value;}
        for(angle=0;;angle++){nested=dcc_find(msg,parent,2,angle);if(!nested)break;if(nested->length%2)return DCC_FIELD;
            {size_t at;for(at=0;at<nested->length;at+=2){if(lane->angle_count==DCC_ROUTE_ANGLES)return DCC_LIMIT;lane->angles[lane->angle_count++]=read_signed(msg->bytes+nested->offset+at);}}}
        set->count++;
    }
    return DCC_OK;
}
DccResult dcc_route_apply(DccRoute *route,const DccMessage *message,uint16_t component){
    DccRoute copy;DccResult result;if(!route||!message)return DCC_ARGUMENT;if(!addressed(message,component))return DCC_UNSUPPORTED;copy=*route;
    if(message->id==0x5201)result=apply_route(&copy,message);
    else if(message->id==0x5202)result=apply_maneuver(&copy,message);
    else if(message->id==0x5204)result=apply_lanes(&copy,message);
    else return DCC_UNSUPPORTED;
    if(!result)*route=copy;
    return result;
}
const DccRouteManeuver *dcc_route_current_maneuver(const DccRoute *r){
    unsigned i;if(!r)return NULL;for(i=0;i<DCC_ROUTE_CACHE;i++)if(r->maneuvers[i].valid&&r->maneuvers[i].index==r->current_index)return &r->maneuvers[i];return NULL;
}
const DccRouteLaneSet *dcc_route_current_lanes(const DccRoute *r){
    unsigned i;if(!r||!r->show_lanes)return NULL;for(i=0;i<DCC_ROUTE_CACHE;i++)if(r->lane_sets[i].valid&&r->lane_sets[i].index==r->lane_index)return &r->lane_sets[i];return NULL;
}
typedef struct{char *p;size_t n,cap;int ok;} Writer;
static void putc_safe(Writer *w,char c){if(w->n<w->cap)w->p[w->n++]=c;else w->ok=0;}
static void literal(Writer *w,const char *s){while(*s)putc_safe(w,*s++);}
static uint64_t divide10(uint64_t value,unsigned *remainder){
    uint64_t quotient=0;unsigned rest=0;int bit;
    for(bit=63;bit>=0;bit--){rest=(rest<<1)|(unsigned)((value>>(unsigned)bit)&1u);if(rest>=10){rest-=10;quotient|=(uint64_t)1u<<(unsigned)bit;}}
    *remainder=rest;return quotient;
}
static void decimal(Writer *w,int64_t value){char digits[24];unsigned n=0;uint64_t magnitude;
    if(value<0){putc_safe(w,'-');magnitude=(uint64_t)(-(value+1))+1;}else magnitude=(uint64_t)value;
    do{unsigned remainder;magnitude=divide10(magnitude,&remainder);digits[n++]=(char)('0'+remainder);}while(magnitude&&n<sizeof(digits));while(n)putc_safe(w,digits[--n]);}
static void safe_text(Writer *w,const char *s){unsigned char c;while((c=(unsigned char)*s++)!=0)putc_safe(w,(char)(c<32||c==127?' ':c));}
DccResult dcc_route_snapshot(const DccRoute *r,char *out,size_t cap,size_t *written){
    char temp[4096];Writer w={temp,0,sizeof(temp),1};const DccRouteManeuver *m;const DccRouteLaneSet *set;unsigned i,j;
    if(!r||!out||!written)return DCC_ARGUMENT;
    literal(&w,"active=");decimal(&w,r->active?1:0);putc_safe(&w,'\n');
    if(r->active){m=dcc_route_current_maneuver(r);literal(&w,"type=");decimal(&w,m?m->type:-1);literal(&w,"\nangle=");decimal(&w,m?m->angle:1000);literal(&w,"\nlht=");decimal(&w,m?m->left_hand:0);literal(&w,"\nroads=");
        if(m)for(i=0;i<m->road_count;i++){if(i)putc_safe(&w,',');decimal(&w,m->roads[i]);}
        literal(&w,"\ndist=");decimal(&w,r->distance_m);literal(&w,"\nstep=");decimal(&w,m?m->step_m:0);literal(&w,"\nstreet=");safe_text(&w,r->road);literal(&w,"\nrtt=");decimal(&w,r->remaining_s);literal(&w,"\ndest=");decimal(&w,r->destination_m);putc_safe(&w,'\n');
        set=dcc_route_current_lanes(r);literal(&w,"lanes=");decimal(&w,set?(int)set->count:0);putc_safe(&w,'\n');
        if(set)for(i=0;i<set->count;i++){literal(&w,"lane");decimal(&w,i);putc_safe(&w,'=');decimal(&w,set->lanes[i].index);putc_safe(&w,',');decimal(&w,set->lanes[i].status);putc_safe(&w,',');decimal(&w,set->lanes[i].highlight);for(j=0;j<set->lanes[i].angle_count;j++){putc_safe(&w,',');decimal(&w,set->lanes[i].angles[j]);}putc_safe(&w,'\n');}
    }
    if(!w.ok||cap<w.n)return DCC_LIMIT;
    memcpy(out,temp,w.n);*written=w.n;return DCC_OK;
}
