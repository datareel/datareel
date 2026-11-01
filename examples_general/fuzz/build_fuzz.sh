#!/bin/bash
# build_fuzz.sh SRC OUT - 6.3: build the libFuzzer targets in
# examples_general/fuzz (fuzz_text.cpp: FUZZ_CFG, FUZZ_STR, FUZZ_B64) with
# clang, AddressSanitizer and UndefinedBehaviorSanitizer. Needs clang,
# compiler-rt, cmake and openssl-devel.
set -e
SRC=${1:?usage: build_fuzz.sh SRC OUT}; OUT=${2:?usage: build_fuzz.sh SRC OUT}
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O1"
cmake -S "$SRC" -B "$OUT/lib" -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DDATAREEL_64BIT_DATABASE=ON \
  -DCMAKE_CXX_FLAGS="$SAN -fsanitize=fuzzer-no-link" > "$OUT/cmake.log"
cmake --build "$OUT/lib" -j"$(nproc)" > "$OUT/build.log"
DEFS=$(grep -o -- '-D[A-Za-z0-9_=]*' "$OUT/lib/CMakeFiles/datareel.dir/flags.make" | sort -u | tr '\n' ' ')
for t in CFG STR B64; do
  clang++ -std=c++17 $SAN -fsanitize=fuzzer $DEFS -DFUZZ_$t -I"$SRC/include" \
    "$SRC/examples_general/fuzz/fuzz_text.cpp" "$OUT/lib/libdatareel.a" -lssl -lcrypto -lpthread \
    -o "$OUT/fuzz_$(echo $t | tr A-Z a-z)"
done
echo "built $OUT/fuzz_cfg $OUT/fuzz_str $OUT/fuzz_b64"
