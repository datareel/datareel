# Building the datareel Python package (DataReel 7.5)

This guide builds the `datareel` wheel from the DataReel source, tests it, and installs it. The package
is one C++ extension (`datareel._datareel`) plus a small Python wrapper. The DataReel C++ library is built
from the same source tree and linked into the extension, so the wheel needs no DataReel install on the
target machine.

## What you need

| Item | RHEL 9 and RHEL 10 package | Notes |
| --- | --- | --- |
| C++17 compiler | `gcc-c++` | GCC 11 (RHEL 9) or GCC 14 (RHEL 10) |
| CMake 3.16 or later | `cmake` | setup.py runs it |
| OpenSSL 3 headers | `openssl-devel` | the wheel links the system `libssl.so.3` |
| Python 3.12 and its headers | `python3.12`, `python3.12-devel` | `Python.h` |
| setuptools and wheel | `python3.12-setuptools`, `python3.12-wheel` | from the Red Hat repositories |
| pip | `python3.12-pip` | |

No third-party Python packages are needed to build or to run the module: the extension uses the plain
CPython C API. numpy is optional; the examples use it when it is installed.

## Build the wheel

From the top of the DataReel source tree (tag `7.5`):

```
python3.12 -m pip wheel python/ --no-build-isolation --no-deps -w dist/
```

`--no-build-isolation` uses the setuptools and wheel already installed, so pip downloads nothing. The
result is `dist/datareel-7.5-cp312-cp312-linux_x86_64.whl`.

What happens inside:

1. `python/setup.py` runs CMake on `python/CMakeLists.txt` in pip's build folder.
2. That project adds the DataReel source as a subproject with the 64-bit database engine
   (`DATAREEL_64BIT_DATABASE=ON`), builds it as a static library with position independent code, and
   builds `_datareel` with CMake's `Python3_add_library`.
3. setup.py copies the extension into the wheel next to `datareel/__init__.py`.

Settings:

- `DATAREEL_JOBS=N` sets the parallel build jobs (default 2). Keep it low on a shared machine.
- `DATAREEL_SOURCE=DIR` builds from another DataReel source tree (the folder that holds `include/` and
  `src/`). By default it is the folder above `python/`.

## Build in a container (no packages on the host)

When the build machine should not get the development packages (a production host, or RHEL 10 without
`python3.12-devel`), build in a UBI 9 container. A wheel built on RHEL 9 runs on RHEL 9 and RHEL 10.

```
cat > Containerfile <<'END'
FROM registry.access.redhat.com/ubi9:latest
RUN dnf -y install gcc-c++ cmake make openssl openssl-devel python3.12 python3.12-devel \
    python3.12-pip python3.12-setuptools python3.12-wheel diffutils findutils && dnf clean all
END
podman build -t drpy-ubi9 .
podman run --rm -v "$PWD":/w:Z drpy-ubi9 bash -c \
  'cd /w && python3.12 -m pip wheel python/ --no-build-isolation --no-deps -w dist/'
```

## Test it

`python/tests/run_python_tests.sh` builds the wheel and the C++ test programs, installs the wheel in a
throwaway virtual environment, and runs both test files from outside the source tree:

```
python/tests/run_python_tests.sh /tmp/drpy          # WORK folder; PYTHON is python3.12 by default
```

- `python/tests/test_datareel.py`: a node written by the C++ library, read through every call; readers in
  Python threads while a C++ primary writes 2000 commits and while a copy applies 800; the node's vector
  index file; a block file.
- `python/tests/test_examples.py`: runs every program in `python/examples/` against a demo node and checks
  what it prints, including example 6 following a running C++ writer.

Every test must pass. A test that fails once on a loaded machine is run again on its own before it is
treated as a bug; never skip one.

To test a wheel on another machine (RHEL 10, for example), install it in a virtual environment and run the
two test files with `DR_TEST_BIN` set to a folder that holds the C++ `objrepl` and
`test_gxobjrepl_reader` programs (a 64-bit engine build of DataReel) and `DR_TEST_CERTS_SH` set to
`utils/objrepl/objrepl_certs.sh`:

```
python3.12 -m venv /tmp/drvenv
/tmp/drvenv/bin/python -m pip install --no-index --no-deps dist/datareel-7.5-cp312-cp312-linux_x86_64.whl
cd /tmp
DR_TEST_BIN=$BUILD/utils DR_TEST_CERTS_SH=$SRC/utils/objrepl/objrepl_certs.sh \
  /tmp/drvenv/bin/python -I $SRC/python/tests/test_datareel.py
```

## Install

```
python3.12 -m pip install --no-index datareel-7.5-cp312-cp312-linux_x86_64.whl
python3.12 -c "import datareel; print(datareel.__version__, datareel.library_version())"
```

The second line prints `7.5 7.5`. Install into a virtual environment, or with `--user`, rather
than into the system Python.

## Problems

| What you see | Why | What to do |
| --- | --- | --- |
| `fatal error: Python.h: No such file or directory` | no Python headers | install `python3.12-devel`, or build in the container |
| `Could NOT find OpenSSL` | no OpenSSL headers | install `openssl-devel` |
| `invalid command 'bdist_wheel'` | no wheel package | install `python3.12-wheel` |
| `the DataReel source is not at ...` | `python/` was copied without the rest of the tree | build from the whole tree, or set `DATAREEL_SOURCE` |
| `ImportError: libssl.so.3` on the target | OpenSSL 3 missing there | RHEL 9 and 10 have it; older systems are not supported |
| `datareel.Error: cannot lock .../node.jnl (gxObjReplReader needs the 64-bit engine)` | not seen with this wheel (it is always 64-bit) | report it with the full message |

## Windows (a build script, not tested or distributed)

DataReel ships and tests Linux wheels only. So that a Windows wheel can be built if one is ever needed,
`python\build_wheel_windows.bat` builds one with Visual Studio (2022 or later, x64), CMake, CPython 3.12 x64 and
OpenSSL 3.x for x64. It has **not been tested**, and no Windows wheel is published. From a "x64 Native Tools Command
Prompt for VS", at the repository top:

```
py -3.12 -m pip install setuptools wheel
python\build_wheel_windows.bat "C:\Program Files\OpenSSL-Win64"
py -3.12 -m pip install --no-index dist\datareel-<version>-cp312-cp312-win_amd64.whl
py -3.12 -c "import datareel; print(datareel.library_version())"
```

`python/CMakeLists.txt` builds DataReel with OpenSSL on every platform (`DATAREEL_SSL OPENSSL`): Windows builds
of DataReel default to Schannel, but replication and gxStream use PEM certificates. The script sets
`OPENSSL_ROOT_DIR` for CMake, runs the same `pip wheel` as on Linux, and adds OpenSSL's two DLLs
(`libssl-3-x64.dll`, `libcrypto-3-x64.dll`) to the wheel next to `_datareel.pyd`. Before using such a wheel, run
`python/tests/run_python_tests.sh`'s tests on Windows (they need the C++ test programs built with the 64-bit
engine), and expect to fix what they find.

## Other Python versions

Wheels for other Python versions are not built. The extension's source builds with any CPython 3.10 or later
that has its headers, but only 3.12 is tested.
