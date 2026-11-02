# MST2 NAR CarPlay Maps

CarPlay turn guidance and secondary-map video for North American VW Delphi
MST2 units with a Virtual Cockpit.

> [!CAUTION]
> This is an untested vehicle beta. It modifies startup configuration on the
> infotainment unit and can cause a boot loop. Test only with stable power, a
> working root serial/Telnet shell, verified backups, and a recovery method.

## Supported unit

Only this exact target is accepted:

| Item | Required value |
|---|---|
| Platform | Delphi MST2 navigation unit |
| Train | `MST2_US_VW_P0867D` |
| MU version | `0867` |
| Variant | `STD2Nav_NAR_VW` |
| Cluster | MQB Virtual Cockpit connected through MOST |

The installer rejects unexpected stock-file sizes. Do not use it on EU
firmware, other NAR trains, non-navigation units, or TechniSat/Preh units.

## Beta status

The complete path is implemented and passes host, protocol, ARM-link, and HMI
build tests. It has not yet been exercised on a physical Delphi unit. Testers
are needed to validate boot behavior, CarPlay negotiation, route guidance,
MPEG-TS compatibility, map presentation, and recovery.

Implemented behavior:

- CarPlay arrows, street, distance, arrival time, and lane guidance;
- encrypted CarPlay secondary-screen stream type 111;
- 1010×376 cluster map with ViewArea and SafeArea metadata;
- H.264 conversion to PCR-bearing MPEG-TS for Delphi `isoTX`;
- IDR gating, stale-stream watchdog, and automatic factory-map fallback;
- target-locked runtime checks that leave stock behavior unchanged on a binary
  mismatch.

## What testers need

1. A confirmed `MST2_US_VW_P0867D` Delphi navigation unit.
2. A root serial or Telnet shell that already works.
3. A FAT32 SD card or USB drive visible from QNX.
4. Stable vehicle or bench power during installation and first boot.
5. The complete beta ZIP from this repository's **Releases** page.
6. A copy of the original P0867D firmware and a known recovery procedure.

If you do not already have root access and a recovery path, do not beta test
this build. This repository does not install a toolbox, unlock Telnet, bypass
firmware signatures, or provide factory firmware.

## Install the beta

### 1. Prepare the card

Extract the beta ZIP directly to the root of a FAT32 SD card. The card should
look like this:

```text
install.sh
uninstall.sh
stage-manifest.json
payload/
patched/
```

### 2. Confirm the unit

In the unit's version screen, confirm the train is exactly:

```text
MST2_US_VW_P0867D
MU 0867
```

Connect the serial/Telnet shell and log in as `root`. Keep that shell open for
the first reboot.

### 3. Find the card

Insert the card and run:

```sh
mount
```

SD slot 1 is commonly `/media/mp000`, but use the path shown on your unit.
Confirm the installer is present:

```sh
ls -l /media/mp000/install.sh
```

### 4. Install

Disconnect the iPhone and run:

```sh
ksh /media/mp000/install.sh
```

The installer will:

1. verify the live files match stock P0867D sizes, SHA-256 values when
   available, and service definitions;
2. prove `/extbin/apps` and `/etc` are writable before changing anything;
3. back up all four modified stock files to
   `/extbin/apps/mst2-nar-carplay-maps-backup`;
4. copy and verify the beta payload and patched configuration; and
5. stop and restore the backup automatically if a copy fails.

Do not reboot unless the script prints `Installation completed successfully`.
Remove the card and reboot the unit manually.

## First test

Perform the first test while parked or on a bench.

1. Confirm the unit reaches the normal HMI and that radio/media still work.
2. Connect an iPhone and start CarPlay.
3. Start Apple Maps or Google Maps navigation.
4. Confirm arrows, distance, street name, arrival time, and lanes appear in the
   cluster.
5. Select the normal map view in the Virtual Cockpit and check whether the
   CarPlay map replaces the factory navigation map.
6. End navigation, disconnect the phone, and verify the factory map returns.
7. Repeat a CarPlay reconnect and one full ignition cycle.

Please report partial success precisely. Working arrows with no map, a black
map, one frozen frame, a disconnect, or a boot loop each identify a different
remaining boundary.

## Uninstall or recover

With the card mounted and a root shell available, run:

```sh
ksh /media/mp000/uninstall.sh
```

The uninstaller restores the four stock files, verifies them, removes the beta
payload, and intentionally keeps the backup directory. Remove the card and
reboot manually.

If the HMI does not start but the root shell still works, run `uninstall.sh`
before doing anything else. If the unit no longer reaches a shell, use the
recovery method and original P0867D firmware prepared before testing.

## Tester report

Open a GitHub issue with:

- vehicle model/year and Virtual Cockpit type;
- head-unit part number, train, MU version, and region;
- iPhone model, iOS version, and map application;
- boot, guidance, map, disconnect, reconnect, and ignition-cycle results;
- whether uninstall restored normal operation; and
- serial/HMI logs with personal data removed.

From the root shell, a useful first log is:

```sh
sloginfo > /media/mp000/mst2-nar-carplay-maps.log
```

## Build a beta package

Firmware is not included. Maintainers need Node.js 22, CMake, Clang/LLD, JDK 8,
an extracted stock P0867D `apps.tmp`, the matching `STDF.bin`, and a converted
P0867D `MIBHMI.jar`.

```sh
npm test
cmake -S native/rgi -B build/rgi && cmake --build build/rgi
ctest --test-dir build/rgi --output-on-failure
cmake -S native/video -B build/video && cmake --build build/video
ctest --test-dir build/video --output-on-failure
sh native/delphi-qnx/build.sh
sh native/delphi-qnx/verify.sh
```

Build the HMI bundle on Windows:

```powershell
.\hmi\test.ps1 -JdkBin C:\jdk8\bin
.\hmi\build.ps1 -MibHmi C:\private\P0867D\MIBHMI.jar -JdkBin C:\jdk8\bin
```

Create the installable stage:

```sh
npm run build-p0867d-stage -- \
  path/to/apps.tmp \
  path/to/STDF.bin \
  native/delphi-qnx/build \
  hmi/build/mst2-nar-rgi-hmi.jar \
  p0867d-beta
```

The builder SHA-256 checks the exact firmware inputs, validates every payload,
and refuses to overwrite an existing output directory. See
[architecture](docs/ARCHITECTURE.md) for the runtime design.

## License and trademarks

Project source is available under the [MIT License](LICENSE). Firmware, signing
material, map data, and vendor binaries are not included. Volkswagen, Audi,
Apple, CarPlay, and other trademarks belong to their respective owners. This
is an independent community project.
