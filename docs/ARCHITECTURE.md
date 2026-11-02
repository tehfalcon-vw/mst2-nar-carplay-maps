# Architecture

The beta adds two independent CarPlay paths to the stock P0867D software.

## Route guidance

```text
iPhone RGI messages
  -> stock mm-ipod transport
  -> bounded RGI decoder and session cache
  -> shared state file
  -> late-load HMI bundle on the ASL thread
  -> stock NavBAP and ClusterServices APIs
  -> Virtual Cockpit guidance
```

The iAP2 preload registers only message group `0x52` after confirming the exact
P0867D tables are unchanged. Unknown messages remain stock-owned. Malformed or
stale input clears the custom state without replacing the stock dispatcher.

## Secondary map video

```text
iPhone AirPlay stream 111
  -> target-locked libAirPlay hooks
  -> continuous AES-CTR and Screen packet parser
  -> AVCC-to-Annex-B conversion and IDR gate
  -> latest-frame shared-memory slot
  -> NavigationServer MPEG-TS worker
  -> /dev/transport_server/spawn/isoTX
  -> MOST GfxStreamSink
  -> Virtual Cockpit map
```

The receiver advertises a 1010×376 auxiliary display. The transport candidate
uses H.264 PID `0x11`, PMT PID `0x1000`, source-derived PTS, PCR, and 64-packet
writes. Stock NavigationServer output is suppressed only after the first full
custom block succeeds. A stop, error, missing IDR, or stale stream immediately
returns ownership to the factory path.

## Target locking

The package builder verifies the stock firmware and every modified component
against `targets/MST2_US_VW_P0867D.json`. Runtime hooks also verify library
addresses and instruction prologues before changing process memory. A mismatch
is a hard stop.

## Modified runtime files

The installer backs up and replaces:

- `/extbin/apps/cfg/iap2.cfg`
- `/extbin/apps/hmi/HMI/runHMI.sh`
- `/extbin/apps/hmi/HMI/config/bundles.properties`
- `/etc/procman_core.cfg`

Payloads are installed under `/extbin/apps/mst2-nar-carplay-maps`. The
uninstaller restores the four originals and removes the payload.
