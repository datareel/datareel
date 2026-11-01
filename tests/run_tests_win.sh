#!/bin/bash
# DataReel 7 test script for Windows (Git Bash), for a build made with
# Visual Studio (CMake). Runs the same checks as run_tests.sh against the
# built programs. Windows has no python3 by default, so the SMTP, POP3
# and FTPS checks need tests/mock_servers.py running elsewhere and its
# ports (MOCK_PORT+0 to +7) reachable on this machine's localhost, for
# example through "ssh -R" tunnels from a Linux machine. Leave MOCK_PORT
# unset to skip them. The certificates come from CERT_DIR (ca.pem,
# server.pem, server.key with localhost, 127.0.0.1 and ::1 in the SAN),
# made with openssl the way run_tests.sh makes them.
#
# usage: tests/run_tests_win.sh BUILD_DIR CONFIG CERT_DIR [MOCK_PORT]
#   CONFIG is Release or Debug (the Visual Studio output folder)
BUILD=${1:?usage: run_tests_win.sh BUILD_DIR CONFIG CERT_DIR [MOCK_PORT]}
CFG=${2:?CONFIG}
CERTS=${3:?CERT_DIR}
MOCK=${4:-}
E="$BUILD/examples/$CFG"; [ -d "$E" ] || E="$BUILD/examples"
T="$BUILD/tests/$CFG"; [ -d "$T" ] || T="$BUILD/tests"
WORK=$(mktemp -d)
PORT=$((20000 + RANDOM % 9000))
PASS=0; FAIL=0; PIDS=()
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
wait_port() { sleep 2; }
cp "$CERTS"/ca.pem "$CERTS"/server.pem "$CERTS"/server.key "$WORK"/
cd "$WORK"

# Database examples, each in its own folder
for p in db_simple db_database db_btree db_btree_cached db_btree_node \
	 db_database_cache db_defrag db_pod db_infohog \
	 config_file logfile thread_pool; do
  mkdir -p "$WORK/run_$p" && cd "$WORK/run_$p" && check $p sh -c "'$E/$p' < /dev/null"
done
cd "$WORK/run_db_infohog" && check db_infohog_reopen sh -c "'$E/db_infohog' < /dev/null"
mkdir -p "$WORK/run_ai_db" && cd "$WORK/run_ai_db" &&
  "$E/ai_record_store" > /dev/null 2>&1 && "$E/ai_indexed_records" > /dev/null 2>&1
check ai_record_store "$E/ai_record_store"
check ai_indexed_records sh -c "'$E/ai_indexed_records' | tr -d '\r\n' | grep -q 'Found Acme Tools'"
mkdir -p "$WORK/run_damaged" && cd "$WORK/run_damaged" && check damaged_files "$T/damaged_files" 300 7
mkdir -p "$WORK/run_journal" && cd "$WORK/run_journal" && check journal_test "$T/journal_test" 20 7
for t in stream_security_test proto_security_test support_security_test; do
  mkdir -p "$WORK/run_$t" && cd "$WORK/run_$t" && check $t "$T/$t"
done
cd "$WORK"

# TLS echo server and client, IPv4 and IPv6, certificate checks
"$E/tls_echo_server" $PORT server.pem server.key > echo_server.log 2>&1 &
PIDS+=($!); wait_port
check tls_echo sh -c "printf 'hello\n' | '$E/tls_echo_client' localhost $PORT --ca ca.pem | tr -d '\r' | grep -qx hello"
check tls_echo_ipv6 sh -c "printf 'six\n' | '$E/tls_echo_client' ::1 $PORT --ca ca.pem | tr -d '\r' | grep -qx six"
check tls_echo_ipv4 sh -c "printf 'four\n' | '$E/tls_echo_client' 127.0.0.1 $PORT --ca ca.pem | tr -d '\r' | grep -qx four"
check tls_untrusted_refused sh -c "! printf 'x\n' | '$E/tls_echo_client' localhost $PORT"
check tls_wrong_host_refused sh -c "! printf 'x\n' | '$E/tls_echo_client' $(hostname) $PORT --ca ca.pem"
printf 'shutdown\n' | "$E/tls_echo_client" localhost $PORT --ca ca.pem > /dev/null 2>&1

