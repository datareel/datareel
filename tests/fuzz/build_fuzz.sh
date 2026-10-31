#!/bin/bash
# build_fuzz.sh SRC OUT - build the libFuzzer targets in tests/fuzz with
# clang, AddressSanitizer and UndefinedBehaviorSanitizer (64-bit engine).
# Needs clang, compiler-rt, cmake and openssl-devel. Run the journal
# target only in a container or throwaway folder (see fuzz_db.cpp).
set -e
SRC=${1:?usage: build_fuzz.sh SRC OUT}; OUT=${2:?usage: build_fuzz.sh SRC OUT}
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O1"
cmake -S "$SRC" -B "$OUT/lib" -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DDATAREEL_64BIT_DATABASE=ON \
  -DDATAREEL_BUILD_EXAMPLES=OFF \
  -DCMAKE_CXX_FLAGS="$SAN -fsanitize=fuzzer-no-link" > "$OUT/cmake.log"
cmake --build "$OUT/lib" -j"$(nproc)" > "$OUT/build.log"
DEFS=$(grep -o -- '-D[A-Za-z0-9_=]*' "$OUT/lib/CMakeFiles/datareel.dir/flags.make" | sort -u | tr '\n' ' ')
for t in GXD BTX JNL; do
  clang++ -std=c++17 $SAN -fsanitize=fuzzer $DEFS -DFUZZ_$t -I"$SRC/include" \
    "$SRC/tests/fuzz/fuzz_db.cpp" "$OUT/lib/libdatareel.a" -lssl -lcrypto -lpthread \
    -o "$OUT/fuzz_$(echo $t | tr A-Z a-z)"
done
clang++ -std=c++17 $SAN $DEFS -I"$SRC/include" "$SRC/tests/fuzz/gen_seeds.cpp" \
  "$OUT/lib/libdatareel.a" -lssl -lcrypto -lpthread -o "$OUT/gen_seeds"
echo "built $OUT/fuzz_gxd $OUT/fuzz_btx $OUT/fuzz_jnl $OUT/gen_seeds"
