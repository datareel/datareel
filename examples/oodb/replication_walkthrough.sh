#!/bin/bash
# replication_walkthrough.sh - 11/14/2026 (DataReel 7.4): gxrepl replication, object by object, on throwaway
# folders of this machine (127.0.0.1 only):
#   1. a primary writes records with two indexes made from them (a B-tree of names, a vector index); gxrepl copies
#      the records to a copy, which keeps its own two indexes object by object (derived_apply = incremental, the
#      example_records extractor): the digests of both indexes are the same on both nodes;
#   2. a simulated fault on the copy (its vector index's stamp damaged): gxrepl sees it, rebuilds that one folder
#      on its own ("replication rebuild"), says so in its log, and goes back to object by object;
#   3. the digests are the same on both nodes again.
#
#   examples/oodb/replication_walkthrough.sh BUILD/examples [WORK_DIR]
#
# BUILD/examples holds replication and replication_gxrepl. Needs the openssl command (for throwaway
# certificates). WORK_DIR (default: a new folder under ${TMPDIR:-/tmp}) is removed at the end unless KEEP=1.
# Exit 0 when every check passes.
set -u
BIN=$(cd "${1:?usage: replication_walkthrough.sh BUILD/examples [WORK_DIR]}" && pwd)
W=${2:-$(mktemp -d "${TMPDIR:-/tmp}/repl_example.XXXXXX")}
mkdir -p "$W"
PORT=$(( 20000 + RANDOM % 20000 ))
GX="$BIN/replication_gxrepl"
APP="$BIN/replication"
pids=()
fail() { echo "FAILED: $*"; for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; KEEP=1; echo "(work folder kept: $W)"; exit 1; }
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; [ "${KEEP:-0}" = 1 ] || rm -rf "$W"; }
trap cleanup EXIT
step() { echo; echo "== $*"; }
wait_for() {   # wait_for SECONDS DESCRIPTION COMMAND... (run again on each try: put $(...) checks inside bash -c)
  local secs=$1 what=$2; shift 2
  local end=$(( $(date +%s) + secs ))
  while [ "$(date +%s)" -lt "$end" ]; do "$@" > /dev/null 2>&1 && return 0; sleep 0.5; done
  fail "timed out waiting for: $what"
}
gen() { od -An -t u8 -j 8 -N 8 "$1" 2>/dev/null | tr -d ' '; }

step "Throwaway certificates (a private CA, the copy's server certificate, the primary's client certificate)"
mkdir -p "$W/pki" && cd "$W/pki" || fail "no work folder"
EC="-newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes"
openssl req -x509 $EC -keyout ca.key -out ca.pem -days 2 -subj /CN=example-ca 2>/dev/null || fail "openssl"
openssl req $EC -keyout copy.key -out copy.csr -subj /CN=copy 2>/dev/null
printf 'subjectAltName=DNS:copy,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > copy.ext
openssl x509 -req -in copy.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out copy.pem -days 2 -extfile copy.ext 2>/dev/null
openssl req $EC -keyout primary.key -out primary.csr -subj /CN=primary 2>/dev/null
printf 'extendedKeyUsage=clientAuth\n' > primary.ext
openssl x509 -req -in primary.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out primary.pem -days 2 -extfile primary.ext 2>/dev/null
chmod 600 *.key
cd "$W"

step "Configs: the primary pushes set 'records' to the copy; the copy keeps by_name.btx and vectors.vix itself"
mkdir -p primary copy
SET="commit_block = commit.bin@8
include = commit.bin
derived = by_name.btx, vectors.vix"
cat > primary.conf <<EOF
node = primary
role = client
connect = 127.0.0.1:$PORT
server_name = copy
cert = $W/pki/primary.pem
key = $W/pki/primary.key
ca = $W/pki/ca.pem
status_file = $W/primary.status.json
poll_ms = 200

[set records]
root = $W/primary
direction = push
scan_interval = 1
$SET
EOF
cat > copy.conf <<EOF
node = copy
role = server
listen = 127.0.0.1:$PORT
allow_cn = primary
cert = $W/pki/copy.pem
key = $W/pki/copy.key
ca = $W/pki/ca.pem
status_file = $W/copy.status.json

[set records]
root = $W/copy
accept = target
$SET
# object by object, with the example's extractor (built into replication_gxrepl)
derived_apply = incremental
derived_extractor = example_records
derived_source = data.gxd
derived_count = by_name.btx
# the fall back: a full rebuild of one folder
derived_rebuild = $APP rebuild {dir} {lsn}
derived_delay = 1
derived_busy_rebuild = 3
EOF
sed -n '/\[set records\]/,$p' copy.conf

