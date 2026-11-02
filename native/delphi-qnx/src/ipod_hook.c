#include "dcc_qnx.h"
#include "dcc_identify.h"

#define DRIVER "/extbin/apps/lib/ipod-drvr-iap2.so"
#define READY "/dev/shmem/mst2-nar-rgi-ready"
#define LOG_FILE "/dev/shmem/mst2-nar-rgi.log"

/* Exact P0867D addresses. The containing binary must first match the SHA-256
 * in targets/MST2_US_VW_P0867D.json. Runtime checks below then bind every
 * writable table relative to an exported symbol. */
#define IPOD_MODULE_VADDR 0x302c0u
#define ADD_BLOB_VADDR 0x226c4u
#define IDENT_TABLE_VADDR 0xb2720u
#define GPS_SLOT_VADDR 0xb27d0u
#define GPS_BUILDER_VADDR 0xe1a0u
#define CONTROL_TABLE_VADDR 0xb5ae8u
#define RGI_GROUP 0x52u
#define RGI_COMPONENT 0x0010u

struct message_info {
    const char *name;
    uint16_t flags;
    uint16_t reply;
    const void *parameters;
};
struct message_group {
    const char *name;
    struct message_info *info;
    uint32_t count;
};

static int (*stock_gps_builder)(void *,void *);
static int (*packet_add_blob)(void *,void *,unsigned,const void *,unsigned);
static struct message_info route_messages[5] = {
    [1] = {"RouteGuidanceUpdate",2,0,NULL},
    [2] = {"RouteGuidanceManeuverUpdate",2,0,NULL},
    [4] = {"RouteGuidanceLaneGuidanceInformation",2,0,NULL}
};

static void note(const char *text){
    int fd=open(LOG_FILE,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(fd>=0){write(fd,text,strlen(text));close(fd);}
}

static int add_route_component(void *device,void *packet){
    static const DccIdentifyProfile profile={
        RGI_COMPONENT,"MST2 NAR Cluster",16,
        255,255,255,255,6,255,6
    };
    uint8_t payload[128];size_t bytes=0;int result=stock_gps_builder(device,packet);
    int fd;
    if(result<0)return result;
    if(dcc_identify_component_payload(&profile,payload,sizeof(payload),&bytes)!=DCC_OK||
       packet_add_blob(device,packet,0x1e,payload,(unsigned)bytes)<0){
        note("RGI component rejected locally; stock Identify retained\n");
        return result;
    }
    fd=open(READY,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(fd>=0){write(fd,"offered\n",8);close(fd);}
    note("P0867D RGI component offered\n");
    return result;
}

void dcc_delphi_init(void){
    void *main_image=dlopen(NULL,RTLD_LAZY),*driver,*module,*blob,*ident,*control;
    uintptr_t base;uint32_t *slot;struct message_group *groups;
    if(!main_image||!dlsym(main_image,"ipod_log"))return;
    driver=dlopen(DRIVER,RTLD_LAZY|RTLD_GLOBAL);
    module=driver?dlsym(driver,"ipod_module"):NULL;
    blob=driver?dlsym(driver,"packet_addparam_blob"):NULL;
    ident=driver?dlsym(driver,"ident_info_funcs"):NULL;
    control=driver?dlsym(driver,"iap2_ctrl_msg_table"):NULL;
    if(!module||!blob||!ident||!control){note("P0867D symbols unavailable; stock behavior\n");return;}
    base=(uintptr_t)module-IPOD_MODULE_VADDR;
    if((uintptr_t)blob!=base+ADD_BLOB_VADDR||
       (uintptr_t)ident!=base+IDENT_TABLE_VADDR||
       (uintptr_t)control!=base+CONTROL_TABLE_VADDR){
        note("P0867D symbol layout mismatch; stock behavior\n");return;
    }
    slot=(uint32_t *)(base+GPS_SLOT_VADDR);
    groups=(struct message_group *)control;
    if(slot[0]!=0x16u||slot[1]!=base+GPS_BUILDER_VADDR||
       groups[RGI_GROUP].name||groups[RGI_GROUP].info||groups[RGI_GROUP].count){
        note("P0867D writable binding mismatch; stock behavior\n");return;
    }
    stock_gps_builder=(int (*)(void *,void *))(uintptr_t)slot[1];
    packet_add_blob=(int (*)(void *,void *,unsigned,const void *,unsigned))blob;
    groups[RGI_GROUP].info=route_messages;
    groups[RGI_GROUP].count=5;
    slot[1]=(uint32_t)(uintptr_t)add_route_component;
    note("P0867D iAP2 RGI bindings active\n");
}
