# P0867D HMI bridge

This original late-load OSGi bundle polls `/dev/shmem/mst2-nar-rgi.state` and
publishes validated CarPlay guidance through the stock P0867D `ASLNavBAPAPI`,
property 732, and list 751. It does not replace or redistribute stock classes.

Build against `MIBHMI.jar` converted from the exact target's `MIBHMI.jxe`:

```powershell
.\hmi\build.ps1 -MibHmi C:\private\P0867D\MIBHMI.jar -JdkBin C:\jdk8\bin
```

The bundle is intended as ID 98 in `bundles.properties`, appended to
`Bundle.Startup.Lateload`. The stage builder performs this edit and refuses an
already-modified or unexpected target. Loading it also requires adding the
generated JAR to the HMI boot class path; no stock Java class is overwritten.
