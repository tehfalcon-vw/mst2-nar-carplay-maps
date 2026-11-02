# P0867D Delphi runtime integration

This stage targets only `MST2_US_VW_P0867D / MU0867`. It establishes the
CarPlay Route Guidance Information (RGI) control path; it is not a renderer or
an installation package.

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

After all checks pass, the hook:

1. registers receive metadata for `0x5201`, `0x5202`, and `0x5204` in the empty
   Delphi iAP2 control group;
2. wraps the stock GPS Identify builder, calling it first;
3. appends RGI component `0x0010` using Delphi's own
   `packet_addparam_blob` routine;
4. leaves every unrelated iAP2 group and Identify component untouched.

`tools/build-iap2-config.cjs` adds send IDs `0x5200`/`0x5203` and receive IDs
`0x5201`/`0x5202`/`0x5204` to an exact stock P0867D configuration. It refuses
unknown input and refuses to overwrite an existing output file.

The stock `mm-ipod` process already records decoded control packets as
`CTRL[...]` in `/dev/shmem/iap2.log`. `dcc_log_feed` consumes arbitrary stream
splits, accepts only complete valid RGI frames, and resets atomically after a
malformed capture. This preserves the stock dispatcher while a Delphi HMI state
adapter is developed.

## Build

On Linux with Clang and LLD:

```sh
sh native/delphi-qnx/build.sh
```

The output is `native/delphi-qnx/build/libmst2_nar_ipod.so`. The build uses a
minimal link-only `libc.so.3` import surface so no proprietary QNX SDK or vendor
library is distributed.

## Bench gate

Before any persistent installation, validate on a recoverable Delphi bench unit
with serial access and a verified backup:

1. inject the library for a single `mm-ipod` launch only;
2. confirm the active log marker and unchanged stock media behavior;
3. capture the outgoing Identify packet and verify exactly one component
   `0x0010` plus the five allowlisted message IDs;
4. connect CarPlay and confirm valid `0x5201`, `0x5202`, and `0x5204` frames in
   the stock diagnostic stream;
5. disconnect and power-cycle, confirming complete return to stock behavior.

Do not proceed to HMI or MOST writes until this gate passes. The repository does
not provide a signature bypass, flash patch, or unattended vehicle installer.
