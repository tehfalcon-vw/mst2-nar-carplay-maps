#!/bin/ksh

# MST2 NAR CarPlay Maps beta installer for MST2_US_VW_P0867D only.

set -u

SELF_DIR=$(cd "$(dirname "$0")" && pwd)
APP_ROOT=/extbin/apps
INSTALL_DIR=$APP_ROOT/mst2-nar-carplay-maps
BACKUP_DIR=$APP_ROOT/mst2-nar-carplay-maps-backup
MARKER=$INSTALL_DIR/.installed-p0867d

fail() {
    echo "ERROR: $1"
    exit 1
}

bytes() {
    wc -c < "$1" | tr -d ' '
}

expect_stock() {
    [ -f "$1" ] || fail "missing stock file: $1"
    ACTUAL=$(bytes "$1")
    [ "$ACTUAL" = "$2" ] || fail "unexpected $1 size ($ACTUAL, expected $2); this is not stock P0867D"
    if command -v sha256sum >/dev/null 2>&1; then
        ACTUAL_HASH=$(sha256sum "$1" | awk '{print $1}')
        [ "$ACTUAL_HASH" = "$3" ] || fail "unexpected $1 SHA-256; this is not stock P0867D"
    fi
}

copy_checked() {
    SOURCE=$1
    DESTINATION=$2
    MODE=$3
    cp "$SOURCE" "$DESTINATION" || return 1
    chmod "$MODE" "$DESTINATION" || return 1
    cmp -s "$SOURCE" "$DESTINATION" || return 1
    return 0
}

discard_incomplete_backup() {
    rm -f "$BACKUP_DIR/iap2.cfg" "$BACKUP_DIR/runHMI.sh"
    rm -f "$BACKUP_DIR/bundles.properties" "$BACKUP_DIR/procman_core.cfg"
    rmdir "$BACKUP_DIR" 2>/dev/null
    fail "$1; no live files were changed"
}

rollback() {
    echo "Install failed; restoring stock files..."
    [ -f "$BACKUP_DIR/iap2.cfg" ] && cp "$BACKUP_DIR/iap2.cfg" "$APP_ROOT/cfg/iap2.cfg"
    [ -f "$BACKUP_DIR/runHMI.sh" ] && cp "$BACKUP_DIR/runHMI.sh" "$APP_ROOT/hmi/HMI/runHMI.sh"
    [ -f "$BACKUP_DIR/bundles.properties" ] && cp "$BACKUP_DIR/bundles.properties" "$APP_ROOT/hmi/HMI/config/bundles.properties"
    [ -f "$BACKUP_DIR/procman_core.cfg" ] && cp "$BACKUP_DIR/procman_core.cfg" /etc/procman_core.cfg
    chmod 644 "$APP_ROOT/cfg/iap2.cfg" "$APP_ROOT/hmi/HMI/config/bundles.properties" /etc/procman_core.cfg 2>/dev/null
    chmod 755 "$APP_ROOT/hmi/HMI/runHMI.sh" 2>/dev/null
    rm -f "$INSTALL_DIR/libmst2_nar_ipod.so" "$INSTALL_DIR/libmst2_nar_launch.so"
    rm -f "$INSTALL_DIR/libmst2_nar_video_gate.so" "$INSTALL_DIR/libmst2_nar_airplay.so"
    rm -f "$INSTALL_DIR/mst2-nar-rgi-hmi.jar" "$MARKER"
    sync
    echo "Stock files restored. Do not reboot until the errors above are understood."
    exit 1
}

echo "MST2 NAR CarPlay Maps beta installer"
echo "Target: MST2_US_VW_P0867D / MU0867 / Delphi"
echo

[ "$(id -u)" = "0" ] || fail "run this script from a root shell"
[ -d "$SELF_DIR/payload" ] || fail "payload directory is missing"
[ -d "$SELF_DIR/patched" ] || fail "patched directory is missing"
[ ! -e "$MARKER" ] || fail "beta is already installed; run uninstall.sh first"
[ ! -e "$BACKUP_DIR" ] || fail "backup directory already exists; preserve it and inspect the previous install"

expect_stock "$APP_ROOT/cfg/iap2.cfg" 6886 e51bb331ef2290a518dc4795bc1e646c5be85fdc3e72cca39ccdb6ca6af3e1ea
expect_stock "$APP_ROOT/hmi/HMI/runHMI.sh" 12225 ecb7ad9e037782e34668c70bec3f5f2e3d90079fc024b2e07ff840046fc0d6ba
expect_stock "$APP_ROOT/hmi/HMI/config/bundles.properties" 9416 e5167a7efc2bacdd7e6817e08c45153339e1f540aad36c3078ae74773372d12b
expect_stock /etc/procman_core.cfg 6408 004783991da86ca2956470636265be212ab189c5f393afa161affead873680a1

