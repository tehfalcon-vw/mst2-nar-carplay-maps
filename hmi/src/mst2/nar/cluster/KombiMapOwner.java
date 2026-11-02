package mst2.nar.cluster;

import de.vw.mib.asl.framework.api.framework.ASLFrameworkFactory;
import de.vw.mib.asl.framework.internal.framework.ServiceManager;

/* Keeps the stock map renderer hidden only while the phone video is live.
 * The cluster remains the authority for OFF/REDUCED/FULL data rate; these are
 * the exact events used by P0867D's NavigationMapAdapter for renderer
 * visibility, and are always dispatched on the ASL thread. */
final class KombiMapOwner {
    private static final int VISIBLE=1074841845;
    private static final int INVISIBLE=1074841846;
    private static final long REFRESH_MS=2000;
    private static boolean owned;
    private static long sent;

    private KombiMapOwner() {}

    static synchronized void update(boolean phoneVideo) {
        long now=System.currentTimeMillis();
        if(phoneVideo&&(!owned||now-sent>=REFRESH_MS)){
            owned=true;sent=now;dispatch(INVISIBLE);
        } else if(!phoneVideo&&owned){
            owned=false;sent=now;dispatch(VISIBLE);
        }
    }

    private static void dispatch(final int event) {
        try {
            ASLFrameworkFactory.getASLFrameworkAPI().getServices().getASLThreadSwitch().enqueue(
                new Runnable(){public void run(){
                    try {
                        ServiceManager.eventMain.getEventDispatcher().sendSafe(
                            ServiceManager.mGenericEventFactory.newEvent(event));
                    } catch(Throwable error){
                        System.out.println("[mst2-nar-rgi] map ownership event failed: "+error);
                    }
                }});
        } catch(Throwable error){
            System.out.println("[mst2-nar-rgi] map ownership ASL unavailable: "+error);
        }
    }
}
