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

This repository is a **host-tested, target-locked research implementation**, not
a flashable release. It currently contains:

- a bounded C99 decoder/session core for CarPlay RGI control messages;
- an atomic iAP2 Identify transformer for the RGI component and message lists;
- a P0867D-only QNX preload hook that registers the RGI control group and adds
  the route-guidance component through Delphi's stock Identify builder;
- a bounded reader for the stock `mm-ipod` `CTRL[...]` receive log;
- an exact-input tool that prepares the P0867D `iap2.cfg` message allowlists;
- a read-only firmware analyzer;
- a reproducible target verifier for P0867D;
- verified P0867D/P0879D fingerprints and file-relative binding addresses.

The P0867D iAP2 driver, AirPlay libraries, and DSI interface are byte-identical
to the analyzed P0879D donor. This transfers the known iAP2 file-relative
addresses exactly. CarPlay, ClusterServices, MOST, NavigationServer, and the HMI
are build-specific and must be bound by signature or per-target metadata.

The hook has not yet been exercised on a bench unit. No installer, firmware
patch, signature bypass, HMI/cluster state adapter, or map-video bridge is
published yet. Do not flash generated research artifacts to a vehicle.

The first implemented milestone is route-guidance ingress: advertising the RGI
component, allowing messages `0x5200` through `0x5204`, and safely recognizing
incoming route, maneuver, and lane messages. It does not yet render anything in
the Virtual Cockpit.

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
```

JavaScript tests:

```sh
npm test
```

The Linux CI job also cross-links the P0867D QNX preload library with Clang/LLD.
See [P0867D runtime integration](docs/P0867D_RUNTIME.md) for the binding and
bench-validation contract.

## Implementation sequence

1. Bench-validate the target-locked Identify hook and stock diagnostic receive
   path without installing a persistent firmware modification.
2. Translate decoded route, maneuver, and lane state into the Delphi NavSD/HMI
   model with atomic reset on disconnect.
3. Negotiate CarPlay stream type 111 and route its video through the verified
   ClusterServices/MOST graphics path.
4. Add stock-navigation ownership, watchdog, rollback, and full bench validation.

See [architecture](docs/ARCHITECTURE.md) and the
[P0867D target manifest](targets/MST2_US_VW_P0867D.json).

## Legal and safety

Only original or permissively licensed source belongs in this repository.
Firmware, extracted binaries, signing material, map data, and vendor assets are
excluded. Vehicle testing must begin on a recoverable bench unit with serial
access, verified backups, and a hardware rollback path.

