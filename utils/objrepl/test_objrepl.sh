#!/bin/bash
# test_objrepl.sh - 11/16/2026: the object replication prototype under faults, on throwaway folders.
#
#   utils/objrepl/test_objrepl.sh BIN [ROUNDS]
#
# BIN is the built objrepl. The wire is gxStream (TLS, mutual certificates made here with openssl). Each round: the primary writes commits (adds, updates of a new size, removes) while the
# copy is killed with SIGKILL and restarted, and the primary itself is killed with SIGKILL and restarted. Then the
# primary drains and the copy must hold exactly the primary's objects (digest: ids and content hashes, no file
# addresses) with applied_seq = the primary's last seq. Then a takeover: the copy moves to epoch 2 and must refuse
# the old primary. Exit 0 when every check passes.
set -u
X=${1:?usage: test_objrepl.sh BIN [ROUNDS]}
ROUNDS=${2:-3}
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_test.XXXXXX")
PORT=$((20000 + RANDOM % 20000))
FAIL=0
ok()  { echo "PASS $1"; }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
CPID=""
start_copy() { "$X" copy "$T/c" --listen $PORT "${CTLS[@]}" >> "$T/copy.log" 2>&1 & CPID=$!; sleep 0.3; }
stop_copy()  {  # TERM, then KILL after 5 s if the copy is still in its accept or handshake
    [ -n "$CPID" ] || return 0
    kill -TERM $CPID 2>/dev/null
    for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
    kill -0 $CPID 2>/dev/null && { echo "note: the copy did not stop on TERM within 5 s; KILL" >> "$T/copy.log"; kill -KILL $CPID 2>/dev/null; }
    wait $CPID 2>/dev/null; CPID=""
}
kill_copy()  { [ -n "$CPID" ] && kill -KILL $CPID 2>/dev/null && wait $CPID 2>/dev/null; CPID=""; }
cleanup() { stop_copy; [ -n "${PPID_W:-}" ] && kill -KILL $PPID_W 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT
mkdir -p "$T/p" "$T/c"
# 11/16/2026: throwaway certificates for gxStream's mutual TLS: a CA, the copy (server, 127.0.0.1) and the primary
# (client, CN objrepl-primary)
K="$T/pki"; mkdir -p "$K"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=objrepl-test-ca" -keyout "$K/ca.key" -out "$K/ca.pem" 2>/dev/null
mk() {  # mk NAME CN [SAN]
    openssl req -newkey rsa:2048 -nodes -subj "/CN=$2" -keyout "$K/$1.key" -out "$K/$1.csr" 2>/dev/null
    if [ -n "${3:-}" ]; then printf 'subjectAltName=%s\n' "$3" > "$K/$1.ext"; else : > "$K/$1.ext"; fi
    openssl x509 -req -in "$K/$1.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -days 2 -extfile "$K/$1.ext" -out "$K/$1.pem" 2>/dev/null
}
mk copy objrepl-copy "IP:127.0.0.1,DNS:localhost"
mk primary objrepl-primary
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")

start_copy
for r in $(seq 1 "$ROUNDS"); do
    kills_c=0 kills_p=0
    # a long writer in the background; faults while it runs
    for part in 1 2 3; do
        "$X" primary "$T/p" --copy 127.0.0.1:$PORT "${PTLS[@]}" --commits 400 --per 8 --sleep 2 --seed $((r * 100 + part)) >> "$T/primary.log" 2>&1 &
        PPID_W=$!
        sleep 0.$((3 + RANDOM % 6))
        kill_copy; kills_c=$((kills_c + 1)); sleep 0.2; start_copy           # the copy dies mid-apply, comes back
        sleep 0.$((2 + RANDOM % 6))
        kill -KILL $PPID_W 2>/dev/null; wait $PPID_W 2>/dev/null; kills_p=$((kills_p + 1))   # the primary dies mid-commit
        PPID_W=""
    done
    # the primary comes back (its journal finishes or undoes the cut commit) and drains to the copy
    "$X" primary "$T/p" --copy 127.0.0.1:$PORT "${PTLS[@]}" --drain >> "$T/primary.log" 2>&1; drc=$?
    dp=$("$X" digest "$T/p"); stop_copy; dc=$("$X" digest "$T/c")
    sp=$("$X" status "$T/p"); sc=$("$X" status "$T/c")
    last=$(echo "$sp" | awk '{print $4 - 1}'); applied=$(echo "$sc" | awk '{print $8}')
    if [ $drc -eq 0 ] && [ "$dp" == "$dc" ] && [ "$last" == "$applied" ]; then
        ok "round $r: $kills_c copy kills, $kills_p primary kills; copy = primary ($dp), applied $applied of $last"
    else
        bad "round $r" "drain rc $drc; primary [$dp] [$sp]; copy [$dc] [$sc]"
    fi
    start_copy
done

# takeover: the copy becomes primary at epoch 2; the old primary (epoch 1) must be refused
stop_copy
"$X" takeover "$T/c" > /dev/null
ep=$("$X" status "$T/c" | awk '{print $2}')
start_copy
"$X" primary "$T/p" --copy 127.0.0.1:$PORT "${PTLS[@]}" --commits 3 --per 2 >> "$T/primary.log" 2>&1; prc=$?
dc2=$("$X" digest "$T/c")
if [ "$ep" == "2" ] && [ $prc -eq 3 ] && [ "$dc2" == "$dc" ]; then
    ok "takeover: the copy is primary at epoch 2 and refused the old primary (exit 3); its objects unchanged"
else
    bad "takeover" "epoch $ep, old primary rc $prc, copy before [$dc] after [$dc2]"
fi
stop_copy
grep -c "refused" "$T/primary.log" > /dev/null || true
echo "$((ROUNDS + 1 - FAIL)) passed, $FAIL failed"
[ $FAIL -eq 0 ]
