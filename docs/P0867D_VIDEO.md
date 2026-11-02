# P0867D CarPlay secondary video

This is the exact-target design for CarPlay auxiliary stream type 111 on
`MST2_US_VW_P0867D`. It is separate from the implemented route-guidance path.

## Verified stock boundaries

- `CarPlay` uses `libAirPlay.so` and `libScreenStream.so`.
- `libAirPlay.so` exports the receiver setup/start/teardown and server-info
  functions needed to advertise and accept a second display.
- Its session-level security setter is at `0x18950` and calls the imported
  CBC-frame initializer with the master session key. The distinct screen-level
  AES-CTR setter is at `0x19e9c`; the two functions must not be conflated.
- `NavigationServer` opens `/dev/transport_server/spawn/isoTX` for the stock
  cluster-map encoder.
- The exact P0867D `transportStreamPackagesReady` routine starts at ELF virtual
  address `0x22cb78`. Its arguments are an array of 8-byte `{pointer,length}`
  elements, an element count and a context. It writes every non-empty element
  to the stock `isoTX` descriptor. P0879D has the structurally matching routine
  at `0x22b350`; the NAR port is therefore a layout shift, not a missing path.
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

- A bounded Annex-B to MPEG-TS packetizer emits PAT, PMT, H.264 PES, 90 kHz
  PTS, PCR on every access unit and independent continuity counters.
- A P0867D-only `NavigationServer` interposer tracks the exact `isoTX` file
  descriptor and passes all stock writes through by default.
- A seqlock-protected `/dev/shmem/mst2_nar_alt111` latest-frame slot carries
  complete Annex-B access units without allowing a slow consumer to build
  latency. The reader rejects torn, oversized and stale frames.
- The in-process worker waits for a live stream and an IDR, packetizes frames,
  and writes them through the captured stock descriptor. Takeover is
  process-local and times out after two seconds without a new producer frame;
  no persistent marker can leave the stock map suppressed after a crash.
- The exact staging builder validates the stock executables and scopes each
  preload to its owning ProcessManager service.
- A target-locked `CarPlay` preload now validates the complete P0867D
  `libAirPlay.so` binding set, installs early ARM trampolines for server-info,
  setup and teardown, and captures the master key only from the verified
  session-security call site.
- The receiver publishes a complete 1010x376 auxiliary display with physical
  size, ViewArea, SafeArea and stream UUID, accepts type 111 on a dedicated
  listener, derives a connection-specific AES-CTR key, parses fragmented Screen
  packets, validates avcC/AVCC and publishes complete Annex-B access units.
- The stage builder installs that receiver only into the two mutually exclusive
  stock `CarPlay` ProcessManager variants; main-screen stream 110 remains stock.
- The `NavigationServer` worker uses the evidence-backed provisional MOST
  profile: H.264 PID `0x11`, PCR, and exact 64x188-byte writes. It suppresses
  stock writes only after the first complete custom block succeeds.
- The HMI watches the seqlock video header, hides the native map renderer only
  while fresh phone video exists, refreshes ownership every two seconds, and
  restores native rendering on stop, timeout or bundle shutdown. The fitted
  cluster remains authoritative for OFF/REDUCED/FULL data-rate requests.

## Still required before claiming video

1. Validate the Delphi application-write block size and TS PID contract from a
   real stock capture. Values proven on MHI2 must not be assumed for Delphi.
2. Confirm display ownership, safe-area geometry and disconnect recovery on a
   parked vehicle or representative bench.

## Why QEMU is not the final test

A generic QNX virtual target can test executable loading, ordinary sockets,
parsers and process-local interposition. It does not reproduce the Delphi video
encoder proxy, `/dev/transport_server`, MOST controller, `ClusterServices`, or
the fitted virtual cockpit. Those devices and their resource-manager behavior
come from the Delphi board-support package and hardware. The portable pieces are
therefore host-tested; exact ARM artifacts are link- and ABI-checked; hardware
claims remain gated on a real target.
