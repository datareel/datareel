#!/bin/bash
# test_objrepl_reader.sh - 11/19/2026 (7.4.1): gxObjReplReader, read only from other processes while a node runs.
# Throwaway folders only.
#
#   utils/objrepl/test_objrepl_reader.sh BIN      (BIN: the objrepl tool; test_gxobjrepl_reader is built next to it)
#
#   primary    a primary makes 2500 commits while two reader processes read it for the whole run: every read is
#              whole commits (a 12000-byte object rewritten by every commit is never seen half written), Seq()
#              never goes back, every key and vector up to the commit read is found; then the final state
#   copy       the same primary streams to a copy (objrepl copy, with the same indexes); a reader reads the copy
#              while it applies: the same checks, then the copy's final state
#   killed     a primary killed (KILL) in the middle of its commits: with no node running, the reader replays the
#              journal it left and reads a whole commit ("big" names as many commits as "vec" holds)
#   lock       the node's lock is untouched: while readers read, a second node cannot open the root
set -u
. "$(dirname "$0")/objrepl_proc.sh"
X=${1:?usage: test_objrepl_reader.sh BIN}
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
R=$(dirname "$X")/test_gxobjrepl_reader
[ -x "$R" ] || [ -x "$R.exe" ] || { echo "FAIL reader: $R is not built"; echo "0 passed, 1 failed"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_reader.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
CPID=""; WP=""
stop_copy() { [ -n "$CPID" ] || return 0; objrepl_stop $CPID
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
cleanup() { [ -n "$WP" ] && kill -KILL $WP 2>/dev/null; stop_copy; rm -rf "$T"; }
trap cleanup EXIT
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || { echo "FAIL reader: certificates"; exit 1; }
INDEX="k0:vec:B:0:8,v0:vec:V:8:4"
P0=$((20000 + RANDOM % 10000))

# primary
mkdir -p "$T/p"
"$R" write "$T/p" "$T/pki" 2500 > "$T/w_p.log" 2>&1 & WP=$!
"$R" read "$T/p" 25 > "$T/r1.log" 2>&1 & R1=$!
"$R" read "$T/p" 25 > "$T/r2.log" 2>&1 & R2=$!
for i in $(seq 1 300); do grep -q "^write: 100" "$T/w_p.log" && break; sleep 0.1; done   # the writer holds the root
"$X" primary "$T/p" --copy 127.0.0.1:1 --ca "$T/pki/ca.pem" --cert "$T/pki/primary.pem" --key "$T/pki/primary.key" --commits 0 > "$T/second.log" 2>&1; sr=$?
wait $WP; wr=$?; WP=""
wait $R1; r1=$?; wait $R2; r2=$?
"$R" final "$T/p" 2500 > "$T/f_p.log" 2>&1; fr=$?
[ $wr -eq 0 ] && [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && [ $fr -eq 0 ] \
  && ok "primary: 2 readers during 2500 commits [$(cat "$T/r1.log" | head -n 1 | cut -c6-)] [$(cat "$T/f_p.log" | cut -c6-)]" \
  || bad "primary" "writer $wr readers $r1/$r2 final $fr: $(cat "$T/w_p.log" "$T/r1.log" "$T/r2.log" "$T/f_p.log" | grep -v '^write: [0-9]' | tr '\n' '|')"
grep -q "in use by another process" "$T/second.log" && [ $sr -ne 0 ] && ok "lock: a second node could not open the root while it ran (readers took no node lock)" \
  || bad "lock" "second node rc $sr: $(tail -n 1 "$T/second.log")"

# copy
mkdir -p "$T/q" "$T/c"
"$X" copy "$T/c" --listen $P0 --ca "$T/pki/ca.pem" --cert "$T/pki/copy.pem" --key "$T/pki/copy.key" --allow-cn objrepl-primary --index "$INDEX" >> "$T/copy.log" 2>&1 & CPID=$!
for i in $(seq 1 50); do grep -q "listening" "$T/copy.log" && break; sleep 0.2; done
"$R" write "$T/q" "$T/pki" 1500 $P0 > "$T/w_q.log" 2>&1 & WP=$!
"$R" read "$T/c" 20 > "$T/r3.log" 2>&1; r3=$?
wait $WP; wr=$?; WP=""
"$R" final "$T/c" 1500 > "$T/f_c.log" 2>&1; fr=$?
stop_copy
[ $wr -eq 0 ] && [ $r3 -eq 0 ] && [ $fr -eq 0 ] \
  && ok "copy: a reader while the copy applied 1500 commits [$(cut -c6- "$T/r3.log" | tr "\n" " ")] [$(cut -c6- "$T/f_c.log")]" \
  || bad "copy" "writer $wr reader $r3 final $fr: $(cat "$T/w_q.log" "$T/r3.log" "$T/f_c.log" | grep -v '^write: [0-9]' | tr '\n' '|')"

# killed
mkdir -p "$T/k"
"$R" write "$T/k" "$T/pki" 100000 > "$T/w_k.log" 2>&1 & WP=$!
for i in $(seq 1 300); do grep -q "^write: 600" "$T/w_k.log" && break; sleep 0.1; done
kill -KILL $WP; wait $WP 2>/dev/null; WP=""
"$R" final "$T/k" > "$T/f_k.log" 2>&1; fr=$?
[ $fr -eq 0 ] && ok "killed: the reader replayed the journal of the killed primary and read a whole commit [$(cut -c6- "$T/f_k.log")]" \
  || bad "killed" "$(cat "$T/f_k.log")"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
