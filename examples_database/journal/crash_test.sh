#!/bin/bash
# crash_test.sh [ROUNDS] [DIR] - kill -9 the journal writer at random
# moments, then reopen (journal recovery) and verify that every commit
# the writer printed is in the files and that no batch is half written.
# Each round picks write-through or write-back, with or without a
# BtreeCache. Simulates a process crash (the OS page cache survives),
# not a power cut. Prints one line per round and a summary.
R=${1:-50}; D=${2:-./crash_dir}
BIN=${BIN:-$(dirname "$0")/testprog}
pass=0; fail=0
for r in $(seq 1 $R); do
  rm -rf "$D"; mkdir -p "$D"
  mode=$((RANDOM % 2)); buckets=$(( (RANDOM % 2) * 64 ))
  t=$(awk -v s=$RANDOM 'BEGIN{srand(s); printf "%.2f", 0.2 + rand()*2.3}')
  "$BIN" --crash-writer "$D" $mode $buckets > "$D.acked" 2>/dev/null &
  pid=$!
  sleep $t
  kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
  acked=$(tail -1 "$D.acked"); acked=${acked:-0}
  res=$("$BIN" --verify "$D" $acked $mode $buckets)
  case "$res" in PASS*) pass=$((pass+1));; *) fail=$((fail+1));; esac
  echo "round $r mode=$mode buckets=$buckets kill_after=${t}s $res"
done
rm -rf "$D" "$D.acked"
echo "crash_test: $pass passed, $fail failed"
[ $fail -eq 0 ]
