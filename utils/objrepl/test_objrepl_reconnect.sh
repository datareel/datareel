#!/bin/bash
# test_objrepl_reconnect.sh - 11/16/2026: the reconnect check after any gxStream disruption. Throwaway folders only.
#
#   utils/objrepl/test_objrepl_reconnect.sh BIN
#
#   drop      the copy drops the gxStream every 7 frames, mid-stream, without restarting: the primary reconnects,
#             checks, resumes from the copy's applied seq; copy = primary
#   restart   each side restarted (TERM) between commits: copy = primary
#   trimmed   copy A away while copy B acknowledges and the primary trims past A: A gets a full send
#   history   a copy that followed primary P1 meets primary P2 (another history at the same seq): refused, alarm
#   name      a primary that is not the copy's expected peer: refused, alarm
#   roles     the copy takes over (epoch 2, role primary) while disconnected: it will not run as a copy, the old
#             primary is refused, alarm
set -u
X=${1:?usage: test_objrepl_reconnect.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's)
"$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_reconnect.XXXXXX")
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
CPID=""
start_copy() { local d=$1 port=$2; shift 2; "$X" copy "$T/$d" --listen $port "${CTLS[@]}" "$@" >> "$T/copy_$d.log" 2>&1 & CPID=$!; sleep 0.4; }
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID 2>/dev/null
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; rm -rf "$T"' EXIT
prim() { local d=$1 port=$2; shift 2; "$X" primary "$T/$d" --copy 127.0.0.1:$port "${PTLS[@]}" "$@"; }
same() { [ "$("$X" digest "$T/$1")" == "$("$X" digest "$T/$2")" ]; }
P0=$((20000 + RANDOM % 10000))

# drop: the copy drops the stream every 7 frames
mkdir -p "$T/p" "$T/a"
OBJREPL_TEST_DROP_EVERY=7 "$X" copy "$T/a" --listen $P0 "${CTLS[@]}" --name copyA --peer-name P1 >> "$T/copy_a.log" 2>&1 & CPID=$!; sleep 0.4
prim p $P0 --name P1 --peer-name copyA --commits 120 --per 6 --seed 21 > "$T/p_drop.log" 2>&1; rc=$?
stop_copy
same p a && [ $rc -eq 0 ] && ok "drop: the stream dropped every 7 frames, the primary reconnected and resumed; copy = primary" || bad "drop" "rc $rc $(tail -1 "$T/p_drop.log")"

# restart: each side restarted between commits
for i in 1 2 3; do
  start_copy a $P0 --name copyA --peer-name P1
  prim p $P0 --name P1 --peer-name copyA --commits 20 --per 6 --seed $((30 + i)) > "$T/p_rs$i.log" 2>&1; rc=$?
  stop_copy
  [ $rc -eq 0 ] || break
done
same p a && [ $rc -eq 0 ] && ok "restart: 3 times both sides restarted; copy = primary" || bad "restart" "rc $rc"

# trimmed: copy A away; copy B acknowledges and the primary trims past A; then A must get a full send
mkdir -p "$T/b"
start_copy b $((P0 + 1)) --name copyB --peer-name P1
prim p $((P0 + 1)) --name P1 --peer-name copyB --commits 40 --per 6 --seed 41 > "$T/p_b.log" 2>&1; rc=$?
stop_copy
start_copy a $P0 --name copyA --peer-name P1
prim p $P0 --name P1 --peer-name copyA --drain > "$T/p_a2.log" 2>&1; rc2=$?
stop_copy
snaps=$(grep -o "[0-9]* snapshot" "$T/p_a2.log" | awk '{print $1}')
same p a && [ $rc -eq 0 ] && [ $rc2 -eq 0 ] && [ "${snaps:-0}" -ge 1 ] && ok "trimmed: copy A behind a trimmed outbox got a full send ($snaps); copy = primary" || bad "trimmed" "rc $rc/$rc2 snaps ${snaps:-0}"

# history: copy A (following P1) meets primary P2 with another history
mkdir -p "$T/p2"
start_copy a $P0 --name copyA
prim p2 $P0 --name P1 --commits 400 --per 4 --seed 99 > "$T/p_p2.log" 2>&1; rc=$?
stop_copy
grep -q "ALARM" "$T/p_p2.log" && [ $rc -eq 3 ] && same p a && ok "history: another primary with a different history at the copy's seq was refused (alarm), the copy unchanged" \
  || bad "history" "rc $rc $(grep -m1 ALARM "$T/p_p2.log")"

# name: the copy expects P1, a primary named P9 connects
start_copy a $P0 --name copyA --peer-name P1
prim p $P0 --name P9 --commits 2 --per 2 > "$T/p_name.log" 2>&1; rc=$?
stop_copy
grep -q "ALARM" "$T/p_name.log" && grep -q "not the expected primary" "$T/a/objrepl.alarm" && [ $rc -eq 3 ] && ok "name: an unexpected primary was refused (alarm on both sides)" || bad "name" "rc $rc"

# roles: the copy takes over while disconnected; it will not run as a copy, and the old primary is refused
"$X" takeover "$T/a" > /dev/null
"$X" copy "$T/a" --listen $P0 "${CTLS[@]}" --name copyA > "$T/copy_a_role.log" 2>&1; crc=$?
grep -q "two primaries" "$T/copy_a_role.log" && [ $crc -eq 4 ] && ok "roles: the node that took over refuses to run as a copy (alarm, exit 4)" || bad "roles copy" "rc $crc"
st=$("$X" status "$T/a")
echo "$st" | grep -q "role primary" && echo "$st" | grep -q "^epoch 2" && ok "roles: the new primary is at epoch 2 with role primary" || bad "roles state" "$st"
start_copy b $((P0 + 1)) --name copyB
prim a $((P0 + 1)) --name copyA --commits 1 --per 1 > "$T/p_newp.log" 2>&1; rc=$?
stop_copy
"$X" status "$T/b" | grep -q "^epoch 2" && [ $rc -eq 0 ] && ok "roles: copy B now follows the new primary at epoch 2" || bad "roles follow" "rc $rc $("$X" status "$T/b")"
start_copy b $((P0 + 1)) --name copyB
prim p $((P0 + 1)) --name P1 --commits 1 --per 1 > "$T/p_old.log" 2>&1; rc=$?
stop_copy
grep -q "ALARM" "$T/p_old.log" && [ $rc -eq 3 ] && ok "roles: the old primary (epoch 1) is refused by a copy at epoch 2 (alarm)" || bad "roles old" "rc $rc"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
