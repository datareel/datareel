#!/bin/bash
# run_vector_client.sh TOOLS_DIR PROG [WORK_DIR] - DataReel 6.4: a
# DataReel 6.x program (PROG, examples_socket/vector_client) searching
# the vectors the DataReel 7.x database object server serves.
#   TOOLS_DIR: the folder with gxrepl (the 6.x CMake build with
#              DATAREEL7_FEATURES_DIR: <build>/dr7inner/tools)
#   PROG:      the vector_client program
#   WORK_DIR:  a new folder for the test (default: a temporary one)
# Linux only (the 7.x object server runs in gxrepl, POSIX only). Uses
# 127.0.0.1 and throwaway certificates made with the openssl program.
set -u
GX=$(cd "${1:?usage: run_vector_client.sh TOOLS_DIR PROG [WORK_DIR]}" && pwd)/gxrepl
PROG=$(cd "$(dirname "${2:?usage: run_vector_client.sh TOOLS_DIR PROG [WORK_DIR]}")" && pwd)/$(basename "$2")
W=${3:-$(mktemp -d "${TMPDIR:-/tmp}/vecsrv.XXXXXX")}
mkdir -p "$W" && W=$(cd "$W" && pwd)
PORT=$(( 20000 + RANDOM % 20000 ))
pass=0; fail=0; pids=()
result() { if [ "$1" -eq 0 ]; then pass=$((pass+1)); echo "PASS $2"; else fail=$((fail+1)); echo "FAIL $2"; fi; }
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT

mkdir -p "$W/pki" "$W/vec" "$W/dummy" && cd "$W/pki" || exit 2
EC="-newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes"
openssl req -x509 $EC -keyout ca.key -out ca.pem -days 2 -subj /CN=vec-ca 2>/dev/null
openssl req $EC -keyout server.key -out server.csr -subj /CN=vecserver 2>/dev/null
printf 'subjectAltName=DNS:vecserver,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > server.ext
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out server.pem -days 2 -extfile server.ext 2>/dev/null
for c in client6 other; do
  openssl req $EC -keyout $c.key -out $c.csr -subj /CN=$c 2>/dev/null
  printf 'extendedKeyUsage=clientAuth\n' > $c.ext
  openssl x509 -req -in $c.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out $c.pem -days 2 -extfile $c.ext 2>/dev/null
done
openssl req -x509 $EC -keyout rogue.key -out rogue.pem -days 2 -subj /CN=client6 2>/dev/null
chmod 600 *.key
cd "$W" || exit 2

"$PROG" make "$W/vec" > make.log 2>&1
result $? "6.x program made a vector set (3 day files) and news.gxd"

cat > server.conf <<EOF
node = vecserver
role = server
listen = 127.0.0.1:$PORT
allow_cn = client6
cert = $W/pki/server.pem
key = $W/pki/server.key
ca = $W/pki/ca.pem
status_file = $W/server.status.json
[db news]
path = $W/vec/news.gxd
access = read
vectors = $W/vec/news.vixset
[set dummy]
root = $W/dummy
accept = target
EOF
"$GX" serve --config server.conf > server.log 2>&1 & pids+=($!)
up=1
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && { up=0; break; }; sleep 0.1; done
result $up "7.x object server (gxrepl serve) listening"

C="$W/pki"
"$PROG" check 127.0.0.1 $PORT vecserver $C/client6.pem $C/client6.key $C/ca.pem news "$W/vec" > check.log 2>&1
rc=$?; sed 's/^/  /' check.log; result $rc "6.x client: VECTOR_INFO and VECTOR_SEARCH equal the local searches"
"$PROG" refused 127.0.0.1 $PORT vecserver $C/other.pem $C/other.key $C/ca.pem news > r1.log 2>&1
result $? "a client certificate not in allow_cn is refused ($(cat r1.log))"
"$PROG" refused 127.0.0.1 $PORT vecserver $C/rogue.pem $C/rogue.key $C/ca.pem news > r2.log 2>&1
result $? "a self-signed certificate with the allowed CN is refused ($(cat r2.log))"
"$PROG" refused 127.0.0.1 $PORT wrongname $C/client6.pem $C/client6.key $C/ca.pem news > r3.log 2>&1
result $? "a server certificate for another name is refused ($(cat r3.log))"
"$PROG" refused 127.0.0.1 $PORT vecserver $C/client6.pem $C/client6.key $C/ca.pem nosuch > r4.log 2>&1
result $? "a database the server does not have is refused ($(cat r4.log))"

echo "$pass passed, $fail failed (work folder $W)"
[ $fail -eq 0 ]
