#!/bin/bash
# test_objrepl_config.sh - 11/16/2026: objrepl.conf, the certificate checks at start, and objrepl_certs.sh.
# Throwaway folders only.
#
#   utils/objrepl/test_objrepl_config.sh BIN
#
#   certs dry  objrepl_certs.sh without --apply writes nothing
#   certs      with --apply: CA + copy + primary certificates, keys mode 600; again: refused (never overwrites)
#   config     both nodes from objrepl.conf alone (no TLS options on the command line): copy = primary
#   override   a command line option wins over the config (--name)
#   key mode   a key readable by others: refused at start (exit 64, says chmod 600)
#   expired    an expired certificate: refused at start
#   wrong CA   a certificate from another CA: refused at start ("not signed by the CA")
#   key match  a key that is not the certificate's: refused at start
#   expiring   a certificate with 10 days left: streams, and objrepl check says WARN (expires in 9-10 days)
#   bad conf   an unknown key in objrepl.conf: refused (exit 64)
#   wrong CN   a primary whose client certificate CN is not the copy's allow-cn: never gets a stream
#   port 2073  7.4.1: a copy with no listen port listens on 2073 and a primary with "copy HOST" connects there;
#              bad copy/listen values are refused at start (needs 127.0.0.1:2073 free)
set -u
. "$(dirname "$0")/objrepl_proc.sh"   # 11/17/2026: objrepl_stop, objrepl_pid (Windows)
X=${1:?usage: test_objrepl_config.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_config.XXXXXX")
FAIL=0; PASS=0
ok()  { echo "PASS $1"; PASS=$((PASS + 1)); }
bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
CPID=""
stop_copy() { [ -n "$CPID" ] || return 0; objrepl_stop $CPID
              for i in $(seq 1 25); do kill -0 $CPID 2>/dev/null || break; sleep 0.2; done
              kill -KILL $CPID 2>/dev/null; wait $CPID 2>/dev/null; CPID=""; }
trap 'stop_copy; rm -rf "$T"' EXIT
K="$T/pki"
P=$((20000 + RANDOM % 10000))
dig() { "$X" digest "$T/$1" --links lk:0:db0 2>/dev/null; }   # the same link map on both (the copy's conf has none)

# certs dry run, then apply, then refused
out=$("$HERE/objrepl_certs.sh" "$K" --copy-host 127.0.0.1,localhost); rc=$?
[ $rc -eq 0 ] && echo "$out" | grep -q "dry run" && [ ! -e "$K/ca.pem" ] && ok "certs dry: nothing written" || bad "certs dry" "rc $rc"
"$HERE/objrepl_certs.sh" "$K" --copy-host 127.0.0.1,localhost --days 400 --apply > "$T/certs.log" 2>&1; rc=$?
modes=$(stat -c %a "$K"/*.key | sort -u | tr '\n' ' ')
san=$(openssl x509 -in "$K/copy.pem" -noout -ext subjectAltName 2>/dev/null | tail -n 1 | tr -d ' ')
"$HERE/objrepl_certs.sh" "$K" --copy-host 127.0.0.1 --apply > "$T/certs2.log" 2>&1; rc2=$?
[ $rc -eq 0 ] && [ "$modes" == "600 " ] && [ "$san" == "IPAddress:127.0.0.1,DNS:localhost" ] && [ $rc2 -eq 1 ] && grep -q "there already" "$T/certs2.log" \
  && ok "certs: CA + copy + primary, keys 600, SAN $san; a second run refused" || bad "certs" "rc $rc modes [$modes] san [$san] again rc $rc2"

# config: both nodes from objrepl.conf alone
mkdir -p "$T/p" "$T/a"
cat > "$T/a/objrepl.conf" <<CONF
# the copy
name copyA
peer-name P1
listen 127.0.0.1:$P
ca $K/ca.pem
cert $K/copy.pem
key $K/copy.key
allow-cn objrepl-primary
CONF
cat > "$T/p/objrepl.conf" <<CONF
name P1
peer-name copyA
copy 127.0.0.1:$P   # the copy
ca $K/ca.pem
cert $K/primary.pem
key $K/primary.key
links lk:0:db0
CONF
"$X" copy "$T/a" >> "$T/copy_a.log" 2>&1 & CPID=$!; sleep 0.5
"$X" primary "$T/p" --commits 60 --per 6 --seed 81 --dbs 2 > "$T/p1.log" 2>&1; rc=$?
[ $rc -eq 0 ] && [ "$(dig p)" == "$(dig a)" ] && grep -q "^name copyA" "$T/a/objrepl.status" && grep -q "^name P1" "$T/p/objrepl.status" \
  && ok "config: both nodes from objrepl.conf alone; copy = primary ($(dig a))" || bad "config" "rc $rc [$(tail -n 1 "$T/p1.log")] [$(dig p)] [$(dig a)]"
days=$(awk '$1 == "cert_days_left" { print $2 }' "$T/p/objrepl.status")

# override: the command line wins (the copy expects P1, so a primary renamed P7 on the command line is refused)
"$X" primary "$T/p" --name P7 --commits 1 --per 1 > "$T/p2.log" 2>&1; rc=$?
[ $rc -eq 3 ] && grep -q "not the expected primary P1" "$T/a/objrepl.alarm" && ok "override: --name P7 on the command line beat the config (refused by the copy)" || bad "override" "rc $rc"
stop_copy; "$X" clear-alarm "$T/a" > /dev/null; "$X" clear-alarm "$T/p" > /dev/null

# expiring: 10 days left
openssl req -newkey rsa:2048 -nodes -subj "/CN=objrepl-primary" -keyout "$T/p10.key" -out "$T/p10.csr" 2>/dev/null
openssl x509 -req -in "$T/p10.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -days 10 -out "$T/p10.pem" 2>/dev/null
"$X" copy "$T/a" >> "$T/copy_a.log" 2>&1 & CPID=$!; sleep 0.5
"$X" primary "$T/p" --cert "$T/p10.pem" --key "$T/p10.key" --commits 5 --per 2 > "$T/p3.log" 2>&1; rc=$?
out=$("$X" check "$T/p"); crc=$?
[ $rc -eq 0 ] && [ $crc -eq 1 ] && echo "$out" | grep -qE "expires in (9|10) day" && [ "${days:-0}" -ge 398 ] \
  && ok "expiring: streams; check WARN [$(echo "$out" | grep -o 'the certificate expires in [0-9]* day(s)')]; the 400-day cert had $days left" \
  || bad "expiring" "rc $rc check $crc [$out] days $days"
stop_copy

# refusals at start
start_refused() {  # NAME EXPECT -- primary options
  local name=$1 want=$2; shift 2
  out=$("$X" primary "$T/p" "$@" --commits 1 --per 1 2>&1); rc=$?
  [ $rc -eq 64 ] && echo "$out" | grep -q "$want" && ok "$name: refused at start [$(echo "$out" | head -n 1)]" || bad "$name" "rc $rc [$out]"
}
cp "$K/primary.key" "$T/open.key"; chmod 644 "$T/open.key"
start_refused "key mode" "chmod 600" --key "$T/open.key" --cert "$K/primary.pem"
openssl x509 -req -in "$T/p10.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -not_before 20250101000000Z -not_after 20250201000000Z -out "$T/old.pem" 2>/dev/null
start_refused "expired" "has expired" --cert "$T/old.pem" --key "$T/p10.key"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=other-ca" -keyout "$T/oca.key" -out "$T/oca.pem" 2>/dev/null
openssl x509 -req -in "$T/p10.csr" -CA "$T/oca.pem" -CAkey "$T/oca.key" -CAcreateserial -days 30 -out "$T/other.pem" 2>/dev/null
start_refused "wrong CA" "not signed by the CA" --cert "$T/other.pem" --key "$T/p10.key"
start_refused "key match" "does not match" --cert "$K/primary.pem" --key "$T/p10.key"

# bad conf
mkdir -p "$T/b"; printf 'name x\nlisen 1234\n' > "$T/b/objrepl.conf"
out=$("$X" copy "$T/b" 2>&1); rc=$?
[ $rc -eq 64 ] && echo "$out" | grep -q "objrepl.conf:2: unknown key" && ok "bad conf: refused [$out]" || bad "bad conf" "rc $rc [$out]"

# port 2073 (7.4.1): no port in either config
if ss -ltn 2>/dev/null | awk '{ print $4 }' | grep -qE '(^|:)2073$'; then
  bad "port 2073" "port 2073 is in use on this host: stop what listens there and run the test again"
else
  mkdir -p "$T/d" "$T/q"
  printf 'name copyD\nca %s\ncert %s\nkey %s\nallow-cn objrepl-primary\n' "$K/ca.pem" "$K/copy.pem" "$K/copy.key" > "$T/d/objrepl.conf"
  printf 'name Q1\ncopy 127.0.0.1\nca %s\ncert %s\nkey %s\n' "$K/ca.pem" "$K/primary.pem" "$K/primary.key" > "$T/q/objrepl.conf"
  "$X" copy "$T/d" >> "$T/copy_d.log" 2>&1 & CPID=$!; sleep 0.5
  "$X" primary "$T/q" --commits 8 --per 2 --seed 7 > "$T/q1.log" 2>&1; rc=$?
  stop_copy
  dq=$("$X" digest "$T/q" 2>/dev/null); dd=$("$X" digest "$T/d" 2>/dev/null)
  [ $rc -eq 0 ] && grep -q "copy listening on 127.0.0.1:2073 " "$T/copy_d.log" && [ -n "$dq" ] && [ "$dq" == "$dd" ] \
    && ok "port 2073: the copy listened on 127.0.0.1:2073 and the primary drained to it ($dd)" \
    || bad "port 2073" "rc $rc [$(tail -n 1 "$T/q1.log")] [$(grep listening "$T/copy_d.log")] [$dq] [$dd]"
  out1=$("$X" primary "$T/q" --copy 127.0.0.1:x --commits 1 --per 1 2>&1); r1=$?
  out2=$("$X" copy "$T/d" --listen 127.0.0.1: 2>&1); r2=$?
  [ $r1 -eq 64 ] && echo "$out1" | grep -q "copy is HOST\[:PORT\]" && [ $r2 -eq 64 ] && echo "$out2" | grep -q "listen is \[ADDR:\]PORT or ADDR" \
    && ok "port 2073: a bad copy or listen value is refused at start" || bad "port 2073 values" "$r1 [$out1] $r2 [$out2]"
fi

# wrong CN: a client certificate from the right CA with another CN
openssl req -newkey rsa:2048 -nodes -subj "/CN=intruder" -keyout "$T/in.key" -out "$T/in.csr" 2>/dev/null
openssl x509 -req -in "$T/in.csr" -CA "$K/ca.pem" -CAkey "$K/ca.key" -CAcreateserial -days 30 -out "$T/in.pem" 2>/dev/null
before=$(awk '$1 == "applied_seq" { print $2 }' "$T/a/objrepl.status")
"$X" copy "$T/a" >> "$T/copy_a.log" 2>&1 & CPID=$!; sleep 0.5
mkdir -p "$T/i"; cp "$T/p/objrepl.conf" "$T/i/objrepl.conf"
timeout 10 "$X" primary "$T/i" --name P1 --cert "$T/in.pem" --key "$T/in.key" --commits 3 --per 1 > "$T/p4.log" 2>&1; rc=$?
after=$(awk '$1 == "applied_seq" { print $2 }' "$T/a/objrepl.status")
stop_copy
[ $rc -eq 124 ] && [ "$before" == "$after" ] && [ "$(awk '$1 == "copy_applied" { print $2 }' "$T/i/objrepl.status")" == "0" ] \
  && [ "$(awk '$1 == "connected" { print $2 }' "$T/i/objrepl.status")" == "0" ] \
  && ok "wrong CN: the TLS handshake refused it for 10 s (never connected, nothing sent); the copy unchanged at $after" || bad "wrong CN" "rc $rc applied $before -> $after"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
