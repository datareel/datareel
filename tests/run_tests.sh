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

# Database examples (they pause for Enter, so give them empty input)
# Each one runs in its own directory because several use the same file names
for p in db_simple db_database db_btree db_btree_cached db_btree_node \
	 db_database_cache db_defrag db_pod db_infohog db_rdbms \
	 config_file logfile thread_pool; do
  mkdir -p "$WORK/run_$p" && cd "$WORK/run_$p" && check $p sh -c "$E/$p < /dev/null"
done
cd "$WORK/run_db_infohog" && check db_infohog_reopen sh -c "$E/db_infohog < /dev/null"
expect rdbms_search "Engineering (2)" "$WORK/db_rdbms.log"
# Sample programs from the AI guides (docs/ai/samples); run twice to
# reopen the files
mkdir -p "$WORK/run_ai_db" && cd "$WORK/run_ai_db" &&
  $E/ai_record_store > /dev/null 2>&1 && $E/ai_indexed_records > /dev/null 2>&1
check ai_record_store $E/ai_record_store
check ai_record_store_walk sh -c "[ \$(grep -c qty $WORK/ai_record_store.log) -eq 4 ]"
check ai_indexed_records sh -c "$E/ai_indexed_records | tr -d '\n' | grep -q 'Found Acme Tools.*Acme Tools, 40 orders  Baker Hardware, 7 orders  Wilson Supply, 12 orders\$'"
mkdir -p "$WORK/run_damaged" && cd "$WORK/run_damaged" && 
  check damaged_files "$BUILD/tests/damaged_files" 300 7
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

# gxStream database blocks over TLS
P3=$((PORT+2))
$E/stream_server $P3 server.pem server.key blocks.gxd > stream_server.log 2>&1 &
PIDS+=($!); wait_port $P3
check stream_blocks $E/stream_client localhost $P3 --ca ca.pem
check stream_blocks_ipv6 $E/stream_client ::1 $P3 --ca ca.pem --stop

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
