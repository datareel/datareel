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
# 11/01/2026: Security sweep targets: the stream (networked OODB)
# protocol, HTTP/FTP/SMTP/POP3/URL/base64 parsers, and telnet, config,
# date, ping and db_debug parsing. fuzz_stream needs DATAREEL_FUZZING.
clang++ -std=c++17 $SAN -fsanitize=fuzzer -DDATAREEL_FUZZING $DEFS -I"$SRC/include" \
  -I"$SRC/examples/tls" "$SRC/tests/fuzz/fuzz_stream.cpp" "$OUT/lib/libdatareel.a" \
  -lssl -lcrypto -lpthread -o "$OUT/fuzz_stream"
for t in proto support; do
  clang++ -std=c++17 $SAN -fsanitize=fuzzer $DEFS -I"$SRC/include" -I"$SRC/utils/db_debug" \
    "$SRC/tests/fuzz/fuzz_$t.cpp" "$OUT/lib/libdatareel.a" -lssl -lcrypto -lpthread \
    -o "$OUT/fuzz_$t"
done
# 11/14/2026: 7.4 scan target: ship log records (incl. type 5 objects records), the gxrepl config, .vixset
# manifests, .vix files, the object server's vector ops 7/8, stamps and derived state
clang++ -std=c++17 $SAN -fsanitize=fuzzer $DEFS -I"$SRC/include" "$SRC/tests/fuzz/fuzz_repl.cpp" \
  "$OUT/lib/libdatareel.a" -lssl -lcrypto -lz -lpthread -ldl -o "$OUT/fuzz_repl"
clang++ -std=c++17 $SAN $DEFS -I"$SRC/include" "$SRC/tests/fuzz/gen_seeds.cpp" \
  "$OUT/lib/libdatareel.a" -lssl -lcrypto -lpthread -o "$OUT/gen_seeds"
echo "built $OUT/fuzz_gxd $OUT/fuzz_btx $OUT/fuzz_jnl $OUT/fuzz_stream $OUT/fuzz_proto $OUT/fuzz_support $OUT/fuzz_repl $OUT/gen_seeds"
