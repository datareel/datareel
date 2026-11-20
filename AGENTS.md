# DataReel 7: AI-assisted setup, builds, upgrades, forks and support

This file is for AI-assisted work with DataReel 7, the C++ object
database (OODB) and TLS communications library. It tells an AI agent
(Gemini CLI, Claude Code, GitHub Copilot, Codex and others) how to
assist a site's own developers and system administrators to set up
DataReel, build it and programs that use it, upgrade from older
releases, maintain a fork, and troubleshoot. The agent assists; it does
not replace the developers, the administrators or DataReel support, and
it never acts autonomously (section 0). The people review and approve
every step, and problems the agent cannot solve go to DataReel
(section 7.4). `GEMINI.md` and `CLAUDE.md` in this folder only import
this file.

The task guides with complete programs are in `docs/ai/`, and
`llms-full.txt` has this file and every guide in one file. The full
documentation is `README.txt`, the release notes, `docs/classes/` and
https://datareel.com/clib7/. This file points to them instead of
repeating them. When this file and the code disagree, the code wins:
say so to the person you work for.

DataReel 7.4.1 is the main, fully supported release. DataReel 6.4.2 is
the legacy release: bug fixes and security updates only, no new
features (it has its own `AGENTS.md`).

## Before you use AI assistance

**Review all of your agency's or company's AI usage policies, and get formal
written approval from your management team, in the form of a business use
case and justification, before you use any AI assistance with DataReel.**
AI desktops and command line tools may be used with this guide only in
strict compliance with those policies and that approval.

- **Use only approved enterprise AI models.** An AI agent working on
  source code or a server may process potentially sensitive data: source
  code, database files, configuration files, certificates, logs and
  command output. Use only your agency's or company's enterprise AI
  models that have built-in safeguards against disclosing information
  and that guarantee your internal information will not be used to
  train public models.
- **Warning: public or free AI tools, open source AI models and self-built
  AI models** may expose your information to public learning models or to
  other parties, depending on how the model is hosted, configured and
  licensed. Use them only after a comprehensive review of what information
  might be exposed and what the potential risks are.
