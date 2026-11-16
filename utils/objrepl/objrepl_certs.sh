#!/bin/bash
# objrepl_certs.sh - 11/16/2026: a private CA and the two node certificates objrepl needs (mutual TLS over gxStream).
# Dry run unless --apply. Never overwrites: it stops if any file it would write is there already.
#
#   utils/objrepl/objrepl_certs.sh DIR --copy-host HOST[,HOST...] [--copy-cn CN] [--primary-cn CN]
#                                  [--ca-cn CN] [--days N] [--apply]
#
#   DIR/ca.pem ca.key             the CA (keep ca.key off the nodes once the certificates are made)
#   DIR/copy.pem copy.key         the copy's server certificate: subjectAltName = each --copy-host (IP or DNS name)
#   DIR/primary.pem primary.key   the primary's client certificate: CN = --primary-cn (the copy's allow-cn)
#   DIR/primary.conf copy.conf    sample objrepl.conf lines for each node
# Keys are written mode 600. Defaults: CNs objrepl-copy, objrepl-primary, objrepl-ca; 825 days.
set -u
DIR=""; HOSTS=""; COPY_CN=objrepl-copy; PRIM_CN=objrepl-primary; CA_CN=objrepl-ca; DAYS=825; APPLY=0
while [ $# -gt 0 ]; do
  case "$1" in
    --copy-host) HOSTS=$2; shift 2 ;;
    --copy-cn) COPY_CN=$2; shift 2 ;;
    --primary-cn) PRIM_CN=$2; shift 2 ;;
    --ca-cn) CA_CN=$2; shift 2 ;;
    --days) DAYS=$2; shift 2 ;;
    --apply) APPLY=1; shift ;;
    -*) echo "unknown option $1" >&2; exit 64 ;;
    *) DIR=$1; shift ;;
  esac
done
[ -n "$DIR" ] && [ -n "$HOSTS" ] || { echo "usage: objrepl_certs.sh DIR --copy-host HOST[,HOST...] [--apply]" >&2; exit 64; }
SAN=""
IFS=, read -ra HS <<< "$HOSTS"
for h in "${HS[@]}"; do
  if [[ "$h" =~ ^[0-9.]+$ || "$h" == *:* ]]; then SAN="$SAN${SAN:+,}IP:$h"; else SAN="$SAN${SAN:+,}DNS:$h"; fi
done
FILES="ca.pem ca.key copy.pem copy.key primary.pem primary.key primary.conf copy.conf"
for f in $FILES; do [ -e "$DIR/$f" ] && { echo "$DIR/$f is there already: nothing written" >&2; exit 1; }; done
echo "CA       $DIR/ca.pem (CN $CA_CN, $DAYS days)"
echo "copy     $DIR/copy.pem (CN $COPY_CN, subjectAltName $SAN)"
echo "primary  $DIR/primary.pem (CN $PRIM_CN)"
echo "configs  $DIR/primary.conf $DIR/copy.conf"
if [ $APPLY -eq 0 ]; then echo "dry run: run again with --apply to write them"; exit 0; fi
mkdir -p "$DIR" || exit 1
umask 077
W=$(mktemp -d "$DIR/.certs.XXXXXX") || exit 1
trap 'rm -rf "$W"' EXIT
openssl req -x509 -newkey rsa:3072 -nodes -days "$DAYS" -subj "/CN=$CA_CN" -keyout "$W/ca.key" -out "$W/ca.pem" 2>/dev/null || exit 1
mk() { openssl req -newkey rsa:3072 -nodes -subj "/CN=$2" -keyout "$W/$1.key" -out "$W/$1.csr" 2>/dev/null || return 1
       printf '%s\n' "$3" > "$W/$1.ext"
       openssl x509 -req -in "$W/$1.csr" -CA "$W/ca.pem" -CAkey "$W/ca.key" -CAcreateserial -days "$DAYS" -extfile "$W/$1.ext" -out "$W/$1.pem" 2>/dev/null; }
mk copy "$COPY_CN" "subjectAltName=$SAN
extendedKeyUsage=serverAuth" || exit 1
mk primary "$PRIM_CN" "extendedKeyUsage=clientAuth" || exit 1
cat > "$W/primary.conf" <<CONF
# objrepl.conf for the primary (copy the .pem/.key files with it; not ca.key)
name primary1
peer-name copy1
copy ${HS[0]}:24600
ca /path/to/ca.pem
cert /path/to/primary.pem
key /path/to/primary.key
CONF
cat > "$W/copy.conf" <<CONF
# objrepl.conf for the copy
name copy1
peer-name primary1
listen ${HS[0]}:24600
ca /path/to/ca.pem
cert /path/to/copy.pem
key /path/to/copy.key
allow-cn $PRIM_CN
CONF
for f in $FILES; do mv -n "$W/$f" "$DIR/$f" || exit 1; done
chmod 600 "$DIR"/*.key; chmod 644 "$DIR"/*.pem "$DIR"/*.conf
echo "written to $DIR (keys mode 600)"
