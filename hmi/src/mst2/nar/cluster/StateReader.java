package mst2.nar.cluster;

import java.io.File;
import java.io.FileInputStream;

final class StateReader implements Runnable {
    private static final String PATH="/dev/shmem/mst2-nar-rgi.state";
    private static final long STALE_MS=5500;
    private volatile boolean running=true;
    private String previous="";private long accepted=0,published=0;private boolean active=false;
    public void stop(){running=false;}
    public void run(){
        while(running){
            long now=System.currentTimeMillis();
            try {
                String text=read();
                if(!text.equals(previous)){
                    Snapshot state=Snapshot.parse(text);previous=text;accepted=now;active=state.active;Publisher.send(state);published=now;
                } else if(active&&now-accepted>STALE_MS){active=false;previous="";Publisher.clear();published=now;
                } else if(active&&now-published>=1000){Snapshot state=Snapshot.parse(text);Publisher.send(state);published=now;}
            } catch(Throwable ignored){if(active&&now-accepted>STALE_MS){active=false;previous="";Publisher.clear();published=now;}}
            try{Thread.sleep(250);}catch(InterruptedException ignored){}
        }
    }
    private static String read() throws Exception {
        File file=new File(PATH);long length=file.length();if(length<=0||length>8192)throw new Exception("bad state length");
        byte[] bytes=new byte[(int)length];FileInputStream input=new FileInputStream(file);int used=0;
        try{while(used<bytes.length){int n=input.read(bytes,used,bytes.length-used);if(n<0)break;used+=n;}}finally{input.close();}
        if(used!=bytes.length)throw new Exception("short state read");return new String(bytes,"UTF-8");
    }
}