grep -q 'CarPlay.*-c15' /etc/procman_core.cfg || fail "P0867D CarPlay service was not found"
grep -q 'NavigationServer.*NavServer.cfg' /etc/procman_core.cfg || fail "P0867D NavigationServer service was not found"
grep -q 'LD_PRELOAD=.*/mst2-nar-carplay-maps' /etc/procman_core.cfg && fail "procman configuration is already modified"

# QNX mount accepts -uw on a mounted filesystem. Some Delphi layouts keep
# /extbin separate; try both mount points, then prove writability before backup.
mount -uw / >/dev/null 2>&1
mount -uw "$APP_ROOT" >/dev/null 2>&1
touch "$APP_ROOT/.mst2-nar-write-test" 2>/dev/null || fail "$APP_ROOT is not writable"
rm -f "$APP_ROOT/.mst2-nar-write-test"
touch /etc/.mst2-nar-write-test 2>/dev/null || fail "/etc is not writable; no files were changed"
rm -f /etc/.mst2-nar-write-test

mkdir "$BACKUP_DIR" || fail "could not create backup directory"
cp "$APP_ROOT/cfg/iap2.cfg" "$BACKUP_DIR/iap2.cfg" || discard_incomplete_backup "could not back up iap2.cfg"
cp "$APP_ROOT/hmi/HMI/runHMI.sh" "$BACKUP_DIR/runHMI.sh" || discard_incomplete_backup "could not back up runHMI.sh"
cp "$APP_ROOT/hmi/HMI/config/bundles.properties" "$BACKUP_DIR/bundles.properties" || discard_incomplete_backup "could not back up bundles.properties"
cp /etc/procman_core.cfg "$BACKUP_DIR/procman_core.cfg" || discard_incomplete_backup "could not back up procman_core.cfg"
chmod 644 "$BACKUP_DIR/iap2.cfg" "$BACKUP_DIR/bundles.properties" "$BACKUP_DIR/procman_core.cfg" || discard_incomplete_backup "could not protect backup file modes"
chmod 755 "$BACKUP_DIR/runHMI.sh" || discard_incomplete_backup "could not protect runHMI.sh mode"
cmp -s "$APP_ROOT/cfg/iap2.cfg" "$BACKUP_DIR/iap2.cfg" || discard_incomplete_backup "iap2.cfg backup verification failed"
cmp -s "$APP_ROOT/hmi/HMI/runHMI.sh" "$BACKUP_DIR/runHMI.sh" || discard_incomplete_backup "runHMI.sh backup verification failed"
cmp -s "$APP_ROOT/hmi/HMI/config/bundles.properties" "$BACKUP_DIR/bundles.properties" || discard_incomplete_backup "bundles.properties backup verification failed"
cmp -s /etc/procman_core.cfg "$BACKUP_DIR/procman_core.cfg" || discard_incomplete_backup "procman_core.cfg backup verification failed"

mkdir "$INSTALL_DIR" || rollback
copy_checked "$SELF_DIR/payload/libmst2_nar_ipod.so" "$INSTALL_DIR/libmst2_nar_ipod.so" 755 || rollback
copy_checked "$SELF_DIR/payload/libmst2_nar_launch.so" "$INSTALL_DIR/libmst2_nar_launch.so" 755 || rollback
copy_checked "$SELF_DIR/payload/libmst2_nar_video_gate.so" "$INSTALL_DIR/libmst2_nar_video_gate.so" 755 || rollback
copy_checked "$SELF_DIR/payload/libmst2_nar_airplay.so" "$INSTALL_DIR/libmst2_nar_airplay.so" 755 || rollback
copy_checked "$SELF_DIR/payload/mst2-nar-rgi-hmi.jar" "$INSTALL_DIR/mst2-nar-rgi-hmi.jar" 644 || rollback
copy_checked "$SELF_DIR/patched/cfg/iap2.cfg" "$APP_ROOT/cfg/iap2.cfg" 644 || rollback
copy_checked "$SELF_DIR/patched/hmi/HMI/runHMI.sh" "$APP_ROOT/hmi/HMI/runHMI.sh" 755 || rollback
copy_checked "$SELF_DIR/patched/hmi/HMI/config/bundles.properties" "$APP_ROOT/hmi/HMI/config/bundles.properties" 644 || rollback
copy_checked "$SELF_DIR/patched/etc/procman_core.cfg" /etc/procman_core.cfg 644 || rollback

echo 'MST2_US_VW_P0867D beta' > "$MARKER" || rollback
sync
command -v MountPathSync >/dev/null 2>&1 && MountPathSync "$APP_ROOT"

echo
echo "Installation completed successfully."
echo "Backup: $BACKUP_DIR"
echo "Remove the SD card, then reboot the unit manually."
echo "Keep serial/Telnet access available for the first boot."
