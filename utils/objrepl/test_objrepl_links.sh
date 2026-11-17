#!/bin/bash
# test_objrepl_links.sh - 11/16/2026: several databases per stream, link fields, the full send to a new copy, a copy
# killed inside the full send's link fix-up. Throwaway folders only.
#
#   utils/objrepl/test_objrepl_links.sh BIN
#
# The primary writes 3 databases plus "lk", whose objects hold a link (offset 0) into db0 ("lk:0:db0"): links to an
# object added later in the same commit, and pairs linked both ways inside lk ("lk:0:lk" is not used: a cycle needs
# the target to be lk; see case c). Checks, each by digest (ids, content, links compared as ids, never addresses):
#   a/c  copy A follows the primary live; links to objects added later in a commit, and link cycles, come out right
#   new  copy B starts empty after the primary trimmed its outbox: it gets the full send, then the stream
#   d    copy C is killed inside the full send's link fix-up: it restarts, is never marked caught up before the
#        fix-up is done (status in_snapshot 1, applied 0), and the next full send finishes it
set -u
. "$(dirname "$0")/objrepl_proc.sh"   # 11/17/2026: objrepl_stop, objrepl_pid (Windows)
X=${1:?usage: test_objrepl_links.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_links.XXXXXX")
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
CPID=""
start_copy() { "$X" copy "$T/$1" --listen $2 "${CTLS[@]}" ${3:+$3} >> "$T/copy_$1.log" 2>&1 & CPID=$!; sleep 0.4; }
stop_copy() { [ -n "$CPID" ] || return 0; objrepl_stop $CPID
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; rm -rf "$T"' EXIT
dig() { "$X" digest "$T/$1" "${LINKS[@]}"; }
mkdir -p "$T/p" "$T/a" "$T/b" "$T/c"
PA=$((20000 + RANDOM % 10000)); PB=$((PA + 1)); PC=$((PA + 2))

# a/c: live, with links (later-added targets, cycles in lk), several databases
start_copy a $PA
"$X" primary "$T/p" --copy 127.0.0.1:$PA "${PTLS[@]}" "${LINKS[@]}" --dbs 3 --commits 300 --per 8 --seed 11 > "$T/p1.log" 2>&1; rc=$?
stop_copy
dp=$(dig p); da=$(dig a)
nlk=$(ls "$T/a" | grep -c "^lk.gxd"); nlkobj=$("$X" digest "$T/a" "${LINKS[@]}" >/dev/null; echo ok)
[ $rc -eq 0 ] && [ "$dp" == "$da" ] && [ "$nlk" -eq 1 ] && ok "a/c live: copy A = primary ($dp), 4 databases incl. lk" || bad "a/c live" "rc $rc primary [$dp] copy [$da]"

# new: copy B empty, the primary's outbox already trimmed by copy A: a full send, then the stream
start_copy b $PB
"$X" primary "$T/p" --copy 127.0.0.1:$PB "${PTLS[@]}" "${LINKS[@]}" --dbs 3 --commits 50 --per 8 --seed 12 > "$T/p2.log" 2>&1; rc=$?
stop_copy
dp=$(dig p); db=$(dig b); snaps=$(grep -o "[0-9]* snapshot" "$T/p2.log" | awk '{print $1}')
[ $rc -eq 0 ] && [ "$dp" == "$db" ] && [ "${snaps:-0}" -ge 1 ] && ok "new copy: full send ($snaps), then 50 commits; copy B = primary ($dp)" || bad "new copy" "rc $rc snaps ${snaps:-0} primary [$dp] copy [$db]"

# d: copy C killed inside the full send's link fix-up
OBJREPL_TEST_KILL_IN_FIXUP=1 "$X" copy "$T/c" --listen $PC "${CTLS[@]}" >> "$T/copy_c.log" 2>&1 & CPID=$!; sleep 0.4
"$X" primary "$T/p" --copy 127.0.0.1:$PC "${PTLS[@]}" "${LINKS[@]}" --dbs 3 --drain > "$T/p3.log" 2>&1; rc1=$?
for i in $(seq 1 50); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
if kill -0 $CPID 2>/dev/null; then crc=-1; kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; else wait $CPID 2>/dev/null; crc=$?; fi; CPID=""
st=$("$X" status "$T/c")
inside=$(echo "$st" | awk '{print $10}'); applied=$(echo "$st" | awk '{print $8}')
[ $crc -eq 9 ] && [ "$inside" == "1" ] && [ "$applied" == "0" ] && ok "d: killed in the fix-up (exit 9): still in_snapshot 1, applied 0" || bad "d kill" "copy exit $crc, status [$st]"
start_copy c $PC
"$X" primary "$T/p" --copy 127.0.0.1:$PC "${PTLS[@]}" "${LINKS[@]}" --dbs 3 --drain > "$T/p4.log" 2>&1; rc=$?
stop_copy
dp=$(dig p); dc=$(dig c); st=$("$X" status "$T/c")
[ $rc -eq 0 ] && [ "$dp" == "$dc" ] && [ "$(echo "$st" | awk '{print $10}')" == "0" ] && ok "d: after the restart a new full send finished it; copy C = primary ($dp)" || bad "d restart" "rc $rc primary [$dp] copy [$dc] [$st]"

# b: targets of links removed in a later commit: each becomes a tombstone (the link names it); copy and primary agree
start_copy a $PA
"$X" primary "$T/p" --copy 127.0.0.1:$PA "${PTLS[@]}" "${LINKS[@]}" --dbs 3 --drop-linked 5 --drain > "$T/p5.log" 2>&1; rc=$?
stop_copy
dp=$("$X" digest "$T/p" "${LINKS[@]}" 2> "$T/dg_p.err"); da=$("$X" digest "$T/a" "${LINKS[@]}" 2> "$T/dg_a.err")
np=$(grep -o "[0-9]* tombstone" "$T/dg_p.err" | awk '{print $1}'); na=$(grep -o "[0-9]* tombstone" "$T/dg_a.err" | awk '{print $1}')
dgp=$(grep -o "[0-9]* dangling" "$T/dg_p.err" | awk '{print $1}'); dga=$(grep -o "[0-9]* dangling" "$T/dg_a.err" | awk '{print $1}')
[ $rc -eq 0 ] && [ "$dp" == "$da" ] && grep -q "removed 5 linked" "$T/p5.log" && [ "${np:-0}" -ge 5 ] && [ "${np:-0}" == "${na:-x}" ] && [ "$dgp$dga" == "00" ] \
  && ok "b: 5 link targets removed later: copy = primary ($dp), $np tombstones on both, 0 dangling" \
  || bad "b" "rc $rc primary [$dp] (${np:-0} tombs, ${dgp:-?} dangling) copy [$da] (${na:-0} tombs, ${dga:-?} dangling)"

# b, live: 5 more link targets removed while copy A is connected and current (the removes stream as ordinary ops)
start_copy a $PA
"$X" primary "$T/p" --copy 127.0.0.1:$PA "${PTLS[@]}" "${LINKS[@]}" --dbs 3 --drop-linked 5 --drain > "$T/p6.log" 2>&1; rc=$?
stop_copy
snaps=$(grep -o "[0-9]* snapshot" "$T/p6.log" | awk '{print $1}')
dp=$("$X" digest "$T/p" "${LINKS[@]}" 2> "$T/dg_p.err"); da=$("$X" digest "$T/a" "${LINKS[@]}" 2> "$T/dg_a.err")
np=$(grep -o "[0-9]* tombstone" "$T/dg_p.err" | awk '{print $1}'); na=$(grep -o "[0-9]* tombstone" "$T/dg_a.err" | awk '{print $1}')
dgp=$(grep -o "[0-9]* dangling" "$T/dg_p.err" | awk '{print $1}'); dga=$(grep -o "[0-9]* dangling" "$T/dg_a.err" | awk '{print $1}')
[ $rc -eq 0 ] && [ "${snaps:-0}" == "0" ] && [ "$dp" == "$da" ] && [ "${np:-0}" -ge 10 ] && [ "${np:-0}" == "${na:-x}" ] && [ "$dgp$dga" == "00" ] \
  && ok "b live: 5 more removed while the copy followed live (no full send): copy = primary, $np tombstones on both, 0 dangling" \
  || bad "b live" "rc $rc snaps ${snaps:-0} primary [$dp] (${np:-0}) copy [$da] (${na:-0})"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
