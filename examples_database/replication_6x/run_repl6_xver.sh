#!/bin/bash
# run_repl6_xver.sh DR74_TREE BUILD_DIR [WORK_DIR] - DataReel 6.4: the
# DataReel 7.4 6.4<->7.4 wire tests (utils/objrepl/xver in the 7.4 tree)
# built against this 6.4 library and the 7.4 library of the same 6.x
# CMake build (11/17/2026). Linux.
#   DR74_TREE: the DataReel 7.4 source tree (DATAREEL7_FEATURES_DIR)
#   BUILD_DIR: the 6.x CMake build with DATAREEL7_FEATURES_DIR
#   WORK_DIR:  a new folder (default: a temporary one)
# Checks:
#   wirecmp   the 32-byte gxStream block header packed by 6.4 and by 7.4
#             is the same (and unpacks as packed)
#   xmatrix   10 rows: 6.4 and 7.4 as server and client over mutual TLS,
#             the old 64-bit Linux raw header (accepted) and a 4-byte-word
#             header (refused)
#   hook64    a plain 6.4 gxStream speaking gxObjRepl's handshake: as a
#             primary to the 7.4 objrepl copy, and as a copy for the
#             7.4 objrepl primary
# Both libraries must use the same database engine; a 32-bit engine run
# needs OBJREPL_ALLOW_32BIT=1.
set -u
D7=$(cd "${1:?usage: run_repl6_xver.sh DR74_TREE BUILD_DIR [WORK_DIR]}" && pwd)
B=$(cd "${2:?usage: run_repl6_xver.sh DR74_TREE BUILD_DIR [WORK_DIR]}" && pwd)
HERE=$(cd "$(dirname "$0")/../.." && pwd)   # the 6.4 source tree
W=${3:-$(mktemp -d "${TMPDIR:-/tmp}/repl6x.XXXXXX")}
mkdir -p "$W" && W=$(cd "$W" && pwd)
XV=$D7/utils/objrepl/xver; X7=$B/dr7inner/tools/objrepl
pass=0; fail=0; pids=()
result() { if [ "$1" -eq 0 ]; then pass=$((pass+1)); echo "PASS $2"; else fail=$((fail+1)); echo "FAIL $2"; fi; }
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT
if [ "${OBJREPL_ALLOW_32BIT:-0}" != 1 ] && ! "$X7" engine > /dev/null 2>&1; then
  echo "$(basename "$0"): a 32-bit engine build; set OBJREPL_ALLOW_32BIT=1 for a 32-bit run"; exit 64
fi
defs() { sed -n 's/^CXX_DEFINES = //p' "$1"; }
D6=$(defs "$B/CMakeFiles/datareel.dir/flags.make")
F7=$(defs "$B/dr7inner/datareel7/CMakeFiles/datareel.dir/flags.make")
L6=$B/libdatareel.a; L7=$B/dr7inner/datareel7/libdatareel.a
LIBS="-lssl -lcrypto -lz -lpthread"
cd "$W" || exit 2
cp "$XV"/xmatrix.sh . && chmod +x xmatrix.sh
g++ -O2 -std=gnu++98 $D6 -I "$HERE/include" "$XV/xv6.cpp" -o xv6 "$L6" $LIBS > build.log 2>&1 &&
g++ -O2 -std=gnu++98 $D6 -I "$HERE/include" "$XV/hook64.cpp" -o hook64 "$L6" $LIBS >> build.log 2>&1 &&
g++ -O2 -std=c++17 $F7 -I "$D7/include" "$XV/xv7.cpp" -o xv7 "$L7" $LIBS >> build.log 2>&1 &&
g++ -O2 -std=gnu++98 $D6 -I "$HERE/include" "$XV/wirecmp.cpp" -o wire6 >> build.log 2>&1 &&
g++ -O2 -std=c++17 -DDR7 $F7 -I "$D7/include" "$XV/wirecmp.cpp" -o wire7 >> build.log 2>&1
result $? "built xv6, hook64, wire6 (6.4 library) and xv7, wire7 (7.4 library)"
[ -x xv6 ] && [ -x xv7 ] || { sed 's/^/  /' build.log | head -20; echo "$pass passed, $fail failed"; exit 1; }

