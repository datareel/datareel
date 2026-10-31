#!/bin/bash
# run_db_tests.sh [WORKDIR] - 6.3: build and run every database example,
# the journal tests (memory-mapped reads off and on) and the kill -9
# crash test. Build the library first: source env/gnu_env.sh; cd build;
# make; make install (export LARGEFILES=1 for the 64-bit engine).
# Each program runs in its own directory under WORKDIR.
HERE=$(cd "$(dirname "$0")" && pwd)
W=${1:-$HERE/test_runs}
ROUNDS=${CRASH_ROUNDS:-50}
EX="btree btree_cached btree_node damaged database database_cache defrag infohog pod simple"
mkdir -p "$W"
pass=0; fail=0
result() { if [ $1 -eq 0 ]; then pass=$((pass+1)); echo "PASS $2"; else fail=$((fail+1)); echo "FAIL $2"; fi; }
for e in $EX journal; do
  (cd "$HERE/$e" && make >/dev/null 2>&1); result $? "build $e"
done
for e in $EX; do
  d="$W/$e"; mkdir -p "$d"
  (cd "$d" && yes '' | "$HERE/$e/testprog" > run1.log 2>&1 && \
   yes '' | "$HERE/$e/testprog" > run2.log 2>&1); result $? "$e (create, then reopen)"
done
for m in 0 1; do
  d="$W/journal_map$m"; mkdir -p "$d"
  (cd "$d" && "$HERE/journal/testprog" 20 7 $m > journal.log 2>&1); result $? "journal tests (memory-mapped reads $m)"
done
d="$W/crash"; mkdir -p "$d"
(cd "$d" && "$HERE/journal/crash_test.sh" $ROUNDS "$d/db" > crash.log 2>&1); result $? "crash test ($ROUNDS kill -9 rounds)"
echo "$pass passed, $fail failed (logs in $W)"
[ $fail -eq 0 ]
