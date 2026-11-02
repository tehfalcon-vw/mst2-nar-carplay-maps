package mst2.nar.cluster;
public final class Check {
    private static void ok(boolean value,String message){if(!value)throw new RuntimeException(message);}
    private static void rejects(String text){try{Snapshot.parse(text);throw new RuntimeException("accepted invalid snapshot");}catch(Exception expected){}}
    public static void main(String[] ignored)throws Exception{
        Snapshot s=Snapshot.parse("active=1\ntype=2\nangle=90\nlht=0\nroads=-90,0\ndist=500\nstep=1000\nstreet=Main St\nrtt=900\ndest=12000\nlanes=1\nlane0=1,2,0,-90,0\n");
        Maneuver m=Maneuver.from(s);ok(s.active&&s.distance==500&&s.lanes.length==1,"snapshot");ok(m.main==13&&m.direction==192,"maneuver mapping");ok(s.lanes[0].getGuidanceInfo()==2,"lane guidance");
        rejects("type=2\n");rejects("active=1\nlanes=1\n");rejects("active=1\nlanes=1\nlane0=1,7,0\n");rejects("active=1\nlanes=1\nlane0=1,2,1001\n");
        System.out.println("HMI MODEL TESTS PASSED");
    }
}
