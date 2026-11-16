#!/bin/bash
# DataReel 7 test script for Linux. Builds the library and examples with
# CMake, then runs the database examples, the TLS clients and servers
# against each other, and the mail and FTPS clients against the mock
# servers in mock_servers.py. Needs cmake, g++, openssl and python3.
#
# usage: tests/run_tests.sh [build_dir]      (default: ./cmake-build)
# Extra cmake options can be passed in CMAKE_OPTS, for example:
#   CMAKE_OPTS="-DDATAREEL_64BIT_DATABASE=ON" tests/run_tests.sh

SRC=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${1:-$SRC/cmake-build}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/datareel-tests.XXXXXX")
# Below the Linux ephemeral port range (32768 and up)
PORT=$((10000 + RANDOM % 20000))
PASS=0
FAIL=0
PIDS=()

cleanup() {
  for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null; done
  wait 2>/dev/null
  if [ $FAIL -eq 0 ]; then rm -rf "$WORK"; else echo "Test files kept in $WORK"; fi
}
trap cleanup EXIT

check() { # check "name" command...
  local name=$1; shift
  if "$@" > "$WORK/$name.log" 2>&1; then
    echo "PASS $name"; PASS=$((PASS+1))
  else
    echo "FAIL $name (see $WORK/$name.log)"; FAIL=$((FAIL+1))
  fi
}

expect() { # expect "name" "text" file : the file must contain the text
  if grep -q -- "$2" "$3"; then echo "PASS $1"; PASS=$((PASS+1))
  else echo "FAIL $1: \"$2\" not in $3"; FAIL=$((FAIL+1)); fi
}

wait_port() {
  for i in $(seq 50); do
    (exec 3<>/dev/tcp/127.0.0.1/$1) 2>/dev/null && return 0
    sleep 0.1
  done
  return 1
}

echo "Building in $BUILD"
cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release $CMAKE_OPTS > "$WORK/cmake.log" 2>&1 &&
cmake --build "$BUILD" -j"$(nproc)" >> "$WORK/cmake.log" 2>&1 || {
  echo "Build failed, see $WORK/cmake.log"; FAIL=1; exit 1; }
E=$BUILD/examples

# Test certificates: a CA and a localhost server certificate
cd "$WORK"
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.pem -days 2 \
  -subj "/CN=DataReel Test CA" > /dev/null 2>&1
openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr \
  -subj "/CN=localhost" > /dev/null 2>&1
printf "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1\n" > san.ext
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial \
  -out server.pem -days 2 -extfile san.ext > /dev/null 2>&1
# 11/01/2026: Client certificates for the gxStream server (mutual TLS):
# a client CA and two clients it issued, one in the server's allow list
# (client_ok) and one not (client_bad)
openssl req -x509 -newkey rsa:2048 -nodes -keyout client_ca.key -out client_ca.pem \
  -days 2 -subj "/CN=DataReel Test Client CA" > /dev/null 2>&1
for c in ok bad; do
  openssl req -newkey rsa:2048 -nodes -keyout client_$c.key -out client_$c.csr \
    -subj "/CN=stream-client-$c" > /dev/null 2>&1
  openssl x509 -req -in client_$c.csr -CA client_ca.pem -CAkey client_ca.key \
    -CAcreateserial -out client_$c.pem -days 2 > /dev/null 2>&1
done
CLIENT_FP=$(openssl x509 -in client_ok.pem -noout -fingerprint -sha256 | sed 's/.*=//')

# Database examples (they pause for Enter, so give them empty input)
# Each one runs in its own directory because several use the same file names
for p in db_simple db_database db_btree db_btree_cached db_btree_node \
	 db_database_cache db_defrag db_pod db_infohog \
	 config_file logfile thread_pool; do
  mkdir -p "$WORK/run_$p" && cd "$WORK/run_$p" && check $p sh -c "$E/$p < /dev/null"
done
cd "$WORK/run_db_infohog" && check db_infohog_reopen sh -c "$E/db_infohog < /dev/null"
# Object database patterns and utilities (examples/README.txt)
for p in trading_sessions string_utils day_events; do
  mkdir -p "$WORK/run_$p" && cd "$WORK/run_$p" && check $p $E/$p