- The entire risk of AI-assisted use rests solely with the user. DataReel
  Software Development cannot be held accountable for actions taken,
  commands run, code changed, information exposed or other outcomes of AI
  assistance (see https://datareel.com/terms.php#ai-assisted).
- **No endorsement.** Any mention of a specific AI tool, model or product
  in this guide, such as Gemini, Gemini CLI, Claude Code or GitHub Copilot,
  is a reference only. It is not an endorsement or a recommendation to use
  that product. Product names are trademarks of their respective owners.
- DataReel's AI-assisted guides send nothing to DataReel. Anything given
  to an AI tool goes to that tool's provider, under the provider's terms
  and your organization's policies (https://datareel.com/privacy.php#ai-assisted).

**For the agent:** at the start of a session, ask the person to confirm
that this AI-assisted use is approved under their organization's AI usage
policy and that you are an approved enterprise AI model. If they cannot
confirm it, stop and do not ask for or read any of their code, data files
or system output.

## 0. Human in the loop: the agent never acts on its own

AI-assisted means the agent assists a person; it never acts
autonomously. Every change to code, a build, a server or a database file
is made by a person or explicitly approved by one, one step at a time.

- **Advisory mode is the default.** The agent explains the next step and
  prints the exact command or code change and where it applies. The
  person runs or applies it and pastes the output back. Use this mode
  unless the organization allows the agent to run tools or edit files.
- **If tool execution or file editing is allowed,** each command and each
  change still needs its own explicit approval before it runs. Never use
  auto-approve modes: no Gemini CLI `--yolo` or auto-accept, no Claude
  Code permission bypass (`--dangerously-skip-permissions`, bypass mode),
  no auto-run in other agents.
- **No unattended work:** no scheduled, background, batch or scripted
  runs of changing commands, no loops over servers or database files, and
  no commits, tags or pushes unless the person asks for that exact step.
- **Read-only checks first.** The agent may propose read-only checks
  (section 7.1) before anything else; they are shown and approved the
  same way.

**Works with strict AI policies.** These files contain no code that runs
by itself: `AGENTS.md`, `GEMINI.md`, `CLAUDE.md`, `llms.txt`,
`llms-full.txt` and `docs/ai/` are text. Everything in this guide works
in advisory mode, with no tool execution by the agent.

## 1. Rules for agents (read these first)

DataReel database files hold an application's data, and its TLS classes
protect its network traffic. A wrong step can lose data, make files
unreadable by the programs that use them, or weaken security. Follow
these rules on every task:

1. **Show, then the person runs or approves it.** Before any command that
   changes something (installing packages, `cmake --install`, writing or
   repairing a database file, editing code, committing), show the command
   or the change. In advisory mode the person runs it; otherwise wait for
   an explicit yes for that one step (section 0).
2. **Back up data before changing it.** Copy every database file (`.gxd`
   data files, `.btx` B-tree indexes, `.vix` vector indexes, journals)
   into a dated backup folder before an upgrade, a repair or a test
   against real data. Never delete; move aside. Work on copies when you
   can.
3. **Never change the file format settings of an existing project.**
   `DATAREEL_64BIT_DATABASE` and `DATAREEL_NATIVE_TYPES` must match the
   settings that wrote the files (section 4.3). Changing either one makes
   existing files unreadable by that build.
4. **Stop on any failed check.** If a build, a test or a check fails, or
   the output is not what this guide says to expect, stop and report. Do
   not improvise a fix on production data or a production server.
5. **Never weaken security to make something work.** Do not turn off
   certificate verification (`SetVerifyPeer(false)` is for tests only),
   add plain text protocols, remove length checks, or ignore a checksum
   or CRC error.
6. **Read-only first.** Start every task with read-only checks: the
   version (`version.txt`), the build options in `CMakeCache.txt`,
   `db_debug` and `ix_debug` on copies of the files, `dr_digest`, the
   test results (section 7.1).
7. **No secrets or site data in files you write** (passwords, private
   keys, certificates' keys, host names, addresses, customer data), unless
   the person asks for that file on their own systems.
8. **Keep a log.** List each command you ran or proposed, where, and its
   result, and give it to the person at the end (section 8).

## 2. What DataReel 7 is

DataReel 7 is an object database (OODB) and cross platform
communications toolkit for C++17, for two jobs the C++ standard library
does not cover:

1. **An embedded object database**: block files with free space reuse
   and CRCs (`gxDatabase`), disk B-tree indexes (`gxBtree`), a data file
   plus its indexes opened together (`POD`), persistent objects with
   variable length members (`InfoHog`), crash safe journals
   (`gxdJournal`), vector indexes with exact and HNSW nearest-neighbor
   search (`gxVectorIndex`, 7.4) and object replication (`gxObjRepl`).
2. **TLS communications**: TLS 1.2/1.3 client and server connections
   (`gxsTLSConnection`, `gxsTLSServer`) and HTTPS, SMTP, POP3, FTPS and
   database-block (`gxStream`) protocols on top of them.

For anything else (strings, containers, threads, files, time, JSON,
SQL) use the C++ standard library or a dedicated library (an embedded
SQL database for relational tables). DataReel 7 removed its old string,
list, thread and file classes.

It runs on Linux (RHEL 8, 9 and 10 and compatible systems, GCC 8 or
later) and Windows (Visual Studio 2022 or later, Schannel or OpenSSL).
It is free software under the GNU Lesser General Public License version
3 or later (`LICENSE`), provided as is.

## 3. Setup: download and install a release

Manual: `README.txt` and `docs/ai/building.md`.

1. Download the release from
   https://datareel.com/downloads/datareel7_downloads.php: the source
   (`datareel-7.4.1.tar.gz` or `datareel-7.4.1.zip`) or, on Windows, a prebuilt
   static library (`datareel-7.4.1-windows-x64-schannel.zip`, 64-bit
   database engine, or `-x86-schannel.zip`, 32-bit engine), each with its
   `.sha256sum` file.
2. Check it before unpacking, and stop if it fails:
   `sha256sum -c datareel-7.4.1.tar.gz.sha256sum` (Linux) or
   `Get-FileHash datareel-7.4.1.zip -Algorithm SHA256` compared with the
   `.sha256sum` file (Windows PowerShell).
3. Install the build tools: on RHEL `dnf install gcc-c++ cmake
   openssl-devel` (the person runs it, as root or with sudo); on Windows
   Visual Studio 2022 or later with the C++ workload and CMake.
4. Decide the two file format settings once for the project, before any
   data file is written (section 4.3), and write them down with the
   project.
5. Build, test and install as in section 4.1. Install into a prefix the
   person chooses (`/usr/local` by default, or a project folder).

## 4. New builds

### 4.1 Build and test the library

```sh
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j
tests/run_tests.sh cmake-build                    # every check must pass
sudo cmake --install cmake-build                  # optional, /usr/local
```

- Windows: `cmake -S . -B cmake-build -G "Visual Studio 17 2022" -A x64`
  then `cmake --build cmake-build --config Release`. Schannel is the
  default TLS backend; `-DDATAREEL_SSL=OPENSSL` selects OpenSSL.
- Without CMake (Linux), `build/makefile` builds `lib/libdatareel.a`;
  copy the compile flags from `env/linux.env`.
- The options, the test script and the examples are in
  `docs/ai/building.md`.

### 4.2 Rules when writing code that uses DataReel

- Include the class header (`gxstls.h`, `gxshttpc.h`, `gxdbase.h`,
  `pod.h`, `infohog.h`, ...). Everything is in
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
  - `gxDatabase`, `gxBtree::Create/Open` and `POD::Open` return
    `gxDatabaseError`;
    `gxDBASE_NO_ERROR` (0) is success. Get text with
    `DatabaseExceptionMessage()` or `gxDatabaseExceptionMessage(err)`.
  - `gxBtree::Insert/Find/Delete/FindFirst/FindNext`, `InfoHog`
    `WriteObject/ReadObject/FindObject/DeleteObject` return non-zero
    (true) on success.
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
- `gxDatabase` objects are not copyable and one object must not be
  used from several threads at once without your own mutex.
- Object addresses (`FAU_t`) returned by `Alloc()` stay valid until
  the object is deleted; store them in your index keys.
- `Flush()` does not reach the disk; `Sync()` does. For writes that
  must survive a crash or power loss, put the data file and its
  indexes under one `gxdJournal` (gxdjrnl.h): `Open()` the journal
  before the files, `Attach()` them, `Begin()`/`Commit()` each batch,
  `Close()` the journal before the files. Without it a crash can leave
  part of a batch, and a cached B-tree can lose keys.

### 4.3 File format settings

File layout depends on `DATAREEL_64BIT_DATABASE` (32-bit and 64-bit
engine files are not interchangeable). `DATAREEL_NATIVE_TYPES=OFF`
changes only the in-memory types of the gx integer and float classes:
the library's files and the wire use the same canonical layout with ON
or OFF, on Linux and Windows (7.4). Records you write as raw bytes that
hold gx types differ in size between ON and OFF builds; use `int32_t`
and the other fixed-size types in them. Build the library, its tools
and every program that opens the same files with the same two settings.

### 4.4 Rules when changing DataReel itself

- C++17 minimum (RHEL 8 has GCC 8). Keep the `gx` class names and the
  `datareel` namespace.
- Never change the on-disk format: block and file headers, B-tree
  nodes, `InfoHog` record layout, CRC tables, or the
  `gxDatabaseError` values (removed codes stay as reserved slots).
  DataReel 7 writes byte-identical files to 6.2; keep it that way.
- Treat every length and count read from a file or the network as
  untrusted: check it against the buffer or a limit before use.
- New network code uses `gxsTLSConnection`; do not add plain text
  protocols.
- Build and test before committing:

```sh
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j
tests/run_tests.sh cmake-build
# the test matrix (7.4): 64-bit ON (above), 64-bit OFF with the objrepl
# suites, 32-bit ON and OFF build with 0 warnings plus run_tests
cmake -S . -B /tmp/dr-off -DDATAREEL_64BIT_DATABASE=ON -DDATAREEL_NATIVE_TYPES=OFF -DGXREPL_TEST_HOOKS=ON
cmake --build /tmp/dr-off -j && tests/run_tests.sh /tmp/dr-off
```

- If you change a sample in `docs/ai/samples`, or this file, run
  `python3 docs/bin/make_ai_docs.py` to update the guides, `llms.txt`
  and `llms-full.txt`.

## 5. AI-assisted upgrades

### 5.1 What changes, and what does not

- **From 7.0 to 7.3.x:** the API is the same apart from the items in the
  release notes (`RELEASE_NOTES_7.x.txt`). Files written by 7.x and 6.2
  open in 7.4 with the same build settings.
- **From 7.4 betas:** 7.4 beta 5 and later write the library's file
  headers in one layout on every platform. Older-layout files are read
  and written as they are; `utils/gxd_upgrade` converts one data file
  (object addresses change, indexes must be rebuilt from it). Never set
  `GXD_CREATE_LAYOUT` in production. gxrepl is removed in 7.4;
  replication is `gxObjRepl` (`RELEASE_NOTES_7.4.txt`, "NEW IN BETA 5"
  and "UPGRADING TO 7.4 BETA 5").
- **From 6.x:** DataReel 7 removed the 6.x string, list, thread, file and
  plain text network classes and the relational classes (gxRDBMS, 7.1).
  Follow `docs/ai/porting-from-6x.md` class by class. The database files
  are compatible: 7.x reads and writes 6.2 files with the same 32/64-bit
  and native types settings.
- **Every program that links DataReel** must be rebuilt from one tree
  with the same settings: the library, its tools and the applications.

### 5.2 Before the upgrade (read-only)

1. Record the current version, build options and platform: `version.txt`
   or the release notes in use, `CMakeCache.txt` (or the makefile
   flags), the OS and compiler.
2. List the programs that link DataReel and the database files they open.
3. Back up every data, index and journal file (rule 2), with the
   programs stopped so the copies are consistent.
4. On the copies, run `db_debug` and `ix_debug` and record `dr_digest`
   values, so the same checks can be compared after the upgrade.

### 5.3 The upgrade (one step at a time, confirm each)

1. Build and test the new release with the same file format settings
   (section 4.1). Stop if any test fails.
2. For 6.x code, port one module at a time with
   `docs/ai/porting-from-6x.md`; build with warnings on and fix each one.
3. Rebuild every program that links DataReel against the new tree.
4. Run the programs against the copies first and compare `db_debug`,
   `ix_debug` and `dr_digest` with step 5.2.4.
5. Only then switch the real data over, with the person's OK.

### 5.4 Roll back

Stop the programs, put the previous build back and restore the backed up
files from step 5.2.3. Do not open files the new release has written or
converted (`gxd_upgrade`) with the old release; use the backup copies.

## 6. Forks

DataReel is under the GNU Lesser General Public License version 3 or
later. A fork, a vendored copy inside another project, or a modified
build must:

- Keep `LICENSE` and the copyright and license headers in every file,
  and state in the changed files that they were changed and when.
- Provide the source of the modified library to anyone who receives a
  program built with it, and let them relink the program with their own
  build of the library, as the license requires.
- Not present the fork as an official DataReel release: use a different
  name or version suffix (for example `7.4-yourname.1`), and do not use
  the DataReel name or logo in a way that suggests DataReel made or
  supports the fork.

To keep a fork compatible with DataReel files and peers:

- Follow section 4.4: never change the on-disk format, the wire format
  of `gxStream` and the object server, or the error code values.
- Keep the test suite passing (`tests/run_tests.sh`) and add tests for
  each change.
- Record which DataReel release and commit the fork is based on, and
  check each new DataReel release for fixes, security fixes first.
- Report bugs you find in DataReel itself to DataReel (section 7.4)
  instead of carrying a private fix, so every user gets the fix.

## 7. AI-assisted troubleshooting and support

Diagnose read-only first; change nothing until the person agrees on the
cause and the fix (rule 1).

### 7.1 Where to look

| What | Command or file |
| --- | --- |
| Version | `version.txt`, `RELEASE_NOTES_7.4.txt` |
| Known issues and flaky tests | `KNOWN_ISSUES.md` |
| Build options | `cmake-build/CMakeCache.txt` (`DATAREEL_64BIT_DATABASE`, `DATAREEL_NATIVE_TYPES`, `DATAREEL_SSL`) |
| Library self-test | `tests/run_tests.sh <build folder>` |
| Data file check | `db_debug file.gxd` (on a copy) |
| Index file check | `ix_debug file.btx` (on a copy) |
| File contents digest | `dr_digest file.gxd` (compare two copies) |
| Database error text | `gxDatabaseExceptionMessage(err)` in the program's log |
| Network error text | `Error()` on the connection object, in the program's log |
| TLS handshake | `openssl s_client -connect host:port -servername host` |

`docs/ai/troubleshooting-and-support.md` lists common problems, the
checks for each, and the support summary in more detail.

### 7.2 Common problems

- **"You must define a target platform and/or compiler":** the headers
  were compiled without the `DataReel::datareel` target's definitions
  (section 4.2).
- **A file will not open, or reads garbage, after a rebuild:** the file
  format settings differ from the build that wrote the file (section
  4.3), or a record type holds native gx types that changed size.
- **A record or key compares wrong or differs on disk:** a pointer,
  `std::string` or unset padding in the record or key (section 4.2).
- **Keys missing after a crash:** the program wrote without a
  `gxdJournal` or without `Sync()` (section 4.2).
- **TLS connection fails:** certificate verification (the system store,
  `SetCAFile`, the host name), the TLS backend in the build, or the
  server only offering old TLS versions. Do not turn verification off
  (rule 5).
- **Link errors after an upgrade from 6.x:** a removed class is still
  used; see `docs/ai/porting-from-6x.md`.

### 7.3 Stop and escalate at once

Do not try to fix these yourself. Report to the person straight away,
with the summary in 7.4:

- `db_debug` or `ix_debug` reports damage, a CRC or checksum error, or a
  journal that does not recover.
- Any sign of data loss or of two programs writing the same files
  without a lock.
- A possible security problem in DataReel: a crash on a damaged file or
  network input, a length check that can be passed, or TLS accepting a
  certificate it should refuse. Do not post it in a public issue; send it
  privately (7.4).
- Anything that needs a change to the on-disk format, the wire format or
  a rule in section 1.

### 7.4 Support summary

When a problem is not solved, give the person a summary they can send to
their team or to DataReel (https://datareel.com/contact.php). AI-assisted
support ends here: the people take it from this summary.

1. DataReel version (`version.txt`), OS, compiler and the build options
   (32/64-bit engine, native types, TLS backend, static or shared).
2. The `tests/run_tests.sh` result on that build.
3. The `db_debug` / `ix_debug` output from copies of the files involved.
4. The smallest program or steps that show the problem, the error text
   and the log lines around it (times included).
5. What happened, when, what changed before it, and every command or
   change already tried with its result.

Remove host names, addresses and other site data from the summary when
the person asks for that, and never include passwords, private keys or
customer data. Projects maintained by DataReel Software Development
report DataReel issues through DataReel's internal handoff process
instead of patching their own DataReel copy.

DataReel is free software under the GNU Lesser General Public License,
provided as is. Check every command and change an agent proposes before
it runs; the person running it is responsible for the changes made.

## 8. Report back

End every task with: each command run or proposed, where, and its
result; each file changed; the build options used; the test results;
every warning and what was decided; and the backups made and where they
are.

## Map

| Path | What |
|------|------|
| `include/`, `src/` | Library headers and sources |
| `examples/database/` | gxDatabase, gxBtree, POD, InfoHog programs |
| `examples/tls/` | TLS echo, HTTPS, SMTP, POP3, FTPS, gxStream programs |
| `examples/general/` | gxConfig, LogFile, ThreadPool |
| `docs/ai/` | Task guides and their sample programs |
| `docs/classes/` | HTML reference for each class |
| `tests/` | `run_tests.sh`, mock mail/FTPS servers, damaged file test |
| `utils/` | `db_debug`, `ix_debug`, `dr_digest`, `gxd_upgrade` and other file tools |
