#!/bin/bash
# drill_objrepl.sh - 11/16/2026: stress and failover drill for gxObjRepl (through the objrepl tool). Throwaway
# folders only; local; every process stopped by its pid (TERM, then KILL for the drill's own crash cases).
#
#   utils/objrepl/drill_objrepl.sh BIN [MINUTES]       (default 30; BIN built with -DGXREPL_TEST_HOOKS=ON)
#
# Two nodes, n1 and n2, swap roles. Each cycle, one of:
#   kill       a high-rate primary (20 changes per commit, no sleep) with SIGKILLs at random times of the primary,
#              the copy or both, restarts in between
#   drop       the copy drops the gxStream every 5-30 frames while the primary commits
# and after them:
#   compare    (every 3rd cycle) drain, then Digest and IndexCheck on both nodes: equal and consistent
#   failover   (every 5th cycle, alternating) crash: the primary is SIGKILLed while committing, the copy takes over,
#              the old primary rejoins (full send); the commits lost = the old primary's last seq - the copy's
#              applied seq. planned: drained first, then the same switch; 0 lost. Then a compare
# Stops at the first failed compare and keeps the folders. Prints one line per cycle and a summary.
set -u
X=${1:?usage: drill_objrepl.sh BIN [MINUTES]}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's)
"$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
MIN=${2:-30}
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_drill.XXXXXX")
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || { echo "certificates failed"; exit 1; }
K="$T/pki"
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
LINKS="lk:0:db0,lk:8:lk"
IX="k0:db0:B:0:8,v0:db0:V:8:8,v1:db1:V:0:16"
PORT=$((20000 + RANDOM % 10000))
P=n1; C=n2                      # the roles now
CPID=""; PPID2=""
mkdir -p "$T/n1" "$T/n2"
log() { echo "$(date +%H:%M:%S) $*"; }
cleanup() { [ -n "$PPID2" ] && kill -KILL $PPID2 2>/dev/null; [ -n "$CPID" ] && kill -TERM $CPID 2>/dev/null; wait 2>/dev/null; }
trap cleanup EXIT
field() { awk -v k="$2" '$1 == k { print $2 }' "$T/$1/objrepl.status" 2>/dev/null; }
start_copy() {  # start_copy [ENV=VAL]: the copy C, waits until it says listening (60 s)
  env ${1:-} "$X" copy "$T/$C" --listen $PORT "${CTLS[@]}" --name $C --links "$LINKS" --index "$IX" >> "$T/copy_$C.log" 2>&1 & CPID=$!
  for i in $(seq 1 300); do
    [ "$(field $C pid)" == "$CPID" ] && grep -qE "^state (listening|streaming)" "$T/$C/objrepl.status" 2>/dev/null && return 0   # up (a primary may be on it already)
    kill -0 $CPID 2>/dev/null || { CPID=""; return 1; }
    sleep 0.2
  done
  return 1
}
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID 2>/dev/null
              for i in $(seq 1 50); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              if kill -0 $CPID 2>/dev/null; then
                log "WARN copy pid $CPID did not stop on TERM in 10 s: its stacks in $T/stuck_$CPID.txt, then KILL"
                timeout 60 gdb -p $CPID -batch -ex "thread apply all bt" > "$T/stuck_$CPID.txt" 2>&1; cp "$T/stuck_$CPID.txt" "${STUCK_DIR:-/tmp}/" 2>/dev/null
                kill -KILL $CPID; TERMFAIL=$((TERMFAIL + 1)); fi
              wait $CPID 2>/dev/null; CPID=""; }
