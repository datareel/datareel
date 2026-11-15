#!/bin/bash
# run_repl6_cross.sh TOOLS_DIR PROGS_DIR [WORK_DIR] - DataReel 6.4: the
# DataReel 7.x gxrepl replicating a database a DataReel 6.x program
# writes (see README.txt).
#   TOOLS_DIR: the folder with gxrepl (the 6.x CMake build with
#              DATAREEL7_FEATURES_DIR: <build>/dr7inner/tools)
#   PROGS_DIR: the folder with repl6_writer and repl6_check
#   WORK_DIR:  a new folder for the test (default: a temporary one)
# Linux only (gxrepl is POSIX only). Uses 127.0.0.1 and throwaway
# certificates made with the openssl program. Returns 0 if every check
# passes.
set -u
GX=$(cd "${1:?usage: run_repl6_cross.sh TOOLS_DIR PROGS_DIR [WORK_DIR]}" && pwd)/gxrepl
P=$(cd "${2:?usage: run_repl6_cross.sh TOOLS_DIR PROGS_DIR [WORK_DIR]}" && pwd)
W=${3:-$(mktemp -d "${TMPDIR:-/tmp}/repl6.XXXXXX")}
mkdir -p "$W" && W=$(cd "$W" && pwd)
PORT=$(( 20000 + RANDOM % 20000 ))
pass=0; fail=0; pids=()
result() { if [ "$1" -eq 0 ]; then pass=$((pass+1)); echo "PASS $2"; else fail=$((fail+1)); echo "FAIL $2"; fi; }
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT
[ -x "$GX" ] || { echo "no gxrepl in $1"; exit 2; }

# Throwaway certificates: a private CA, the copy (server) and the primary (client)
mkdir -p "$W/pki" && cd "$W/pki" || exit 2
EC="-newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes"
openssl req -x509 $EC -keyout ca.key -out ca.pem -days 2 -subj /CN=repl6-ca 2>/dev/null
openssl req $EC -keyout copy.key -out copy.csr -subj /CN=copy 2>/dev/null
printf 'subjectAltName=DNS:copy,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > copy.ext
openssl x509 -req -in copy.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out copy.pem -days 2 -extfile copy.ext 2>/dev/null
openssl req $EC -keyout primary.key -out primary.csr -subj /CN=primary 2>/dev/null
printf 'extendedKeyUsage=clientAuth\n' > primary.ext
openssl x509 -req -in primary.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out primary.pem -days 2 -extfile primary.ext 2>/dev/null
chmod 600 *.key
cd "$W" || exit 2
mkdir -p primary/db copy/db plain_src/db plain_dst/db

conf() { # conf NODE ROLE SETROOT [PLAINROOT]
  if [ "$2" = client ]; then
    printf 'node = %s\nrole = client\nconnect = 127.0.0.1:%s\nserver_name = copy\n' "$1" "$PORT"
  else
    printf 'node = %s\nrole = server\nlisten = 127.0.0.1:%s\nallow_cn = primary\n' "$1" "$PORT"
  fi
  printf 'cert = %s/pki/%s.pem\nkey = %s/pki/%s.key\nca = %s/pki/ca.pem\nstatus_file = %s/%s.status.json\npoll_ms = 200\n' \
    "$W" "$1" "$W" "$1" "$W" "$W" "$1"
  printf '[set db]\nroot = %s\n' "$3"
  if [ "$2" = client ]; then printf 'direction = push\nscan_interval = 1\n'; else printf 'accept = target\n'; fi
  printf '[set plain]\nroot = %s\n' "$4"
  if [ "$2" = client ]; then printf 'direction = push\nscan_interval = 1\n'; else printf 'accept = target\n'; fi
}
conf primary client "$W/primary/db" "$W/plain_src/db" > primary.conf
conf copy server "$W/copy/db" "$W/plain_dst/db" > copy.conf

# 1. A database written by a 6.x program, copied while quiet
"$P/repl6_writer" "$W/primary/db" 20 10 > writer1.log 2>&1
result $? "6.x writer: 20 commits (journal with the write lock)"
"$P/repl6_writer" "$W/plain_src/db" 5 10 0 plain > writer_plain.log 2>&1
result $? "6.x writer: 5 commits without a journal"
"$GX" serve --config copy.conf > copy.log 2>&1 & pids+=($!)
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break; sleep 0.1; done
"$GX" once --config primary.conf > once1.log 2>&1
result $? "7.x gxrepl once: every set the same (exit 0)"
"$P/repl6_check" "$W/copy/db" 200 > check1.log 2>&1
result $? "6.x checker reads the copy: whole, 200 records ($(cat check1.log))"
cmp -s "$W/primary/db/data.gxd" "$W/copy/db/data.gxd"
result $? "copy byte-identical to the 6.x primary (data.gxd)"
"$P/repl6_check" "$W/plain_dst/db" 50 > check_plain.log 2>&1 && cmp -s "$W/plain_src/db/data.gxd" "$W/plain_dst/db/data.gxd"
result $? "a 6.x database written without a journal: copied whole and byte-identical"
[ ! -e "$W/copy/db/data.jnl" ]
result $? "the writer's journal (.jnl) is not copied"

# 2. Copied live while the 6.x program writes (gxrepl run)
"$GX" run --config primary.conf > primary.log 2>&1 & RUN=$!; pids+=($RUN)
"$P/repl6_writer" "$W/primary/db" 300 10 20 > writer2.log 2>&1 & WRITER=$!
samples=0; torn=0; last=0; back=0
while kill -0 $WRITER 2>/dev/null; do
  out=$("$P/repl6_check" "$W/copy/db" 2>&1)
  samples=$((samples+1))
  case "$out" in
    WHOLE*) n=$(echo "$out" | awk '{print $2}'); [ "$n" -lt "$last" ] && back=$((back+1)); last=$n ;;
    *) torn=$((torn+1)); echo "  sample $samples: $out" ;;
  esac
  sleep 0.3
done
wait $WRITER
result $? "6.x writer: 300 more commits while gxrepl runs"
echo "  live samples of the copy: $samples, torn: $torn, went back: $back, last count: $last"
[ "$samples" -gt 5 ] && [ "$torn" -eq 0 ] && [ "$back" -eq 0 ]
result $? "every live sample of the copy is a whole commit, never going back"
want=$(( (20 + 300) * 10 ))
for _ in $(seq 120); do "$P/repl6_check" "$W/copy/db" $want > /dev/null 2>&1 && break; sleep 0.5; done
"$P/repl6_check" "$W/copy/db" $want > check2.log 2>&1
result $? "the copy catches up: $want records ($(cat check2.log))"
kill $RUN 2>/dev/null; wait $RUN 2>/dev/null
"$GX" once --config primary.conf > once2.log 2>&1
result $? "7.x gxrepl once after the live run: every set the same"
cmp -s "$W/primary/db/data.gxd" "$W/copy/db/data.gxd"
result $? "copy byte-identical to the 6.x primary after the live run"

echo "$pass passed, $fail failed (work folder $W)"
[ $fail -eq 0 ]
