package mst2.nar.cluster;

import de.vw.mib.asl.api.navbap.INavLaneGuidanceDataNavBap;

final class LaneData implements INavLaneGuidanceDataNavBap {
    private short direction;private byte guidance;private byte[] sides;
    static LaneData parse(String row)throws Exception{
        int[] v=Snapshot.numbers(row,19,-1000,65534);if(v.length<3)throw new Exception("short lane");
        if(v[0]<0||v[1]<0||v[1]>2||v[2]<-1000||v[2]>1000)throw new Exception("invalid lane");
        for(int i=3;i<v.length;i++)if(v[i]<-1000||v[i]>1000)throw new Exception("invalid lane angle");
        LaneData lane=new LaneData();int primary=code(v[2]);if(primary==255&&v.length==4)primary=code(v[3]);lane.direction=(short)primary;
        lane.guidance=(byte)(primary==255?0:(v[1]>=0&&v[1]<=2?v[1]:0));int count=0;
        for(int i=3;i<v.length;i++){int c=code(v[i]);if(c!=255&&c!=primary&&!has(v,3,i,c))count++;}
        lane.sides=new byte[count];count=0;for(int i=3;i<v.length;i++){int c=code(v[i]);if(c!=255&&c!=primary&&!contains(lane.sides,count,c))lane.sides[count++]=(byte)c;}
        return lane;
    }
    private static boolean has(int[] values,int from,int to,int code){for(int i=from;i<to;i++)if(code(values[i])==code)return true;return false;}
    private static boolean contains(byte[] values,int count,int code){for(int i=0;i<count;i++)if((values[i]&255)==code)return true;return false;}
    private static int code(int degrees){if(degrees<-180||degrees>180)return 255;int sectors=(Math.abs(degrees)*16+180)/360;return(((degrees<0?sectors:-sectors)*16)&255);}
    public short getLaneDirection(){return direction;}public byte getGuidanceInfo(){return guidance;}
    public byte getLaneDescription(){return 15;}public byte getLaneMarkingLeft(){return 0;}public byte getLaneMarkingRight(){return 0;}
    public short getLaneType(){return 0;}public byte[] getLaneSideStreets(){return sides;}
}
