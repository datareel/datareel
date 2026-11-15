#!/bin/bash
# vector_search_walkthrough.sh - 11/15/2026 (DataReel 7.4): vector searches through the database object server,
# on 127.0.0.1 with throwaway certificates (a private CA, the server's and the client's):
#   1. three day files and their .vixset manifest (vector_search_service make);
#   2. the server (vector_search_service serve), database "notes" with its vectors;
#   3. the client: VECTOR_INFO, a search of all the days, and a search of one day (the other two files skipped
#      by the server from the manifest).
#
#   examples/oodb/vector_search_walkthrough.sh BUILD/examples [WORK_DIR]
#
# Needs the openssl command. WORK_DIR (default: a new folder under ${TMPDIR:-/tmp}) is removed at the end unless
# KEEP=1 (or a check fails). Exit 0 when every check passes.
set -u
BIN=$(cd "${1:?usage: vector_search_walkthrough.sh BUILD/examples [WORK_DIR]}" && pwd)
W=${2:-$(mktemp -d "${TMPDIR:-/tmp}/vector_search.XXXXXX")}
mkdir -p "$W"
APP="$BIN/vector_search_service"
PORT=$(( 20000 + RANDOM % 20000 ))
SRV=""
fail() { echo "FAILED: $*"; [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; KEEP=1; echo "(work folder kept: $W)"; exit 1; }
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; wait 2>/dev/null; [ "${KEEP:-0}" = 1 ] || rm -rf "$W"; }
trap cleanup EXIT

echo "== Throwaway certificates"
mkdir -p "$W/pki" && cd "$W/pki" || fail "no work folder"
EC="-newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes"
openssl req -x509 $EC -keyout ca.key -out ca.pem -days 2 -subj /CN=example-ca 2>/dev/null || fail "openssl"
openssl req $EC -keyout srv.key -out srv.csr -subj /CN=notes-server 2>/dev/null
printf 'subjectAltName=DNS:notes-server,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > srv.ext
openssl x509 -req -in srv.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out srv.pem -days 2 -extfile srv.ext 2>/dev/null
openssl req $EC -keyout cli.key -out cli.csr -subj /CN=notes-client 2>/dev/null
printf 'extendedKeyUsage=clientAuth\n' > cli.ext
openssl x509 -req -in cli.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out cli.pem -days 2 -extfile cli.ext 2>/dev/null
chmod 600 *.key
cd "$W"

echo; echo "== 1. Three day files and the manifest"
"$APP" make "$W/data" || fail "make"
sed 's/^/  /' "$W/data/notes.vixset"

echo; echo "== 2. The server"
"$APP" serve $PORT pki/srv.pem pki/srv.key pki/ca.pem notes-client "$W/data" > server.log 2>&1 & SRV=$!
# Wait for the listen without connecting (a plain TCP probe would be logged as a refused TLS client)
for _ in $(seq 100); do
  if command -v ss >/dev/null; then ss -Hltn "sport = :$PORT" 2>/dev/null | grep -q . && break
  else grep -q "^serving" server.log 2>/dev/null && { sleep 1; break; }; fi
  sleep 0.1
done
cat server.log

echo; echo "== 3. The client"
"$APP" search 127.0.0.1 $PORT notes-server pki/cli.pem pki/cli.key pki/ca.pem > all.txt 2>&1 || fail "search: $(cat all.txt)"
cat all.txt
grep -q "VECTOR_INFO: version 1, dimension 8, element 1, metric 2, 3 file(s), 900 nodes" all.txt || fail "VECTOR_INFO"
[ "$(grep -c '^  id ' all.txt)" = 5 ] || fail "5 results expected"
"$APP" search 127.0.0.1 $PORT notes-server pki/cli.pem pki/cli.key pki/ca.pem 20261113 20261113 > day.txt 2>&1 || fail "search: $(cat day.txt)"
echo; cat day.txt
grep -q "2 file(s) skipped by their day range" day.txt || fail "the other two days were not skipped"
[ "$(grep '^  id ' day.txt | grep -vc 'day 20261113')" = 0 ] || fail "a result outside the day"
echo; echo "OK: VECTOR_INFO, a search of every day and a search of one day (two files skipped)"
exit 0
