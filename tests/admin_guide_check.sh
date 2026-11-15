#!/bin/bash
# admin_guide_check.sh - 11/15/2026 (DataReel 7.4): runs the command blocks of docs/replication_admin_guide.md, as
# printed and in order, on a throwaway local setup: /tmp/repl_demo, 127.0.0.1:12073, the replication example
# (examples/oodb/replication) as the writer. Linux only (the guide's Windows block: replication_copy_walkthrough.cmd).
#
#   tests/admin_guide_check.sh BUILD        (BUILD holds utils/gxrepl, utils/vix_rebuild, examples/replication)
#
# Exit 0 when every command exits as the guide says and every digest pair agrees. /tmp/repl_demo is removed at the
# end unless KEEP=1 or a check fails; an existing /tmp/repl_demo is never reused.
set -u
B=$(cd "${1:?usage: admin_guide_check.sh BUILD}" && pwd)
export PATH=$B/utils:$B/examples:$PATH
[ -e /tmp/repl_demo ] && { echo "/tmp/repl_demo exists, not reused"; exit 1; }
FAILS=0
bad() { echo "FAILED: $*"; FAILS=$((FAILS+1)); }
same() { [ "$($2 | awk '{print $1, $2, $3}' | sed 's|/tmp/repl_demo/[^ ]*||')" = "$($3 | awk '{print $1, $2, $3}' | sed 's|/tmp/repl_demo/[^ ]*||')" ] || bad "$1 differ"; }
pids=()
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT
r() { echo "\$ $*"; "$@"; local rc=$?; echo "[rc=$rc]"; [ $rc = 0 ] || bad "$* exited $rc"; return $rc; }
blk() { echo; echo "######## $*"; }
waitlag() {  # wait until the copy is at the primary's commit generation
  for _ in $(seq 240); do a=$(od -An -t u8 -j 8 -N 8 /tmp/repl_demo/primary/db/commit.bin 2>/dev/null|tr -d ' '); b=$(od -An -t u8 -j 8 -N 8 /tmp/repl_demo/copy/db/commit.bin 2>/dev/null|tr -d ' '); [ -n "$a" ] && [ "$a" = "$b" ] && { echo "(copy at generation $a)"; return 0; }; sleep 0.5; done; echo "(copy NOT caught up: $a vs $b)"; return 1; }

blk "2.1 Certificates"
mkdir -p /tmp/repl_demo/pki && cd /tmp/repl_demo/pki
r openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout ca.key -out ca.pem -days 30 -subj /CN=demo-ca 2>/dev/null
r openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout copy.key -out copy.csr -subj /CN=copy 2>/dev/null
printf 'subjectAltName=DNS:copy,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > copy.ext
r openssl x509 -req -in copy.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out copy.pem -days 30 -extfile copy.ext 2>/dev/null
r openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout primary.key -out primary.csr -subj /CN=primary 2>/dev/null
printf 'extendedKeyUsage=clientAuth\n' > primary.ext
r openssl x509 -req -in primary.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out primary.pem -days 30 -extfile primary.ext 2>/dev/null
chmod 600 *.key
r gxrepl fingerprint primary.pem

blk "2.2 Configs"
cd /tmp/repl_demo
mkdir -p primary/db copy/db
cat > primary.conf <<'C'
node = primary
role = client
connect = 127.0.0.1:12073
server_name = copy
cert = /tmp/repl_demo/pki/primary.pem
key = /tmp/repl_demo/pki/primary.key
ca = /tmp/repl_demo/pki/ca.pem
status_file = /tmp/repl_demo/primary.status.json

[set db]
root = /tmp/repl_demo/primary/db
direction = push
commit_block = commit.bin@8
include = commit.bin
derived = by_*.btx, vectors.vix
deletes = yes
C
cat > copy.conf <<C
node = copy
role = server
listen = 127.0.0.1:12073
allow_cn = primary
cert = /tmp/repl_demo/pki/copy.pem
key = /tmp/repl_demo/pki/copy.key
ca = /tmp/repl_demo/pki/ca.pem
status_file = /tmp/repl_demo/copy.status.json

[set db]
root = /tmp/repl_demo/copy/db
accept = target
commit_block = commit.bin@8
include = commit.bin
derived = by_*.btx, vectors.vix
derived_rebuild = $B/examples/replication rebuild {dir} {lsn}
derived_delay = 30
C
echo "(demo data: replication write, 60 commits)"
replication write /tmp/repl_demo/primary/db --commits 60 | tail -1 || bad "the writer"

