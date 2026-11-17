#!/bin/bash
# test_objrepl_review.sh - 11/16/2026: the independent review's findings on objrepl-lib 5c67c3b, through the tool
# (the others are in test_gxobjrepl_api.cpp). Throwaway folders only.
#
#   utils/objrepl/test_objrepl_review.sh BIN
#
#   1  lock      while a copy serves, takeover and rejoin of its folder and a second copy on it are refused
#   2  takeover  refused for a primary and for a folder never used
#   3  idle      a primary stopped (SIGSTOP) on an open connection: the copy drops it after --idle-timeout and the
#                next primary is answered (here refused by name) at once, not after the TLS timeout
#   5  history   a primary with another history (its digest at the copy's seq differs) is refused; the copy unchanged
#   6  epoch     a primary at epoch 2 that refuses the copy does not raise the copy's epoch; the epoch-1 primary
#                still streams to it
#   8  index     the same index name with another offset is built again (no failed writes)
#   13 state     a state file that cannot be read: the node does not start (and is not reset)
#   rejoin       a copy made to wait for a full send gets one and equals the primary
#   alarm        the same refusal three times: one alarm line
set -u
X=${1:?usage: test_objrepl_review.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_review.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || { echo "certificates failed"; exit 1; }
K="$T/pki"
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
CPID=""; BG=""
start_copy() { local d=$1 port=$2; shift 2; "$X" copy "$T/$d" --listen $port "${CTLS[@]}" "$@" >> "$T/copy_$d.log" 2>&1 & CPID=$!
	       for i in $(seq 1 100); do grep -qE "^state (listening|streaming)" "$T/$d/objrepl.status" 2>/dev/null && \
		 [ "$(awk '$1=="pid"{print $2}' "$T/$d/objrepl.status")" == "$CPID" ] && return 0; sleep 0.1; done; return 1; }
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID 2>/dev/null
	      for i in $(seq 1 50); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
	      kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; [ -n "$BG" ] && { kill -CONT $BG; kill -KILL $BG; } 2>/dev/null; rm -rf "$T"' EXIT
prim() { local d=$1 port=$2; shift 2; "$X" primary "$T/$d" --copy 127.0.0.1:$port "${PTLS[@]}" "$@"; }
dig() { "$X" digest "$T/$1" 2>/dev/null; }
st() { "$X" status "$T/$1"; }
P=$((20000 + RANDOM % 10000))
mkdir -p "$T/p1" "$T/a" "$T/b" "$T/x" "$T/d"

# base: copy a and copy b follow p1 (6 commits each way)
start_copy a $P --name copyA
prim p1 $P --name P1 --commits 6 --per 4 --seed 1 > "$T/p1_a.log" 2>&1; r1=$?
stop_copy
start_copy b $((P + 1)) --name copyB
prim p1 $((P + 1)) --name P1 --commits 0 --drain > "$T/p1_b.log" 2>&1; r2=$?
stop_copy
[ $r1 -eq 0 ] && [ $r2 -eq 0 ] && [ "$(dig a)" == "$(dig p1)" ] && [ "$(dig b)" == "$(dig p1)" ] || bad "setup" "rc $r1/$r2"

# 1 lock
start_copy a $P --name copyA
t=$("$X" takeover "$T/a" 2>&1); tr=$?; j=$("$X" rejoin "$T/a" 2>&1); jr=$?
"$X" copy "$T/a" --listen $((P + 5)) "${CTLS[@]}" > "$T/second.log" 2>&1; sr=$?
stop_copy
[ $tr -ne 0 ] && [ $jr -ne 0 ] && [ $sr -eq 64 ] && echo "$t" | grep -q "in use" && st a | grep -q "role copy" && st a | grep -q "^epoch 1 " \
  && ok "1 lock: takeover, rejoin and a second copy refused while the copy serves [$t]" || bad "1 lock" "takeover $tr [$t] rejoin $jr second $sr"

# 2 takeover role
t=$("$X" takeover "$T/p1" 2>&1); tr=$?; mkdir -p "$T/empty"; t2=$("$X" takeover "$T/empty" 2>&1); tr2=$?
[ $tr -ne 0 ] && [ $tr2 -ne 0 ] && echo "$t" | grep -q "not a copy" && st p1 | grep -q "role primary" && st p1 | grep -q "next_seq 7 " \
  && ok "2 takeover: refused for a primary [$t] and for an unused folder" || bad "2 takeover" "primary $tr [$t] empty $tr2 [$t2]"

# 3 idle: P1 stopped on an open connection; the copy drops it after 3 s; P9 is answered (refused by name) at once
start_copy a $P --name copyA --peer-name P1 --idle-timeout 3
"$X" primary "$T/p1" --copy 127.0.0.1:$P "${PTLS[@]}" --name P1 --commits 100000 --per 1 --sleep 50 > "$T/p1_idle.log" 2>&1 & BG=$!   # the binary itself (its pid)
f0=$(awk '$1=="frames"{print $2}' "$T/a/objrepl.status"); for i in $(seq 1 100); do f=$(awk '$1=="frames"{print $2}' "$T/a/objrepl.status"); [ "${f:-0}" -gt $(( ${f0:-0} + 3 )) ] && break; sleep 0.1; done   # streaming (not inside its handshake)
kill -STOP $BG
s0=$(date +%s); timeout 60 "$X" primary "$T/x" --copy 127.0.0.1:$P "${PTLS[@]}" --name P9 --commits 1 --per 1 > "$T/p9.log" 2>&1; r=$?; s1=$(date +%s)
kill -CONT $BG; kill -KILL $BG; wait $BG 2>/dev/null; BG=""
stop_copy; "$X" clear-alarm "$T/a" > /dev/null; "$X" clear-alarm "$T/x" > /dev/null
[ $r -eq 3 ] && [ $((s1 - s0)) -le 15 ] && grep -q "connection dropped" "$T/copy_a.log" \
  && ok "3 idle: the stopped primary's connection dropped after 3 s; P9 answered in $((s1 - s0)) s (refused by name)" || bad "3 idle" "rc $r in $((s1 - s0)) s; copy log: $(tail -n 4 "$T/copy_a.log" | tr '\n' '|')"
# bring a and b back to p1's state (p1 committed while a streamed)
start_copy a $P --name copyA; prim p1 $P --name P1 --commits 0 --drain > /dev/null 2>&1; stop_copy
start_copy b $((P + 1)) --name copyB; prim p1 $((P + 1)) --name P1 --commits 0 --drain > /dev/null 2>&1; stop_copy

# 5 history: x, a primary of its own (more commits than a has), to its copy d; then x connects to a
rm -rf "$T/x"; mkdir -p "$T/x"
start_copy d $((P + 2)) --name copyD; prim x $((P + 2)) --name X --commits 400 --per 2 --seed 9 > /dev/null 2>&1; stop_copy
before=$(dig a)
start_copy a $P --name copyA
prim x $P --name X --commits 1 --per 1 > "$T/x_a.log" 2>&1; r=$?
stop_copy
[ $r -eq 3 ] && grep -q "another history" "$T/x/objrepl.alarm" && [ "$(dig a)" == "$before" ] \
  && ok "5 history: a primary with another history refused (alarm), the copy unchanged" || bad "5 history" "rc $r $(tail -n 1 "$T/x/objrepl.alarm" 2>/dev/null)"
"$X" clear-alarm "$T/x" > /dev/null

# 6 epoch: b takes over (epoch 2) and connects to a expecting a copy named copyZ: it refuses a; a stays at epoch 1
"$X" takeover "$T/b" > /dev/null
start_copy a $P --name copyA
prim b $P --name B --peer-name copyZ --commits 1 --per 1 > "$T/b_a.log" 2>&1; r=$?
e=$(st a | awk '{print $2}')
prim p1 $P --name P1 --commits 2 --per 2 > "$T/p1_after.log" 2>&1; r2=$?
stop_copy
[ $r -eq 3 ] && [ "$e" == "1" ] && [ $r2 -eq 0 ] && [ "$(dig a)" == "$(dig p1)" ] \
  && ok "6 epoch: the refusing epoch-2 primary left the copy at epoch 1; P1 still streams to it" || bad "6 epoch" "rc $r epoch $e then P1 rc $r2"

# 8 index: the same name, another offset
mkdir -p "$T/i"
"$X" primary "$T/i" --copy 127.0.0.1:1 "${PTLS[@]}" --commits 30 --per 4 --index k:db0:B:0:4 > "$T/i1.log" 2>&1 &
I1=$!; for k in $(seq 1 100); do [ "$(awk '$1=="last_seq"{print $2}' "$T/i/objrepl.status" 2>/dev/null)" == "30" ] && break; sleep 0.1; done; kill -TERM $I1; wait $I1 2>/dev/null
"$X" primary "$T/i" --copy 127.0.0.1:1 "${PTLS[@]}" --commits 30 --per 4 --index k:db0:B:8:4 > "$T/i2.log" 2>&1 &
I2=$!; for k in $(seq 1 100); do [ "$(awk '$1=="last_seq"{print $2}' "$T/i/objrepl.status" 2>/dev/null)" == "60" ] && break; sleep 0.1; done; kill -TERM $I2; wait $I2 2>/dev/null
out=$("$X" ixcheck "$T/i" --index k:db0:B:8:4); xr=$?
[ $xr -eq 0 ] && grep -q "index k on db0 built from" "$T/i2.log" && ! grep -q "cannot" "$T/i2.log" \
  && ok "8 index: another offset under the same name was built again; 60 commits, ixcheck [$out]" || bad "8 index" "rc $xr [$out] $(grep -m1 cannot "$T/i2.log")"

# 13 state: an unreadable state file
cp -r "$T/a" "$T/a2"; : > "$T/a2/state.gxd"
out=$("$X" copy "$T/a2" --listen $((P + 6)) "${CTLS[@]}" 2>&1); r=$?
[ $r -eq 64 ] && echo "$out" | grep -q "state.gxd" && "$X" status "$T/a2" | grep -q "no state" && ok "13 state: an unreadable state file stops the start, no new state written [$out]" || bad "13 state" "rc $r [$out] size $(stat -c %s "$T/a2/state.gxd")"

# rejoin of a copy: it waits for a full send and gets one
j=$("$X" rejoin "$T/a" 2>&1); jr=$?
start_copy a $P --name copyA
prim p1 $P --name P1 --commits 0 --drain > "$T/p1_rj.log" 2>&1; r=$?
stop_copy
[ $jr -eq 0 ] && [ $r -eq 0 ] && grep -q "1 snapshot" "$T/p1_rj.log" && [ "$(dig a)" == "$(dig p1)" ] \
  && ok "rejoin: the copy got a full send and equals the primary" || bad "rejoin" "rejoin $jr [$j] drain $r"

# alarm: the same refusal three times, one alarm line
start_copy a $P --name copyA --peer-name P1
for k in 1 2 3; do prim x $P --name X9 --commits 0 --drain > /dev/null 2>&1; done
stop_copy
n=$(grep -c "not the expected primary" "$T/a/objrepl.alarm")
[ "$n" == "1" ] && ok "alarm: three equal refusals, one alarm line" || bad "alarm" "$n lines"

# design 1/10: an old primary that committed while its copy was away: after the copy takes over, the old primary is
# refused (it cannot fork silently); it rejoins, gets the new primary's data by a full send, and its status counts
# the commits it dropped
mkdir -p "$T/o1" "$T/o2"
start_copy o2 $((P + 3)) --name O2; prim o1 $((P + 3)) --name O1 --commits 20 --per 3 --seed 5 > /dev/null 2>&1; stop_copy
prim o1 $((P + 3)) --name O1 --commits 7 --per 3 --seed 6 > "$T/o1_away.log" 2>&1     # the copy is away: 7 not shipped
"$X" takeover "$T/o2" > /dev/null
out=$(timeout 60 "$X" primary "$T/o1" --copy 127.0.0.1:$((P + 3)) "${PTLS[@]}" --name O1 --commits 1 --per 1 2>&1); r1=$?   # o2 is not listening: away
j=$("$X" rejoin "$T/o1"); jr=$?
start_copy o1 $((P + 3)) --name O1
prim o2 $((P + 3)) --name O2 --commits 3 --per 3 --seed 7 > "$T/o2_new.log" 2>&1; r2=$?
stop_copy
dropped=$(awk '$1 == "dropped_at_rejoin" { print $2 }' "$T/o1/objrepl.status")
[ $jr -eq 0 ] && [ $r2 -eq 0 ] && grep -q "1 snapshot" "$T/o2_new.log" && [ "$(dig o1)" == "$(dig o2)" ] && [ "${dropped:-0}" -ge 7 ] \
  && ok "design 1/10: the old primary rejoined by a full send (copy = new primary); dropped_at_rejoin $dropped [$j]" \
  || bad "design 1/10" "rejoin $jr [$j] new primary rc $r2 dropped ${dropped:-?}"

# design 5: a byte changed on the copy's disk during an apply is caught by the read-back: not applied, alarm,
# and the resend applies cleanly
mkdir -p "$T/v1" "$T/v2"
OBJREPL_TEST_CORRUPT_APPLY=3 "$X" copy "$T/v2" --listen $((P + 7)) "${CTLS[@]}" --name V2 >> "$T/copy_v2.log" 2>&1 & CPID=$!; sleep 0.5
prim v1 $((P + 7)) --name V1 --commits 10 --per 4 --seed 8 > "$T/v1.log" 2>&1; r=$?
stop_copy
[ $r -eq 0 ] && grep -q "read back differently" "$T/v2/objrepl.alarm" && [ "$(dig v1)" == "$(dig v2)" ] \
  && ok "design 5: a changed byte after apply was caught by the read-back (alarm, not applied); the resend applied; copy = primary" \
  || bad "design 5" "rc $r alarm [$(cat "$T/v2/objrepl.alarm" 2>/dev/null)] [$(dig v1)] [$(dig v2)]"

# stop file: a copy stops when ROOT/objrepl.stop appears (the Windows way to stop it gracefully), and removes it
mkdir -p "$T/sf"
"$X" copy "$T/sf" --listen $((P + 8)) "${CTLS[@]}" --name SF > "$T/copy_sf.log" 2>&1 & SPID=$!
for i in $(seq 1 50); do grep -q "^state listening" "$T/sf/objrepl.status" 2>/dev/null && break; sleep 0.1; done
touch "$T/sf/objrepl.stop"; s0=$(date +%s)
for i in $(seq 1 50); do kill -0 $SPID 2>/dev/null || break; sleep 0.1; done
alive=0; kill -0 $SPID 2>/dev/null && { alive=1; kill -KILL $SPID; }; wait $SPID 2>/dev/null; rc=$?
[ $alive -eq 0 ] && [ $rc -eq 0 ] && [ ! -e "$T/sf/objrepl.stop" ] && grep -q "^state stopped" "$T/sf/objrepl.status" \
  && ok "stop file: the copy stopped cleanly within $(( $(date +%s) - s0 )) s (exit 0, state stopped, the file removed)" || bad "stop file" "alive $alive rc $rc"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
