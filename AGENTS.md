# DataReel 6.4 (legacy): AI-assisted setup, builds, upgrades, forks and support

This file is for AI-assisted work with DataReel 6.4.x, the legacy
release of the DataReel C++ library. It tells an AI agent (Gemini CLI,
Claude Code, GitHub Copilot, Codex and others) how to assist a site's
own developers and system administrators to set up DataReel 6.4, build
it and programs that use it, upgrade within 6.x or move to DataReel 7,
maintain a fork, and troubleshoot. The agent assists; it does not
replace the developers, the administrators or DataReel support, and it
never acts autonomously (section 0). The people review and approve
every step, and problems the agent cannot solve go to DataReel (section
7.4). `GEMINI.md` and `CLAUDE.md` in this folder only import this file.

The full documentation is `README.txt`, the release notes
(`RELEASE_NOTES_6.*.txt`), `docs/` and https://datareel.com/clib/. This
file points to them instead of repeating them. When this file and the
code disagree, the code wins: say so to the person you work for.

**Release status.** DataReel 7.4 is the main, fully supported release
(https://datareel.com/clib7/). DataReel 6.4.1 is the legacy release: it
gets bug fixes and security updates only, and no new features. New
projects should use DataReel 7; propose 6.x only for existing 6.x code.

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
by itself: `AGENTS.md`, `GEMINI.md` and `CLAUDE.md` are text. Everything
in this guide works in advisory mode, with no tool execution by the
agent.

## 1. Rules for agents (read these first)

DataReel database files hold an application's data, and its socket and
TLS classes carry its network traffic. A wrong step can lose data, make
files unreadable by the programs that use them, or weaken security.
Follow these rules on every task:

1. **Show, then the person runs or approves it.** Before any command that
   changes something (installing packages, `make install`, writing or
   repairing a database file, editing code, committing), show the command
   or the change. In advisory mode the person runs it; otherwise wait for
   an explicit yes for that one step (section 0).
2. **Back up data before changing it.** Copy every database file (data
   files, B-tree indexes, journals) into a dated backup folder before an
   upgrade, a repair or a test against real data. Never delete; move
   aside. Work on copies when you can.
3. **Never change the file format settings of an existing project.** The
   64-bit database engine setting (`DATAREEL_64BIT_DATABASE`, or
   `LARGEFILES` in the makefiles) and the native types setting must match
   the build that wrote the files.
4. **Stop on any failed check.** If a build, a test or a check fails, or
   the output is not what this guide says to expect, stop and report. Do
   not improvise a fix on production data or a production server.
5. **Never weaken security to make something work.** Do not turn off
   certificate verification or TLS, send passwords over plain text
   connections, remove length checks, or ignore a checksum or CRC error.
6. **Legacy rules: no new features in 6.x.** Changes to DataReel 6.x
   itself are bug fixes and security fixes only. A request for a new
   feature points to DataReel 7.4.
7. **Read-only first.** Start every task with read-only checks: the
   version (`version.txt`), the build settings (`env/linux.env`, the
   CMake cache), `db_debug` and `ix_debug` on copies of the files, the
   test results (section 7.1).
8. **No secrets or site data in files you write** (passwords, private
   keys, host names, addresses, customer data), unless the person asks
   for that file on their own systems.
9. **Keep a log.** List each command you ran or proposed, where, and its
   result, and give it to the person at the end (section 8).

## 2. What DataReel 6.4 is

DataReel 6.x is a cross platform C++ library for multi-threaded database
and communication applications: an embedded object database
(`gxDatabase` block files, `gxBtree` indexes, `POD`, `InfoHog`
persistent objects, an optional page cache and write-ahead journal since
6.3), sockets and TLS (`gxSocket`, `gxSSL`, HTTP, SMTP, POP3, FTP and
`gxStream` clients and servers), threads, strings, containers,
configuration and log files. 6.4 can also build the DataReel 7.x vector
database features for 6.x programs (`DATAREEL7_FEATURES_DIR`, see
`README.txt`).

It runs on RHEL 8, 9 and 10 and compatible Linux systems (GCC, the
system OpenSSL) and on Windows 11 and Windows Server 2022 and 2025
(Visual Studio 2022 or 2026, Schannel by default or OpenSSL 3.5.x LTS).
It is free software under the GNU Lesser General Public License version
3 or later (`LICENSE`), provided as is.

## 3. Setup: download and install a release

Manual: `README.txt` (BUILDING UNIX SHARED AND STATIC LIBRARIES,
BUILDING WITH CMAKE, BUILDING WINDOWS STATIC LIBRARY and DLL).

1. Download the release from
   https://datareel.com/downloads/datareel_downloads.php: the source
   (`datareel-6.4.1.tar.gz` or `datareel-6.4.1.zip`) or, on Windows, a
   prebuilt static library (`datareel-6.4.1-windows-x64-schannel.zip` or
   `-x86-schannel.zip`), each with its `.sha256sum` file.
2. Check it before unpacking, and stop if it fails:
   `sha256sum -c datareel-6.4.1.tar.gz.sha256sum` (Linux) or
   `Get-FileHash datareel-6.4.1.zip -Algorithm SHA256` compared with the
   `.sha256sum` file (Windows PowerShell).
3. Install the build tools: on RHEL `dnf install gcc-c++ make
   openssl-devel` (and `cmake` for the CMake build), run by the person;
   on Windows Visual Studio 2022 or 2026 with the C++ workload.
4. Keep the file format settings of the project's existing data files
   (rule 3).
5. Build and install as in section 4.

## 4. New builds

Linux, makefiles (`README.txt`):

```sh
cd env && source gnu_env.sh     # edit linux.env first if needed
cd ../build && make && make install
```

Linux or Windows, CMake:

```sh
cmake -S . -B cmake-build
cmake --build cmake-build -j
```

Windows, command line: `env\msvc.bat` (64-bit, the default) or
`env\msvc32_vs2026.bat` (32-bit, legacy code only), then the makefiles
described in `README.txt` and `README_VisualStudio_2022.txt`.

Tests: `examples_database/run_db_tests.sh` (database classes) and
`examples_database/journal/crash_test.sh` (journal crash recovery).
Programs that link the library must be built with the same settings
(SSL, 64-bit engine, native types); with CMake,
`target_link_libraries(app PRIVATE DataReel::datareel)` passes them on.

Rules when changing DataReel 6.x itself:

- Bug fixes and security fixes only (rule 6). Keep the API, the class
  names and the behavior of existing programs.
- Never change the on-disk format: 6.x writes the same bytes as 6.2,
  and DataReel 7 reads and writes the same files. Keep it that way.
- Treat every length and count read from a file or the network as
  untrusted: check it against the buffer or a limit before use.
- Mark each change in the source with a dated comment and describe it in
  the release notes, as earlier 6.x releases do.
- Build on Linux and Windows and run the tests before committing.

## 5. AI-assisted upgrades

### 5.1 Within 6.x (6.0 to 6.4.x)

The file formats are unchanged across 6.x. Some releases add class
members, so every program that links DataReel must be recompiled and
relinked against the new release (the release notes for each version
say so). Read the release notes from the version in use up to 6.4.1.

### 5.2 From 6.x to DataReel 7.4

DataReel 7 removed the 6.x string, list, thread, file and plain text
network classes and the relational classes. The database files are
compatible with the same 32/64-bit and native types settings. Follow
`docs/ai/porting-from-6x.md` and `AGENTS.md` in the DataReel 7.4
release.

### 5.3 Steps (one at a time, confirm each)

1. Record the current version, build settings, platform and compiler,
   and list the programs that link DataReel and the files they open.
2. Back up every data, index and journal file with the programs stopped
   (rule 2). Run `db_debug` and `ix_debug` on the copies and keep the
   output.
3. Build and test the new release with the same settings. Stop if any
   test fails.
4. Rebuild every program that links DataReel and run it against the
   copies first; compare `db_debug` and `ix_debug` with step 2.
5. Only then switch the real data over, with the person's OK.
6. Roll back: stop the programs, put the previous build back and restore
   the backup copies.

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
  name or version suffix, and do not use the DataReel name or logo in a
  way that suggests DataReel made or supports the fork.

Keep the on-disk format and wire formats unchanged, keep the tests
passing, record which DataReel release the fork is based on, take each
new 6.x security fix, and report bugs in DataReel itself to DataReel
(section 7.4) instead of carrying a private fix. For new work, consider
moving the fork to DataReel 7.4.

## 7. AI-assisted troubleshooting and support

Diagnose read-only first; change nothing until the person agrees on the
cause and the fix (rule 1).

### 7.1 Where to look

| What | Command or file |
| --- | --- |
| Version | `version.txt`, `RELEASE_NOTES_6.4.1.txt` |
| Build settings | `env/linux.env`, the CMake cache, `USE_OPEN_SSL` |
| Database tests | `examples_database/run_db_tests.sh` |
| Data file check | `db_debug file` (on a copy) |
| Index file check | `ix_debug file` (on a copy) |
| Error text | the class's error message functions, in the program's log |
| TLS handshake | `openssl s_client -connect host:port -servername host` |

### 7.2 Common problems

- **A file will not open, or reads garbage, after a rebuild:** the
  64-bit engine or native types setting differs from the build that
  wrote the file (rule 3).
- **Link errors or crashes after an upgrade:** a program was not
  recompiled against the new release, or was built with different
  settings than the library.
- **TLS fails on Windows but not Linux (or the reverse):** Schannel and
  OpenSSL differ; check the backend in the build and
  `README_windows_openssl.txt`.
- **Keys missing after a crash:** the program wrote without the journal
  (6.3 and later) or without syncing to disk.

### 7.3 Stop and escalate at once

Do not try to fix these yourself. Report to the person straight away,
with the summary in 7.4:

- `db_debug` or `ix_debug` reports damage, or a journal does not
  recover.
- Any sign of data loss or of two programs writing the same files
  without a lock.
- A possible security problem in DataReel: a crash on a damaged file or
  network input, a length check that can be passed, or TLS accepting a
  certificate it should refuse. Do not post it in a public issue; send it
  privately (7.4).

### 7.4 Support summary

When a problem is not solved, give the person a summary they can send to
their team or to DataReel (https://datareel.com/contact.php). AI-assisted
support ends here: the people take it from this summary. Security
problems in 6.4.x are fixed; new features are not added (use 7.4).

1. DataReel version, OS, compiler and build settings.
2. The test results on that build.
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
result; each file changed; the build settings used; the test results;
every warning and what was decided; and the backups made and where they
are.
