# P0867D Delphi iAP2 preload

This target-locked QNX/ARM preload is the first live integration stage. In the
`mm-ipod` process it loads the stock Delphi iAP2 driver, validates the exported
P0867D layout, installs the previously empty control-message group `0x52`, and
wraps the stock GPS Identify builder. The stock builder always runs first; the
wrapper then adds RouteGuidanceDisplayComponent parameter `0x001E` with component
ID `0x0010`. Any mismatch leaves the original tables untouched.

The companion configuration must advertise outgoing `0x5200,0x5203` and incoming
`0x5201,0x5202,0x5204`. Unknown incoming messages remain in the driver's normal
diagnostic path, allowing a separate bounded reader to consume the existing
`/dev/shmem/iap2.log` stream without replacing the transport.

Build on a host with Clang and LLD:

```sh
sh native/delphi-qnx/build.sh
```

The output is not installed automatically. It must be loaded only into P0867D's
`mm-ipod`, after the exact stock driver and main executable hashes are checked.
Bench validation must confirm Identify acceptance and real `0x52xx` receipt
before any persistent deployment.

The separate `libmst2_nar_video_gate.so` interposer is loaded only into the
exact P0867D `NavigationServer`. It tracks the stock
`/dev/transport_server/spawn/isoTX` descriptor. Stock writes pass through until
an in-process Stream-111 worker explicitly enables takeover. The ownership flag
exists only in process memory, so a crash or reboot cannot leave a stale marker
that suppresses the stock navigation stream.
