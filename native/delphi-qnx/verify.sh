#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-"$HERE/build"}
IPOD="$OUT/libmst2_nar_ipod.so"
LAUNCH="$OUT/libmst2_nar_launch.so"
VIDEO="$OUT/libmst2_nar_video_gate.so"
AIRPLAY_RUNTIME="$OUT/libmst2_nar_airplay_runtime.so"
imports(){ readelf --dyn-syms --wide "$1" | awk '$7=="UND" && $8!=""{print $8}' | sort -u; }

for file in "$IPOD" "$LAUNCH" "$VIDEO" "$AIRPLAY_RUNTIME"; do
  echo "verifying $file"
  readelf -h "$file" | grep -E 'Class:|Data:|Type:|Machine:'
  readelf -d "$file" | grep -E 'NEEDED|INIT'
  echo "imports: $(imports "$file" 2>/dev/null || true)"
  test -s "$file"
  readelf -h "$file" | grep -q 'Class:.*ELF32'
  readelf -h "$file" | grep -q 'Data:.*little endian'
  readelf -h "$file" | grep -q 'Type:.*DYN'
  readelf -h "$file" | grep -q 'Machine:.*ARM'
  test "$(readelf -d "$file" | awk '/NEEDED/{gsub(/[][]/,"",$5);print $5}')" = libc.so.3
done
readelf --dyn-syms --wide "$LAUNCH" | grep -Eq 'GLOBAL +DEFAULT +[0-9]+ +spawn$'
readelf --dyn-syms --wide "$LAUNCH" | grep -Eq 'GLOBAL +DEFAULT +[0-9]+ +spawnp$'
readelf -d "$IPOD" | grep -q '(INIT)'
readelf -d "$VIDEO" | grep -q '(INIT)'
readelf --syms --wide "$IPOD" | grep -Eq '[0-9]+ +dcc_delphi_init$'

unexpected=$(imports "$IPOD" | grep -Ev '^(close|delay|dlopen|dlsym|lseek|memcmp|memcpy|memset|open|pthread_create|pthread_detach|read|rename|strlen|write)$' || true)
test -z "$unexpected"
unexpected=$(imports "$LAUNCH" | grep -Ev '^(dlsym|strlen)$' || true)
test -z "$unexpected"
unexpected=$(imports "$VIDEO" | grep -Ev '^(close|delay|dlsym|lseek|memcpy|memset|mmap|open|pthread_create|pthread_detach|write|writev)$' || true)
test -z "$unexpected"
unexpected=$(imports "$AIRPLAY_RUNTIME" | grep -Ev '^(dlsym|memcpy|memset|mmap|mprotect|msync|munmap)$' || true)
test -z "$unexpected"
readelf --dyn-syms --wide "$AIRPLAY_RUNTIME" | grep -Eq 'GLOBAL +DEFAULT +[0-9]+ +dcc_p0867d_airplay_resolve$'
readelf --dyn-syms --wide "$AIRPLAY_RUNTIME" | grep -Eq 'GLOBAL +DEFAULT +[0-9]+ +dcc_runtime_hook_install$'
echo 'P0867D ARM ELF payload verified'
