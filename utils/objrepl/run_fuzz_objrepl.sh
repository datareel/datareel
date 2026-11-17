#!/bin/bash
# run_fuzz_objrepl.sh - 11/16/2026: runs fuzz_objrepl (the gxObjRepl copy wire fuzzer) in chunks at nice 10, on an
# ASan/UBSan build. A chunk that dies (a crash or a sanitizer report: UBSan is set to halt) is recorded with its last
# case and the run goes on from the next case. Throwaway certificates and folders in a mktemp -d folder under /tmp.
#
#   utils/objrepl/run_fuzz_objrepl.sh BUILD_DIR [CASES] [CHUNK] [OUT] [FIRST_CASE] [SEED]
#
# Defaults: 20000 cases from case 0, chunks of 1000, seed 1, OUT /home4/open_source_projects/datareel/
# objrepl_prop_failures/fuzz_run_<date> (chunk logs, crash logs, problem cases with the copy folder, summary.txt).
# One case again: BUILD_DIR/utils/fuzz_objrepl --pki PKI --work DIR --port P --start K --cases 1 --seed S --verbose
# (PKI from objrepl_certs.sh PKI --copy-host 127.0.0.1 --apply).
# The host has no libasan; the run on 11/16/2026 used the local RHEL 10.2 image with gcc 14.3 and libasan:
#   podman run --rm --network=none --security-opt label=disable -v SRC:SRC -v BUILD:BUILD -v OUT:OUT \
#     -v $(mktemp -d /tmp/objrepl_fuzz_host.XXXXXX):/tmp localhost/drsecscan63:latest SRC/utils/objrepl/run_fuzz_objrepl.sh BUILD ...
set -u
B=${1:?usage: run_fuzz_objrepl.sh BUILD_DIR [CASES] [CHUNK] [OUT] [FIRST_CASE] [SEED]}
CASES=${2:-20000}; CHUNK=${3:-1000}
OUT=${4:-/home4/open_source_projects/datareel/objrepl_prop_failures/fuzz_run_$(date +%Y%m%d_%H%M%S)}
FIRST=${5:-0}; SEED=${6:-1}
HERE=$(cd "$(dirname "$0")" && pwd)
X="$B/utils/fuzz_objrepl"
[ -x "$X" ] || { echo "no $X" >&2; exit 1; }
T=$(mktemp -d /tmp/objrepl_fuzz.XXXXXX) || exit 1
trap 'rm -rf "$T"' EXIT
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --apply > /dev/null || { echo "certificates failed" >&2; exit 1; }
mkdir -p "$OUT/problems" || exit 1
PORT=$((20000 + (RANDOM % 4900) * 2))
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:print_summary=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:print_summary=1
echo "cases $FIRST..$((FIRST + CASES - 1)) seed $SEED, chunks of $CHUNK, port $PORT, out $OUT" | tee "$OUT/summary.txt"
START=$(date +%s)
i=$FIRST; END=$((FIRST + CASES)); CRASHES=0
while [ $i -lt $END ]; do
  n=$CHUNK; [ $((i + n)) -gt $END ] && n=$((END - i))
  L="$OUT/chunk_$i.log"
  nice -n 19 "$X" --pki "$T/pki" --work "$T/work" --port $PORT --start $i --cases $n --seed $SEED --failures "$OUT/problems" > "$L" 2>&1
  rc=$?
  if grep -aq '^SUMMARY cases' "$L"; then
    i=$((i + n))
  else
    last=$(grep -ao '^case [0-9]* start' "$L" | tail -1 | awk '{ print $2 }')
    if [ -z "$last" ]; then                                # the setup stalled (an fsync on a busy disk): try again
      RETRY=$((${RETRY:-0} + 1)); cp "$L" "$OUT/setup_stall_${i}_$RETRY.log"
      echo "SETUP STALL chunk at $i (rc $rc, try $RETRY): $(grep -am1 'SETUP' "$L")" | tee -a "$OUT/summary.txt"
      [ $RETRY -ge 3 ] && { echo "chunk at $i failed its setup 3 times: stopped" | tee -a "$OUT/summary.txt"; exit 1; }
      continue
    fi
    RETRY=0
    CRASHES=$((CRASHES + 1))
    cp "$L" "$OUT/crash_case_$last.log"
    echo "CRASH case $last (rc $rc): $(grep -m1 -E 'ERROR: AddressSanitizer|runtime error|ERROR: LeakSanitizer|ABORT|SEGV' "$L")" | tee -a "$OUT/summary.txt"
    echo "  reproduce: $X --pki PKI --work DIR --port P --start $last --cases 1 --seed $SEED --verbose" | tee -a "$OUT/summary.txt"
    i=$((last + 1))
  fi
done
END_T=$(date +%s)
cat "$OUT"/chunk_*.log > "$OUT/all.log"
sum() { grep -a "^SUMMARY $1" "$OUT/all.log" | grep -aoE "(^| |\()$2 [0-9]+" | awk '{ s += $NF } END { print s + 0 }'; }
{
  echo "wall $((END_T - START)) s"
  echo "cases run $(grep -ac '^case [0-9]* start' "$OUT/all.log"), crashed chunks $CRASHES, problems $(sum cases problems)"
  echo "sanitizer reports: ASan $(grep -ac 'ERROR: AddressSanitizer' "$OUT/all.log"), UBSan $(grep -ac 'runtime error:' "$OUT/all.log"), LSan $(grep -ac 'ERROR: LeakSanitizer' "$OUT/all.log")"
  echo "stream: hello refused $(sum stream hello_refused), dropped by the copy $(sum stream dropped) (with an alarm $(sum stream alarm), silent $(sum stream silent)), every frame answered $(sum stream all_frames_answered)"
  echo "copy after the case: unchanged $(sum copy_after_case unchanged), valid prefix $(sum copy_after_case valid_prefix), changed data applied $(sum copy_after_case mutated_applied)"
  echo "clean primary: ok $(sum clean_primary ok), refused then ok after rejoin $(sum clean_primary refused_then_rejoined_ok), fenced by a bogus epoch $(sum clean_primary fenced_by_epoch), UNDETECTED $(sum clean_primary undetected), not recovered $(sum clean_primary not_recovered), stuck $(sum clean_primary stuck), index bad $(sum clean_primary index_bad), copy dead $(sum clean_primary copy_dead)"
  echo "alarm reasons on the copy:"
  grep -a '^SUMMARY alarm' "$OUT/all.log" | sed 's/^SUMMARY alarm \([0-9]*\) x /\1\t/' | awk -F'\t' '{ s[$2] += $1 } END { for(k in s) printf "  %7d  %s\n", s[k], k }' | sort -rn
  echo "problem cases:"; grep -a '^PROBLEM' "$OUT/all.log" | head -50
} | tee -a "$OUT/summary.txt"
