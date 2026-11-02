# P0867D HMI bridge

Late-load OSGi bundle that publishes validated CarPlay guidance through the
stock P0867D navigation APIs. It also arbitrates native-map visibility from the
Type-111 video heartbeat and restores the stock renderer when video stops. The
bundle does not replace or redistribute stock classes.

Build against `MIBHMI.jar` converted from the exact target's `MIBHMI.jxe`:

```powershell
.\hmi\build.ps1 -MibHmi C:\private\P0867D\MIBHMI.jar -JdkBin C:\jdk8\bin
```

The stage builder registers it as late-load bundle ID 98 and adds the generated
JAR to the HMI boot class path. No stock Java class is overwritten.
