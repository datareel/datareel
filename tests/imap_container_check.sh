#!/bin/bash
# 11/16/2026 (DataReel 7.4): gxsIMAPClient (examples/tls/imap_fetch) against a real IMAP server: Dovecot in a
# rootless podman container that listens on 127.0.0.1 only, with implicit TLS (993), STARTTLS and plain text
# (143). The container is removed at the end, pass or fail; the image (localhost/datareel-imap-test) is kept
# so the next run does not download Dovecot again (podman rmi localhost/datareel-imap-test removes it).
#
# usage: tests/imap_container_check.sh EXAMPLES_DIR WORK_DIR CA.pem SERVER.pem SERVER.key
# Needs rootless podman, and network access the first time (the image is built from Rocky Linux 9 + dovecot).
# Exit 0 when every check passes. Run by tests/run_tests.sh.

E=$1; WORK=$2; CA=$3; CERT=$4; KEY=$5
[ -n "$KEY" ] || { echo "usage: $0 EXAMPLES_DIR WORK_DIR CA.pem SERVER.pem SERVER.key"; exit 2; }
command -v podman > /dev/null || { echo "FAIL imap_container: needs rootless podman"; exit 1; }
IMAGE=localhost/datareel-imap-test:rocky9
NAME=datareel-imap-test-$$
PASS=0; FAIL=0
mkdir -p "$WORK/conf" || exit 1
cd "$WORK" || exit 1

cleanup() { podman rm -f -t 2 "$NAME" > /dev/null 2>&1; }
trap cleanup EXIT

check() { # check "name" command...
  local name=$1; shift
  if "$@" > "$WORK/$name.log" 2>&1; then echo "PASS $name"; PASS=$((PASS+1))
  else echo "FAIL $name (see $WORK/$name.log)"; FAIL=$((FAIL+1)); fi
}

if ! podman image exists $IMAGE; then
  printf 'FROM quay.io/rockylinux/rockylinux:9\nRUN dnf -y install dovecot && dnf clean all && useradd -u 5000 -M -d /srv/mail vmail\n' \
    > Containerfile
  podman build -q -t $IMAGE -f Containerfile . > build.log 2>&1 || { echo "FAIL imap_container: image build (see $WORK/build.log)"; exit 1; }
fi

cp "$CERT" conf/server.pem && cp "$KEY" conf/server.key || exit 1
chmod 644 conf/server.pem conf/server.key
printf 'test:{PLAIN}secret\n'"$(printf 't\303\253st')"':{PLAIN}'"$(printf 's\303\253cret')"'\n' > conf/users
printf 'quote:{PLAIN}a b"c\\d\n' >> conf/users      # a space, a quote and a backslash in the password
cat > conf/dovecot.conf <<'EOF'
protocols = imap
listen = *
ssl = yes
ssl_cert = </conf/server.pem
ssl_key = </conf/server.key
ssl_min_protocol = TLSv1.2
disable_plaintext_auth = no
auth_mechanisms = plain login
passdb {
  driver = passwd-file
  args = scheme=PLAIN username_format=%u /conf/users
}
userdb {
  driver = static
  args = uid=5000 gid=5000 home=/srv/mail/%u
}
mail_location = maildir:~/Maildir
first_valid_uid = 5000
log_path = /dev/stderr
service imap-login {
  inet_listener imap { port = 143 }
  inet_listener imaps { port = 993 ssl = yes }
}
EOF

# Ports below the ephemeral range, on 127.0.0.1 only
P=$((10000 + RANDOM % 20000))
podman run -d --rm --name "$NAME" -p 127.0.0.1:$P:993 -p 127.0.0.1:$((P+1)):143 \
  -v "$WORK/conf:/conf:ro,Z" $IMAGE dovecot -F -c /conf/dovecot.conf > run.log 2>&1 ||
  { echo "FAIL imap_container: container start (see $WORK/run.log)"; exit 1; }
for i in $(seq 100); do
  (exec 3<>/dev/tcp/127.0.0.1/$((P+1))) 2>/dev/null && break
  sleep 0.1
done

# Two messages: the first holds text that cut the 2007 client short and is over 5 MB, so it arrives in many TLS
# records; CR LF line ends, as IMAP returns them
{ printf 'First message\r\nNO problems, nothing BAD here }\r\nA0006 OK not a reply\r\n'
  for i in $(seq 90000); do printf 'line %06d of a long report body ......................\r\n' $i; done
  printf 'End\r\n'; } > body1.txt
{ printf 'From: a@example.com\r\nTo: test@example.com\r\nSubject: one (report)\r\nDate: Tue, 17 Apr 2007 11:59:00 -0400\r\n\r\n'
  cat body1.txt; } > msg1.eml
printf 'From: a@example.com\r\nTo: test@example.com\r\nSubject: two\r\nDate: Wed, 18 Apr 2007 08:29:00 -0400\r\n\r\nSecond\r\n' > msg2.eml
for m in msg1.eml msg2.eml; do
  podman exec -i "$NAME" doveadm save -u test < $m > save.log 2>&1 || { echo "FAIL imap_container: doveadm save"; cat save.log; exit 1; }
done

IM="$E/imap_fetch localhost"
check imapc_implicit_tls sh -c "$IM $P test secret --ca $CA -r 1 > c1.out && grep -q '2 messages' c1.out && grep -q '(TLS)' c1.out && grep -q '^A0006 OK not a reply' c1.out && grep -q '^line 090000 ' c1.out && grep -q '^End' c1.out"
check imapc_starttls sh -c "$IM $((P+1)) test secret --starttls --ca $CA | grep -q '(TLS)'"
check imapc_plain sh -c "$IM $((P+1)) test secret --plain | grep -q '(plain text)'"
check imapc_bad_password_refused sh -c "! $IM $P test wrong --ca $CA > c2.out 2>&1 && grep -q 'Login refused' c2.out"
check imapc_8bit_login sh -c "$IM $P $(printf 't\303\253st') $(printf 's\303\253cret') --ca $CA | grep -q '0 messages'"
check imapc_search sh -c "$IM $P test secret --ca $CA --search 'SUBJECT \"two\"' > c3.out && grep -q '^1 match' c3.out && grep -q '^2: two' c3.out"
check imapc_dates sh -c "$IM $P test secret --ca $CA | grep -q 'sent Tue, 17 Apr 2007 11:59:00 -0400'"
check imapc_password_quoted sh -c "$IM $P quote 'a b\"c\\d' --ca $CA | grep -q '0 messages'"
check imapc_untrusted_cert_refused sh -c "! $IM $P test secret > c5.out 2>&1 && grep -q 'Cannot verify certificate' c5.out"
check imapc_starttls_untrusted_cert_refused sh -c "! $IM $((P+1)) test secret --starttls > c6.out 2>&1 && grep -q 'Cannot verify certificate' c6.out"
rm -rf spool; mkdir spool
check imapc_5mb_byte_for_byte sh -c "[ \$(stat -c %s body1.txt) -gt 5000000 ] && $IM $P test secret --ca $CA --spool spool > c4.out && f=\$(ls spool/*_1.txt) && cmp \"\$f\" body1.txt && rm -rf spool && mkdir spool"
check imapc_spool sh -c "$IM $P test secret --ca $CA --spool spool > c4.out && f=\$(ls spool/*_1.txt) && cmp \"\$f\" body1.txt && $IM $P test secret --ca $CA --spool spool | grep -c 'already in the spool' | grep -q 2"
check imapc_unseen_kept sh -c "$IM $P test secret --ca $CA --search UNSEEN | grep -q '^2 match'"

echo "$PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
