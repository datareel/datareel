#!/bin/bash
# build_fuzz.sh SRC OUT - 6.3: build the libFuzzer targets in
# examples_general/fuzz with clang, AddressSanitizer and
# UndefinedBehaviorSanitizer. Needs clang, compiler-rt, cmake and
# openssl-devel.
#   fuzz_text.cpp:    FUZZ_CFG, FUZZ_STR, FUZZ_B64
#   fuzz_parsers.cpp: FUZZ_HTML, FUZZ_XML, FUZZ_RSS, FUZZ_RDBDEF,
#                     FUZZ_RDBREC, FUZZ_DFILE, FUZZ_MEMBUF, FUZZ_MISC
# Each target is OUT/fuzz_<name> (lower case). Run one in an empty
# directory (the file targets write fz.* files there), for example:
#   mkdir run && cd run && ../fuzz_xml -max_total_time=600 corpus_xml
set -e
SRC=${1:?usage: build_fuzz.sh SRC OUT}; OUT=${2:?usage: build_fuzz.sh SRC OUT}
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O1"
cmake -S "$SRC" -B "$OUT/lib" -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DDATAREEL_64BIT_DATABASE=ON \
  -DCMAKE_CXX_FLAGS="$SAN -fsanitize=fuzzer-no-link" > "$OUT/cmake.log"
cmake --build "$OUT/lib" -j"$(nproc)" > "$OUT/build.log"
DEFS=$(grep -o -- '-D[A-Za-z0-9_=]*' "$OUT/lib/CMakeFiles/datareel.dir/flags.make" | sort -u | tr '\n' ' ')
build() { # build FILE TARGET
  clang++ -std=c++17 $SAN -fsanitize=fuzzer $DEFS -DFUZZ_$2 -I"$SRC/include" \
    "$SRC/examples_general/fuzz/$1" "$OUT/lib/libdatareel.a" -lssl -lcrypto -lpthread \
    -o "$OUT/fuzz_$(echo $2 | tr A-Z a-z)"
  echo "built $OUT/fuzz_$(echo $2 | tr A-Z a-z)"
}
for t in CFG STR B64; do build fuzz_text.cpp $t; done
for t in HTML XML RSS RDBDEF RDBREC DFILE MEMBUF MISC; do build fuzz_parsers.cpp $t; done
