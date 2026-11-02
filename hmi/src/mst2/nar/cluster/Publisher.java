package mst2.nar.cluster;

import de.vw.mib.asl.api.navbap.ASLNavBAPAPI;
import de.vw.mib.asl.api.navbap.ASLNavBAPFactory;
import de.vw.mib.asl.framework.api.framework.ASLFrameworkFactory;
import de.vw.mib.asl.framework.api.framework.Services;
import de.vw.mib.asl.framework.internal.framework.GenericCollector;

final class Publisher {
    private static final int ROUTE_STATE=732,MANEUVER_LIST=751,UNIT_AUTO=255;
    static void clear(){Snapshot empty=new Snapshot();send(empty);}
    static void send(final Snapshot state){
        try{services().getASLThreadSwitch().enqueue(new Runnable(){public void run(){try{write(state);}catch(Throwable error){System.out.println("[mst2-nar-rgi] publish failed: "+error);}}});}
        catch(Throwable error){System.out.println("[mst2-nar-rgi] ASL unavailable: "+error);}
    }
    private static Services services(){return ASLFrameworkFactory.getASLFrameworkAPI().getServices();}
    private static void write(Snapshot s){Services svc=services();ASLNavBAPAPI api=ASLNavBAPFactory.getNavBAPApi();Maneuver mapped=Maneuver.from(s);GenericCollector item=new GenericCollector();
        item.setIntItem(0,s.active?mapped.main:0);item.setIntItem(1,s.active?mapped.direction:0);item.setIntItem(2,0);item.setStringItem(3,s.active?Maneuver.sides(s):"");
        svc.getASLListManager().getGenericASLList(MANEUVER_LIST).updateList(new Object[]{item});
        if(s.active){int bar=s.step>0&&s.distance>=0&&s.distance<=s.step?(int)(((long)s.distance*100L)/s.step):0;api.updateBapDistanceToNextManeuver(s.distance,0,bar>0,bar);api.updateBapTurnToInfo(s.road,"");api.updateBapTimeToDestinationRTT(s.remaining);api.updateBapDistanceToDestination(s.destination,UNIT_AUTO,false);api.updateBapLaneGuidance(s.lanes,s.lanes.length>0);}
        else{api.updateBapDistanceToNextManeuver(-1,0,false,0);api.updateBapTurnToInfo("","");api.updateBapTimeToDestinationRTT(-1);api.updateBapDistanceToDestination(-1,UNIT_AUTO,false);api.updateBapLaneGuidance(new LaneData[0],false);}
        svc.getASLPropertyManager().valueChangedInteger(ROUTE_STATE,s.active?1:0);
    }
}
