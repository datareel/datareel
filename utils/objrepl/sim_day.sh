#!/bin/bash
# sim_day.sh - 11/16/2026: a working day of gxObjRepl on a compressed clock (test data only; local; nice 10).
#
#   utils/objrepl/sim_day.sh BIN_DIR [OBJECTS] [DAY_SECS]     (defaults 1000000 objects, a 1200 s day)
#
# BIN_DIR holds objrepl and objrepl_sim (a -DGXREPL_TEST_HOOKS=ON build). Steps:
#   seed       OBJECTS objects on node n1 (ticks, docs up to multi-MB, links: chains, fan-out, cycles; 3 B-trees per
#              database and a vector index on docs)
#   first sync the copy n2 gets them by a full send (timed)
#   the day    n1 primary, n2 copy, both with reader threads (key and nearest-vector lookups, latency measured).
#              Simulated hours (an hour = DAY_SECS / 24 seconds):
#              02:00 copy SIGKILLed, back 10 s later   03:00 primary SIGKILLed, back at once (the clock carries on)
#              04:00-05:00 the copy drops the stream every 50 frames   06:00-07:00 the copy is down (outbox grows)
#              09:30-10:00 open burst (20x)   11:00 crash failover: n1 SIGKILLed while committing, n2 takes over,
#              n1 rejoins (a full send at full size, timed)   15:30-16:00 close burst (40x)   18:00 rollover to
#              ticks2   20:00 planned switchover back (drained first, 0 lost)   22:00 nightly purge   24:00 end
#   checkpoints after the first sync, the failover, the switchover and the end: both stopped, Digest equal and
#              IndexCheck consistent with equal index digests on both; disk use of each node
# One line per event; the summary at the end. Stops at the first failed checkpoint and keeps the folders.
set -u
B=${1:?usage: sim_day.sh BIN_DIR [OBJECTS] [DAY_SECS]}
N=${2:-1000000}; DAY=${3:-1200}
X="$B/objrepl"; S="$B/objrepl_sim"
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_simday.XXXXXX")
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || { echo "certificates failed"; exit 1; }
K="$T/pki"; PORT=$((20000 + RANDOM % 10000))
LINKS="links:0:links,links:8:docs,links:16:docs"
INDEX="sym:ticks:B:0:8,tm:ticks:B:8:8,px:ticks:B:16:8,sym:ticks2:B:0:8,tm:ticks2:B:8:8,px:ticks2:B:16:8,key:docs:B:0:8,cat:docs:B:8:8,auth:docs:B:16:8,vec:docs:V:24:16,grp:links:B:24:8,kind:links:B:32:8,lseq:links:B:40:8"
CT=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary --listen 127.0.0.1:$PORT)
PT=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key" --copy 127.0.0.1:$PORT)
P=n1; C=n2; PPID2=""; CPID=""; FAILED=0; LOST=0
H=$(echo "$DAY / 24" | bc -l)
log() { echo "$(date +%H:%M:%S) $*"; }
cleanup() { [ -n "$PPID2" ] && kill -KILL $PPID2 2>/dev/null; [ -n "$CPID" ] && kill -TERM $CPID 2>/dev/null; wait 2>/dev/null; }
trap cleanup EXIT
field() { awk -v k="$2" '$1 == k { print $2 }' "$T/$1/objrepl.status" 2>/dev/null; }
up() {  # up NODE PID: its status names PID and it is listening or streaming (5 min: index builds at full size)
  for i in $(seq 1 1500); do
    [ "$(field $1 pid)" == "$2" ] && grep -qE "^state (listening|streaming)" "$T/$1/objrepl.status" && return 0
    kill -0 $2 2>/dev/null || return 1; sleep 0.2
  done; return 1; }
start_copy() { [ -n "$CPID" ] && stop_copy; env ${1:-} nice -n 10 "$S" copy "$T/$C" "${CT[@]}" --name $C --readers 2 --stats "$T/stats_copy_$C.log" >> "$T/copy_$C.log" 2>&1 & CPID=$!
	       up $C $CPID || { log "FAIL the copy $C did not start"; return 1; }; }
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID; for i in $(seq 1 100); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
	      kill -0 $CPID 2>/dev/null && { log "WARN copy did not stop on TERM: KILL"; kill -KILL $CPID; }; wait $CPID 2>/dev/null; CPID=""; }
