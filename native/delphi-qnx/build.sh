#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-"$HERE/build"}
CLANG=${CLANG:-clang}
LLD=${LLD:-ld.lld}
CFLAGS="--target=armv7a-none-eabi -marm -mfloat-abi=soft -fPIC -Os -ffreestanding -nostdlib -fno-stack-protector -fno-builtin -fvisibility=hidden -std=c99 -Wall -Wextra -Werror -I$HERE/include -I$HERE/../rgi/include -I$HERE/../video/include"
rm -rf "$OUT"
mkdir -p "$OUT"
$CLANG $CFLAGS -fvisibility=default -c "$HERE/stubs/libc_stub.c" -o "$OUT/libc_stub.o"
$LLD -shared --hash-style=sysv -soname libc.so.3 -o "$OUT/libc.so.3" "$OUT/libc_stub.o"
$CLANG $CFLAGS -c "$HERE/src/ipod_hook.c" -o "$OUT/ipod_hook.o"
$CLANG $CFLAGS -c "$HERE/src/launch_hook.c" -o "$OUT/launch_hook.o"
$CLANG $CFLAGS -fvisibility=default -c "$HERE/src/video_gate.c" -o "$OUT/video_gate.o"
$CLANG $CFLAGS -c "$HERE/../video/src/dcc_ts.c" -o "$OUT/dcc_ts.o"
$CLANG $CFLAGS -c "$HERE/../video/src/dcc_video_slot.c" -o "$OUT/dcc_video_slot.o"
$CLANG $CFLAGS -c "$HERE/src/relay.c" -o "$OUT/relay.o"
$CLANG $CFLAGS -c "$HERE/src/aeabi.c" -o "$OUT/aeabi.o"
$CLANG $CFLAGS -c "$HERE/../rgi/src/dcc_rgi.c" -o "$OUT/dcc_rgi.o"
$CLANG $CFLAGS -c "$HERE/../rgi/src/dcc_identify.c" -o "$OUT/dcc_identify.o"
$CLANG $CFLAGS -c "$HERE/../rgi/src/dcc_log.c" -o "$OUT/dcc_log.o"
$CLANG $CFLAGS -c "$HERE/../rgi/src/dcc_route.c" -o "$OUT/dcc_route.o"
$LLD -shared -z defs -z max-page-size=0x1000 --hash-style=sysv --gc-sections \
  -soname libmst2_nar_ipod.so -init dcc_delphi_init \
  -o "$OUT/libmst2_nar_ipod.so" "$OUT/ipod_hook.o" "$OUT/relay.o" "$OUT/aeabi.o" "$OUT/dcc_rgi.o" \
  "$OUT/dcc_identify.o" "$OUT/dcc_log.o" "$OUT/dcc_route.o" "$OUT/libc.so.3"
$LLD -shared -z defs -z max-page-size=0x1000 --hash-style=sysv --gc-sections \
  -soname libmst2_nar_launch.so -o "$OUT/libmst2_nar_launch.so" \
  "$OUT/launch_hook.o" "$OUT/libc.so.3"
$LLD -shared -z defs -z max-page-size=0x1000 --hash-style=sysv --gc-sections \
  -soname libmst2_nar_video_gate.so -init dcc_video_gate_init \
  -o "$OUT/libmst2_nar_video_gate.so" "$OUT/video_gate.o" "$OUT/dcc_ts.o" \
  "$OUT/dcc_video_slot.o" "$OUT/libc.so.3"
echo "$OUT/libmst2_nar_ipod.so"
echo "$OUT/libmst2_nar_launch.so"
echo "$OUT/libmst2_nar_video_gate.so"
