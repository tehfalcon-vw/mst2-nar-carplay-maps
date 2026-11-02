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
- Incoming RGI `0x5201`, `0x5202`, and `0x5204` currently fall through the
  stock driver's default/no-specialized-handler branch.

Addresses are file-relative research observations, not runtime virtual
addresses. A future native binder must resolve the actual QNX load mapping and
prove buffer ownership, capacity, lifetime, and failure behavior.

## Data path

```text
iPhone
  -> iAP2 Identify 0x1d01 (advertise RGI component 0x001e)
  -> RGI control frames 0x5201/0x5202/0x5204
  -> bounded decoder and session cache
  -> Delphi NavSD/HMI translation
  -> ClusterServices navigation guidance
  -> MOST graphics/navigation information
  -> Virtual Cockpit
```

The secondary map image is a separate path:

```text
CarPlay stream type 111
  -> AirPlay/ScreenStream receiver
  -> NavigationServer source arbitration
  -> ClusterServices GfxStreamSink
  -> MOST video stream
  -> Virtual Cockpit map layer
```

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

