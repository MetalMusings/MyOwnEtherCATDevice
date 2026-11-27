#!/bin/sh
# Host unit tests for the CiA402 logic and the generated object dictionary.
# Usage: test/host/run_tests.sh   (from anywhere, needs gcc and g++)
set -e
FW="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/easercat3000-host-tests"
mkdir -p "$OUT"
INC="-I$FW/include -I$FW/lib/soes -I$FW/lib/soes-esi -I$FW/lib/soes/include/sys/gcc"
for f in esc.c esc_coe.c ecat_slv.c; do
  gcc -c -g -O0 -Wall $INC "$FW/lib/soes/$f" -o "$OUT/${f%.c}.o"
done
gcc -c -g -O0 -Wall $INC "$FW/lib/soes-esi/objectlist.c" -o "$OUT/objectlist.o"
g++ -g -O0 -Wall -Wextra $INC \
  "$FW/test/host/test_main.cpp" "$FW/src/Cia402Axis.cpp" "$FW/src/ObjectHooks.cpp" "$FW/src/StepGenLoop.cpp" \
  "$OUT"/esc.o "$OUT"/esc_coe.o "$OUT"/ecat_slv.o "$OUT"/objectlist.o -lm -o "$OUT/tests"
"$OUT/tests"
