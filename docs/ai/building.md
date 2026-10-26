# Building and linking DataReel 7

## Requirements

- A C++17 compiler: GCC 8 or later (RHEL 8, 9, 10) or Visual Studio
  2022/2026 (64-bit).
- CMake 3.16 or later.
- TLS: OpenSSL 1.1.1 or later on Linux (`openssl-devel`); on Windows
  Schannel (built in, the default) or OpenSSL 3.x.

## Build the library

```sh
# Linux
sudo dnf install gcc-c++ cmake openssl-devel
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j
sudo cmake --install cmake-build              # /usr/local
```

```bat
rem Windows, Developer Command Prompt
cmake -S . -B cmake-build -G "Visual Studio 17 2022" -A x64
cmake --build cmake-build --config Release
cmake --install cmake-build --config Release --prefix C:\datareel
rem OpenSSL instead of Schannel:
rem   -DDATAREEL_SSL=OPENSSL -DOPENSSL_ROOT_DIR=C:/openssl64
```

## Options

| Option | Default | Meaning |
|--------|---------|---------|
| `DATAREEL_SSL` | `OPENSSL` (Linux), `SCHANNEL` (Windows) | TLS backend |
| `DATAREEL_64BIT_DATABASE` | `OFF` | 64-bit file addresses (files over 2 GB). Changes the file format. |
| `DATAREEL_NATIVE_TYPES` | `ON` | Native integer/float types in records. `OFF` uses portable byte-order classes so files can move between Linux and Windows. Changes the file format. |
| `DATAREEL_GUI` | `CONSOLE` | `WX` shows errors in wxWidgets message boxes |
| `BUILD_SHARED_LIBS` | `OFF` | Shared library (`libdatareel.so.7`, `datareel.dll`) |
| `DATAREEL_BUILD_EXAMPLES` | `ON` | Examples, utilities, tests and the docs/ai samples |

Pick the two file format options once per project and keep them: a
program can only open files written with the same settings.

## Use it from your project

```cmake
cmake_minimum_required(VERSION 3.16)
project(myapp CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(DataReel 7 REQUIRED)         # after cmake --install
# or: add_subdirectory(third_party/datareel)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE DataReel::datareel)
```

The `DataReel::datareel` target adds the include directory, the
compile definitions that select the platform and file format, OpenSSL
or the Windows libraries, and threads. Compiling DataReel headers
without those definitions fails with "You must define a target
platform and/or compiler".

Without CMake (Linux), `build/makefile` builds `lib/libdatareel.a`
(`cd build; make; make install`; `make LARGEFILES=1` for the 64-bit
engine). Copy the compile flags from `env/linux.env`.

## Test

```sh
tests/run_tests.sh                       # build in ./cmake-build and run all checks
CMAKE_OPTS="-DDATAREEL_64BIT_DATABASE=ON" tests/run_tests.sh /tmp/dr-b64
CMAKE_OPTS="-DBUILD_SHARED_LIBS=ON" tests/run_tests.sh /tmp/dr-so
```

The script makes a test CA and server certificate, runs the database
examples, the TLS examples against each other, the mail and FTPS
clients against `tests/mock_servers.py` (needs python3), the docs/ai
samples, and `tests/damaged_files`, which opens thousands of damaged
database files.

## Run the examples

Built programs are in `cmake-build/examples/`. Database examples write
their files to the current directory. TLS examples take
`--ca ca.pem` to trust a test CA; see `docs/examples/examples.htm`.
