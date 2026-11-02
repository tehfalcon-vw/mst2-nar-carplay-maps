package mst2.nar.cluster;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public final class Activator implements BundleActivator {
    private StateReader reader;
    public void start(BundleContext context) {
        try {
            reader=new StateReader();
            Thread thread=new Thread(reader,"mst2-nar-rgi-hmi");
            thread.setDaemon(true);thread.start();
            System.out.println("[mst2-nar-rgi] HMI bridge started");
        } catch(Throwable error){System.out.println("[mst2-nar-rgi] HMI start failed: "+error);}
    }
    public void stop(BundleContext context){if(reader!=null){reader.stop();reader=null;}Publisher.clear();}
}