done
mkdir -p "$WORK/run_typed_objects" && cd "$WORK/run_typed_objects" &&
  check typed_objects $E/typed_objects "$WORK/server.pem" "$WORK/server.key" --ca "$WORK/ca.pem"
# 11/14/2026 (7.4): replication with gxrepl, object by object, a simulated fault and the automatic folder rebuild
mkdir -p "$WORK/run_replication" && cd "$WORK/run_replication" &&
  check replication_walkthrough "$SRC/examples/oodb/replication_walkthrough.sh" "$E" "$WORK/run_replication/w"
# 11/15/2026 (7.4): the same with an unchanged gxrepl and the extractor as a .so (derived_extractor = path.so)
mkdir -p "$WORK/run_replication_so" && cd "$WORK/run_replication_so" &&
  check replication_walkthrough_so env EXTRACTOR="$E/example_records.so" GXREPL="$BUILD/utils/gxrepl" \
    "$SRC/examples/oodb/replication_walkthrough.sh" "$E" "$WORK/run_replication_so/w"
# 11/15/2026 (7.4): gxrepl digest --kind btree on keys whose names repeat (two writer runs with the same seed: the
# address breaks the tie). The digest walked with FindNext() and a raw byte compare and skipped keys; it must count
# one key per live record and match the same B-tree rebuilt from the records.
mkdir -p "$WORK/run_digest_repeat" && cd "$WORK/run_digest_repeat" &&
  check gxrepl_digest_repeated_names sh -c "set -e; mkdir -p p; R='$E/replication'
    \$R write p --commits 60 --per 8 > /dev/null; LIVE=\$(\$R write p --commits 80 --per 8 | sed -n 's/.*, \([0-9]*\) live records\$/\1/p')
    \$R digest p > p.d; grep -q \"btree [0-9a-f]* \$LIVE keys\$\" p.d
    mkdir r; cp -a p/data.gxd p/by_name.btx p/vectors.vix p/commit.bin r/; \$R rebuild r > /dev/null; \$R digest r > r.d; cmp p.d r.d
    '$BUILD/utils/gxrepl' digest p/by_name.btx | grep -q \" \$LIVE \""
# 11/15/2026 (7.4, review of c59a264): gxrepl digest walks B-trees of node order 3 (made 5: 4 keys a node) and 16
mkdir -p "$WORK/run_digest_orders" && cd "$WORK/run_digest_orders" &&
  check gxrepl_digest_node_orders sh -c "set -e; for o in 3 16; do mkdir -p o\$o
    '$BUILD/utils/gxrepl_testwriter' btree o\$o \$o 1000 > /dev/null
    '$BUILD/utils/gxrepl' digest o\$o/keys.btx | grep -q ' 1000 '; done"
# 11/15/2026 (7.4): the old 4-byte layout (gxdbase.h). tests/data/old_layout: files written by the 64-bit and 32-bit
# FAU Windows builds of 7.4 beta 3 (replication_copy write --commits 40 --per 6). This build reads them as Windows
# did (the digests Windows printed), writes them byte for byte with GXD_CREATE_LAYOUT=4, keeps an old file in its
# layout when it writes more to it, and gxd_upgrade makes a canonical copy.
OL="$SRC/tests/data/old_layout"
if grep -q "DATAREEL_64BIT_DATABASE:BOOL=ON" "$BUILD/CMakeCache.txt" 2>/dev/null; then
  mkdir -p "$WORK/run_old_layout" && cd "$WORK/run_old_layout" &&
    check old_layout_read sh -c "set -e; cp -r '$OL/win64' w; '$E/replication_copy' digest w > d.txt
      cmp d.txt '$OL/win64/replication_copy_digest.txt'"
  mkdir -p "$WORK/run_old_layout_write" && cd "$WORK/run_old_layout_write" &&
    check old_layout_write sh -c "set -e; mkdir -p w; GXD_CREATE_LAYOUT=4 '$E/replication_copy' write w --commits 40 --per 6 > /dev/null
      for f in data.gxd by_name.btx vectors.vix; do cmp w/\$f '$OL/win64/'\$f; done"
  mkdir -p "$WORK/run_old_layout_rw" && cd "$WORK/run_old_layout_rw" &&
    check old_layout_read_write sh -c "set -e; cp -r '$OL/win64' w; '$E/replication_copy' write w --commits 20 --per 6 --seed 5 > /dev/null
      python3 -c \"import sys; d=open('w/data.gxd','rb').read(24); sys.exit(0 if int.from_bytes(d[16:24],'little') == 92 else 1)\"
      '$E/replication_copy' digest w > a.txt; cp -r w r; rm r/by_name.btx r/vectors.vix; '$E/replication_copy' rebuild r > /dev/null
      '$E/replication_copy' digest r > b.txt; cmp a.txt b.txt"
