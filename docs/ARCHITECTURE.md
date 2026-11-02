# MST2 Delphi NAR architecture

## Verified boundary

The P0867D and P0879D copies of `ipod-drvr-iap2.so`, `libiap2client.so`,
`libAirPlay.so`, `libAirPlaySupport.so`, and `dsiifc.jar` are byte-identical.
The donor's file-relative iAP2 findings therefore apply to P0867D without an
offset translation:

- Identify assembly: `ident_info_request` at `0x10ca4` in
  `ipod-drvr-iap2.so`.
- Identify control message: `0x1d01`.
- Raw-send export: `iap2_raw_msg_send` at `0x16e8` in `libiap2client.so`.
- The stock table has no control group `0x52`. The P0867D-only preload hook
  installs bounded metadata for incoming RGI `0x5201`, `0x5202`, and `0x5204`
  after verifying that group is still empty.
- P0867D `MediaHandler` imports QNX `spawn` and `spawnp`. A scoped launcher
  interposer uses that existing seam to give only the `mm-ipod` child the
  target-locked driver hook.

The manifest values are link-time virtual addresses, not assumed process
addresses. The runtime binder derives the actual QNX load base from the
exported `ipod_module` symbol, verifies every other symbol/table relative to
that base, and leaves stock behavior unchanged on any mismatch. The first
remaining runtime proof is a non-persistent bench capture of the resulting
Identify packet and incoming diagnostic stream.

## Data path

```text
iPhone
  -> iAP2 Identify 0x1d01 (advertise RGI component 0x001e)
  -> RGI control frames 0x5201/0x5202/0x5204
  -> stock mm-ipod CTRL diagnostic stream
  -> bounded decoder and atomic session cache
  -> shared-memory snapshot (atomic rename; stale-state timeout)
  -> late-load HMI bundle on the stock ASL thread
  -> ASLNavBAPAPI + maneuver list 751 + route property 732
  -> ClusterServices navigation guidance
  -> MOST graphics/navigation information
  -> Virtual Cockpit
```

This route-guidance path is implemented and offline-tested. The secondary map
image is implemented as a separate target-locked integration candidate:

```text
CarPlay stream type 111
  -> encrypted AirPlay/ScreenStream receiver
  -> AVCC-to-Annex-B and PCR-bearing MPEG-TS
  -> NavigationServer fixed-block source arbitration
  -> ClusterServices GfxStreamSink
  -> MOST video stream
  -> Virtual Cockpit map layer
```

Everything through the `isoTX` application-write boundary is host-tested or
ARM link/ABI checked. The proprietary transport resource, MOST hardware and
physical cockpit presentation cannot be validated without a target.

## Regional rule

Feature-flag numbers are not stable across regions. For example,
`isSTARTUP_NAV_VARIANT_AVAILABLE` is index 368 in EU P0879D and index 367 in
NAR P0867D. Bind by feature meaning or a verified per-target manifest, never by
copying an EU numeric constant into a NAR binary.

## Failure policy

- Transform into a separate buffer; keep the stock Identify frame unchanged
  until the transformed frame has passed all bounds checks.
- Unknown iAP2 messages remain owned by the stock dispatcher.
- A malformed or stale RGI frame cannot update the cluster cache.
- Disconnect, timeout, or ownership transfer atomically clears route, maneuver,
  lane, and video state before returning control to stock navigation.
- A target fingerprint mismatch is a hard stop.

