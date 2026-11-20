#!/bin/bash
# test_objrepl_stall.sh - 11/19/2026 (7.4.1): a copy that stalls (a slow disk), and a primary that comes back on a
# new connection while the copy still holds the old one. Throwaway folders only. Needs a build with
# -DGXREPL_TEST_HOOKS=ON (the copy's stall).
#
#   utils/objrepl/test_objrepl_stall.sh BIN
#
#   stall      the copy stalls 20 s before it applies a frame, without reading its socket. The primary's
#              exchange ends after ack-timeout (3 s): it keeps committing, and its Pump() never waits for the
#              copy. After the stall the copy catches up: equal, and no connection is left in CLOSE-WAIT on the
#              copy's port (before 7.4.1 the primary's tries piled up there, their TLS hello unread)
#   reconnect  the primary is stopped while connected (its connection stays open and idle, as when the network
#              loses it without a close) and another primary process connects with the same certificate: the copy
#              takes the new connection at once, not after idle-timeout (60 s here); it refuses that primary (another
#              history), and the first primary, resumed, reconnects and finishes: equal
set -u
. "$(dirname "$0")/objrepl_proc.sh"
X=${1:?usage: test_objrepl_stall.sh BIN}
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_stall.XXXXXX")
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
CPID=""; PPID1=""
stop_copy() { [ -n "$CPID" ] || return 0; objrepl_stop $CPID
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
cleanup() { [ -n "$PPID1" ] && { kill -CONT $PPID1 2>/dev/null; kill -KILL $PPID1 2>/dev/null; }; stop_copy; rm -rf "$T"; }
trap cleanup EXIT
same() { [ "$("$X" digest "$T/$1")" == "$("$X" digest "$T/$2")" ]; }
st() { awk -v k="$2" '$1 == k { print $2 }' "$T/$1/objrepl.status" 2>/dev/null; }
# connections in CLOSE-WAIT on local port $1 (ss on Linux, netstat elsewhere)
closewait() {
  if command -v ss > /dev/null 2>&1; then ss -tan state close-wait "( sport = :$1 )" 2>/dev/null | tail -n +2 | wc -l
  else netstat -an 2>/dev/null | grep -i "close_wait" | grep -c "[:.]$1 "; fi
}
P0=$((20000 + RANDOM % 10000)); P1=$((P0 + 1))

# stall
mkdir -p "$T/p" "$T/a"
OBJREPL_TEST_STALL_AT=40 OBJREPL_TEST_STALL_SECS=20 "$X" copy "$T/a" --listen $P0 "${CTLS[@]}" >> "$T/copy_a.log" 2>&1 & CPID=$!
for i in $(seq 1 50); do grep -q "listening" "$T/copy_a.log" && break; sleep 0.2; done
"$X" primary "$T/p" --copy 127.0.0.1:$P0 "${PTLS[@]}" --ack-timeout 3 --commits 400 --per 3 --sleep 50 --seed 71 > "$T/p_stall.log" 2>&1 & PP=$!
for i in $(seq 1 300); do grep -q "test stall" "$T/copy_a.log" && break; sleep 0.1; done
sleep 1; s0=$(st p last_seq)
sleep 12; s1=$(st p last_seq)
wait $PP; rc=$?
sleep 2
cw=$(closewait $P0)
stop_copy
if [ $rc -eq 0 ] && same p a && grep -q "ack-timeout" "$T/p_stall.log" && [ $(( ${s1:-0} - ${s0:-0} )) -ge 3 ] && [ "$cw" -eq 0 ]; then
  ok "stall: the primary went on committing during the copy's 20 s stall (seq ${s0:-?} -> ${s1:-?} in 12 s), dropped the stalled exchange after 3 s, then the copy caught up: equal, $cw in CLOSE-WAIT"
else
  bad "stall" "rc $rc seq ${s0:-?} -> ${s1:-?} close-wait $cw ack-timeout said $(grep -c ack-timeout "$T/p_stall.log") $(tail -n 1 "$T/p_stall.log")"
fi

# reconnect
mkdir -p "$T/q" "$T/b" "$T/r"
"$X" copy "$T/b" --listen $P1 "${CTLS[@]}" --idle-timeout 60 >> "$T/copy_b.log" 2>&1 & CPID=$!
for i in $(seq 1 50); do grep -q "listening" "$T/copy_b.log" && break; sleep 0.2; done
"$X" primary "$T/q" --copy 127.0.0.1:$P1 "${PTLS[@]}" --commits 200 --per 3 --sleep 50 --seed 72 > "$T/q.log" 2>&1 & PPID1=$!
for i in $(seq 1 100); do [ "$(st b applied_seq)" -ge 20 ] 2>/dev/null && break; sleep 0.1; done
if [ $OBJREPL_WIN = 1 ]; then
  # Git Bash cannot stop a native process: the second primary's connection alone shows the copy takes it at once
  PPID1_STOPPED=0
else
  kill -STOP $PPID1; PPID1_STOPPED=1
fi
t0=$(date +%s)
"$X" primary "$T/r" --copy 127.0.0.1:$P1 "${PTLS[@]}" --commits 1 --per 1 --seed 73 > "$T/r.log" 2>&1; rrc=$?
dt=$(( $(date +%s) - t0 ))
[ $PPID1_STOPPED = 1 ] && kill -CONT $PPID1
wait $PPID1; qrc=$?; PPID1=""
stop_copy
if [ $rrc -eq 3 ] && [ $dt -lt 20 ] && [ $qrc -eq 0 ] && same q b && { [ $PPID1_STOPPED = 0 ] || grep -q "connected again" "$T/copy_b.log"; }; then
  ok "reconnect: the copy took the new connection in ${dt} s (idle-timeout 60), refused that primary (exit 3), and the first primary, resumed, finished: equal"
else
  bad "reconnect" "second primary rc $rrc in ${dt} s, first primary rc $qrc, said $(grep -c 'connected again' "$T/copy_b.log") $(tail -n 1 "$T/q.log")"
fi

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
