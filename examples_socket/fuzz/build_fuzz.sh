#!/bin/bash
# build_fuzz.sh SRC OUT - 6.3: build the libFuzzer targets in
# examples_socket/fuzz (fuzz_proto.cpp: FUZZ_HTTPHDR, FUZZ_URL,
# FUZZ_REPLY, FUZZ_RECV) with clang, AddressSanitizer and
# UndefinedBehaviorSanitizer. Needs clang, compiler-rt, cmake and
# openssl-devel. Run a target with, for example:
#   OUT/fuzz_httphdr -max_total_time=600 corpus_dir
set -e
SRC=${1:?usage: build_fuzz.sh SRC OUT}; OUT=${2:?usage: build_fuzz.sh SRC OUT}
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O1"
cmake -S "$SRC" -B "$OUT/lib" -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DDATAREEL_64BIT_DATABASE=ON \
  -DCMAKE_CXX_FLAGS="$SAN -fsanitize=fuzzer-no-link" > "$OUT/cmake.log"
cmake --build "$OUT/lib" -j"$(nproc)" > "$OUT/build.log"
DEFS=$(grep -o -- '-D[A-Za-z0-9_=]*' "$OUT/lib/CMakeFiles/datareel.dir/flags.make" | sort -u | tr '\n' ' ')
for t in HTTPHDR URL REPLY RECV; do
  clang++ -std=c++17 $SAN -fsanitize=fuzzer $DEFS -DFUZZ_$t -I"$SRC/include" \
    "$SRC/examples_socket/fuzz/fuzz_proto.cpp" "$OUT/lib/libdatareel.a" -lssl -lcrypto -lpthread \
    -o "$OUT/fuzz_$(echo $t | tr A-Z a-z)"
done
echo "built $OUT/fuzz_httphdr $OUT/fuzz_url $OUT/fuzz_reply $OUT/fuzz_recv"
