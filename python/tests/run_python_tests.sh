#!/bin/bash
# run_python_tests.sh - DataReel 7.5: builds the datareel wheel and the C++ test programs, installs the wheel
# in a throwaway virtual environment and runs python/tests/test_datareel.py and test_examples.py (every program in
# python/examples). Throwaway folders only.
#
#   python/tests/run_python_tests.sh WORK [PYTHON]      (from the repository top; PYTHON: python3.12 by default)
#
# Needs a C++17 compiler, CMake, the OpenSSL headers and the Python headers (python3.12-devel). WORK gets the wheel
# (WORK/dist), the C++ build (WORK/cpp) and the virtual environment (WORK/venv). DATAREEL_JOBS sets the build jobs (2).
set -eu
WORK=${1:?usage: run_python_tests.sh WORK [PYTHON]}
PY=${2:-python3.12}
SRC=$(cd "$(dirname "$0")/../.." && pwd)
JOBS=${DATAREEL_JOBS:-2}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
echo "== wheel"
rm -rf "$WORK/dist"
DATAREEL_JOBS=$JOBS "$PY" -m pip wheel "$SRC/python" --no-build-isolation --no-deps -w "$WORK/dist" > "$WORK/wheel.log" 2>&1 \
  || { tail -n 30 "$WORK/wheel.log"; echo "FAIL the wheel did not build"; exit 1; }
ls "$WORK/dist"
echo "== C++ test programs (64-bit engine)"
cmake -S "$SRC" -B "$WORK/cpp" -DCMAKE_BUILD_TYPE=Release -DDATAREEL_64BIT_DATABASE=ON -DDATAREEL_BUILD_EXAMPLES=ON > "$WORK/cpp.log" 2>&1
cmake --build "$WORK/cpp" -j "$JOBS" --target objrepl test_gxobjrepl_reader object_server >> "$WORK/cpp.log" 2>&1 \
  || { tail -n 30 "$WORK/cpp.log"; echo "FAIL the C++ test programs did not build"; exit 1; }
BIN=$(dirname "$(find "$WORK/cpp" -name test_gxobjrepl_reader -type f | head -n 1)")
echo "== virtual environment"
rm -rf "$WORK/venv"
"$PY" -m venv "$WORK/venv"
"$WORK/venv/bin/python" -m pip install --no-index --no-deps "$WORK"/dist/datareel-*.whl > "$WORK/install.log" 2>&1 \
  || { cat "$WORK/install.log"; echo "FAIL the wheel did not install"; exit 1; }
echo "== tests"
cd "$WORK"      # not the source tree: the installed package is the one tested
export DR_TEST_BIN="$BIN" DR_TEST_CERTS_SH="$SRC/utils/objrepl/objrepl_certs.sh"
"$WORK/venv/bin/python" -I "$SRC/python/tests/test_datareel.py" -v
echo "== the examples (python/examples, as the user's manual runs them)"
"$WORK/venv/bin/python" -I "$SRC/python/tests/test_examples.py" -v
