# MST2 NAR CarPlay Maps

MST2-only research and implementation work for displaying CarPlay route guidance
and, ultimately, the CarPlay secondary map stream in North American VW/Audi
Virtual Cockpit systems using **Delphi MST2** head units.

## Scope

- Delphi MST2 only.
- North American Region (NAR) firmware only.
- First verified target: `MST2_US_VW_P0867D / STD2Nav_NAR_VW`.
- EU `MST2_EU_VW_P0879D / STD2Nav_EU_VW` is the comparison donor.
- TechniSat/Preh, MHI2, MHI2Q, MHS2, and MH2P are intentionally out of scope.

## Current status

This repository is a **host-tested, target-locked integration candidate**, not
a vehicle-validated release. It currently contains:

- a bounded C99 decoder/session core for CarPlay RGI control messages;
- a bounded C99 H.264 Annex-B to MPEG-TS packetizer for the Delphi MOST path;
- a strict avcC/AVCC parser that emits decoder-ready Annex-B access units and
  prepends the negotiated SPS/PPS to every IDR;
- a seqlock-protected latest-frame transport from the CarPlay process to a
  P0867D NavigationServer worker, with IDR gating and automatic stock fallback;
- an atomic iAP2 Identify transformer for the RGI component and message lists;
- a P0867D-only QNX preload hook that registers the RGI control group and adds
  the route-guidance component through Delphi's stock Identify builder;
- a bounded reader for the stock `mm-ipod` `CTRL[...]` receive log;
- an atomic route, maneuver, and lane cache with disconnect timeout;
- a P0867D HMI bundle that publishes through the stock `ASLNavBAPAPI`, list
  `751`, and route-guidance property `732`;
- a `MediaHandler` spawn interposer that adds the child preload only when the
  launched argv names `mm-ipod`;
- an exact-input tool that prepares the P0867D `iap2.cfg` message allowlists;
- a fail-closed staging builder for the two patched HMI files and payload;
- a read-only firmware analyzer;
- a reproducible target verifier for P0867D;
- verified P0867D/P0879D fingerprints and file-relative binding addresses.

The P0867D iAP2 driver, AirPlay libraries, and DSI interface are byte-identical
to the analyzed P0879D donor. This transfers the known iAP2 file-relative
addresses exactly. CarPlay, ClusterServices, MOST, NavigationServer, and the HMI
are build-specific and must be bound by signature or per-target metadata.

The complete route-guidance path has not yet been exercised on a bench unit.
The staging builder does not modify firmware, patch signatures, or install to a
unit. It verifies and extracts the exact MediaHandler service definition from
P0867D `STDF.bin`, then emits a patched copy that launches MediaHandler through
stock QNX `env` with a scoped preload. Do not flash generated research artifacts
to a vehicle.

The first implemented milestone is end-to-end arrow, street, distance, time,
and lane guidance. The video consumer, H.264 conversion and Delphi MOST output
side are implemented offline. The remaining video milestone is the target-locked
AirPlay stream-111 negotiation/CTR receiver that publishes into that consumer;
the repository does not claim working map video until that boundary and the
physical MOST/Virtual Cockpit path are exercised.

## Verify a firmware image

The repository never contains firmware. Supply your own `EXTBIN_APPS.bin`:

```sh
npm run analyze -- path/to/EXTBIN_APPS.bin reports/p0867d.json
npm run verify-target -- reports/p0867d.json targets/MST2_US_VW_P0867D.json
```

The analyzer does not extract or execute files. The verifier requires all
security-relevant fingerprints to match before a target is accepted.

## Native core tests

```sh
cmake -S native/rgi -B build/rgi
cmake --build build/rgi
ctest --test-dir build/rgi --output-on-failure
cmake -S native/video -B build/video
cmake --build build/video
ctest --test-dir build/video --output-on-failure
```

JavaScript tests:

```sh
npm test
```

The Linux CI job also cross-links the P0867D QNX preload library with Clang/LLD.
See [P0867D runtime integration](docs/P0867D_RUNTIME.md) for the binding and
bench-validation contract.

See [P0867D secondary video](docs/P0867D_VIDEO.md) for the exact Delphi
stream-111/MOST path and the remaining target gates.

The HMI model can be checked with JDK 8 and the actual bundle can be compiled
against a locally converted, user-supplied P0867D `MIBHMI.jar`:

```powershell
.\hmi\test.ps1 -JdkBin C:\path\to\jdk8\bin
.\hmi\build.ps1 -MibHmi C:\private\MIBHMI.jar -JdkBin C:\path\to\jdk8\bin
```

After both native libraries and the HMI jar are built, create a non-installing
stage from the exact extracted `apps.tmp` directory:

```sh
npm run build-p0867d-stage -- path/to/apps.tmp path/to/STDF.bin native/delphi-qnx/build hmi/build/mst2-nar-rgi-hmi.jar p0867d-stage
```

## Implementation sequence

1. Bench-validate the target-locked complete RGI-to-NavBAP path without an
   unattended persistent installation.
2. Negotiate CarPlay stream type 111 and route its video through the verified
   ClusterServices/MOST graphics path.
3. Add stock-navigation ownership, watchdog, rollback, and full bench validation.

See [architecture](docs/ARCHITECTURE.md) and the
[P0867D target manifest](targets/MST2_US_VW_P0867D.json).

## Legal and safety

Only original or permissively licensed source belongs in this repository.
Firmware, extracted binaries, signing material, map data, and vendor assets are
excluded. Vehicle testing must begin on a recoverable bench unit with serial
access, verified backups, and a hardware rollback path.

