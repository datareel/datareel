#!/bin/bash
# run_prop_objrepl.sh - 11/16/2026: runs prop_objrepl (the gxObjRepl randomized property test) over a range of seeds
# with WORKERS workers in parallel, at nice 10. Throwaway certificates and node folders in a mktemp -d folder under
# /tmp (removed at the end; failing seeds are saved to FAILURES first, with their reproducer).
#
#   utils/objrepl/run_prop_objrepl.sh BUILD_DIR [FIRST_SEED] [SEEDS] [WORKERS] [FAILURES]
#
# Defaults: seeds 1..10000, 2 workers, FAILURES /home4/open_source_projects/datareel/objrepl_prop_failures.
# BUILD_DIR: a build with -DGXREPL_TEST_HOOKS=ON (BUILD_DIR/utils/prop_objrepl). Each worker uses two ports from a
# random base in 20000-30000 on 127.0.0.1. Logs: FAILURES/run_<date>/worker_N.log (one line per seed) and summary.txt.
set -u
B=${1:?usage: run_prop_objrepl.sh BUILD_DIR [FIRST_SEED] [SEEDS] [WORKERS] [FAILURES]}
FIRST=${2:-1}; SEEDS=${3:-10000}; WORKERS=${4:-2}
FAILURES=${5:-/home4/open_source_projects/datareel/objrepl_prop_failures}
HERE=$(cd "$(dirname "$0")" && pwd)
X="$B/utils/prop_objrepl"
[ -x "$X" ] || { echo "no $X" >&2; exit 1; }
T=$(mktemp -d /tmp/objrepl_prop.XXXXXX) || exit 1
PIDS=()
cleanup() { for p in "${PIDS[@]}"; do kill -TERM "$p" 2>/dev/null; done; wait 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT
trap 'exit 130' INT TERM
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --apply > /dev/null || { echo "certificates failed" >&2; exit 1; }
RUN="$FAILURES/run_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RUN" || exit 1
BASE=$((20000 + (RANDOM % 4900) * 2))
PER=$(( (SEEDS + WORKERS - 1) / WORKERS ))
echo "seeds $FIRST..$((FIRST + SEEDS - 1)), $WORKERS workers, ports from $BASE, logs $RUN"
START=$(date +%s)
for w in $(seq 0 $((WORKERS - 1))); do
  S=$((FIRST + w * PER)); N=$PER
  [ $((S + N)) -gt $((FIRST + SEEDS)) ] && N=$((FIRST + SEEDS - S))
  [ $N -le 0 ] && continue
  nice -n 10 "$X" --pki "$T/pki" --work "$T/w$w" --port $((BASE + 4 * w)) --seed $S --count $N --failures "$FAILURES" > "$RUN/worker_$w.log" 2>&1 &
  PIDS+=($!)
done
for p in "${PIDS[@]}"; do wait "$p"; done
PIDS=()
END=$(date +%s)
cat "$RUN"/worker_*.log | grep '^seed ' > "$RUN/all_seeds.log"
RUNS=$(wc -l < "$RUN/all_seeds.log"); PASS=$(grep -c ' PASS ' "$RUN/all_seeds.log"); FAIL=$(grep -c ' FAIL ' "$RUN/all_seeds.log")
CHECKS=$(awk '{ for(i = 1; i < NF; i++) if($i == "checks") s += $(i + 1) } END { print s + 0 }' "$RUN/all_seeds.log")
AVG=$(awk '{ for(i = 1; i < NF; i++) if($i == "secs") { s += $(i + 1); n++ } } END { if(n) printf "%.2f", s / n }' "$RUN/all_seeds.log")
{
  echo "seeds run $RUNS passed $PASS failed $FAIL; drained compares $CHECKS; mean $AVG s/seed; wall $((END - START)) s"
  grep ' FAIL ' "$RUN/all_seeds.log" | sed "s|^seed \([0-9]*\) FAIL|seed \1 FAIL (reproducer $FAILURES/seed_\1/repro.sh)|"
} | tee "$RUN/summary.txt"
[ "$FAIL" -eq 0 ]