kill_copy() { [ -n "$CPID" ] && kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
start_primary() {  # start_primary OPTIONS...: in the background
  "$X" primary "$T/$P" --copy 127.0.0.1:$PORT "${PTLS[@]}" --name $P --links "$LINKS" --index "$IX" --dbs 3 --seed $RANDOM "$@" >> "$T/primary_$P.log" 2>&1 & PPID2=$!
}
kill_primary() { [ -n "$PPID2" ] && kill -KILL $PPID2 2>/dev/null; wait $PPID2 2>/dev/null; PPID2=""; }
drain() {  # the copy up, the primary drains; returns the primary's exit code
  start_copy || { log "FAIL the copy $C did not start"; return 9; }
  "$X" primary "$T/$P" --copy 127.0.0.1:$PORT "${PTLS[@]}" --name $P --links "$LINKS" --index "$IX" --dbs 3 --drain >> "$T/primary_$P.log" 2>&1
  local rc=$?
  stop_copy
  return $rc
}
compare() {  # compare WHAT: drained nodes equal and every index consistent
  local what=$1 dp dc ip ic rp rc
  drain; local drc=$?
  dp=$("$X" digest "$T/$P" --links "$LINKS" 2>/dev/null); dc=$("$X" digest "$T/$C" --links "$LINKS" 2>/dev/null)
  ip=$("$X" ixcheck "$T/$P" --index "$IX"); rp=$?; ic=$("$X" ixcheck "$T/$C" --index "$IX"); rc=$?
  local shared_p shared_c
  shared_p=$(echo "$ip" | awk '{print $2, $6, $8}'); shared_c=$(echo "$ic" | awk '{print $2, $6, $8}')   # name, entries, digest
  COMPARES=$((COMPARES + 1))
  if [ $drc -eq 0 ] && [ "$dp" == "$dc" ] && [ $rp -eq 0 ] && [ $rc -eq 0 ] && [ "$shared_p" == "$shared_c" ]; then
    log "compare OK ($what): $P = $C [$dp], indexes $(echo "$ip" | awk '{printf "%s %s, ", $2, $6}')consistent and equal"
    return 0
  fi
  log "FAIL compare ($what): drain rc $drc; $P [$dp] $C [$dc]; ixcheck $P rc $rp [$(echo "$ip" | tr '\n' ';')] $C rc $rc [$(echo "$ic" | tr '\n' ';')]"
  return 1
}
switch() {  # switch crash|planned
  local kind=$1 last applied lost msg
  if [ "$kind" == planned ]; then
    drain || { log "FAIL planned switch: drain failed"; return 1; }
  else
    start_copy || return 1
    start_primary --commits 100000 --per 20
    sleep 0.$((3 + RANDOM % 7)); sleep $((1 + RANDOM % 3))
    kill_primary                                   # the primary site is lost
    stop_copy
  fi
  last=$("$X" status "$T/$P" | awk '{print $4 - 1}')
  applied=$("$X" status "$T/$C" | awk '{print $8}')
  lost=$((last - applied))
  msg=$("$X" takeover "$T/$C") || { log "FAIL takeover: $msg"; return 1; }
  msg=$("$X" rejoin "$T/$P") || { log "FAIL rejoin: $msg"; return 1; }
  if [ "$kind" == planned ] && [ $lost -ne 0 ]; then log "FAIL planned switch lost $lost commit(s)"; return 1; fi
  log "switch $kind: $C takes over at $("$X" status "$T/$C" | awk '{print $2}') (applied $applied); $P (last $last) rejoins; lost $lost"
  LOST=$((LOST + lost)); SWITCHES=$((SWITCHES + 1))
  local t=$P; P=$C; C=$t
  compare "after the $kind switch, full send to $C"
}

END=$(( $(date +%s) + MIN * 60 ))
WHO=""; CYCLES=0; KILLS=0; DROPS=0; COMPARES=0; SWITCHES=0; LOST=0; TERMFAIL=0; FAILED=0; CRASH=1
log "drill: $MIN min, nodes $T/n1 $T/n2, port $PORT, links $LINKS, indexes $IX"
while [ $(date +%s) -lt $END ]; do
  CYCLES=$((CYCLES + 1))
  if [ $((RANDOM % 3)) -ne 0 ]; then
    # kill: a high-rate primary and random SIGKILLs
    start_copy || { log "FAIL the copy did not start"; FAILED=1; break; }
    start_primary --commits 100000 --per 20
    for k in 1 2 3; do
      sleep 0.$((RANDOM % 10)); sleep $((RANDOM % 3))
      case $((RANDOM % 3)) in
	0) kill_primary; KILLS=$((KILLS + 1)); start_primary --commits 100000 --per 20; w=primary ;;
	1) kill_copy; KILLS=$((KILLS + 1)); start_copy || { FAILED=1; break; }; w=copy ;;
	2) kill_primary; kill_copy; KILLS=$((KILLS + 2)); start_copy || { FAILED=1; break; }; start_primary --commits 100000 --per 20; w=both ;;
      esac
      WHO="$WHO $w"
    done
    kill_primary; KILLS=$((KILLS + 1)); stop_copy
    log "cycle $CYCLES kill: primary $P at seq $("$X" status "$T/$P" | awk '{print $4 - 1}'), copy $C applied $("$X" status "$T/$C" | awk '{print $8}'); killed:${WHO:-}"
    WHO=""
  else
    # drop: the copy drops the stream every N frames
    n=$((5 + RANDOM % 26))
    start_copy "OBJREPL_TEST_DROP_EVERY=$n" || { FAILED=1; break; }
    "$X" primary "$T/$P" --copy 127.0.0.1:$PORT "${PTLS[@]}" --name $P --links "$LINKS" --index "$IX" --dbs 3 --seed $RANDOM --commits 300 --per 10 >> "$T/primary_$P.log" 2>&1
    rc=$?
    stop_copy; DROPS=$((DROPS + 1))
    log "cycle $CYCLES drop every $n frames: primary exit $rc, seq $("$X" status "$T/$P" | awk '{print $4 - 1}'), copy applied $("$X" status "$T/$C" | awk '{print $8}')"
  fi
  [ $FAILED -eq 0 ] || break
  if [ $((CYCLES % 5)) -eq 0 ]; then
    if [ $CRASH -eq 1 ]; then switch crash || { FAILED=1; break; }; CRASH=0; else switch planned || { FAILED=1; break; }; CRASH=1; fi
  elif [ $((CYCLES % 3)) -eq 0 ]; then
    compare "cycle $CYCLES" || { FAILED=1; break; }
  fi
done
[ $FAILED -eq 0 ] && { compare "final" || FAILED=1; }
alarms=$(cat "$T"/n1/objrepl.alarm "$T"/n2/objrepl.alarm 2>/dev/null | wc -l)
log "summary: $CYCLES cycles, $KILLS SIGKILLs, $DROPS drop runs, $SWITCHES switches ($LOST commits lost in crash switches), $COMPARES compares, $TERMFAIL TERM stops that needed KILL, $alarms alarm line(s); objects $("$X" digest "$T/$P" --links "$LINKS" 2>/dev/null)"
if [ $FAILED -ne 0 ]; then log "FAILED: folders kept in $T"; trap - EXIT; cleanup; exit 1; fi
log "PASSED"
rm -rf "$T"
