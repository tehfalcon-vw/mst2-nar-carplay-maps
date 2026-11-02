package mst2.nar.cluster;

import java.io.File;
import java.io.FileInputStream;

final class StateReader implements Runnable {
    private static final String PATH="/dev/shmem/mst2-nar-rgi.state";
    private static final String VIDEO_PATH="/dev/shmem/mst2_nar_alt111";
    private static final long STALE_MS=5500;
    private static final long VIDEO_STALE_MS=2500;
    private static final int VIDEO_MAGIC=0x31434344;
    private volatile boolean running=true;
    private String previous="";private long accepted=0,published=0;private boolean active=false;
    private int videoSequence;private long videoAccepted;private boolean videoActive;
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
            pollVideo(now);
            try{Thread.sleep(250);}catch(InterruptedException ignored){}
        }
        KombiMapOwner.update(false);
    }
    private static String read() throws Exception {
        File file=new File(PATH);long length=file.length();if(length<=0||length>8192)throw new Exception("bad state length");
        byte[] bytes=new byte[(int)length];FileInputStream input=new FileInputStream(file);int used=0;
        try{while(used<bytes.length){int n=input.read(bytes,used,bytes.length-used);if(n<0)break;used+=n;}}finally{input.close();}
        if(used!=bytes.length)throw new Exception("short state read");return new String(bytes,"UTF-8");
    }
    private void pollVideo(long now){
        try {
            int[] header=readVideoHeader();int sequence=header[0],flags=header[3],length=header[4];
            if(sequence!=videoSequence){
                videoSequence=sequence;
                videoActive=(flags&1)!=0&&length>0;
                videoAccepted=now;
            } else if(videoActive&&now-videoAccepted>VIDEO_STALE_MS) videoActive=false;
        } catch(Throwable ignored){if(videoActive&&now-videoAccepted>VIDEO_STALE_MS)videoActive=false;}
        KombiMapOwner.update(videoActive);
    }
    private static int[] readVideoHeader() throws Exception {
        byte[] bytes=new byte[28];FileInputStream input=new FileInputStream(new File(VIDEO_PATH));int used=0;
        try{while(used<bytes.length){int n=input.read(bytes,used,bytes.length-used);if(n<0)break;used+=n;}}finally{input.close();}
        if(used!=bytes.length)throw new Exception("short video header");
        int[] result=new int[7];for(int i=0;i<result.length;i++)result[i]=le32(bytes,i*4);
        if((result[0]&1)!=0||result[1]!=VIDEO_MAGIC||result[2]!=1||result[4]<0||result[4]>250000)throw new Exception("bad video header");
        byte[] sequenceBytes=new byte[4];input=new FileInputStream(new File(VIDEO_PATH));used=0;
        try{while(used<4){int n=input.read(sequenceBytes,used,4-used);if(n<0)break;used+=n;}}finally{input.close();}
        if(used!=4||le32(sequenceBytes,0)!=result[0])throw new Exception("torn video header");
        return result;
    }
    private static int le32(byte[] b,int at){return (b[at]&255)|((b[at+1]&255)<<8)|((b[at+2]&255)<<16)|(b[at+3]<<24);}
}
