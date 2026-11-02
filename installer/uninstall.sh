#!/bin/ksh

# Restore the stock files saved by install.sh.

set -u

APP_ROOT=/extbin/apps
INSTALL_DIR=$APP_ROOT/mst2-nar-carplay-maps
BACKUP_DIR=$APP_ROOT/mst2-nar-carplay-maps-backup

fail() {
    echo "ERROR: $1"
    exit 1
}

restore() {
    cp "$BACKUP_DIR/$1" "$2" || fail "could not restore $2"
    chmod "$3" "$2" || fail "could not restore permissions for $2"
    cmp -s "$BACKUP_DIR/$1" "$2" || fail "verification failed for $2"
}

echo "MST2 NAR CarPlay Maps beta uninstaller"
[ "$(id -u)" = "0" ] || fail "run this script from a root shell"
[ -f "$BACKUP_DIR/iap2.cfg" ] || fail "stock backup was not found"
[ -f "$BACKUP_DIR/runHMI.sh" ] || fail "stock backup is incomplete"
[ -f "$BACKUP_DIR/bundles.properties" ] || fail "stock backup is incomplete"
[ -f "$BACKUP_DIR/procman_core.cfg" ] || fail "stock backup is incomplete"

mount -uw / >/dev/null 2>&1
mount -uw "$APP_ROOT" >/dev/null 2>&1
touch "$APP_ROOT/.mst2-nar-write-test" 2>/dev/null || fail "$APP_ROOT is not writable"
rm -f "$APP_ROOT/.mst2-nar-write-test"
touch /etc/.mst2-nar-write-test 2>/dev/null || fail "/etc is not writable"
rm -f /etc/.mst2-nar-write-test

restore iap2.cfg "$APP_ROOT/cfg/iap2.cfg" 644
restore runHMI.sh "$APP_ROOT/hmi/HMI/runHMI.sh" 755
restore bundles.properties "$APP_ROOT/hmi/HMI/config/bundles.properties" 644
restore procman_core.cfg /etc/procman_core.cfg 644

rm -f "$INSTALL_DIR/libmst2_nar_ipod.so" "$INSTALL_DIR/libmst2_nar_launch.so"
rm -f "$INSTALL_DIR/libmst2_nar_video_gate.so" "$INSTALL_DIR/libmst2_nar_airplay.so"
rm -f "$INSTALL_DIR/mst2-nar-rgi-hmi.jar" "$INSTALL_DIR/.installed-p0867d"
rmdir "$INSTALL_DIR" 2>/dev/null
sync
command -v MountPathSync >/dev/null 2>&1 && MountPathSync "$APP_ROOT"

echo "Stock files restored. The backup was intentionally kept at $BACKUP_DIR"
echo "Remove the SD card, then reboot the unit manually."