else
  mkdir -p "$WORK/run_old_layout" && cd "$WORK/run_old_layout" &&
    check old_layout_read sh -c "set -e; cp -r '$OL/win32' w
      '$BUILD/utils/gxrepl' digest w/data.gxd --kind objects | grep -q '^cfb8e9e470ce02d7 155 '
      '$BUILD/utils/gxrepl' digest w/by_name.btx --kind btree | grep -q '^67917c9acb47dd07 155 '
      '$BUILD/utils/vix_rebuild' --digest w/vectors.vix | grep -q '^logical bcd02d0e141e67e8 155 '"
fi
mkdir -p "$WORK/run_old_layout_upgrade" && cd "$WORK/run_old_layout_upgrade" &&
  check old_layout_upgrade sh -c "set -e; W='$OL/win64'; grep -q 'DATAREEL_64BIT_DATABASE:BOOL=ON' '$BUILD/CMakeCache.txt' || W='$OL/win32'
    cp -r \$W w; '$BUILD/utils/gxd_upgrade' w/data.gxd up.gxd | grep -q 'byte check ok'
    a=\$('$BUILD/utils/gxrepl' digest w/data.gxd --kind objects | cut -d' ' -f1-2); b=\$('$BUILD/utils/gxrepl' digest up.gxd --kind objects | cut -d' ' -f1-2)
    [ \"\$a\" = \"\$b\" ]; test -s up.gxd.map
    if '$BUILD/utils/gxd_upgrade' w/by_name.btx x.btx; then exit 1; fi
    '$BUILD/utils/gxd_upgrade' up.gxd again.gxd | grep -q 'canonical layout already'; test ! -e again.gxd"
# 11/14/2026 (7.4): the folder copy example (replication_copy_walkthrough.cmd on Windows), the same steps with cp
mkdir -p "$WORK/run_replication_copy" && cd "$WORK/run_replication_copy" &&
  check replication_copy sh -c "set -e; mkdir -p p c; R='$E/replication_copy'
    \$R write p --commits 200 --per 8 > /dev/null; cp -a p/data.gxd p/by_name.btx p/vectors.vix c/
    \$R digest p > p.d; \$R digest c > c.d; cmp p.d c.d
    \$R write p --commits 100 --per 8 --seed 7 > /dev/null; cp -a p/data.gxd p/by_name.btx p/vectors.vix c/
    \$R digest p > p.d; \$R digest c > c.d; cmp p.d c.d
    rm c/by_name.btx c/vectors.vix; if \$R digest c > /dev/null; then exit 1; fi; \$R rebuild c > /dev/null
    \$R digest c > c.d; cmp p.d c.d"
# 11/15/2026 (7.4): the vector examples
for p in vector_basics vector_records vector_days; do
  mkdir -p "$WORK/run_$p" && cd "$WORK/run_$p" && check $p "$E/$p" "$WORK/run_$p/w"
done
mkdir -p "$WORK/run_vector_search" && cd "$WORK/run_vector_search" &&
  check vector_search_walkthrough "$SRC/examples/oodb/vector_search_walkthrough.sh" "$E" "$WORK/run_vector_search/w"
mkdir -p "$WORK/run_journal_recovery" && cd "$WORK/run_journal_recovery" &&
  check journal_recovery $E/journal_recovery 5
# Sample programs from the AI guides (docs/ai/samples); run twice to
# reopen the files
mkdir -p "$WORK/run_ai_db" && cd "$WORK/run_ai_db" &&
  $E/ai_record_store > /dev/null 2>&1 && $E/ai_indexed_records > /dev/null 2>&1
check ai_record_store $E/ai_record_store
mkdir -p "$WORK/run_ai_vec" && (cd "$WORK/run_ai_vec" && "$E/ai_vector_search" > /dev/null 2>&1) &&
  check ai_vector_search sh -c "cd '$WORK/run_ai_vec' && '$E/ai_vector_search' | grep -q 'on 2026-11-11 only: 2 note'"
check ai_record_store_walk sh -c "[ \$(grep -c qty $WORK/ai_record_store.log) -eq 4 ]"
check ai_indexed_records sh -c "$E/ai_indexed_records | tr -d '\n' | grep -q 'Found Acme Tools.*Acme Tools, 40 orders  Baker Hardware, 7 orders  Wilson Supply, 12 orders\$'"
mkdir -p "$WORK/run_damaged" && cd "$WORK/run_damaged" && 
  check damaged_files "$BUILD/tests/damaged_files" 300 7
mkdir -p "$WORK/run_journal" && cd "$WORK/run_journal" &&
  check journal_test "$BUILD/tests/journal_test" 20 7
# Vector index (gxvindex.h, 7.4): kernels, exact and graph search,
# Merge, Repair, concurrent searches and 20 crash rounds
mkdir -p "$WORK/run_vix" && cd "$WORK/run_vix" &&
  check vix_test "$BUILD/utils/vix_test"
mkdir -p "$WORK/run_derived_apply" && cd "$WORK/run_derived_apply" &&
  check derived_apply_test "$BUILD/tests/derived_apply_test"
for t in stream_security_test proto_security_test support_security_test; do
  mkdir -p "$WORK/run_$t" && cd "$WORK/run_$t" && check $t "$BUILD/tests/$t"
done
cd "$WORK"

# TLS echo server and client, IPv4 and IPv6, certificate checks
$E/tls_echo_server $PORT server.pem server.key > echo_server.log 2>&1 &
PIDS+=($!); wait_port $PORT
check tls_echo sh -c "printf 'hello\n' | $E/tls_echo_client localhost $PORT --ca ca.pem | grep -qx hello"
check tls_echo_ipv6 sh -c "printf 'six\n' | $E/tls_echo_client ::1 $PORT --ca ca.pem | grep -qx six"
check tls_echo_ipv4 sh -c "printf 'four\n' | $E/tls_echo_client 127.0.0.1 $PORT --ca ca.pem | grep -qx four"
check tls_untrusted_refused sh -c "! printf 'x\n' | $E/tls_echo_client localhost $PORT"
check tls_wrong_host_refused sh -c "! printf 'x\n' | $E/tls_echo_client $(hostname -f) $PORT --ca ca.pem"
printf 'shutdown\n' | $E/tls_echo_client localhost $PORT --ca ca.pem > /dev/null 2>&1

# AI guide TLS samples
P5=$((PORT+3))
$E/ai_tls_server $P5 server.pem server.key > ai_tls_server.log 2>&1 &
PIDS+=($!); wait_port $P5
check ai_tls_client sh -c "$E/ai_tls_client localhost $P5 ca.pem | grep -q 'got \"HELLO\"'"
printf 'stop\n' | $E/tls_echo_client localhost $P5 --ca ca.pem > /dev/null 2>&1

# HTTPS server and client
P2=$((PORT+1)); mkdir -p www/sub
echo "<h1>DataReel 7</h1>" > www/index.html
head -c 200000 /dev/urandom > www/sub/data.bin
$E/https_server $P2 server.pem server.key www > https_server.log 2>&1 &
PIDS+=($!); wait_port $P2
check https_get sh -c "$E/https_get https://localhost:$P2/ --ca ca.pem | grep -q 'DataReel 7'"
check https_download sh -c "$E/https_get https://localhost:$P2/sub/data.bin data.out --ca ca.pem && cmp data.out www/sub/data.bin"
check https_traversal_refused sh -c "$E/https_get 'https://localhost:$P2/%2e%2e/%2e%2e/etc/passwd' --ca ca.pem | head -1 | grep -q 403"
check http_refused sh -c "! $E/https_get http://localhost:$P2/"
check ai_https_fetch sh -c "$E/ai_https_fetch https://localhost:$P2/ ca.pem | grep -q 'POST -> 405'"

# gxStream database blocks over TLS, with client certificates
P3=$((PORT+2))
CLIENT_OK="--cert client_ok.pem --key client_ok.key"
$E/stream_server $P3 server.pem server.key blocks.gxd --allow-remote-shutdown \
  --client-ca client_ca.pem --allow-fingerprint "$CLIENT_FP" > stream_server.log 2>&1 &
PIDS+=($!); wait_port $P3
check stream_blocks $E/stream_client localhost $P3 --ca ca.pem $CLIENT_OK
expect stream_client_identity_logged "authenticated: CN=stream-client-ok SHA-256=$CLIENT_FP" stream_server.log
check stream_no_client_cert_refused sh -c "! $E/stream_client localhost $P3 --ca ca.pem"
check stream_wrong_client_cert_refused sh -c "! $E/stream_client localhost $P3 --ca ca.pem --cert client_bad.pem --key client_bad.key"
check stream_blocks_ipv6 $E/stream_client ::1 $P3 --ca ca.pem $CLIENT_OK --stop
# A stream server without client certificate settings must not start,
# unless --open asks for an open server
P6=$((PORT+4))
check stream_server_refuses_without_auth_config sh -c "timeout 10 $E/stream_server $P6 server.pem server.key noauth.gxd > noauth.out 2>&1; rc=\$?; cat noauth.out; [ \$rc -ne 0 ] && [ \$rc -ne 124 ] && grep -q 'requires client certificates' noauth.out"
$E/stream_server $P6 server.pem server.key open.gxd --allow-remote-shutdown --open > stream_open_server.log 2>&1 &
PIDS+=($!); wait_port $P6
check stream_open_server $E/stream_client localhost $P6 --ca ca.pem --stop

# SMTP, POP3 and FTPS clients against the mock servers
P4=$((PORT+10))
python3 "$SRC/tests/mock_servers.py" server.pem server.key "$WORK" $P4 > mock.log 2>&1 &
PIDS+=($!); wait_port $((P4+6))
echo "Test body line" > body.txt
check smtp_implicit_tls sh -c "$E/smtp_send localhost $P4 test secret a@example.com b@example.com Hello --ca ca.pem < body.txt"
expect smtp_body "Test body line" mail.txt
check smtp_starttls sh -c "$E/smtp_send localhost $((P4+1)) test secret a@example.com b@example.com Hi --starttls --ca ca.pem < body.txt"
check ai_send_mail $E/ai_send_mail localhost $((P4+1)) test secret a@example.com b@example.com ca.pem
check smtp_no_starttls_refused sh -c "! $E/smtp_send localhost $((P4+6)) test secret a@example.com b@example.com Hi --starttls --ca ca.pem < body.txt"
check pop3_implicit_tls sh -c "$E/pop3_list localhost $((P4+2)) test secret -r 1 --ca ca.pem | grep -q '^.dot line'"
check pop3_stls sh -c "$E/pop3_list localhost $((P4+3)) test secret --starttls --ca ca.pem | grep -q '2 messages'"
head -c 150000 /dev/urandom > upload.bin
check ftps_put sh -c "$E/ftps_client localhost $((P4+5)) test secret put upload.bin up.bin --ca ca.pem"
check ftps_list sh -c "$E/ftps_client localhost $((P4+5)) test secret list --ca ca.pem | grep -q up.bin"
check ftps_get sh -c "$E/ftps_client localhost $((P4+4)) test secret get up.bin down.bin --implicit --ca ca.pem && cmp down.bin upload.bin"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
