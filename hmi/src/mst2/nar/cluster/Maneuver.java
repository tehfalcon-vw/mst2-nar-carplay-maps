package mst2.nar.cluster;

final class Maneuver {
    final int main,direction;Maneuver(int m,int d){main=m;direction=d;}
    static Maneuver from(Snapshot s){int t=s.type,a=s.angle;boolean known=a!=-1000&&a!=1000;
        if(t==0||t==5||t==15||t==17)return new Maneuver(11,0);
        if(t==1)return new Maneuver(13,64);if(t==2)return new Maneuver(13,192);if(t==3)return new Maneuver(13,0);
        if(t==4||t==18||t==26)return new Maneuver(25,known&&a!=0?(a<0?64:192):(s.leftHand?192:64));
        if(t==6)return new Maneuver(13,s.leftHand?64:192);if(t==7)return new Maneuver(s.leftHand?27:26,s.leftHand?64:192);
        if(t==8||t==9)return new Maneuver(13,known&&a!=0?(a<0?32:224):(s.leftHand?32:224));
        if(t==10||t==12||t==27)return new Maneuver(3,0);if(t==13||t==22||t==49)return new Maneuver(13,32);if(t==14||t==23||t==50)return new Maneuver(13,224);
        if(t==19)return new Maneuver(s.leftHand?22:21,128);if(t==24)return new Maneuver(3,64);if(t==25)return new Maneuver(3,192);
        if(t==47)return new Maneuver(13,96);if(t==48)return new Maneuver(13,160);if(t==52)return new Maneuver(12,64);if(t==53)return new Maneuver(12,192);
        if(t>=28&&t<=46){int d=known&&a!=0?direction(a):exit(t-27,s.leftHand);return new Maneuver(s.leftHand?22:21,d);}
        if(t==11||t==16||t==51||t==20||t==21)return new Maneuver(13,known?turn(a):(t==20?64:t==21?192:0));
        return new Maneuver(1,0);
    }
    static String sides(Snapshot s){StringBuffer out=new StringBuffer();for(int i=0;i<s.roads.length&&out.length()<17;i++){int a=s.roads[i];if(a>=-180&&a<=180&&a!=s.angle)out.append((char)direction(a));}return out.toString();}
    private static int direction(int a){int step=(int)Math.floor(-a/22.5+0.5);return(step*16)&255;}
    private static int turn(int a){int n=Math.abs(a);if(n<=22)return 0;if(n<=67)return a<0?32:224;if(n<=112)return a<0?64:192;return a<0?96:160;}
    private static int exit(int number,boolean left){if(number==1)return left?64:192;if(number==2)return 0;if(number==3)return left?192:64;return 128;}
}
