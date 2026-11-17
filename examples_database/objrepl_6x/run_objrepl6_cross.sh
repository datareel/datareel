#!/bin/bash
# run_objrepl6_cross.sh TOOLS_DIR OBJREPL6 [WORK_DIR] - DataReel 6.4: a
# DataReel 6.x program (objrepl6, gxv7ObjRepl) and the DataReel 7.4
# objrepl tool replicating to each other, both ways (11/17/2026).
#   TOOLS_DIR: the folder with the 7.4 objrepl tool (the 6.x CMake build
#              with DATAREEL7_FEATURES_DIR: <build>/dr7inner/tools)
#   OBJREPL6:  the objrepl6 program (examples_database/objrepl_6x)
#   WORK_DIR:  a new folder for the test (default: a temporary one)
# Linux (and Windows under a POSIX shell with openssl). Uses 127.0.0.1
# and throwaway certificates made with the openssl program. A 32-bit
# engine build is refused unless OBJREPL_ALLOW_32BIT=1 (as the 7.4
# objrepl tests): the run makes its own folders, so no engine reads the
# other's files.
set -u
T=$(cd "${1:?usage: run_objrepl6_cross.sh TOOLS_DIR OBJREPL6 [WORK_DIR]}" && pwd)
X7=$T/objrepl
X6=$(cd "$(dirname "${2:?usage: run_objrepl6_cross.sh TOOLS_DIR OBJREPL6 [WORK_DIR]}")" && pwd)/$(basename "$2")
W=${3:-$(mktemp -d "${TMPDIR:-/tmp}/objrepl6.XXXXXX")}
mkdir -p "$W" && W=$(cd "$W" && pwd)
if [ "${OBJREPL_ALLOW_32BIT:-0}" != 1 ] && ! "$X6" engine > /dev/null 2>&1; then
  echo "$(basename "$0"): $X6 is a 32-bit engine build ($("$X6" engine)); set OBJREPL_ALLOW_32BIT=1 for a 32-bit run"; exit 64
