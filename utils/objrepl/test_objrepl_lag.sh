#!/bin/bash
# test_objrepl_lag.sh - 11/16/2026: a long lag. The copy is away while the primary commits COMMITS commits (the
# outbox grows); then it comes back and catches up from the outbox (no full send). Measures the outbox size, the
# commit rate with no copy, and the catch-up time; ends with Digest equal and IndexCheck consistent. Throwaway folders.
#   utils/objrepl/test_objrepl_lag.sh BIN [COMMITS]      (default 20000 commits of 20 changes)
set -u
. "$(dirname "$0")/objrepl_proc.sh"   # 11/17/2026: objrepl_stop, objrepl_pid (Windows)
X=${1:?usage: test_objrepl_lag.sh BIN [COMMITS]}; N=${2:-20000}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_lag.XXXXXX")
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || exit 1
K="$T/pki"; P=$((20000 + RANDOM % 10000)); CPID=""
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
LINKS=(--links "lk:0:db0,lk:8:lk"); IX=(--index "k0:db0:B:0:8,v0:db0:V:8:8")
trap '[ -n "$CPID" ] && objrepl_stop $CPID; wait 2>/dev/null; rm -rf "$T"' EXIT
mkdir -p "$T/p" "$T/c"
"$X" copy "$T/c" --listen $P "${CTLS[@]}" "${IX[@]}" > "$T/c.log" 2>&1 & CPID=$!; sleep 0.5
"$X" primary "$T/p" --copy 127.0.0.1:$P "${PTLS[@]}" "${LINKS[@]}" "${IX[@]}" --dbs 3 --commits 50 --per 20 > /dev/null 2>&1
objrepl_stop $CPID; wait $CPID 2>/dev/null; CPID=""
# away: the primary commits N commits (the drain at the end finds no copy; it is cut short by --drain's own wait)
t0=$(date +%s.%N)
timeout 7200 "$X" primary "$T/p" --copy 127.0.0.1:$P "${PTLS[@]}" "${LINKS[@]}" "${IX[@]}" --dbs 3 --commits $N --per 20 --seed 9 > "$T/away.log" 2>&1 &
PP=$!
while :; do s=$(awk '$1=="last_seq"{print $2}' "$T/p/objrepl.status" 2>/dev/null); [ "${s:-0}" -ge $((N + 50)) ] && break; kill -0 $PP 2>/dev/null || break; sleep 1; done
t1=$(date +%s.%N)
kill -TERM $PP; wait $PP 2>/dev/null
ent=$(awk '$1=="outbox_entries"{print $2}' "$T/p/objrepl.status"); byt=$(awk '$1=="outbox_bytes"{print $2}' "$T/p/objrepl.status")
echo "away: $N commits of 20 changes in $(printf %.1f $(echo "$t1 - $t0" | bc)) s ($(echo "$N / ($t1 - $t0)" | bc) commits/s with no copy); outbox $ent entries, $(echo "$byt / 1048576" | bc) MB; disk primary $(du -sm "$T/p" | cut -f1) MB"
# back: catch up
"$X" copy "$T/c" --listen $P "${CTLS[@]}" "${IX[@]}" >> "$T/c.log" 2>&1 & CPID=$!; sleep 0.5
t0=$(date +%s.%N)
"$X" primary "$T/p" --copy 127.0.0.1:$P "${PTLS[@]}" "${LINKS[@]}" "${IX[@]}" --commits 0 --drain > "$T/back.log" 2>&1; r=$?
t1=$(date +%s.%N)
objrepl_stop $CPID; wait $CPID 2>/dev/null; CPID=""
snaps=$(grep -o "[0-9]* snapshot" "$T/back.log" | awk '{print $1}')
a=$("$X" digest "$T/p" "${LINKS[@]}" 2>/dev/null); b=$("$X" digest "$T/c" "${LINKS[@]}" 2>/dev/null)
"$X" ixcheck "$T/c" "${IX[@]}" > "$T/ix.txt"; xr=$?
echo "back: caught up in $(printf %.1f $(echo "$t1 - $t0" | bc)) s ($(echo "$N / ($t1 - $t0)" | bc) commits/s), $snaps full send(s), drain rc $r; primary [$a] copy [$b]; ixcheck rc $xr"
[ $r -eq 0 ] && [ "$a" == "$b" ] && [ $xr -eq 0 ] && [ "${snaps:-1}" == "0" ] && { echo "PASS lag: caught up from the outbox, equal and consistent"; exit 0; }
echo "FAIL lag"; exit 1