kill_copy() { kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
start_primary() { [ -n "$PPID2" ] && stop_primary; nice -n 10 "$S" primary "$T/$P" "${PT[@]}" --name $P --start $START --day-secs $DAY --readers 2 --stats "$T/stats_primary_$P.log" >> "$T/primary_$P.log" 2>&1 & PPID2=$!; }
stop_primary() { [ -n "$PPID2" ] || return 0; kill -TERM $PPID2; for i in $(seq 1 300); do kill -0 $PPID2 2>/dev/null || break; sleep 0.2; done
		 kill -0 $PPID2 2>/dev/null && { log "WARN primary did not stop on TERM: KILL"; kill -KILL $PPID2; }; wait $PPID2 2>/dev/null; PPID2=""; }
kill_primary() { kill -KILL $PPID2 2>/dev/null; wait $PPID2 2>/dev/null; PPID2=""; }
drain() {  # the copy up, the tool drains the primary; seconds
  local t0=$(date +%s.%N) rc
  start_copy || return 9
  nice -n 10 "$X" primary "$T/$P" "${PT[@]}" --name $P --links "$LINKS" --index "$INDEX" --commits 0 --drain >> "$T/drain.log" 2>&1; rc=$?
  stop_copy
  DRAIN_SECS=$(echo "$(date +%s.%N) - $t0" | bc); return $rc
}
checkpoint() {  # checkpoint WHAT: both stopped and drained
  local dp dc ip ic rp rc
  drain || { log "FAIL checkpoint $1: drain rc $?"; return 1; }
  dp=$("$X" digest "$T/$P" --links "$LINKS" 2>/dev/null); dc=$("$X" digest "$T/$C" --links "$LINKS" 2>/dev/null)
  ip=$("$X" ixcheck "$T/$P" --index "$INDEX"); rp=$?; ic=$("$X" ixcheck "$T/$C" --index "$INDEX"); rc=$?
  local du_p du_c; du_p=$(du -sm "$T/$P" | cut -f1); du_c=$(du -sm "$T/$C" | cut -f1)
  if [ "$dp" == "$dc" ] && [ $rp -eq 0 ] && [ $rc -eq 0 ] && [ "$(echo "$ip" | awk '{print $2,$3,$6,$8}')" == "$(echo "$ic" | awk '{print $2,$3,$6,$8}')" ]; then
    log "checkpoint $1: OK $P = $C [$dp], 13 indexes consistent and equal (vec $(echo "$ip" | awk '$2=="vec"{print $6}') vectors); disk $P ${du_p} MB, $C ${du_c} MB; drain ${DRAIN_SECS} s"
    return 0
  fi
  log "FAIL checkpoint $1: $P [$dp] $C [$dc] ixcheck $rp/$rc"; echo "$ip" > "$T/ix_$P.txt"; echo "$ic" > "$T/ix_$C.txt"
  return 1
}
at_hour() { local w; w=$(echo "$START + $1 * $H - $(date +%s.%N)" | bc); [ "$(echo "$w > 0" | bc)" == 1 ] && sleep $w; }
switch() {  # switch crash|planned: P stops (killed / drained), C takes over, P rejoins; then the roles swap
  local kind=$1 last applied lost t0 t1
  if [ $kind == crash ]; then kill_primary; stop_copy; else stop_primary; stop_copy; drain; fi
  last=$("$X" status "$T/$P" | awk '{print $4 - 1}'); applied=$("$X" status "$T/$C" | awk '{print $8}')
  lost=$((last - applied)); LOST=$((LOST + lost))
  "$X" takeover "$T/$C" > /dev/null || { log "FAIL takeover"; return 1; }
  "$X" rejoin "$T/$P" > /dev/null || { log "FAIL rejoin"; return 1; }
  log "switch $kind: $C takes over (applied $applied), $P (last $last) rejoins; lost $lost"
  local t=$P; P=$C; C=$t
  t0=$(date +%s.%N); drain || { log "FAIL the full send after the $kind switch"; return 1; }
  log "rejoin full send to $C: $DRAIN_SECS s at $("$X" digest "$T/$P" --links "$LINKS" 2>/dev/null)"
  REJOIN="$REJOIN ${DRAIN_SECS}s"
}
sample() {  # the primary's outbox and lag, once a second, into outbox.log
  while :; do
    [ -f "$T/$P/objrepl.status" ] && echo "$(date +%s) $P $(field $P lag_seq) $(field $P outbox_entries) $(field $P outbox_bytes) $(field $P oldest_unacked_secs)" >> "$T/outbox.log"
    sleep 1
  done; }
REJOIN=""
mkdir -p "$T/n1" "$T/n2"
log "sim day: $N objects, a $DAY s day (an hour = $(printf %.1f $H) s), nodes $T/n1 $T/n2, port $PORT"

# seed and first sync
t0=$(date +%s.%N)
nice -n 10 "$S" seed "$T/n1" "${PT[@]}" --name n1 --objects $N > "$T/seed.log" 2>&1 || { log "FAIL seed: $(tail -n 2 "$T/seed.log")"; exit 1; }
log "seed: $(tail -n 1 "$T/seed.log"); disk n1 $(du -sm "$T/n1" | cut -f1) MB"
checkpoint "first sync (full send of the seed)" || { FAILED=1; }
[ $FAILED -eq 0 ] && log "first sync full send: $DRAIN_SECS s"

# the day
if [ $FAILED -eq 0 ]; then
  START=$(date +%s)
  sample & SAMPLER=$!
  start_copy || FAILED=1
  start_primary
  at_hour 2;  log "02:00 copy SIGKILLed"; kill_copy; sleep 10; start_copy || FAILED=1
  at_hour 3;  log "03:00 primary SIGKILLed, restarted"; kill_primary; start_primary
  at_hour 4;  log "04:00 the copy drops the stream every 50 frames"; stop_copy; start_copy OBJREPL_TEST_DROP_EVERY=50 || FAILED=1
  at_hour 5;  log "05:00 drops end"; stop_copy; start_copy || FAILED=1
  at_hour 6;  log "06:00 the copy is down for an hour"; stop_copy
  at_hour 7;  log "07:00 the copy is back: lag $(field $P lag_seq) commits, outbox $(field $P outbox_entries) entries ($(field $P outbox_bytes) bytes), oldest $(field $P oldest_unacked_secs) s"
  t0=$(date +%s); start_copy || FAILED=1
  for i in $(seq 1 600); do [ "$(field $P lag_seq)" == "0" ] && break; sleep 1; done
  log "07:00 catch-up: lag 0 after $(( $(date +%s) - t0 )) s"
  at_hour 11; log "11:00 crash failover"; switch crash || FAILED=1
  [ $FAILED -eq 0 ] && { checkpoint "after the crash failover" || FAILED=1; }
  [ $FAILED -eq 0 ] && { start_copy && start_primary; }
  at_hour 15.4; log "15:24 (the close burst is next)"
  at_hour 20; [ $FAILED -eq 0 ] && { log "20:00 planned switchover"; switch planned || FAILED=1; }
  [ $FAILED -eq 0 ] && { checkpoint "after the planned switchover" || FAILED=1; }
  [ $FAILED -eq 0 ] && { start_copy && start_primary; }
  at_hour 24; log "24:00 end of the day"
  stop_primary; stop_copy
  [ $FAILED -eq 0 ] && { checkpoint "end of the day" || FAILED=1; }
  kill $SAMPLER 2>/dev/null; wait $SAMPLER 2>/dev/null
fi

# the numbers
st() { cat "$T"/stats_primary_*.log 2>/dev/null; }
# primary stats: 1 time, 3 hour, 5 commits, 7 objects, 9 last, 11 applied, 13 lag, 15 reads, 17 p50, 19 p99, 21 rate_x
rate=$(st | awk '{c+=$5; o+=$7; n++; if($5>mc) mc=$5; if($7>mo) mo=$7} END {if(n) printf "avg %.1f commits/s (peak %d), avg %.0f objects/s (peak %d) over %d s", c/n, mc, o/n, mo, n}')
lag=$(st | awk '{if($13>m) m=$13} END {print m+0}')
lagsec=$(awk '{if($6>m) m=$6} END {print m+0}' "$T/outbox.log" 2>/dev/null)
ob=$(awk '{if($4>me) me=$4; if($5>mb) mb=$5} END {printf "%d entries, %.1f MB", me, mb/1048576}' "$T/outbox.log" 2>/dev/null)
rp=$(st | awk '{if($17>0){a[n++]=$17; if($19>m) m=$19}} END {asort(a); printf "p50 %d us (median of the per-second p50s), worst per-second p99 %d us", a[int(n/2)], m}')
rc=$(cat "$T"/stats_copy_*.log 2>/dev/null | awk '{if($8>0){a[n++]=$8; if($10>m) m=$10}} END {asort(a); printf "p50 %d us, worst per-second p99 %d us", a[int(n/2)], m}')
log "summary: commit rate $rate; max lag $lag commits, oldest unacked ${lagsec} s; max outbox $ob; rejoin full sends:$REJOIN; lost in the crash failover $LOST; readers primary [$rp] copy [$rc]; final disk n1 $(du -sm "$T/n1" | cut -f1) MB, n2 $(du -sm "$T/n2" | cut -f1) MB"
if [ $FAILED -ne 0 ]; then log "FAILED: folders kept in $T"; trap - EXIT; cleanup; exit 1; fi
log "PASSED"
rm -rf "$T"
