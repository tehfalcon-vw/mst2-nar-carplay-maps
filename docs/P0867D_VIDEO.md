# P0867D CarPlay secondary video

This is the exact-target design for CarPlay auxiliary stream type 111 on
`MST2_US_VW_P0867D`. It is separate from the implemented route-guidance path.

## Verified stock boundaries

- `CarPlay` uses `libAirPlay.so` and `libScreenStream.so`.
- `libAirPlay.so` exports the receiver setup/start/teardown and server-info
  functions needed to advertise and accept a second display.
- `NavigationServer` opens `/dev/transport_server/spawn/isoTX` for the stock
  cluster-map encoder.
- `ClusterServices` owns the MOST `FB_GfxStreamSink` state machine.
- The HMI exposes `DSIKOMOGfxStreamSink` data-rate and foreground-layer control.

The intended path is:

```text
iPhone stream 111 (H.264/AVCC)
  -> target-locked libAirPlay receiver hook
  -> validated AVCC-to-Annex-B access units
  -> MPEG-TS packetizer
  -> exclusive in-process NavigationServer isoTX gate
  -> stock MOST transport
  -> Virtual Cockpit GfxStreamSink
```

The main CarPlay stream 110 remains stock-owned.

## Implemented offline

- A bounded Annex-B to MPEG-TS packetizer emits PAT, PMT, H.264 PES and 90 kHz
  PTS with independent continuity counters.
- A P0867D-only `NavigationServer` interposer tracks the exact `isoTX` file
  descriptor and passes all stock writes through by default.
- Takeover is process-local and can only be enabled by the future stream worker;
  no persistent marker can leave the stock map suppressed after a crash.
- The exact staging builder validates the stock `NavigationServer`, installs the
  gate and patches only its ProcessManager launch entry.

## Still required before claiming video

1. Port the stream-111 AirPlay negotiation and AES receive boundary to the exact
   P0867D `libAirPlay.so` ARM prologues.
2. Connect the validated access-unit output to the TS packetizer and write
   complete transport blocks through the captured `isoTX` descriptor.
3. Make takeover conditional on live stream, codec config, an IDR and confirmed
   cluster full-data-rate state; restore stock output on every failure path.
4. Validate the Delphi application-write block size and TS PID contract from a
   real stock capture. Values proven on MHI2 must not be assumed for Delphi.
5. Confirm display ownership, safe-area geometry and disconnect recovery on a
   parked vehicle or representative bench.

## Why QEMU is not the final test

A generic QNX virtual target can test executable loading, ordinary sockets,
parsers and process-local interposition. It does not reproduce the Delphi video
encoder proxy, `/dev/transport_server`, MOST controller, `ClusterServices`, or
the fitted virtual cockpit. Those devices and their resource-manager behavior
come from the Delphi board-support package and hardware. The portable pieces are
therefore host-tested; exact ARM artifacts are link- and ABI-checked; hardware
claims remain gated on a real target.
