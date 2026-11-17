#!/bin/bash
# test_objrepl_tombs.sh - 11/16/2026: removes are ordinary ops; tombstones for removed and unknown ids; purge.
# Throwaway folders only.
#
#   utils/objrepl/test_objrepl_tombs.sh BIN
#
#   unknown-target  a link to an id the copy does not hold yet (its add held to the next commit): the copy makes a
#                   placeholder tombstone, the later add replaces it at the same address; copy = primary, 0 dangling
#   live-links      removes of objects that links point to: tombstones on both sides, links still name them
#   unknown-remove  removes of ids that never existed: a tombstone on each side, counted in the digest
#   purge           every tombstone no link points to is purged (replicated); linked tombstones stay; copy = primary
#   full send       a new copy gets the tombstones in the full send: copy = primary
set -u
X=${1:?usage: test_objrepl_tombs.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_tombs.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
K="$T/pki"; mkdir -p "$K"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=objrepl-test-ca" -keyout "$K/ca.key" -out "$K/ca.pem" 2>/dev/null
mk() { openssl req -newkey rsa:2048 -nodes -subj "/CN=$2" -keyout "$K/$1.key" -out "$K/$1.csr" 2>/dev/null
       if [ -n "${3:-}" ]; then printf 'subjectAltName=%s\n' "$3" > "$K/$1.ext"; else : > "$K/$1.ext"; fi
       openssl x509 -req -in "$K/$1.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -days 2 -extfile "$K/$1.ext" -out "$K/$1.pem" 2>/dev/null; }
mk copy objrepl-copy "IP:127.0.0.1,DNS:localhost"; mk primary objrepl-primary
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
LINKS=(--links "lk:0:db0,lk:8:lk")
CPID=""
start_copy() { "$X" copy "$T/$1" --listen $2 "${CTLS[@]}" >> "$T/copy_$1.log" 2>&1 & CPID=$!; sleep 0.4; }
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID 2>/dev/null
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; rm -rf "$T"' EXIT
prim() { "$X" primary "$T/p" --copy 127.0.0.1:$1 "${PTLS[@]}" "${LINKS[@]}" --dbs 2 "${@:2}"; }
# digest of D: "objects N digest H" plus the tombstone and dangling counts from stderr
dg() { local o e; o=$("$X" digest "$T/$1" "${LINKS[@]}" 2> "$T/dg.err"); e=$(cat "$T/dg.err")
       echo "$o tombs $(echo "$e" | grep -o '[0-9]* tombstone' | awk '{print $1}') dangling $(echo "$e" | grep -o '[0-9]* dangling' | awk '{print $1}')"; }
tombs() { echo "$1" | awk '{print $6}'; }
dang()  { echo "$1" | awk '{print $8}'; }
mkdir -p "$T/p" "$T/a" "$T/b"
PA=$((20000 + RANDOM % 10000)); PB=$((PA + 1))

# unknown-target
start_copy a $PA
prim $PA --commits 200 --per 8 --seed 51 --defer-target > "$T/p1.log" 2>&1; rc=$?
stop_copy
dp=$(dg p); da=$(dg a); ph=$(grep -c "placeholder tombstone" "$T/copy_a.log")
[ $rc -eq 0 ] && [ "$dp" == "$da" ] && [ "$ph" -ge 1 ] && [ "$(dang "$da")" == "0" ] \
  && ok "unknown-target: $ph placeholder(s) on the copy, replaced by the later add; copy = primary ($dp)" \
  || bad "unknown-target" "rc $rc placeholders $ph primary [$dp] copy [$da]"

# live-links
start_copy a $PA
prim $PA --drop-linked 6 --drain > "$T/p2.log" 2>&1; rc=$?
stop_copy
t0=$(tombs "$dp"); dp=$(dg p); da=$(dg a)
[ $rc -eq 0 ] && [ "$dp" == "$da" ] && grep -q "removed 6 linked" "$T/p2.log" && [ $(tombs "$dp") -eq $((t0 + 6)) ] && [ "$(dang "$dp")" == "0" ] \
  && ok "live-links: 6 linked objects removed: tombstones $t0 -> $(tombs "$dp") on both, 0 dangling" \
  || bad "live-links" "rc $rc primary [$dp] copy [$da] (before $t0)"

# unknown-remove
start_copy a $PA
prim $PA --unknown-removes 4 --drain > "$T/p3.log" 2>&1; rc=$?
stop_copy
t0=$(tombs "$dp"); dp=$(dg p); da=$(dg a)
[ $rc -eq 0 ] && [ "$dp" == "$da" ] && [ $(tombs "$dp") -eq $((t0 + 4)) ] \
  && ok "unknown-remove: 4 removes of ids never seen: tombstones $t0 -> $(tombs "$dp") on both" \
  || bad "unknown-remove" "rc $rc primary [$dp] copy [$da] (before $t0)"

# purge
start_copy a $PA
prim $PA --purge --drain > "$T/p4.log" 2>&1; rc=$?
stop_copy
dp=$(dg p); da=$(dg a)
kept=$(grep -o "[0-9]* kept" "$T/p4.log" | awk '{print $1}'); pur=$(grep -o "purged [0-9]*" "$T/p4.log" | awk '{print $2}')
[ $rc -eq 0 ] && [ "$dp" == "$da" ] && [ "${pur:-0}" -ge 1 ] && [ $(tombs "$dp") -le "${kept:-0}" ] && [ "$(dang "$dp")" == "0" ] \
  && ok "purge: $pur tombstone(s) purged on both, $(tombs "$dp") linked kept, 0 dangling; copy = primary" \
  || bad "purge" "rc $rc purged ${pur:-0} kept ${kept:-0} primary [$dp] copy [$da]"

# full send: a new copy gets tombstones too, then more commits with removes
start_copy b $PB
prim $PB --commits 30 --per 8 --seed 52 --unknown-removes 2 > "$T/p5.log" 2>&1; rc=$?
stop_copy
dp=$(dg p); db=$(dg b); snaps=$(grep -o "[0-9]* snapshot" "$T/p5.log" | awk '{print $1}')
[ $rc -eq 0 ] && [ "$dp" == "$db" ] && [ "${snaps:-0}" -ge 1 ] \
  && ok "full send: a new copy got the tombstones ($(tombs "$db")) and the stream after; copy = primary" \
  || bad "full send" "rc $rc snaps ${snaps:-0} primary [$dp] copy [$db]"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
