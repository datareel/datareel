#!/bin/bash
# test_objrepl_check.sh - 11/16/2026: ROOT/objrepl.status and "objrepl check" (OK 0, WARN 1, FAIL 2). Throwaway folders.
#
#   utils/objrepl/test_objrepl_check.sh BIN
#
#   never      a folder never run: WARN (no status)
#   live       primary and copy running: both OK; the status file has every field
#   stopped    after a drained run: the primary's status says lag 0, "stopped, drained"; check WARN (not running);
#              primary and copy report the same tombstone count
#   away       a running primary with no copy: WARN "the copy is away", lag and oldest unacked age grow
#   alarm      a refused primary: the copy's check FAIL (exit 2) with the alarm; clear-alarm, then OK again
#   stopping   a clean stop ends "stopped" (close_secs); "stopping" over 10 s is WARN; gone while closing is WARN
#   killed     the copy SIGKILLed: WARN (not running)
set -u
. "$(dirname "$0")/objrepl_proc.sh"   # 11/17/2026: objrepl_stop, objrepl_pid (Windows)
X=${1:?usage: test_objrepl_check.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_check.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
K="$T/pki"; mkdir -p "$K"
openssl req -x509 -newkey rsa:2048 -nodes -days 400 -subj "/CN=objrepl-test-ca" -keyout "$K/ca.key" -out "$K/ca.pem" 2>/dev/null
mk() { openssl req -newkey rsa:2048 -nodes -subj "/CN=$2" -keyout "$K/$1.key" -out "$K/$1.csr" 2>/dev/null
       if [ -n "${3:-}" ]; then printf 'subjectAltName=%s\n' "$3" > "$K/$1.ext"; else : > "$K/$1.ext"; fi
       openssl x509 -req -in "$K/$1.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -days 400 -extfile "$K/$1.ext" -out "$K/$1.pem" 2>/dev/null; }
mk copy objrepl-copy "IP:127.0.0.1,DNS:localhost"; mk primary objrepl-primary
CTLS=(--ca "$K/ca.pem" --cert "$K/copy.pem" --key "$K/copy.key" --allow-cn objrepl-primary)
PTLS=(--ca "$K/ca.pem" --cert "$K/primary.pem" --key "$K/primary.key")
CPID=""; PPID2=""
start_copy() { local d=$1 port=$2; shift 2; "$X" copy "$T/$d" --listen $port "${CTLS[@]}" "$@" >> "$T/copy_$d.log" 2>&1 & CPID=$!; sleep 0.5; }
stop_copy() { [ -n "$CPID" ] || return 0; objrepl_stop $CPID
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; [ -n "$PPID2" ] && kill -TERM $PPID2 2>/dev/null; rm -rf "$T"' EXIT
chk() { "$X" check "$T/$1" "${@:2}"; }
field() { awk -v k="$2" '$1 == k { print $2 }' "$T/$1/objrepl.status"; }
mkdir -p "$T/p" "$T/a" "$T/q" "$T/n"
PA=$((20000 + RANDOM % 10000)); PQ=$((PA + 1))

# never
out=$(chk n); rc=$?
[ $rc -eq 1 ] && echo "$out" | grep -q "^WARN .*no status" && ok "never: WARN, no status (rc 1)" || bad "never" "rc $rc [$out]"

# live: both running
start_copy a $PA --name copyA --peer-name P1
"$X" primary "$T/p" --copy 127.0.0.1:$PA "${PTLS[@]}" --name P1 --peer-name copyA --commits 300 --per 6 --seed 71 --sleep 20 > "$T/p1.log" 2>&1 &
PPID2=$!
for i in $(seq 1 75); do [ "$(field p copy_applied 2>/dev/null)" -gt 5 ] 2>/dev/null && break; sleep 0.2; done   # streaming
po=$(chk p); prc=$?; co=$(chk a); crc=$?
missing=""
for k in role name epoch state connected last_seq copy_applied lag_seq behind_secs oldest_unacked_secs outbox_entries outbox_bytes snapshots last_contact tombstones updated pid; do
  [ -n "$(field p $k)" ] || missing="$missing p:$k"; done
for k in role name epoch state connected primary applied_seq in_snapshot frames last_frame last_contact tombstones rollbacks refusals updated pid; do
  [ -n "$(field a $k)" ] || missing="$missing a:$k"; done
[ $prc -eq 0 ] && [ $crc -eq 0 ] && [ -z "$missing" ] && [ "$(field p connected)" == "1" ] && [ "$(field a primary)" == "P1" ] \
  && ok "live: primary OK [$po] copy OK [$co]" || bad "live" "rc $prc/$crc missing [$missing] [$po] [$co]"
wait $PPID2; prc=$?; PPID2=""

# stopped (drained)
po=$(chk p); rc=$?
pt=$(field p tombstones); ct=$(field a tombstones)
[ $prc -eq 0 ] && [ $rc -eq 1 ] && echo "$po" | grep -q "not running" && [ "$(field p lag_seq)" == "0" ] && [ "$(field p state)" == "stopped," ] \
  && [ "$pt" == "$ct" ] && [ "${pt:-0}" -gt 0 ] && [ ! -e "$T/p/objrepl.status.tmp" ] \
  && ok "stopped: lag 0, drained, WARN not running (rc 1); $pt tombstones on both" \
  || bad "stopped" "primary rc $prc check rc $rc [$po] lag $(field p lag_seq) state [$(grep '^state' "$T/p/objrepl.status")] tombs $pt/$ct"
stop_copy

# away: a running primary, no copy
"$X" primary "$T/p" --copy 127.0.0.1:$PA "${PTLS[@]}" --name P1 --commits 200 --per 4 --seed 72 --sleep 30 > "$T/p2.log" 2>&1 &
# 11/16/2026: Windows (Git Bash): a connect to a closed port takes ~2 s there, so the primary writes its status less
# often: a longer wait before the check
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) AWAY_WAIT=8 ;; *) AWAY_WAIT=4 ;; esac
PPID2=$!; sleep $AWAY_WAIT
po=$(chk p --max-behind 2); rc=$?
lag=$(field p lag_seq); old=$(field p oldest_unacked_secs)
[ $rc -eq 1 ] && echo "$po" | grep -q "the copy is away" && [ "${lag:-0}" -gt 10 ] && [ "${old:-0}" -ge 2 ] \
  && ok "away: WARN the copy is away (lag $lag, oldest unacked ${old}s)" || bad "away" "rc $rc lag $lag oldest $old [$po]"
kill -TERM $PPID2 2>/dev/null; wait $PPID2 2>/dev/null; PPID2=""

# alarm: copy A expects P1, a primary named P9 connects
start_copy a $PA --name copyA --peer-name P1
"$X" primary "$T/q" --copy 127.0.0.1:$PA "${PTLS[@]}" --name P9 --commits 2 --per 2 > "$T/p3.log" 2>&1
co=$(chk a); rc=$?; po=$(chk q); qrc=$?
[ $rc -eq 2 ] && echo "$co" | grep -q "^FAIL .*not the expected primary" && [ $qrc -eq 2 ] && [ "$(field a refusals)" == "1" ] \
  && ok "alarm: copy FAIL (rc 2) with the alarm; the refused primary FAIL too" || bad "alarm" "copy rc $rc [$co] primary rc $qrc [$po]"
"$X" clear-alarm "$T/a" > /dev/null; co=$(chk a); rc=$?
[ $rc -eq 0 ] && [ -s "$T/a/objrepl.alarm.seen" ] && [ ! -e "$T/a/objrepl.alarm" ] && ok "alarm cleared: OK again, kept in objrepl.alarm.seen" || bad "clear-alarm" "rc $rc [$co]"

# killed
kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""
co=$(chk a); rc=$?
[ $rc -eq 1 ] && echo "$co" | grep -q "not running" && ok "killed: WARN not running (rc 1)" || bad "killed" "rc $rc [$co]"

# 11/17/2026: stopping. A clean stop ends "stopped" with the close's time; a status left "stopping" (a close on a
# saturated disk: the journal checkpoint syncs every file) is WARN after 10 s while the process lives, and WARN
# "stopped while closing" once it is gone (never FAIL)
mkdir -p "$T/st"
start_copy st $((PA + 7)) --name copyS
stop_copy
[ "$(field st state)" == "stopped" ] && [ -n "$(field st close_secs)" ] && [ -z "$(field st stopping_since)" ] \
  && ok "stopping: a clean stop ends \"stopped\" (close took $(field st close_secs)s)" || bad "stopping clean" "state [$(field st state)] close_secs [$(field st close_secs)]"
sleep 300 & SLP=$!
now=$(date +%s)
cat > "$T/st/objrepl.status" <<EOS
role copy
name copyS
epoch 1
state stopping
stopping_since $((now - 30))
pid $SLP
updated $now
applied_seq 0
EOS
co=$(chk st); rc=$?
[ $rc -eq 1 ] && echo "$co" | grep -q "stopping for 3[0-9]s" && ok "stopping: a close still running after 30 s is WARN (rc 1): $(echo "$co" | cut -c1-90)" || bad "stopping slow" "rc $rc [$co]"
sed -i "s/^stopping_since .*/stopping_since $((now - 3))/" "$T/st/objrepl.status"
co=$(chk st); rc=$?
[ $rc -eq 0 ] && ok "stopping: a close of 3 s is OK" || bad "stopping short" "rc $rc [$co]"
kill -TERM $SLP 2>/dev/null; wait $SLP 2>/dev/null
co=$(chk st); rc=$?
[ $rc -eq 1 ] && echo "$co" | grep -q "stopped while closing" && ok "stopping: the process gone while closing is WARN \"stopped while closing\" (not FAIL)" || bad "stopping dead" "rc $rc [$co]"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
