# Cross-platform findings for the Delphi P0867D port

This document separates reusable CarPlay protocol facts from head-unit-specific
delivery mechanisms. A working result on MHI2, MHI2Q or TechniSat is evidence
for the common protocol layer; it is not evidence that the same renderer or
MOST device contract applies to MST2 Delphi.

## Evidence matrix

| Platform | Proven source/receiver path | Proven cluster delivery | What transfers to Delphi |
|---|---|---|---|
| MHI2/MQB | Type-111 receiver, avcC plus encrypted AVCC access units | H.264 remux to MPEG-TS, strict 64x188-byte writes to `/dev/mlb/isoTX2`, MOST150 | AirPlay capability/session/crypto protocol, IDR recovery and exclusive stock/custom ownership. The device path and 12032-byte block rule do not transfer without Delphi evidence. |
| MHI2 Audi B9 | Type-111 receiver with dynamic ViewArea and key-frame controls | Direct remux/renderer variants, native-map fallback | Feature negotiation, `features=2` frame-rate observation, ViewArea lifecycle and reconnect behavior. |
| MHI2Q Audi | Type-111 handled by stock AirPlay/OMX path | Decoded NV12 readback, GLES/displayable plane and Java Context ownership | Negotiation, stock-decoder reuse and lifecycle lessons. OMX, displayables and Context80 are MHI2Q-specific. |
| MST2 TechniSat | Type-111 receiver publishes complete H.264 access units | DisplayManager's stock encoder/RingIO/Jacinto path is substituted before MOST | Latest-complete-AU buffering, consumer watchdog, IDR gating, stock restoration and HMI data-rate lessons. The Jacinto/RingIO hook does not exist on Delphi. |
| MST2 Delphi P0867D | Static firmware proof of AirPlay functions and stock `NavigationServer` isoTX producer | `/dev/transport_server/spawn/isoTX`, `ClusterServices` GfxStreamSink | This project's target. Packet/block/PID, HMI ownership and physical presentation still require target evidence. |

Primary public evidence:

- [MHI2 vehicle-proven Stream-111 to direct MOST path](https://github.com/harman-f/mhi2_altscreen_carplay/blob/main/docs/findings/MU1440_CARPLAY_STREAM111_POC_2026-09-29.md)
- [MHI2 media/control plane architecture](https://github.com/harman-f/mhi2_altscreen_carplay/blob/main/docs/architecture/SYSTEM_OVERVIEW.md)
- [MHI2 Audi B9 implementation and vehicle status](https://github.com/joeyQuery/MHI2-altScreen)
- [MHI2Q stock-decode/readback architecture](https://github.com/yuedizhibo/MHI2Q-CarPlay-AltScreen/blob/main/README_EN.md)
- [MHI2Q compressed-stream renderer implementation](https://github.com/luka-dev/mib2q-carplay-rgi/blob/main/docs/altscreen-and-video/README.md)
- [MST2 TechniSat implementation](https://github.com/1mikew/mst2-altscreen-carplay)

## Common Type-111 contract

The compatible implementations agree on these receiver-side rules:

1. Advertise a second display and the `altScreen` feature; advertise
   `viewAreas` only when the declared area records are complete and consistent.
2. Accept SETUP stream type 111 on a dedicated data port and retain its
   `streamConnectionID`.
3. Derive the screen key and IV with
   `AirPlay_DeriveAESKeySHA512ForScreen` for that connection ID.
4. Read a 128-byte little-endian Screen header followed by `bodySize` bytes.
5. Treat opcode 1 as a clear avcC record. Treat opcode 0 as one complete AVCC
   access unit encrypted by a continuous AES-CTR context.
6. Advance AES-CTR for every received encrypted access unit, even if the unit
   will be dropped. A receiver must drop before encryption on the sender side,
   or after decryption on the receiver side; otherwise the stream desynchronizes.
7. Reject frames before a valid configuration and do not take the stock map
   until a complete IDR has been accepted.
8. On a new configuration, stream generation, timeout, decode error or
   teardown, revoke custom ownership and wait for a fresh IDR.
9. `suggestUI`, `showUI`, Type-111 SETUP and actual video activity are separate
   state transitions. No single one is sufficient proof of a healthy map.

The portable parser, lifecycle machine, AVCC converter, shared latest-frame
slot and MPEG-TS packetizer under `native/video` implement the offline-testable
part of this contract without importing GPL source.

## Delphi-specific plan

The exact P0867D `libAirPlay.so` cannot be intercepted only through exported
symbol preloading because `CarPlay` makes the relevant receiver calls inside
the library. The runtime must therefore install target-locked ARM hooks only
after validating every expected prologue. Setup needs relocation of its
PC-relative second instruction; other planned entry hooks can copy their first
two instructions into a trampoline.

The producer publishes only validated Annex-B access units into
`/dev/shmem/mst2_nar_alt111`. The `NavigationServer`-local consumer owns the
only custom write path, waits for IDR, and suppresses stock writes only while a
fresh custom frame generation is active. This keeps a CarPlay or producer crash
from permanently blanking the cluster.

The following values deliberately remain target-unknown:

- Delphi MPEG-TS video PID and PAT/PMT expectations;
- accepted application-write grouping for the transport-server resource;
- whether stock GfxStreamSink full-data-rate state must be explicitly acquired;
- NAR cluster canvas, physical dimensions and safe areas;
- reconnect and ignition-cycle timing on the fitted cluster.

MHI2's `0x11` PID and 12032-byte MOST writes are useful experiment candidates,
not defaults to label as verified. The Delphi stage must retain a fail-closed
target fingerprint and a one-step stock restore until those values are captured.

## What emulation can and cannot close

A host or generic QNX VM can exercise fragmented TCP input, packet limits,
continuous cipher calls, avcC/AVCC validation, IDR acquisition, shared-memory
handoff, MPEG-TS syntax, ARM relocation planning and timeout fallback. It cannot
emulate Delphi's proprietary transport-server resource manager, MOST controller,
`ClusterServices` state, encoder proxy or the Virtual Cockpit. Those boundaries
are board-support-package and hardware behavior, not generic QNX services.
