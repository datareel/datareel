#!/bin/bash
# test_objrepl_linkmap.sh - 11/16/2026: each node configures its own link map; a copy configured with one refuses a
# primary whose map differs (alarm), accepts one with the same entries in another order, and a copy with no map
# configured takes the primary's (as before). Throwaway folders only.
#
#   utils/objrepl/test_objrepl_linkmap.sh BIN
set -u
X=${1:?usage: test_objrepl_linkmap.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_linkmap.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || { echo "certificates failed"; exit 1; }
K="$T/pki"
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
L1="lk:0:db0,lk:8:lk"; L1R="lk:8:lk,lk:0:db0"; L2="lk:0:db0,lk:16:lk"
CPID=""; P=$((20000 + RANDOM % 10000))
start_copy() {  # start_copy DIR PORT [COPY OPTIONS...]
  local d=$1 port=$2; shift 2
  "$X" copy "$T/$d" --listen $port "${CTLS[@]}" "$@" >> "$T/copy_$d.log" 2>&1 & CPID=$!
  for i in $(seq 1 100); do grep -qE "^state (listening|streaming)" "$T/$d/objrepl.status" 2>/dev/null && \
    [ "$(awk '$1=="pid"{print $2}' "$T/$d/objrepl.status")" == "$CPID" ] && return 0; kill -0 $CPID 2>/dev/null || return 1; sleep 0.1; done; return 1
}
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID 2>/dev/null
	      for i in $(seq 1 50); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
	      kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; [ "${KEEP:-0}" == 1 ] && echo "kept $T" || rm -rf "$T"' EXIT
prim() { local d=$1 port=$2 l=$3; shift 3; timeout 60 "$X" primary "$T/$d" --copy 127.0.0.1:$port "${PTLS[@]}" --links "$l" --dbs 2 "$@"; }
dig() { "$X" digest "$T/$1" --links "$L1" 2>/dev/null; }

# 1 same map, another order: streams, equal
mkdir -p "$T/p1" "$T/c1"
start_copy c1 $P --links "$L1R"
prim p1 $P "$L1" --commits 30 --per 6 --seed 1 --drain > "$T/p1.log" 2>&1; r=$?
stop_copy
[ $r -eq 0 ] && [ "$(dig p1)" == "$(dig c1)" ] && [ ! -s "$T/c1/objrepl.alarm" ] \
  && ok "1 same map in another order: accepted, the copy equals the primary" || bad "1 same map" "rc $r $(tail -n 1 "$T/c1/objrepl.alarm" 2>/dev/null)"

# 2 another map: refused with an alarm naming both maps, nothing applied
mkdir -p "$T/p2" "$T/c2"
start_copy c2 $((P + 1)) --links "$L1"
prim p2 $((P + 1)) "$L2" --commits 10 --per 6 --seed 2 --drain > "$T/p2.log" 2>&1; r=$?
sleep 0.5; stop_copy
applied=$("$X" status "$T/c2" | awk '{print $8}')
[ $r -ne 0 ] && grep -q "link map \[lk:0:db0,lk:16:lk\] differs from this copy's \[lk:0:db0,lk:8:lk\] (hash" "$T/c2/objrepl.alarm" \
  && [ "${applied:-0}" == 0 ] \
  && ok "2 another map: refused (primary exit $r), alarm: $(grep -m1 -o 'link map.*' "$T/c2/objrepl.alarm" | cut -c1-90)..., applied $applied" \
  || bad "2 another map" "rc $r applied $applied $(tail -n 1 "$T/c2/objrepl.alarm" 2>/dev/null)"

# 3 refused three times: one alarm line
"$X" clear-alarm "$T/c2" > /dev/null
start_copy c2 $((P + 1)) --links "$L1"
for i in 1 2 3; do prim p2 $((P + 1)) "$L2" --commits 0 --drain > /dev/null 2>&1; done
stop_copy
n=$(grep -c "link map" "$T/c2/objrepl.alarm" 2>/dev/null)
[ "$n" == 1 ] && ok "3 three refusals: one alarm line" || bad "3 dedup" "$n lines"

# 4 the copy with no map configured takes the primary's (unchanged behaviour)
mkdir -p "$T/p4" "$T/c4"
start_copy c4 $((P + 2))
prim p4 $((P + 2)) "$L1" --commits 30 --per 6 --seed 4 --drain > "$T/p4.log" 2>&1; r=$?
stop_copy
[ $r -eq 0 ] && [ "$(dig p4)" == "$(dig c4)" ] && ok "4 no map on the copy: the primary's is used, equal" || bad "4 no map" "rc $r"

# 5 the right map after a refusal: the copy streams and equals the primary
"$X" clear-alarm "$T/c2" > /dev/null
start_copy c2 $((P + 1)) --links "$L2"
prim p2 $((P + 1)) "$L2" --commits 0 --drain > "$T/p5.log" 2>&1; r=$?
stop_copy
[ $r -eq 0 ] && [ "$("$X" digest "$T/p2" --links "$L2")" == "$("$X" digest "$T/c2" --links "$L2")" ] \
  && ok "5 the copy restarted with the same map: caught up, equal" || bad "5 healed" "rc $r $(tail -n 2 "$T/p5.log" | tr '\n' '|')"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
