#!/bin/bash
# test_objrepl_rejoin.sh - 11/20/2026 (7.5 beta 3): the incremental rejoin, the schema check, and the rebuild and
# swap. Throwaway folders and scratch ports only.
#
#   utils/objrepl/test_objrepl_rejoin.sh BIN
#
#   emergency  P is primary with copy C. C takes over (epoch 2) while P keeps writing 15 commits that C never gets,
#              and C writes 10 of its own. P rejoins under C: an incremental rejoin. Only the objects either side
#              changed since the split cross, never the whole database. Then the digests are equal, P's indexes
#              pass ixcheck, and the roles go back (P takes over, C rejoins with no full send): equal again
#   schema     the same split, but the new primary has another schema setting: the rejoin is refused with what
#              differs and the old node is untouched; a rebuild into an empty folder takes a full send, and swap
#              puts it in place and keeps the old folder
#   history    a node with another history rejoins: a full send instead (it says why), then equal
#   share      rejoin-max-share 1: a full send instead of the incremental rejoin, then equal
#   full       rejoin --full: a full send as before, then equal
#   mixed      with OBJREPL_OLD_BIN=an objrepl of 7.4.1 or 7.5 beta 2: an old copy under a new primary and a new
#              copy under an old primary work as before, and a new node waiting for an incremental rejoin takes a
#              full send from an old primary
#   crash      the rejoining node is killed inside a batch's transaction, then inside the end's (a build with
#              -DGXREPL_TEST_HOOKS=ON): each time it still waits, and the next connect does the rejoin: equal
set -u
. "$(dirname "$0")/objrepl_proc.sh"
X=${1:?usage: test_objrepl_rejoin.sh BIN}
"$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build"; exit 64; }
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_rejoin.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
K="$T/pki"; mkdir -p "$K"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=objrepl-test-ca" -keyout "$K/ca.key" -out "$K/ca.pem" 2>/dev/null
mk() { openssl req -newkey rsa:2048 -nodes -subj "/CN=$2" -keyout "$K/$1.key" -out "$K/$1.csr" 2>/dev/null
       if [ -n "${3:-}" ]; then printf 'subjectAltName=%s\n' "$3" > "$K/$1.ext"; else : > "$K/$1.ext"; fi
       openssl x509 -req -in "$K/$1.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -days 2 -extfile "$K/$1.ext" -out "$K/$1.pem" 2>/dev/null; }
# one certificate per role, used by every node in that role (each node takes either role in turn)
mk copy objrepl-copy "IP:127.0.0.1,DNS:localhost"; mk primary objrepl-primary
IX=(--index "k0:db0:B:0:8,k1:db1:B:4:6")
LK=(--links "lk:0:db0")
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
CPID=""
stop_copy() { [ -n "$CPID" ] || return 0; objrepl_stop $CPID
              for i in $(seq 1 50); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
cleanup() { stop_copy; [ -n "${KEEP_T:-}" ] && { echo "kept $T"; return; }; rm -rf "$T"; }
trap cleanup EXIT
dig() { "$X" digest "$T/$1" "${LK[@]}" 2>/dev/null; }
st() { awk -v k="$2" '$1 == k { $1 = ""; sub(/^ /, ""); print }' "$T/$1/objrepl.status" 2>/dev/null; }
role() { "$X" status "$T/$1" | sed -n 's/.* role \([a-z]*\).*/\1/p'; }
objects() { "$X" digest "$T/$1" "${LK[@]}" 2>/dev/null | awk '{ print $2 }'; }
PORT=$((20000 + RANDOM % 9000))
# copy NAME [options]: a copy of folder NAME on a new port (PORT), until stop_copy
copy() { local n=$1; shift; PORT=$((PORT + 1))
         local seen; seen=$(grep -c "listening" "$T/$n.copy.log" 2>/dev/null); seen=${seen:-0}   # the log is shared by every start
         "$X" copy "$T/$n" --listen 127.0.0.1:$PORT "${CTLS[@]}" "${IX[@]}" "$@" >> "$T/$n.copy.log" 2>&1 & CPID=$!
         for i in $(seq 1 100); do [ "$(grep -c "listening" "$T/$n.copy.log" 2>/dev/null)" -gt "$seen" ] && break; sleep 0.1; done; }
# prim NAME COMMITS SEED [options]: COMMITS commits on primary NAME, pumped to the copy on PORT (or a dead port)
prim() { local n=$1 c=$2 sd=$3; shift 3
         "$X" primary "$T/$n" --copy 127.0.0.1:$PORT "${PTLS[@]}" "${IX[@]}" "${LK[@]}" --dbs 2 --commits "$c" --per 4 --seed "$sd" "$@" >> "$T/$n.primary.log" 2>&1; }
DEAD=$((PORT + 5000))
# split A B SEED [options for B]: A primary with copy B (300 commits, drained), B takes over, A writes 15 more that B
# never gets, B writes 10 of its own (nobody follows either), and A rejoins
split() { local a=$1 b=$2 sd=$3; shift 3
  mkdir -p "$T/$a" "$T/$b"
  copy "$b"
  prim "$a" 300 "$sd" --drain-secs 60 || { echo "split: $a did not drain: $(tail -n 2 "$T/$a.primary.log")"; return 1; }
  stop_copy
  "$X" takeover "$T/$b" > /dev/null || return 1
  local keep=$PORT; PORT=$DEAD
  prim "$a" 15 $((sd + 1)) --drain-secs 0
  prim "$b" 10 $((sd + 2)) --drain-secs 0 "$@"
  PORT=$keep
  "$X" rejoin "$T/$a" ${REJOIN_FULL:-} > "$T/$a.rejoin.txt" 2>&1
}

# emergency
split p c 101 || bad "emergency" "setup"
grep -q "incremental rejoin" "$T/p.rejoin.txt" || bad "emergency" "rejoin did not say incremental: $(cat "$T/p.rejoin.txt")"
total=$(objects c)
copy p
prim c 1 104 --drain-secs 60; rc=$?
stop_copy
sent=$(st c last_rejoin_objects); how=$(st c last_rejoin)
if [ $rc -eq 0 ] && [ "$how" == "incremental" ] && [ -n "$sent" ] && [ "$sent" -gt 0 ] && [ "$sent" -lt "$total" ] &&
   [ "$(dig p)" == "$(dig c)" ] && "$X" ixcheck "$T/p" "${IX[@]}" > "$T/p.ix.txt" 2>&1; then
  ok "emergency: P rejoined under C incrementally ($sent of $total objects sent), digests equal, ixcheck clean"
else
  bad "emergency" "rc $rc how ${how:-?} sent ${sent:-?} of $total, digests $(dig p | cut -c1-16) / $(dig c | cut -c1-16); $(tail -n 3 "$T/c.primary.log")"
fi
# the roles go back: P takes over (it has every commit C made), C rejoins with no full send
"$X" takeover "$T/p" > /dev/null
"$X" rejoin "$T/c" > "$T/c.rejoin.txt" 2>&1
copy c
prim p 5 105 --drain-secs 60; rc=$?
stop_copy
if [ $rc -eq 0 ] && grep -q "no full send" "$T/c.rejoin.txt" && [ "$(dig p)" == "$(dig c)" ] && [ "$(role p)" == primary ] && [ "$(role c)" == copy ]; then
  ok "emergency: the roles went back (C rejoined with no full send), digests equal"
else
  bad "emergency back" "rc $rc $(cat "$T/c.rejoin.txt") roles $(role p)/$(role c)"
fi

# schema
split q r 201 --schema news-v2 || bad "schema" "setup"
before=$(dig q)
copy q --schema news-v1
prim r 1 204 --schema news-v2 --drain-secs 20; rc=$?
stop_copy
if [ $rc -eq 3 ] && grep -q "schema differs" "$T/q.copy.log" && grep -q "news-v1" "$T/q.copy.log" && [ "$(dig q)" == "$before" ] &&
   "$X" status "$T/q" | grep -q "in_snapshot 2"; then
  ok "schema: the rejoin was refused (exit 3, the alarm names the schema), the old node is untouched and still waits"
else
  bad "schema" "rc $rc, said $(grep -c 'schema differs' "$T/q.copy.log"), digest kept $([ "$(dig q)" == "$before" ] && echo yes || echo no)"
fi
mkdir -p "$T/q_new"
copy q_new --schema news-v2
prim r 1 205 --schema news-v2 --drain-secs 60; rc=$?
stop_copy
"$X" swap "$T/q" "$T/q_new" "${IX[@]}" > "$T/swap.txt" 2>&1; src=$?
kept=$(ls -d "$T"/q.before-rebuild-* 2>/dev/null | head -n 1)
if [ $rc -eq 0 ] && [ $src -eq 0 ] && [ "$(dig q)" == "$(dig r)" ] && [ -n "$kept" ] && [ "$("$X" digest "$kept" "${LK[@]}" 2>/dev/null)" == "$before" ] && [ ! -e "$T/q_new" ]; then
  ok "schema: the rebuild took a full send, swap put it in place (equal digests) and kept the old node at $(basename "$kept")"
else
  bad "schema rebuild" "rc $rc swap $src $(cat "$T/swap.txt")"
fi
"$X" swap "$T/q" "$T/q" > /dev/null 2>&1 && bad "swap" "the same folder twice was not refused"
copy q
"$X" swap "$T/q" "$T/r" > "$T/swap2.txt" 2>&1; src=$?
stop_copy
if [ $src -ne 0 ] && grep -q "stop it first" "$T/swap2.txt" && [ -e "$T/r/state.gxd" ]; then ok "swap: refused for the same folder and for a running node"
else bad "swap" "a running node: rc $src $(cat "$T/swap2.txt")"; fi

# history: a primary of its own, with another history, rejoins under r
mkdir -p "$T/x"
PORT0=$PORT; PORT=$DEAD; prim x 140 301 --drain-secs 0; PORT=$PORT0
"$X" rejoin "$T/x" > "$T/x.rejoin.txt" 2>&1
copy x --schema news-v2
prim r 1 302 --schema news-v2 --drain-secs 60; rc=$?
stop_copy
if [ $rc -eq 0 ] && grep -q "does not match" "$T/x.copy.log" && [ "$(st r last_rejoin)" == "full send" ] && [ "$(dig x)" == "$(dig r)" ]; then
  ok "history: another history took a full send instead (it said why), digests equal"
else
  bad "history" "rc $rc last_rejoin $(st r last_rejoin) $(grep -c 'does not match' "$T/x.copy.log")"
fi

# share
split s1 s2 401 --rejoin-max-share 1 || bad "share" "setup"
copy s1
prim s2 1 404 --rejoin-max-share 1 --drain-secs 60; rc=$?
stop_copy
if [ $rc -eq 0 ] && [ "$(st s2 last_rejoin)" == "full send" ] && grep -q "rejoin-max-share" "$T/s2.primary.log" && [ "$(dig s1)" == "$(dig s2)" ]; then
  ok "share: over rejoin-max-share 1%, a full send instead, digests equal"
else
  bad "share" "rc $rc last_rejoin $(st s2 last_rejoin)"
fi

# full
REJOIN_FULL=--full split f1 f2 501 || bad "full" "setup"
copy f1
prim f2 1 504 --drain-secs 60; rc=$?
stop_copy
if [ $rc -eq 0 ] && grep -q "waiting for a full send" "$T/f1.rejoin.txt" && [ "$(st f2 last_rejoin)" == "full send" ] && [ "$(dig f1)" == "$(dig f2)" ]; then
  ok "full: rejoin --full took a full send, digests equal"
else
  bad "full" "rc $rc $(cat "$T/f1.rejoin.txt") last_rejoin $(st f2 last_rejoin)"
fi

# mixed
if [ -n "${OBJREPL_OLD_BIN:-}" ]; then
  NEW=$X
  mkdir -p "$T/m1" "$T/m2"
  X=$OBJREPL_OLD_BIN copy m2; prim m1 50 701 --drain-secs 60; r1=$?; stop_copy
  d1=$([ "$(dig m1)" == "$(dig m2)" ] && echo equal || echo DIFFERENT)
  # the other way: an old primary, a new copy (m3 -> m4); then a split the other way round: new m4 primary with old
  # copy m3, the old m3 takes over (an old node: no epoch history), new m4 writes on and rejoins under it
  mkdir -p "$T/m3" "$T/m4"
  copy m4; X=$OBJREPL_OLD_BIN prim m3 50 702; r2=$?; stop_copy   # (an old tool: no --drain-secs)
  d2=$([ "$(dig m3)" == "$(dig m4)" ] && echo equal || echo DIFFERENT)
  rm -rf "$T/m3" "$T/m4"; mkdir -p "$T/m3" "$T/m4"
  X=$OBJREPL_OLD_BIN copy m3; prim m4 300 705 --drain-secs 60; stop_copy
  "$OBJREPL_OLD_BIN" takeover "$T/m3" > /dev/null
  keep=$PORT; PORT=$DEAD; prim m4 15 703 --drain-secs 0; PORT=$keep
  "$X" rejoin "$T/m4" > "$T/m4.rejoin.txt" 2>&1
  copy m4; X=$OBJREPL_OLD_BIN prim m3 1 704; r3=$?; stop_copy
  d3=$([ "$(dig m3)" == "$(dig m4)" ] && echo equal || echo DIFFERENT)
  if [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && [ $r3 -eq 0 ] && [ $d1 == equal ] && [ $d2 == equal ] && [ $d3 == equal ] &&
     grep -q "incremental rejoin" "$T/m4.rejoin.txt" && grep -q "older primary" "$T/m4.copy.log"; then
    ok "mixed: old copy under a new primary, new copy under an old primary, and a waiting node took a full send from the old primary: equal"
  else
    bad "mixed" "rc $r1/$r2/$r3 digests $d1/$d2/$d3 $(grep -c 'older primary' "$T/m4.copy.log")"
  fi
  X=$NEW
fi

# crash
split k1 k2 601 || bad "crash" "setup"
before=$(dig k1); ecs=""
for at in batch end; do
  export OBJREPL_TEST_KILL_IN_REJOIN=$at
  copy k1
  unset OBJREPL_TEST_KILL_IN_REJOIN
  prim k2 1 $((602 + ${#ecs})) --drain-secs 10
  for i in $(seq 1 100); do kill -0 $CPID 2>/dev/null || break; sleep 0.1; done
  kill -0 $CPID 2>/dev/null && stop_copy
  wait $CPID 2>/dev/null; ecs="$ecs $?"; CPID=""
  "$X" status "$T/k1" | grep -q "in_snapshot 2" || ecs="$ecs not-waiting-after-$at"
done
copy k1
prim k2 1 609 --drain-secs 60; rc=$?
stop_copy
if [ "$ecs" == " 9 9" ] && [ $rc -eq 0 ] && [ "$(st k2 last_rejoin)" == "incremental" ] && [ "$(dig k1)" == "$(dig k2)" ] &&
   "$X" ixcheck "$T/k1" "${IX[@]}" > /dev/null 2>&1; then
  ok "crash: killed in a batch and in the end (exit 9 each, still waiting), then the rejoin completed: equal, ixcheck clean"
else
  bad "crash" "exits${ecs} (9 9 expected: a -DGXREPL_TEST_HOOKS=ON build), rc $rc last_rejoin $(st k2 last_rejoin)"
fi

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