fi
"$X6" engine > /dev/null 2>&1 || echo "a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
PORT=$(( 24000 + RANDOM % 8000 ))
pass=0; fail=0; pids=()
result() { if [ "$1" -eq 0 ]; then pass=$((pass+1)); echo "PASS $2"; else fail=$((fail+1)); echo "FAIL $2"; fi; }
cleanup() { for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT

# Certificates: a CA, the copy's server certificate (127.0.0.1), the
# primary's client certificate (CN objrepl-primary) and a client
# certificate from the same CA with another CN (not in allow-cn: refused)
mkdir -p "$W/pki" && cd "$W/pki" || exit 2
EC="-newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes"
openssl req -x509 $EC -keyout ca.key -out ca.pem -days 2 -subj /CN=objrepl-ca 2>/dev/null
openssl req $EC -keyout copy.key -out copy.csr -subj /CN=objrepl-copy 2>/dev/null
printf 'subjectAltName=IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > copy.ext
openssl x509 -req -in copy.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out copy.pem -days 2 -extfile copy.ext 2>/dev/null
openssl req $EC -keyout primary.key -out primary.csr -subj /CN=objrepl-primary 2>/dev/null
printf 'extendedKeyUsage=clientAuth\n' > primary.ext
openssl x509 -req -in primary.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out primary.pem -days 2 -extfile primary.ext 2>/dev/null
openssl req $EC -keyout other.key -out other.csr -subj /CN=objrepl-other 2>/dev/null
openssl x509 -req -in other.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out other.pem -days 2 -extfile primary.ext 2>/dev/null
chmod 600 *.key
cd "$W" || exit 2
# Windows (Git bash): the config files hold Windows paths for the programs
winpath() { if command -v cygpath > /dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
K=$(winpath "$W/pki")

# conf NAME PEER ROLE PORT [CERT]: an objrepl.conf for one node
conf() {
  local f=$W/$1.conf c=${5:-primary}
  if [ "$3" == primary ]; then
    printf 'name %s\npeer-name %s\ncopy 127.0.0.1:%s\nca %s/ca.pem\ncert %s/%s.pem\nkey %s/%s.key\n' "$1" "$2" "$4" "$K" "$K" "$c" "$K" "$c" > "$f"
  else
    printf 'name %s\npeer-name %s\nlisten 127.0.0.1:%s\nca %s/ca.pem\ncert %s/copy.pem\nkey %s/copy.key\nallow-cn objrepl-primary\n' "$1" "$2" "$4" "$K" "$K" "$K" > "$f"
  fi
  echo "$f"
}
listening() { for _ in $(seq 100); do (exec 3<>/dev/tcp/127.0.0.1/$1) 2>/dev/null && return 0; sleep 0.1; done; return 1; }
# stop_copy ROOT PID: the copy stops on ROOT/objrepl.stop; its exit code
stop_copy() { touch "$1/objrepl.stop"; for _ in $(seq 100); do kill -0 "$2" 2>/dev/null || break; sleep 0.1; done; wait "$2"; }
# digest X ROOT: the digest line (stdout; the 7.4 tool also writes a
# tombstone count line to stderr)
digest() { "$1" digest "$2" 2>/dev/null; }
same() { [ -n "$1" ] && [ "$1" == "$2" ]; }

# 1. 6.4 primary -> 7.4 copy
P=$((PORT + 1)); mkdir -p r1p r1c
"$X7" copy r1c --config "$(conf copy1 primary1 copy $P)" > r1c.log 2>&1 & cp1=$!; pids+=($cp1)
listening $P; result $? "7.4 copy (objrepl tool) listening"
"$X6" primary r1p "$(conf primary1 copy1 primary $P)" 2000 > r1p.log 2>&1
result $? "6.4 primary (objrepl6) wrote 2000 records and drained to the 7.4 copy ($(tail -1 r1p.log))"
stop_copy r1c $cp1; result $? "7.4 copy stopped cleanly on objrepl.stop"
d6=$(digest "$X6" r1p); d7=$(digest "$X7" r1c)
same "$d6" "$d7"; result $? "the 7.4 copy's digest equals the 6.4 primary's ($d6)"
d7p=$(digest "$X7" r1p); same "$d6" "$d7p"; result $? "the 7.4 tool and the 6.4 program give the same digest of one folder"

# 2. 7.4 primary -> 6.4 copy
P=$((PORT + 2)); mkdir -p r2p r2c
"$X6" copy r2c "$(conf copy2 primary2 copy $P)" > r2c.log 2>&1 & cp2=$!; pids+=($cp2)
listening $P; result $? "6.4 copy (objrepl6) listening"
"$X7" primary r2p --config "$(conf primary2 copy2 primary $P)" --commits 300 --per 8 --dbs 3 > r2p.log 2>&1
result $? "7.4 primary (objrepl tool) wrote 300 commits to 3 databases and drained to the 6.4 copy"
stop_copy r2c $cp2; result $? "6.4 copy stopped cleanly on objrepl.stop ($(grep applied_seq r2c.log))"
d7=$(digest "$X7" r2p); d6=$(digest "$X6" r2c)
same "$d7" "$d6"; result $? "the 6.4 copy's digest equals the 7.4 primary's ($d7)"

# 3. 6.4 primary -> 6.4 copy, the copy checks every record's CRC
P=$((PORT + 3)); mkdir -p r3p r3c
"$X6" copy r3c "$(conf copy3 primary3 copy $P)" --check > r3c.log 2>&1 & cp3=$!; pids+=($cp3)
listening $P
"$X6" primary r3p "$(conf primary3 copy3 primary $P)" 1000 > r3p.log 2>&1; result $? "6.4 primary -> 6.4 copy: 1000 records drained"
stop_copy r3c $cp3; result $? "6.4 copy: every live record read back with a good CRC ($(grep 'live records' r3c.log))"
same "$(digest "$X6" r3p)" "$(digest "$X6" r3c)"; result $? "6.4 copy's digest equals the 6.4 primary's"

# 4. The copy away: the 6.4 primary commits, the 7.4 copy starts 5 s later and catches up
P=$((PORT + 4)); mkdir -p r4p r4c
"$X6" primary r4p "$(conf primary4 copy4 primary $P)" 500 > r4p.log 2>&1 & pp4=$!; pids+=($pp4)
sleep 5
"$X7" copy r4c --config "$(conf copy4 primary4 copy $P)" > r4c.log 2>&1 & cp4=$!; pids+=($cp4)
wait $pp4; result $? "6.4 primary with the copy away at first: drained once the 7.4 copy came up ($(tail -1 r4p.log))"
stop_copy r4c $cp4
same "$(digest "$X6" r4p)" "$(digest "$X7" r4c)"; result $? "the late 7.4 copy's digest equals the 6.4 primary's"

# 5. More commits later from the same 6.4 primary folder (a second run; the 7.4 copy gets only the new ones)
"$X7" copy r1c --config "$W/copy1.conf" > r1c2.log 2>&1 & cp5=$!; pids+=($cp5)
listening $((PORT + 1))
"$X6" primary r1p "$W/primary1.conf" 300 > r1p2.log 2>&1; result $? "a second 6.4 primary run on the same folder drained 300 more records"
stop_copy r1c $cp5
same "$(digest "$X6" r1p)" "$(digest "$X7" r1c)"; result $? "the 7.4 copy's digest equals the 6.4 primary's after the second run"
"$X7" status r1c > st1.txt 2>&1; grep -q "role copy\|role=copy\|\"role\": \"copy\"\|copy" st1.txt; result $? "7.4 status of the copy folder: $(head -c 120 st1.txt | tr '\n' ' ')"

# 6. A primary with a certificate the copy does not trust is refused (no data applied)
P=$((PORT + 6)); mkdir -p r6p r6c
"$X6" copy r6c "$(conf copy6 primary6 copy $P)" > r6c.log 2>&1 & cp6=$!; pids+=($cp6)
listening $P
"$X7" primary r6p --config "$(conf primary6 copy6 primary $P other)" --commits 5 > r6p.log 2>&1; r=$?
[ $r -ne 0 ]; result $? "a primary certificate whose CN is not in allow-cn is refused by the 6.4 copy (7.4 primary exit $r)"
stop_copy r6c $cp6
[ "$(grep -o 'applied_seq [0-9]*' r6c.log)" == "applied_seq 0" ]; result $? "the 6.4 copy applied nothing from it"

# 7. Takeover with the 7.4 tool on the 6.4 copy's folder: it becomes the primary at the next epoch
"$X7" takeover r2c > to.log 2>&1; result $? "7.4 takeover of the 6.4 copy's folder ($(head -1 to.log))"
same "$(digest "$X6" r2c)" "$d7"; result $? "the new primary keeps every object (digest unchanged)"
"$X6" status r2c > st2.txt 2>&1; grep -q "primary" st2.txt; result $? "6.4 status of the folder after the takeover: $(head -c 120 st2.txt | tr '\n' ' ')"

echo "$pass passed, $fail failed (work folder $W)"
[ $fail -eq 0 ]
