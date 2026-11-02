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

This repository is an **offline, host-tested research implementation**, not a
flashable release. It currently contains:

- a bounded C99 decoder/session core for CarPlay RGI control messages;
- an atomic iAP2 Identify transformer for the RGI component and message lists;
- a read-only firmware analyzer;
- a reproducible target verifier for P0867D;
- verified P0867D/P0879D fingerprints and file-relative binding addresses.

The P0867D iAP2 driver, AirPlay libraries, and DSI interface are byte-identical
to the analyzed P0879D donor. This transfers the known iAP2 file-relative
addresses exactly. CarPlay, ClusterServices, MOST, NavigationServer, and the HMI
are build-specific and must be bound by signature or per-target metadata.

No installer, firmware patch, signature bypass, or on-device receiver is
published yet. Do not flash generated research artifacts to a vehicle.

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

## Implementation sequence

1. Bind the Identify transformer after complete `0x1D01` serialization and
   before iAP2 transport framing.
2. Add specialized receive dispatch for RGI `0x5201`, `0x5202`, and `0x5204`,
   preserving stock dispatch for every unrelated message.
3. Translate decoded route, maneuver, and lane state into the Delphi NavSD/HMI
   model with atomic reset on disconnect.
4. Negotiate CarPlay stream type 111 and route its video through the verified
   ClusterServices/MOST graphics path.
5. Add stock-navigation ownership, watchdog, rollback, and bench validation.

See [architecture](docs/ARCHITECTURE.md) and the
[P0867D target manifest](targets/MST2_US_VW_P0867D.json).

## Legal and safety

Only original or permissively licensed source belongs in this repository.
Firmware, extracted binaries, signing material, map data, and vendor assets are
excluded. Vehicle testing must begin on a recoverable bench unit with serial
access, verified backups, and a hardware rollback path.

