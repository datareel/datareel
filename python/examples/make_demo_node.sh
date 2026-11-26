#!/bin/bash
# make_demo_node.sh - DataReel 7.5: a demo gxObjRepl node for the Python examples. Throwaway folders only.
#
#   python/examples/make_demo_node.sh BUILD DIR [COMMITS]
#
# BUILD: a CMake build of DataReel with the 64-bit engine (cmake -DDATAREEL_64BIT_DATABASE=ON) that has built
# utils/test_gxobjrepl_reader. DIR: an empty folder; the node goes in DIR/node, its test certificates in DIR/pki.
# COMMITS: 300 by default. The node has two databases, written by test_gxobjrepl_reader (C++):
#   big  one 12000-byte object, rewritten by every commit: the commit number (8 bytes), then bytes from it
#   vec  one object per commit: the commit number as an 8-byte little-endian key, then 4 floats (k, 0, 0, 0)
# and two indexes: k0 (a B-tree on the 8-byte key) and v0 (a vector index on the 4 floats):
#   INDEX="k0:vec:B:0:8,v0:vec:V:8:4"
set -eu
BUILD=${1:?usage: make_demo_node.sh BUILD DIR [COMMITS]}
DIR=${2:?usage: make_demo_node.sh BUILD DIR [COMMITS]}
N=${3:-300}
W=$(find "$BUILD" -name test_gxobjrepl_reader -type f -perm -u+x | head -n 1)
[ -n "$W" ] || { echo "make_demo_node.sh: no test_gxobjrepl_reader under $BUILD (build the utils with the 64-bit engine)"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$DIR"
[ -e "$DIR/pki" ] || "$HERE/../../utils/objrepl/objrepl_certs.sh" "$DIR/pki" --copy-host 127.0.0.1 --days 30 --apply > /dev/null
"$W" write "$DIR/node" "$DIR/pki" "$N" | tail -n 1
echo "demo node: $DIR/node (index k0:vec:B:0:8,v0:vec:V:8:4)"