step "Start the copy (serve) and the primary's replicator (run), then the primary's writer"
"$GX" serve --config copy.conf > copy.log 2>&1 & pids+=($!)
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break; sleep 0.1; done
"$GX" run --config primary.conf > primary.log 2>&1 & pids+=($!)
# one writer process for the whole walkthrough (a new writer process makes the copy verify its files once)
"$APP" write "$W/primary" --commits 400 --per 8 --sleep 200 > writer.log 2>&1 & WRITER=$!

step "1. The copy follows object by object"
wait_for 120 "the copy's indexes stamped" test -s "$W/copy/vectors.vix.stamp" -a -s "$W/copy/by_name.btx.stamp"
wait_for 120 "derived state 'ok incremental'" grep -q "ok incremental" "$W/copy/.gxrepl/derived/_root"
echo "derived state: $(cat "$W/copy/.gxrepl/derived/_root")"
"$GX" derived status --config copy.conf | sed 's/^/  /'
REBUILDS1=$(grep -c "derived indexes rebuilt" copy.log)
echo "full rebuilds so far: $REBUILDS1 (the first one makes the stamped indexes the copy goes on from)"

step "2. A simulated fault: the copy's vectors.vix.stamp damaged"
printf 'XXXXXXXXXXXXXXXX' | dd of="$W/copy/vectors.vix.stamp" bs=1 seek=$(( $(stat -c %s "$W/copy/vectors.vix.stamp") - 40 )) conv=notrunc 2>/dev/null
wait_for 60 "gxrepl to see it (rebuild needed)" grep -q "rebuild needed" copy.log
grep "rebuild needed" copy.log | tail -1 | sed 's/^/  log: /'
wait_for 90 "the folder rebuilt" bash -c '[ "$(grep -c "derived indexes rebuilt" copy.log)" -gt '"$REBUILDS1"' ]'
grep "derived indexes rebuilt" copy.log | tail -1 | sed 's/^/  log: /'
"$GX" derived status --config copy.conf | sed 's/^/  /'
REBUILDS2=$(grep -c "derived indexes rebuilt" copy.log)
wait_for 60 "back to object by object" sh -c "grep -q 'ok incremental' '$W/copy/.gxrepl/derived/_root' && test -s '$W/copy/vectors.vix.stamp'"
echo "derived state: $(cat "$W/copy/.gxrepl/derived/_root")"

step "3. The writer finishes; both nodes hold the same indexes"
wait "$WRITER" || fail "the writer failed: $(cat writer.log)"
cat writer.log
wait_for 120 "the copy at the primary's commit" bash -c 'g() { od -An -t u8 -j 8 -N 8 "$1" 2>/dev/null | tr -d " "; }; [ "$(g "$1/copy/commit.bin")" = "$(g "$1/primary/commit.bin")" ]' _ "$W"
wait_for 60 "the copy's indexes current" sh -c "! '$GX' torn --config '$W/copy.conf' | grep -q INDEXES"
P=$("$APP" digest "$W/primary") || fail "primary digest: $P"
C=$("$APP" digest "$W/copy") || fail "copy digest: $C"
echo "primary:"; echo "$P" | sed 's/^/  /'
echo "copy:";    echo "$C" | sed 's/^/  /'
[ "$P" = "$C" ] || fail "the digests differ"
LIVE=$(sed -n 's/.*, \([0-9]*\) live records$/\1/p' writer.log)
KEYS=$(echo "$P" | sed -n 's/^by_name.btx btree [0-9a-f]* \([0-9]*\) keys$/\1/p')
[ -n "$LIVE" ] && [ "$KEYS" = "$LIVE" ] || fail "the B-tree digest saw $KEYS keys for $LIVE live records"
REBUILDS3=$(grep -c "derived indexes rebuilt" copy.log)
[ "$REBUILDS3" = "$REBUILDS2" ] || fail "a rebuild ran after the fault was handled ($REBUILDS2 -> $REBUILDS3): $(tail -5 copy.log)"
grep -o '"derived": {[^}]*}' copy.status.json | head -1 | sed 's/^/  status.json: /'
"$GX" torn --config copy.conf | sed 's/^/  /'
echo
echo "OK: object by object, one automatic folder rebuild after the fault, and the same indexes on both nodes"
exit 0
