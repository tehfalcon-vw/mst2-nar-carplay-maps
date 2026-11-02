#ifndef DCC_ROUTE_H
#define DCC_ROUTE_H
#include "dcc_rgi.h"

#define DCC_ROUTE_TEXT 256u
#define DCC_ROUTE_CACHE 8u
#define DCC_ROUTE_ROADS 16u
#define DCC_ROUTE_LANES 8u
#define DCC_ROUTE_ANGLES 16u

typedef struct {
    int valid,index,type,left_hand,step_m,angle;
    unsigned road_count;
    int roads[DCC_ROUTE_ROADS];
    char street[DCC_ROUTE_TEXT],description[DCC_ROUTE_TEXT];
} DccRouteManeuver;
typedef struct {
    int index,status,highlight;
    unsigned angle_count;
    int angles[DCC_ROUTE_ANGLES];
} DccRouteLane;
typedef struct {
    int valid,index;
    unsigned count;
    DccRouteLane lanes[DCC_ROUTE_LANES];
} DccRouteLaneSet;
typedef struct {
    int active,state,current_index,lane_index,show_lanes;
    int distance_m,destination_m;
    int64_t remaining_s;
    char road[DCC_ROUTE_TEXT];
    unsigned maneuver_next,lane_next;
    DccRouteManeuver maneuvers[DCC_ROUTE_CACHE];
    DccRouteLaneSet lane_sets[DCC_ROUTE_CACHE];
} DccRoute;

void dcc_route_init(DccRoute *route);
/* Applies one already-validated RGI message atomically. Messages explicitly
 * addressed only to another component are ignored with DCC_UNSUPPORTED. */
DccResult dcc_route_apply(DccRoute *route,const DccMessage *message,uint16_t component);
const DccRouteManeuver *dcc_route_current_maneuver(const DccRoute *route);
const DccRouteLaneSet *dcc_route_current_lanes(const DccRoute *route);
/* Stable line protocol consumed by the optional HMI bundle. Control characters
 * in phone text are replaced with spaces. Output is unchanged on failure. */
DccResult dcc_route_snapshot(const DccRoute *route,char *output,size_t capacity,size_t *written);

#endif
