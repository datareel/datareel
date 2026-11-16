#!/bin/bash
# test_objrepl_iofaults.sh - 11/16/2026: disk full and I/O errors on each side, mid-commit and mid-apply. Throwaway
# folders only.
#
#   utils/objrepl/test_objrepl_iofaults.sh BIN SHIM
#
# BIN: the objrepl tool; SHIM: eio_shim.so built from utils/objrepl/eio_shim.c (gcc -shared -fPIC ... -ldl).
# Disk full: the file size limit (ulimit -f, SIGXFSZ ignored) makes a write that grows a file past it fail (EFBIG),
# as a full disk would. I/O errors: the shim fails pwrite or fsync/fdatasync with EIO on one node's files.
#   copy full      the copy runs out of space while applying: nothing half applied, no ack for what failed; with
#                  space again it catches up and equals the primary
#   primary full   the primary runs out of space while committing: the commit fails (the tool exits 1) and is not
#                  in the outbox; after a restart the node is whole and the copy equals it
#   copy EIO once  one pwrite on the copy fails mid-apply: the copy rolls back, the resend applies; equal
#   copy fsync EIO one fsync on the copy fails at the commit: rolled back, no ack, the resend applies; equal
#   primary fsync EIO  one fsync on the primary fails at the commit: Commit() fails, the node is whole after a
#                  restart; equal
#   copy EIO always    every pwrite after a point fails: the copy applies nothing more (never half), the primary is
#                  not drained; without the fault it catches up; equal
# Every case ends with Digest equal and IndexCheck consistent on both nodes.
set -u
X=${1:?usage: test_objrepl_iofaults.sh BIN SHIM}
SHIM=${2:?usage: test_objrepl_iofaults.sh BIN SHIM}
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_iofaults.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || { echo "certificates failed"; exit 1; }
K="$T/pki"
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
LINKS=(--links "lk:0:db0,lk:8:lk"); IX=(--index "k0:db0:B:0:8,v0:db0:V:8:8")
CPID=""
P=$((20000 + RANDOM % 10000))
# start_copy DIR PORT [ENV...] [-- LIMIT_KB]: the copy, optionally with env (the shim) and a file size limit
start_copy() {
  local d=$1 port=$2 lim=""; shift 2
  local envs=()
  while [ $# -gt 0 ]; do case "$1" in --limit) lim=$2; shift 2 ;; *) envs+=("$1"); shift ;; esac; done
  ( trap '' XFSZ; [ -n "$lim" ] && ulimit -f $lim; exec env "${envs[@]}" "$X" copy "$T/$d" --listen $port "${CTLS[@]}" "${IX[@]}" ) >> "$T/copy_$d.log" 2>&1 & CPID=$!
  for i in $(seq 1 100); do grep -qE "^state (listening|streaming)" "$T/$d/objrepl.status" 2>/dev/null && \
    [ "$(awk '$1=="pid"{print $2}' "$T/$d/objrepl.status")" == "$CPID" ] && return 0; kill -0 $CPID 2>/dev/null || return 1; sleep 0.1; done; return 1
}
stop_copy() { [ -n "$CPID" ] || return 0; kill -TERM $CPID 2>/dev/null
	      for i in $(seq 1 50); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
	      kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; [ "${KEEP:-0}" == 1 ] && echo "kept $T" || rm -rf "$T"' EXIT
# prim DIR PORT [ENV...] [--limit KB] -- OPTIONS: the primary (foreground), the same way
prim() {
  local d=$1 port=$2 lim=""; shift 2
  local envs=()
  while [ $# -gt 0 ] && [ "$1" != "--" ]; do case "$1" in --limit) lim=$2; shift 2 ;; *) envs+=("$1"); shift ;; esac; done
  shift
  ( trap '' XFSZ; [ -n "$lim" ] && ulimit -f $lim; exec env "${envs[@]}" "$X" primary "$T/$d" --copy 127.0.0.1:$port "${PTLS[@]}" "${LINKS[@]}" "${IX[@]}" --dbs 2 "$@" )
}
same() {  # same PRIMARY COPY: digests equal, both ixchecks consistent with equal index digests
  local a b ia ib
  a=$("$X" digest "$T/$1" "${LINKS[@]}" 2>/dev/null); b=$("$X" digest "$T/$2" "${LINKS[@]}" 2>/dev/null)
  ia=$("$X" ixcheck "$T/$1" "${IX[@]}") || return 1; ib=$("$X" ixcheck "$T/$2" "${IX[@]}") || return 1
  [ "$a" == "$b" ] && [ "$(echo "$ia" | awk '{print $2,$6,$8}')" == "$(echo "$ib" | awk '{print $2,$6,$8}')" ]
}
heal() {  # heal PRIMARY COPY PORT: no faults, drain, compare
  start_copy $2 $3 || return 1
  prim $1 $3 -- --commits 0 --drain > "$T/heal_$1.log" 2>&1; local rc=$?
  stop_copy
  [ $rc -eq 0 ] && same $1 $2
}
size_kb() { du -k --apparent-size "$T/$1" | awk 'END {print $1}'; }
maxfile_kb() { find "$T/$1" -type f -printf '%s\n' | sort -n | tail -n 1 | awk '{print int($1 / 1024) + 1}'; }

# copy full: the copy's largest file may grow 300 KB more, then writes fail
mkdir -p "$T/a1" "$T/b1"
start_copy b1 $P; prim a1 $P -- --commits 40 --per 8 --seed 1 > /dev/null 2>&1; stop_copy
lim=$(( $(maxfile_kb b1) + 300 ))
start_copy b1 $P --limit $lim
prim a1 $P -- --commits 400 --per 8 --seed 2 > "$T/a1_full.log" 2>&1; r=$?
sleep 1; stop_copy
applied=$("$X" status "$T/b1" | awk '{print $8}'); last=$("$X" status "$T/a1" | awk '{print $4 - 1}')
grep -qE "did not apply|failed" "$T/copy_b1.log"; failed=$?
heal a1 b1 $P && [ $failed -eq 0 ] && [ "$applied" -lt "$last" ] \
  && ok "copy full: the copy stopped applying at $applied of $last (limit ${lim} KB), then caught up: equal and consistent" \
  || bad "copy full" "primary rc $r applied $applied last $last failed-line $failed"

# primary full: the primary's files may grow 200 KB more
mkdir -p "$T/a2" "$T/b2"
start_copy b2 $((P + 1)); prim a2 $((P + 1)) -- --commits 40 --per 8 --seed 3 > /dev/null 2>&1; stop_copy
lim=$(( $(maxfile_kb a2) + 200 ))
prim a2 $((P + 1)) --limit $lim -- --commits 400 --per 8 --seed 4 > "$T/a2_full.log" 2>&1; r=$?
last=$("$X" status "$T/a2" | awk '{print $4 - 1}')
heal a2 b2 $((P + 1)) && [ $r -ne 0 ] && grep -qE "commit|write" "$T/a2_full.log" \
  && ok "primary full: the commit failed (exit $r: $(grep -m1 -oE 'objrepl primary: [a-z]+: [^/]*' "$T/a2_full.log")), the node was whole at seq $last, the copy equals it" \
  || bad "primary full" "rc $r last $last $(tail -n 2 "$T/a2_full.log" | tr '\n' '|')"

# copy EIO once (pwrite) and copy fsync EIO once
for call in pwrite fsync; do
  mkdir -p "$T/a_$call" "$T/b_$call"
  start_copy b_$call $((P + 2)) LD_PRELOAD="$SHIM" EIO_MATCH="/b_$call/" EIO_CALL=$call EIO_AFTER=200 EIO_MODE=once
  prim a_$call $((P + 2)) -- --commits 200 --per 8 --seed 5 > "$T/a_$call.log" 2>&1; r=$?
  stop_copy
  eio=$(grep -c "eio_shim" "$T/copy_b_$call.log")
  [ $r -eq 0 ] && [ "$eio" -ge 1 ] && same a_$call b_$call \
    && ok "copy $call EIO once: the copy hit it ($eio), rolled back, the resend applied; equal and consistent" \
    || bad "copy $call EIO once" "primary rc $r, eio lines $eio, $(grep -m2 -E 'did not apply|failed|EIO' "$T/copy_b_$call.log" | tr '\n' '|')"
done

# primary fsync EIO once
mkdir -p "$T/a3" "$T/b3"
prim a3 $((P + 3)) LD_PRELOAD="$SHIM" EIO_MATCH="/a3/" EIO_CALL=fsync EIO_AFTER=100 EIO_MODE=once -- --commits 200 --per 8 --seed 6 > "$T/a3.log" 2>&1; r=$?
last=$("$X" status "$T/a3" | awk '{print $4 - 1}')
heal a3 b3 $((P + 3)) && [ $r -ne 0 ] && grep -q "eio_shim" "$T/a3.log" \
  && ok "primary fsync EIO once: Commit() failed (exit $r), the node was whole at seq $last after a restart; the copy equals it" \
  || bad "primary fsync EIO once" "rc $r last $last $(grep -m2 -E 'eio|commit' "$T/a3.log" | tr '\n' '|')"

# copy EIO always: every pwrite after 300 fails
mkdir -p "$T/a4" "$T/b4"
start_copy b4 $((P + 4)) LD_PRELOAD="$SHIM" EIO_MATCH="/b4/" EIO_CALL=pwrite EIO_AFTER=300 EIO_MODE=always
timeout 120 bash -c "$(declare -f prim); X='$X'; T='$T'; PTLS=(${PTLS[*]}); LINKS=(${LINKS[*]}); IX=(${IX[*]}); prim a4 $((P + 4)) -- --commits 150 --per 8 --seed 7" > "$T/a4.log" 2>&1; r=$?
sleep 1; stop_copy
applied=$("$X" status "$T/b4" | awk '{print $8}'); last=$("$X" status "$T/a4" | awk '{print $4 - 1}')
heal a4 b4 $((P + 4)) && [ "$applied" -lt "$last" ] \
  && ok "copy EIO always: the copy applied nothing past seq $applied of $last while it failed (primary exit $r), then caught up: equal" \
  || bad "copy EIO always" "rc $r applied $applied last $last"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
