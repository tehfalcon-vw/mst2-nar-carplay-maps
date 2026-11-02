# P0867D Delphi runtime integration

This stage targets only `MST2_US_VW_P0867D / MU0867`. It establishes the
CarPlay Route Guidance Information (RGI) path through the stock cluster
navigation APIs. It is not an installation package or a map-video renderer.

## What is bound

The target manifest SHA-256-locks the complete EXTBIN image and every component
used by this stage, including `ipod-drvr-iap2.so`, `iap2.cfg`, and `mm-ipod`.
The preload library then performs a second set of live checks before writing any
process memory:

- locate the Delphi driver through its exported `ipod_module` symbol;
- derive its load base and verify `packet_addparam_blob`,
  `ident_info_funcs`, and `iap2_ctrl_msg_table` at exact P0867D offsets;
- verify the GPS Identify slot still contains parameter `0x16` and the stock
  builder address;
- verify control group `0x52` is still completely empty.

Any mismatch leaves stock behavior intact and writes a short reason to
`/dev/shmem/mst2-nar-rgi.log`.

## Runtime behavior

After all checks pass, the child hook:

1. registers receive metadata for `0x5201`, `0x5202`, and `0x5204` in the empty
   Delphi iAP2 control group;
2. wraps the stock GPS Identify builder, calling it first;
3. appends RGI component `0x0010` using Delphi's own
   `packet_addparam_blob` routine;
4. tails the stock diagnostic stream into a bounded, atomic route cache;
5. writes the current state with temp-file plus rename semantics; and
6. leaves every unrelated iAP2 group and Identify component untouched.

The separate `libmst2_nar_launch.so` interposes only `spawn` and `spawnp` in
`MediaHandler`. It retains the exact path, argv, file mappings, inheritance, and
environment, except that a child whose path or argv basename is `mm-ipod`
receives `libmst2_nar_ipod.so` at the front of `LD_PRELOAD`. Capacity failure,
missing environment, or any other child passes through unchanged.

`tools/build-iap2-config.cjs` adds send IDs `0x5200`/`0x5203` and receive IDs
`0x5201`/`0x5202`/`0x5204` to an exact stock P0867D configuration. It refuses
unknown input and refuses to overwrite an existing output file.

The stock `mm-ipod` process already records decoded control packets as
`CTRL[...]` in `/dev/shmem/iap2.log`. `dcc_log_feed` consumes arbitrary stream
splits, accepts only complete valid RGI frames, and resets atomically after a
malformed capture. A detached relay thread accepts only component `0x0010`,
applies each message to a copy of the cache, and commits only on complete
success. A five-second missing route heartbeat clears all state.

The original HMI bundle polls the atomic snapshot and then switches onto the
stock ASL thread. It updates maneuver list `751`, distances, RTT, street, lane
guidance through `ASLNavBAPAPI`, and changes route state property `732` last.
The bundle starts in P0867D's existing late-load phase as ID `98`; no Volkswagen
class is copied or replaced.

## Build

On Linux with Clang and LLD:

```sh
sh native/delphi-qnx/build.sh
```

The outputs are `libmst2_nar_ipod.so` and `libmst2_nar_launch.so`. The build uses a
minimal link-only `libc.so.3` import surface so no proprietary QNX SDK or vendor
library is distributed. See the repository README for HMI build and staging
commands.

The stage builder SHA-256 checks the exact stock P0867D `MediaHandler`,
`mm-ipod`, `iap2.cfg`, `runHMI.sh`, and `bundles.properties`; validates both
payloads as ARM ELF32 shared objects; validates the HMI jar; refuses an existing
output directory; and publishes its output by a final directory rename. It
produces patched copies, never edits the supplied firmware tree.

## Bench gate

Before any persistent installation, validate on a recoverable Delphi bench unit
with serial access and a verified backup:

1. install the generated `cpu.conf` line only inside the exact MediaHandler
   service after separately backing up and fingerprinting the unit's file;
2. inject the library for a single `mm-ipod` launch only;
3. confirm the active log marker and unchanged stock media behavior;
4. capture the outgoing Identify packet and verify exactly one component
   `0x0010` plus the five allowlisted message IDs;
5. connect CarPlay and confirm valid `0x5201`, `0x5202`, and `0x5204` frames in
   the stock diagnostic stream;
6. confirm arrows, distance, street, RTT, and lanes in the cluster; then
   disconnect and power-cycle, confirming complete return to stock behavior.

Do not proceed to HMI or MOST writes until this gate passes. The repository does
not provide a signature bypass, flash patch, or unattended vehicle installer.