./wire6 > w6.txt 2> w6.err; r6=$?; ./wire7 > w7.txt 2> w7.err; r7=$?
[ $r6 -eq 0 ] && [ $r7 -eq 0 ] && cmp -s w6.txt w7.txt && [ -s w6.txt ]
result $? "wirecmp: 6.4 and 7.4 pack the same 32-byte block header ($(wc -l < w6.txt) headers; $(cat w6.err w7.err | tr '\n' ' '))"

# Certificates as utils/objrepl/objrepl_certs.sh names them (pki/)
mkdir -p pki && cd pki || exit 2
export MSYS2_ARG_CONV_EXCL="/CN="
EC="-newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes"
openssl req -x509 $EC -keyout ca.key -out ca.pem -days 2 -subj /CN=objrepl-ca 2>/dev/null
openssl req $EC -keyout copy.key -out copy.csr -subj /CN=objrepl-copy 2>/dev/null
printf 'subjectAltName=IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > copy.ext
openssl x509 -req -in copy.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out copy.pem -days 2 -extfile copy.ext 2>/dev/null
openssl req $EC -keyout primary.key -out primary.csr -subj /CN=objrepl-primary 2>/dev/null
printf 'extendedKeyUsage=clientAuth\n' > primary.ext
openssl x509 -req -in primary.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out primary.pem -days 2 -extfile primary.ext 2>/dev/null
chmod 600 *.key; cd "$W" || exit 2
K=$W/pki

./xmatrix.sh > xmatrix.log 2>&1; r=$?
sed 's/^/  /' xmatrix.log | grep -E "^  (PASS|FAIL)" | cut -c1-150
result $r "xmatrix: $(tail -1 xmatrix.log)"

# listening PORT: without connecting (hook64's copy accepts one connection only)
listening() { for _ in $(seq 100); do ss -ltn 2>/dev/null | grep -q "[:.]$1 " && return 0; sleep 0.1; done; return 1; }
P=$(( 24000 + RANDOM % 8000 ))
# hook64 primary -> 7.4 objrepl copy
mkdir -p hc
"$X7" copy hc --listen 127.0.0.1:$P --ca $K/ca.pem --cert $K/copy.pem --key $K/copy.key --allow-cn objrepl-primary > hc.log 2>&1 & cp=$!; pids+=($cp)
listening $P
timeout 60 ./hook64 primary 127.0.0.1 $P $K > h1.log 2>&1; r=$?
grep -q "F_EPOCH acked" h1.log; [ $r -eq 0 ] && [ $? -eq 0 ]; result $? "hook64 (6.4 gxStream) as a primary: the 7.4 objrepl copy answers the hello and acks F_EPOCH ($(tr '\n' ' ' < h1.log | cut -c1-160))"
touch hc/objrepl.stop; wait $cp
# 7.4 objrepl primary -> hook64 copy
P=$((P + 1)); mkdir -p hp
timeout 60 ./hook64 copy $P $K > h2.log 2>&1 & hp=$!; pids+=($hp)
listening $P
timeout 30 "$X7" primary hp --copy 127.0.0.1:$P --ca $K/ca.pem --cert $K/primary.pem --key $K/primary.key --commits 1 > hp.log 2>&1
wait $hp; r=$?
grep -q "hello magic=3550524f" h2.log && grep -q "F_EPOCH epoch=1" h2.log && [ $r -eq 0 ]
result $? "hook64 (6.4 gxStream) as a copy: the 7.4 objrepl primary's hello and F_EPOCH read ($(tr '\n' ' ' < h2.log | cut -c1-160))"

echo "$pass passed, $fail failed (work folder $W)"
[ $fail -eq 0 ]
