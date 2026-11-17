#!/bin/bash
# run_db_tests.sh [WORKDIR] - 6.3.1: build and run every database example,
# the journal tests (memory-mapped reads off and on) and the kill -9
# crash test. Build the library first: source env/gnu_env.sh; cd build;
# make; make install (export LARGEFILES=1 for the 64-bit engine).
# Each program runs in its own directory under WORKDIR.
HERE=$(cd "$(dirname "$0")" && pwd)
W=${1:-$HERE/test_runs}
ROUNDS=${CRASH_ROUNDS:-50}
EX="btree btree_cached btree_node damaged database database_cache defrag infohog layout pod simple"
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
# 10/31/2026: db_debug ends on damaged block lengths (7.3 security sweep)
d="$W/db_debug"; mkdir -p "$d"
(cd "$HERE/../utils/db_debug" && make >/dev/null 2>&1)
(cd "$d" && "$HERE/damaged/testprog" --make-damaged dmg.gxd && \
 timeout 60 "$HERE/../utils/db_debug/db_debug" dmg.gxd f > db_debug.log 2>&1 && \
 grep -q "good database blocks" db_debug.log); result $? "db_debug ends on damaged block lengths"
# 10/31/2026: config writer, log file and RSA key file (7.3 security sweep)
d="$W/security_sweep"; mkdir -p "$d"
(cd "$HERE/../examples_general/security_sweep" && make >/dev/null 2>&1)
(cd "$d" && "$HERE/../examples_general/security_sweep/testprog" > sweep.log 2>&1); result $? "security sweep (config writer, log file, RSA key file)"
# 10/31/2026: support code and serial/telnet/ping (7.3.1 security sweep)
# 11/14/2026: 6.3.1: the networking sweep (sockets, TLS, gxStream
# mutual TLS, gxDatagram, HTTP/URL/SMTP/POP3/FTP) and the print drivers.
# The TLS checks make throwaway certificates with the openssl program and
# use 127.0.0.1.
for t in examples_general/support_security examples_socket/comm_security \
	 examples_general/print_security examples_socket/socket_security \
	 examples_socket/ssl_security examples_socket/stream_security \
	 examples_socket/proto_security; do
  n=$(basename $t); d="$W/$n"; mkdir -p "$d"
  (cd "$HERE/../$t" && make >/dev/null 2>&1)
  (cd "$d" && "$HERE/../$t/testprog" > $n.log 2>&1); result $? "$n"
done
# 11/16/2026: block reuse, best fit and first fit (ReclaimBestFit() reused only exact fits)
(cd "$HERE/reclaim" && make >/dev/null 2>&1); result $? "build reclaim"
d="$W/reclaim"; mkdir -p "$d"
(cd "$d" && "$HERE/reclaim/testprog" "$d" > reclaim.log 2>&1); result $? "reclaim (best fit and first fit reuse freed blocks)"
# 11/16/2026: journal salts distinct across processes forked in the same second
(cd "$HERE/journal_salt" && make >/dev/null 2>&1); result $? "build journal_salt"
d="$W/journal_salt"; mkdir -p "$d"
(cd "$d" && "$HERE/journal_salt/testprog" "$d" > salt.log 2>&1); result $? "journal salts distinct across forked processes"
for m in 0 1; do
  d="$W/journal_map$m"; mkdir -p "$d"
  (cd "$d" && "$HERE/journal/testprog" 20 7 $m > journal.log 2>&1); result $? "journal tests (memory-mapped reads $m)"
done
d="$W/crash"; mkdir -p "$d"
(cd "$d" && "$HERE/journal/crash_test.sh" $ROUNDS "$d/db" > crash.log 2>&1); result $? "crash test ($ROUNDS kill -9 rounds)"
echo "$pass passed, $fail failed (logs in $W)"
[ $fail -eq 0 ]
