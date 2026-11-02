package mst2.nar.cluster;

final class Snapshot {
    boolean active,leftHand;int type=-1,angle=1000,distance=-1,step=0,destination=-1;
    long remaining=-1;String road="";int[] roads=new int[0];LaneData[] lanes=new LaneData[0];
    static Snapshot parse(String text) throws Exception {
        Snapshot s=new Snapshot();String[] lines=split(text,'\n');String[] laneRows=new String[8];int laneCount=0;boolean sawActive=false;
        for(int i=0;i<lines.length;i++){String line=lines[i];int eq=line.indexOf('=');if(eq<=0)continue;String key=line.substring(0,eq),value=line.substring(eq+1);
            if(key.equals("active")){s.active=one(value);sawActive=true;}
            else if(key.equals("type"))s.type=integer(value,-1,53);
            else if(key.equals("angle"))s.angle=integer(value,-1000,1000);
            else if(key.equals("lht"))s.leftHand=one(value);
            else if(key.equals("roads"))s.roads=numbers(value,16,-1000,1000);
            else if(key.equals("dist"))s.distance=integer(value,-1,Integer.MAX_VALUE);
            else if(key.equals("step"))s.step=integer(value,0,Integer.MAX_VALUE);
            else if(key.equals("street"))s.road=clean(value,120);
            else if(key.equals("rtt"))s.remaining=longValue(value,-1,2147483647L);
            else if(key.equals("dest"))s.destination=integer(value,-1,Integer.MAX_VALUE);
            else if(key.equals("lanes"))laneCount=integer(value,0,8);
            else if(key.startsWith("lane")){int slot=integer(key.substring(4),0,7);if(laneRows[slot]!=null)throw new Exception("duplicate lane");laneRows[slot]=value;}
        }
        if(!sawActive)throw new Exception("missing active");
        if(s.active){s.lanes=new LaneData[laneCount];for(int i=0;i<laneCount;i++){if(laneRows[i]==null)throw new Exception("missing lane");s.lanes[i]=LaneData.parse(laneRows[i]);}}
        return s;
    }
    private static boolean one(String text) throws Exception {int n=integer(text,0,1);return n==1;}
    private static int integer(String text,int min,int max)throws Exception{long n=longValue(text,min,max);return(int)n;}
    private static long longValue(String text,long min,long max)throws Exception{long n;try{n=Long.parseLong(text.trim());}catch(NumberFormatException e){throw new Exception("bad number");}if(n<min||n>max)throw new Exception("number range");return n;}
    static int[] numbers(String text,int max,int min,int high)throws Exception{if(text.length()==0)return new int[0];String[] parts=split(text,',');if(parts.length>max)throw new Exception("too many values");int[] out=new int[parts.length];for(int i=0;i<out.length;i++)out[i]=integer(parts[i],min,high);return out;}
    private static String clean(String text,int max)throws Exception{if(text.length()>max)text=text.substring(0,max);for(int i=0;i<text.length();i++)if(text.charAt(i)<32||text.charAt(i)==127)throw new Exception("control in text");return text;}
    private static String[] split(String text,char separator){int count=1;for(int i=0;i<text.length();i++)if(text.charAt(i)==separator)count++;String[] out=new String[count];int start=0;for(int i=0;i<count;i++){int end=text.indexOf(separator,start);if(end<0)end=text.length();out[i]=text.substring(start,end);start=end+1;}return out;}
}
