# AGENTS.md: DataReel 7 for AI coding assistants

DataReel 7 is a C++17 library for two jobs the C++ standard library does
not cover:

1. **An embedded object database**: block files with free space reuse
   and CRCs (`gxDatabase`), disk B-tree indexes (`gxBtree`), a data file
   plus its indexes opened together (`POD`), persistent objects with
   variable length members (`InfoHog`) and simple relational tables
   (`gxRDBMS`).
2. **TLS communications**: TLS 1.2/1.3 client and server connections
   (`gxsTLSConnection`, `gxsTLSServer`) and HTTPS, SMTP, POP3, FTPS and
   database-block (`gxStream`) protocols on top of them.

For anything else (strings, containers, threads, files, time, JSON,
SQL) use the C++ standard library or a dedicated library. DataReel 7
removed its old string, list, thread and file classes.

The task guides are in `docs/ai/`; `llms-full.txt` has all of them in
one file. Every sample program in them is built and run by
`tests/run_tests.sh`.

## Rules when writing code that uses DataReel

- Include the class header (`gxstls.h`, `gxshttpc.h`, `gxdbase.h`,
  `pod.h`, `infohog.h`, `gxrdbms.h`, ...). Everything is in
  `namespace datareel`; `using namespace datareel;` is added by the
  headers unless `DATAREEL_NO_GLOBAL_NAMES` is defined.
- Link with CMake: `find_package(DataReel 7 REQUIRED)` and
  `target_link_libraries(app PRIVATE DataReel::datareel)`. The target
  carries required compile definitions; do not compile DataReel
  headers without them (you get "You must define a target platform").
- Return values differ by area. Check each one:
  - Network classes (`Connect`, `Listen`, `Accept`, `Login`, `Get`,
    ...) return `0` on success and `-1` on failure, with the message
    in `Error()`. `ReadLine()` returns `bool`.
  - `gxDatabase`, `gxBtree::Create/Open`, `POD::Open` and
    `gxRDBMS::CreateTable` return `gxDatabaseError`;
    `gxDBASE_NO_ERROR` (0) is success. Get text with
    `DatabaseExceptionMessage()` or `gxDatabaseExceptionMessage(err)`.
  - `gxBtree::Insert/Find/Delete/FindFirst/FindNext`, `InfoHog`
    `WriteObject/ReadObject/FindObject/DeleteObject` and `gxRDBMS`
    `AddRecord/FindRecord/ChangeRecord` return non-zero (true) on
    success.
  - `gxStream` functions return `gxSOCKET_NO_ERROR` (0) on success.
- TLS: certificates are verified against the system store and the
  host name by default. For a private CA call `SetCAFile("ca.pem")`.
  Only use `SetVerifyPeer(false)` in tests. There is no plain text
  HTTP, SMTP, POP3 or FTP; do not try to add it. Only telnet and ping
  are plain text, for diagnostics.
- Records and keys are written as raw bytes. Use fixed-size members:
  `char name[N]`, `gxINT32`, `gxUINT32`, `gxINT64`, `gxFLOAT64`,
  `CDate`, `FAU`. Never store pointers, `std::string`, `std::vector`
  or classes with virtual functions in a record or key. `memset` a
  record to zero before filling it so padding bytes are not random.
- Key classes derive from `DatabaseKeyB`, pass the key data address to
  the base constructor, and implement `KeySize()` and
  `CompareKey()` (the CMake build defines `__USE_SINGLE_COMPARE__`).
  Also define `operator==` and `operator>` so the class works without
  that macro.
- File layout depends on two build settings: `DATAREEL_64BIT_DATABASE`
  and `DATAREEL_NATIVE_TYPES`. A program must use the settings the
  files were created with. Linux and Windows native builds write
  different layouts (64-bit Linux `long` is 8 bytes); use
  `DATAREEL_NATIVE_TYPES=OFF` on both to share files.
- `gxDatabase` objects are not copyable and one object must not be
  used from several threads at once without your own mutex.
- Object addresses (`FAU_t`) returned by `Alloc()` stay valid until
  the object is deleted; store them in your index keys.

## Rules when changing DataReel itself

- C++17 minimum (RHEL 8 has GCC 8). Keep the `gx` class names and the
  `datareel` namespace.
- Never change the on-disk format: block and file headers, B-tree
  nodes, `InfoHog` and `gxRDBMS` record layout, CRC tables, or
  `gxrdDatabaseRecord::CreateKey()` (index keys are CRCs of field
  bytes). 7.0 writes byte-identical files to 6.2; keep it that way.
- Treat every length and count read from a file or the network as
  untrusted: check it against the buffer or a limit before use.
- New network code uses `gxsTLSConnection`; do not add plain text
  protocols.
- Build and test before committing:

```sh
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j
tests/run_tests.sh cmake-build                    # 42 checks
CMAKE_OPTS="-DDATAREEL_64BIT_DATABASE=ON -DDATAREEL_NATIVE_TYPES=OFF" \
  tests/run_tests.sh /tmp/dr-b64                   # the other layouts
```

- Windows: `cmake -S . -B cmake-build -G "Visual Studio 17 2022" -A x64`
  then `cmake --build cmake-build --config Release`. Schannel is the
  default TLS backend; `-DDATAREEL_SSL=OPENSSL` selects OpenSSL.
- If you change a sample in `docs/ai/samples`, run
  `python3 docs/bin/make_ai_docs.py` to update the guides and
  `llms-full.txt`.

## Map

| Path | What |
|------|------|
| `include/`, `src/` | Library headers and sources |
| `examples/database/` | gxDatabase, gxBtree, POD, InfoHog, gxRDBMS programs |
| `examples/tls/` | TLS echo, HTTPS, SMTP, POP3, FTPS, gxStream programs |
| `examples/general/` | gxConfig, LogFile, ThreadPool |
| `docs/ai/` | Task guides and their sample programs |
| `docs/classes/` | HTML reference for each class |
| `tests/` | `run_tests.sh`, mock mail/FTPS servers, damaged file test |
| `utils/` | `db_debug`, `ix_debug` file analysis and repair tools |
