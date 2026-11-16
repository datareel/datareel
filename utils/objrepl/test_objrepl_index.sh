#!/bin/bash
# test_objrepl_index.sh - 11/16/2026: each node builds its own B-tree and vector indexes from the objects it holds,
# in the same transaction as each object. Indexes are local (--index on each node), never sent. Throwaway folders.
#
#   utils/objrepl/test_objrepl_index.sh BIN
#
#   live       primary and copy A with the same indexes (B-tree key + vector on db0, vector on db1), copy A with one
#              more of its own (k1 on db1): every index consistent with the objects; shared ones equal primary's
#   kills      copy A SIGKILLed 3 times mid-stream: after the drain every index still consistent and equal
#   new index  copy A restarted with an index it never had (v2 on db2): built from its objects, consistent
#   build kill copy A killed inside an index build (v3): the next start builds it again from the start, consistent
#   full send  new copy B: the full send fills its indexes; consistent, equal primary's
#   fixup kill copy C killed in the full send's link fix-up: after the restart the indexes are consistent
#   tombs      removes of linked objects, unknown removes, purge: index entries leave with the object; consistent
#   dropped    an index left out of a node's config once is dropped (it would go stale), and rebuilt when configured
#   checker    a stale index file put back is reported by ixcheck (MISMATCH, rc 5)
set -u
X=${1:?usage: test_objrepl_index.sh BIN}
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_index.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
K="$T/pki"; mkdir -p "$K"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=objrepl-test-ca" -keyout "$K/ca.key" -out "$K/ca.pem" 2>/dev/null
mk() { openssl req -newkey rsa:2048 -nodes -subj "/CN=$2" -keyout "$K/$1.key" -out "$K/$1.csr" 2>/dev/null
       if [ -n "${3:-}" ]; then printf 'subjectAltName=%s\n' "$3" > "$K/$1.ext"; else : > "$K/$1.ext"; fi
       openssl x509 -req -in "$K/$1.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -days 2 -extfile "$K/$1.ext" -out "$K/$1.pem" 2>/dev/null; }
mk copy objrepl-copy "IP:127.0.0.1,DNS:localhost"; mk primary objrepl-primary
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
LINKS=(--links "lk:0:db0,lk:8:lk")
IX="k0:db0:B:0:8,v0:db0:V:8:8,v1:db1:V:0:16"
CIX="$IX,k1:db1:B:4:4"
CPID=""
# started = listening (index builds done): its status file names its pid with state listening (60 s at most)
start_copy() { local d=$1 port=$2 ix=$3; shift 3; "$X" copy "$T/$d" --listen $port "${CTLS[@]}" --index "$ix" "$@" >> "$T/copy_$d.log" 2>&1 & CPID=$!
               for i in $(seq 1 300); do
                 [ "$(awk '$1 == "pid" { print $2 }' "$T/$d/objrepl.status" 2>/dev/null)" == "$CPID" ] && grep -q "^state listening" "$T/$d/objrepl.status" && break
                 kill -0 $CPID 2>/dev/null || break; sleep 0.2; done; }
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID 2>/dev/null
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
kill_copy() { kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; rm -rf "$T"' EXIT
prim() { local port=$1; shift; "$X" primary "$T/p" --copy 127.0.0.1:$port "${PTLS[@]}" "${LINKS[@]}" --index "$IX" --dbs 3 "$@"; }
# ixcheck D IX: rc and the lines; shared(D) = the lines of the indexes the primary has too
ixc() { "$X" ixcheck "$T/$1" "${LINKS[@]}" --index "$2"; }
shared() { grep -E "^index (k0|v0|v1) " ; }
dig() { "$X" digest "$T/$1" "${LINKS[@]}" 2>/dev/null; }
check() {  # check NAME COPYDIR COPYIX: copy = primary (objects), every index consistent, shared indexes equal
  local name=$1 c=$2 cix=$3 po co prc crc
  po=$(ixc p "$IX"); prc=$?; co=$(ixc $c "$cix"); crc=$?
  if [ $prc -eq 0 ] && [ $crc -eq 0 ] && [ "$(echo "$po" | shared)" == "$(echo "$co" | shared)" ] && [ "$(dig p)" == "$(dig $c)" ]; then
    ok "$name: $(echo "$co" | awk '{printf "%s %s, ", $2, $6}')all consistent; shared = primary"
  else
    bad "$name" "primary rc $prc [$(echo "$po" | tr '\n' ';')] copy rc $crc [$(echo "$co" | tr '\n' ';')] objects [$(dig p)] [$(dig $c)]"
  fi
}
mkdir -p "$T/p" "$T/a" "$T/b" "$T/c"
PA=$((20000 + RANDOM % 10000)); PB=$((PA + 1)); PC=$((PA + 2))

# live
start_copy a $PA "$CIX"
prim $PA --commits 200 --per 8 --seed 61 > "$T/p1.log" 2>&1; rc=$?
stop_copy
[ $rc -eq 0 ] && check "live" a "$CIX" || bad "live" "primary rc $rc $(tail -n 1 "$T/p1.log")"

# kills: copy A killed 3 times while the primary commits (the primary keeps going; the copy catches up)
"$X" primary "$T/p" --copy 127.0.0.1:$PA "${PTLS[@]}" "${LINKS[@]}" --index "$IX" --dbs 3 --commits 300 --per 8 --seed 62 --sleep 5 > "$T/p2.log" 2>&1 &
PPID2=$!
for i in 1 2 3; do start_copy a $PA "$CIX"; sleep 0.$((3 + RANDOM % 5)); kill_copy; done
wait $PPID2; start_copy a $PA "$CIX"; prim $PA --drain > "$T/p2d.log" 2>&1; rc=$?; stop_copy
[ $rc -eq 0 ] && check "kills (3 copy SIGKILLs mid-stream)" a "$CIX" || bad "kills" "drain rc $rc"

# new index on the data already there
start_copy a $PA "$CIX,v2:db2:V:0:4"; stop_copy
grep -q "index v2 on db2 built from" "$T/copy_a.log" && check "new index (v2 $(grep -o 'built from [0-9]*' "$T/copy_a.log" | tail -n 1))" a "$CIX,v2:db2:V:0:4" || bad "new index" "no build line"

# a kill inside an index build: the next start builds it from the start
OBJREPL_TEST_KILL_IN_IXBUILD=1 "$X" copy "$T/a" --listen $PA "${CTLS[@]}" --index "$CIX,v2:db2:V:0:4,v3:db0:V:40:2" > "$T/copy_a_kb.log" 2>&1; krc=$?
ls "$T/a" | grep -q "v3.vix.building" && marked=1 || marked=0
start_copy a $PA "$CIX,v2:db2:V:0:4,v3:db0:V:40:2"; stop_copy
[ $krc -eq 9 ] && [ $marked -eq 1 ] && [ ! -e "$T/a/db0.v3.vix.building" ] && grep -q "index v3 on db0 built from" "$T/copy_a.log" \
  && check "build kill (exit $krc, rebuilt)" a "$CIX,v2:db2:V:0:4,v3:db0:V:40:2" || bad "build kill" "exit $krc marked $marked"

# full send to a new copy B
start_copy b $PB "$IX"
prim $PB --commits 20 --per 8 --seed 63 > "$T/p3.log" 2>&1; rc=$?
stop_copy
snaps=$(grep -o "[0-9]* snapshot" "$T/p3.log" | awk '{print $1}')
[ $rc -eq 0 ] && [ "${snaps:-0}" -ge 1 ] && check "full send ($snaps)" b "$IX" || bad "full send" "rc $rc snaps ${snaps:-0}"

# killed in the full send's link fix-up, then restarted
OBJREPL_TEST_KILL_IN_FIXUP=1 "$X" copy "$T/c" --listen $PC "${CTLS[@]}" --index "$IX" >> "$T/copy_c.log" 2>&1 & CPID=$!; sleep 0.5
prim $PC --drain > "$T/p4.log" 2>&1
for i in $(seq 1 50); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; crc=$?; CPID=""
start_copy c $PC "$IX"; prim $PC --drain > "$T/p4b.log" 2>&1; rc=$?; stop_copy
[ $crc -eq 9 ] && [ $rc -eq 0 ] && check "fixup kill (exit $crc, then a new full send)" c "$IX" || bad "fixup kill" "copy exit $crc primary rc $rc"

# tombstones and purge
start_copy a $PA "$CIX,v2:db2:V:0:4,v3:db0:V:40:2"
prim $PA --drop-linked 6 --unknown-removes 3 --purge --drain > "$T/p5.log" 2>&1; rc=$?
stop_copy
[ $rc -eq 0 ] && grep -q "removed 6 linked" "$T/p5.log" && check "tombs (6 linked removed, 3 unknown, $(grep -o 'purged [0-9]*' "$T/p5.log"))" a "$CIX,v2:db2:V:0:4,v3:db0:V:40:2" || bad "tombs" "rc $rc"

# dropped: copy A run once without k1 (k1 would go stale): k1 is dropped, and configured again it is rebuilt
start_copy a $PA "$IX"
prim $PA --commits 30 --per 8 --seed 64 > "$T/p6.log" 2>&1; rc=$?
stop_copy
gone=$([ -e "$T/a/db1.k1.ix.btx" ] && echo 0 || echo 1)
start_copy a $PA "$CIX,v2:db2:V:0:4,v3:db0:V:40:2"; stop_copy
[ $rc -eq 0 ] && [ $gone -eq 1 ] && grep -q "index k1 on db1 is not configured here: dropped" "$T/copy_a.log" && grep -q "index k1 on db1 built from" "$T/copy_a.log" \
  && check "dropped (k1 left out once: dropped, then rebuilt)" a "$CIX,v2:db2:V:0:4,v3:db0:V:40:2" || bad "dropped" "rc $rc gone $gone"

# the checker itself: an old k0 file put back after more commits must be reported (MISMATCH, rc 5)
cp "$T/a/db0.k0.ix.btx" "$T/k0.old"
start_copy a $PA "$CIX,v2:db2:V:0:4,v3:db0:V:40:2"
prim $PA --commits 20 --per 8 --seed 65 > "$T/p7.log" 2>&1
stop_copy
cp "$T/k0.old" "$T/a/db0.k0.ix.btx"
out=$(ixc a "$CIX,v2:db2:V:0:4,v3:db0:V:40:2"); xrc=$?
[ $xrc -eq 5 ] && echo "$out" | grep -q "^index k0 db0 B .* MISMATCH" && ok "checker: a stale k0 is reported ($(echo "$out" | grep '^index k0' | grep -o '(.*)'))" || bad "checker" "rc $xrc [$out]"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