P5=$((PORT+3))
"$E/ai_tls_server" $P5 server.pem server.key > ai_tls_server.log 2>&1 &
PIDS+=($!); wait_port
check ai_tls_client sh -c "'$E/ai_tls_client' localhost $P5 ca.pem | grep -q 'got \"HELLO\"'"
printf 'stop\n' | "$E/tls_echo_client" localhost $P5 --ca ca.pem > /dev/null 2>&1

P2=$((PORT+1)); mkdir -p www/sub
echo "<h1>DataReel 7</h1>" > www/index.html
head -c 200000 /dev/urandom > www/sub/data.bin
"$E/https_server" $P2 server.pem server.key www > https_server.log 2>&1 &
PIDS+=($!); wait_port
check https_get sh -c "'$E/https_get' https://localhost:$P2/ --ca ca.pem | grep -q 'DataReel 7'"
check https_download sh -c "'$E/https_get' https://localhost:$P2/sub/data.bin data.out --ca ca.pem && cmp data.out www/sub/data.bin"
check https_traversal_refused sh -c "'$E/https_get' 'https://localhost:$P2/%2e%2e/%2e%2e/windows/win.ini' --ca ca.pem | head -1 | grep -q 403"
check http_refused sh -c "! '$E/https_get' http://localhost:$P2/"
check ai_https_fetch sh -c "'$E/ai_https_fetch' https://localhost:$P2/ ca.pem | grep -q 'POST -> 405'"

P3=$((PORT+2))
"$E/stream_server" $P3 server.pem server.key blocks.gxd --allow-remote-shutdown > stream_server.log 2>&1 &
PIDS+=($!); wait_port
check stream_blocks "$E/stream_client" localhost $P3 --ca ca.pem
check stream_blocks_ipv6 "$E/stream_client" ::1 $P3 --ca ca.pem --stop

if [ -n "$MOCK" ]; then
  P4=$MOCK
  echo "Test body line" > body.txt
  check smtp_implicit_tls sh -c "'$E/smtp_send' localhost $P4 test secret a@example.com b@example.com Hello --ca ca.pem < body.txt"
  check smtp_starttls sh -c "'$E/smtp_send' localhost $((P4+1)) test secret a@example.com b@example.com Hi --starttls --ca ca.pem < body.txt"
  check ai_send_mail "$E/ai_send_mail" localhost $((P4+1)) test secret a@example.com b@example.com ca.pem
  check smtp_no_starttls_refused sh -c "! '$E/smtp_send' localhost $((P4+6)) test secret a@example.com b@example.com Hi --starttls --ca ca.pem < body.txt"
  check pop3_implicit_tls sh -c "'$E/pop3_list' localhost $((P4+2)) test secret -r 1 --ca ca.pem | grep -q '^.dot line'"
  check pop3_stls sh -c "'$E/pop3_list' localhost $((P4+3)) test secret --starttls --ca ca.pem | grep -q '2 messages'"
  head -c 150000 /dev/urandom > upload.bin
  check ftps_put sh -c "'$E/ftps_client' localhost $((P4+5)) test secret put upload.bin up.bin --ca ca.pem"
  check ftps_list sh -c "'$E/ftps_client' localhost $((P4+5)) test secret list --ca ca.pem | grep -q up.bin"
  check ftps_get sh -c "'$E/ftps_client' localhost $((P4+4)) test secret get up.bin down.bin --implicit --ca ca.pem && cmp down.bin upload.bin"
else
  echo "SKIP SMTP, POP3 and FTPS checks (no MOCK_PORT)"
fi

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