blk "2.3 Check and start"
r gxrepl check --config /tmp/repl_demo/copy.conf
r gxrepl check --config /tmp/repl_demo/primary.conf
echo "\$ gxrepl serve --config /tmp/repl_demo/copy.conf &"
gxrepl serve --config /tmp/repl_demo/copy.conf > copy.log 2>&1 & pids+=($!)
for _ in $(seq 100); do ss -Hltn "sport = :12073" | grep -q . && break; sleep 0.1; done
r gxrepl once --config /tmp/repl_demo/primary.conf
echo "\$ gxrepl run --config /tmp/repl_demo/primary.conf &"
gxrepl run --config /tmp/repl_demo/primary.conf > primary.log 2>&1 & RUN=$!; pids+=($RUN)
echo "(demo data: a live writer, 80 more commits)"
replication write /tmp/repl_demo/primary/db --commits 80 --per 8 --sleep 100 | tail -1
waitlag
sleep 40   # derived_delay 30: let the copy's rebuild run once the folder is quiet

blk "3. Watching it"
r gxrepl status --config /tmp/repl_demo/primary.conf | head -c 600; echo
r gxrepl derived status --config /tmp/repl_demo/copy.conf
r gxrepl torn --config /tmp/repl_demo/copy.conf
r ls /tmp/repl_demo/primary/db/.gxrepl/log
r gxrepl shiplog /tmp/repl_demo/primary/db/.gxrepl/log/writer.jnl | tail -4
r gxrepl digest /tmp/repl_demo/copy/db/by_name.btx --kind btree
r gxrepl digest /tmp/repl_demo/primary/db/by_name.btx --kind btree
r vix_rebuild --digest /tmp/repl_demo/copy/db/vectors.vix
r vix_rebuild --digest /tmp/repl_demo/primary/db/vectors.vix
same "the B-tree digests" "gxrepl digest /tmp/repl_demo/copy/db/by_name.btx --kind btree" "gxrepl digest /tmp/repl_demo/primary/db/by_name.btx --kind btree"
same "the vector digests" "vix_rebuild --digest /tmp/repl_demo/copy/db/vectors.vix" "vix_rebuild --digest /tmp/repl_demo/primary/db/vectors.vix"

blk "4.1 Before a node takes over (the primary's replicator stopped: the primary is gone)"
kill $RUN; wait $RUN 2>/dev/null
r gxrepl promote --config /tmp/repl_demo/copy.conf
r gxrepl promote --config /tmp/repl_demo/copy.conf --apply
echo "(the copy takes over: its writer, 20 commits)"
replication write /tmp/repl_demo/copy/db --commits 20 | tail -1

blk "4.2 Failback (the copy serves with --accept-both, the old primary pulls)"
kill ${pids[0]}; wait ${pids[0]} 2>/dev/null
echo "\$ gxrepl serve --config /tmp/repl_demo/copy.conf --accept-both &"
gxrepl serve --config /tmp/repl_demo/copy.conf --accept-both > copy2.log 2>&1 & pids+=($!)
for _ in $(seq 100); do ss -Hltn "sport = :12073" | grep -q . && break; sleep 0.1; done
r gxrepl once --config /tmp/repl_demo/primary.conf --pull-all
r gxrepl torn --config /tmp/repl_demo/primary.conf --copied-only
r replication rebuild /tmp/repl_demo/primary/db
r gxrepl torn --config /tmp/repl_demo/primary.conf --copied-only
echo "(the data on both: replication digest)"
replication digest /tmp/repl_demo/primary/db > p.dig; replication digest /tmp/repl_demo/copy/db > c.dig
cat p.dig c.dig; cmp -s p.dig c.dig && echo "(data the same)" || bad "the nodes differ after the failback"
cleanup; pids=()

blk "8. A plain file copy (one machine: local paths in place of copy-host:)"
mkdir -p /tmp/repl_demo/copy2/db
r rsync -a --delete -n --exclude '*.jnl' --exclude .gxrepl/ /tmp/repl_demo/primary/db/ /tmp/repl_demo/copy2/db/ 
r rsync -a --delete    --exclude '*.jnl' --exclude .gxrepl/ /tmp/repl_demo/primary/db/ /tmp/repl_demo/copy2/db/
r gxrepl digest /tmp/repl_demo/primary/db/data.gxd --kind objects
r gxrepl digest /tmp/repl_demo/copy2/db/data.gxd --kind objects
same "the data digests after the copy" "gxrepl digest /tmp/repl_demo/primary/db/data.gxd --kind objects" "gxrepl digest /tmp/repl_demo/copy2/db/data.gxd --kind objects"
echo; echo "######## logs"; tail -5 copy.log; tail -5 primary.log; tail -5 copy2.log
if [ $FAILS = 0 ]; then echo "OK: every block of the guide ran as printed"; [ "${KEEP:-0}" = 1 ] || { cd /; rm -rf /tmp/repl_demo; }; exit 0; fi
echo "$FAILS check(s) failed; /tmp/repl_demo kept"; exit 1
