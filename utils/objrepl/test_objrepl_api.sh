#!/bin/bash
# test_objrepl_api.sh - 11/16/2026: runs test_gxobjrepl_api (the gxObjRepl calls the tool does not make: FindKey,
# Nearest, Rollback, Purge rules, config errors, a copy serving in a thread) with throwaway certificates.
#
#   utils/objrepl/test_objrepl_api.sh BIN      (BIN: the objrepl tool; test_gxobjrepl_api is built next to it)
set -u
X=${1:?usage: test_objrepl_api.sh BIN}
# 11/16/2026: a 32-bit engine build is refused (its database files are not the 64-bit engine's), unless
# OBJREPL_ALLOW_32BIT=1 says a 32-bit run is meant (the run makes its own folders, so no engine reads the other's files)
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] && ! "$X" engine > /dev/null 2>&1 && echo "$(basename "$0"): a 32-bit engine run (OBJREPL_ALLOW_32BIT=1)"
[ "${OBJREPL_ALLOW_32BIT:-0}" == 1 ] || "$X" engine > /dev/null 2>&1 || { echo "$(basename "$0"): $X is not a 64-bit engine build: $("$X" engine 2>&1 | head -n 1); configure with -DDATAREEL_64BIT_DATABASE=ON"; exit 64; }
A=$(dirname "$X")/test_gxobjrepl_api
HERE=$(cd "$(dirname "$0")" && pwd)
[ -x "$A" ] || { echo "FAIL api: $A is not built"; echo "0 passed, 1 failed"; exit 1; }
T=$(mktemp -d "${TMPDIR:-/tmp}/objrepl_api.XXXXXX")
trap 'rm -rf "$T"' EXIT
"$HERE/objrepl_certs.sh" "$T/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null || { echo "FAIL api: certificates"; exit 1; }
# 11/16/2026: 600 s: about 80 s on Linux, 240 s on the Windows VM (r7-1's two writers)
timeout 600 "$A" "$T" "$T/pki" $((20000 + RANDOM % 10000))
