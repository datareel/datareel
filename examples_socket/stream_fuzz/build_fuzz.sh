#!/bin/bash
# build_fuzz.sh SRC OUT - 6.3: build the libFuzzer target in
# examples_socket/stream_fuzz (fuzz_stream.cpp: gxStream block headers
# and blocks read from the peer) with clang, AddressSanitizer and
# UndefinedBehaviorSanitizer. Needs clang, compiler-rt, cmake and
# openssl-devel. Run: OUT/fuzz_stream -max_len=16384 -timeout=10 CORPUS
set -e
SRC=${1:?usage: build_fuzz.sh SRC OUT}; OUT=${2:?usage: build_fuzz.sh SRC OUT}
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O1"
cmake -S "$SRC" -B "$OUT/lib" -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DDATAREEL_64BIT_DATABASE=ON \
  -DCMAKE_CXX_FLAGS="$SAN -fsanitize=fuzzer-no-link" > "$OUT/cmake.log"
cmake --build "$OUT/lib" -j"$(nproc)" > "$OUT/build.log"
DEFS=$(grep -o -- '-D[A-Za-z0-9_=]*' "$OUT/lib/CMakeFiles/datareel.dir/flags.make" | sort -u | tr '\n' ' ')
clang++ -std=c++17 $SAN -fsanitize=fuzzer $DEFS -I"$SRC/include" \
  "$SRC/examples_socket/stream_fuzz/fuzz_stream.cpp" "$OUT/lib/libdatareel.a" \
  -lssl -lcrypto -lpthread -o "$OUT/fuzz_stream"
echo "built $OUT/fuzz_stream"
