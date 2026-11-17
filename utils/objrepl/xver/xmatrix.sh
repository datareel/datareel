#!/bin/bash
# the 6.4 <-> 7.4 gxStream cross matrix on Linux; each row: server, client, expected server rc, expected client rc
S=$(cd "$(dirname "$0")" && pwd); P=$((30000 + RANDOM % 5000)); pass=0; fail=0
row() {
  local srv=$1 cli=$2 how=$3 esrv=$4 ecli=$5; P=$((P + 1))
  timeout 30 $S/$srv server $P $S/pki > $S/srv.out 2>&1 & local sp=$!
  for i in $(seq 1 100); do ss -ltn 2>/dev/null | grep -q "127.0.0.1:$P \|:$P " && break; sleep 0.1; done
  timeout 20 $S/$cli client $P $S/pki $how > $S/cli.out 2>&1; local cr=$?
  wait $sp; local sr=$?
  if [ $sr -eq $esrv ] && { [ "$ecli" == any ] || [ $cr -eq $ecli ]; }; then echo "PASS $srv <- $cli $how: $(cat $S/srv.out) | $(cat $S/cli.out)"; pass=$((pass+1))
  else echo "FAIL $srv <- $cli $how: server rc $sr (want $esrv) client rc $cr (want $ecli): $(cat $S/srv.out) | $(cat $S/cli.out)"; fail=$((fail+1)); fi
}
row xv7 xv7 "" 0 0
row xv6 xv6 "" 0 0
row xv7 xv6 "" 0 0
row xv6 xv7 "" 0 0
row xv7 xv7 raw32 0 0
row xv6 xv7 raw32 0 0
row xv7 xv7 old24 3 any
row xv6 xv7 old24 3 any
row xv7 xv6 old24 3 any
row xv6 xv6 old24 3 any
echo "$pass passed, $fail failed"
