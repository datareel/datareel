# DataReel 7.5 Python module: AI-assisted builds, use and support

This file is for AI-assisted work with the `datareel` Python module (DataReel 7.5 beta 3), the
pip package that reads the object databases the DataReel C++ library writes. It tells an AI agent
how to assist a site's own developers and administrators to build the wheel, install it, write Python
code that reads DataReel stores, and troubleshoot. The agent assists; it never acts autonomously
(section 0). `GEMINI.md` and `CLAUDE.md` in this folder only import this file.

The rules for the C++ library, its builds and its support are in the top `AGENTS.md`; this file covers
the Python module and points there for the rest. The task guide with complete programs is
`docs/ai/python.md` (also in `llms-full.txt`). The full documentation is `python/docs/users-manual.md`
(the user's manual) and `python/docs/building.md` (the build guide). When this file and the code
disagree, the code wins: say so to the person you work for.

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
  (section 5 here; section 7.1 of the top AGENTS.md) before anything else; they are shown and approved the
  same way.

**Works with strict AI policies.** These files contain no code that runs
by itself: `AGENTS.md`, `GEMINI.md`, `CLAUDE.md` (here and at the top of the tree),
`llms.txt`, `llms-full.txt`, `docs/ai/` and `python/docs/` are text. Everything in this guide works
in advisory mode, with no tool execution by the agent.

## 1. Rules for agents (read these first)

1. **Writes go through `datareel.Primary` only (beta 2).** Never change a `.gxd`, `.btx`, `.vix` or node
   file by other means (open(..., "r+b"), mmap writes): that damages the database. A node has one writer:
   never open as a `Primary` a node that a C++ program (or another Python process) runs, and never a copy.
   Writing to a production node from Python is the person's decision, made explicitly; propose it, do not
   do it. Every change goes in a transaction (`with p.transaction():`), encoded with the C++ readers'
   record layout.
2. **DEV reads PROD; it writes only standalone test nodes.** A test node is a file copy of a stopped copy's
   folder made a primary with `datareel.detach()` (the same call as `takeover()`; user's manual, section
   5a). Never run `detach()` or `takeover()`
   on a production copy (the standby): that ends its replication. Give a test node no `copy` setting that
   points at a production node. The emergency path (beta 3: production rejoins under a DEV primary, and only
   the changed objects cross) is always the person's decision, made by hand: propose the steps (the user's
   manual, section 5b), never start it. Never run `swap()` on production folders without the person's go.
3. **A running node is read with `Reader` only.** `BlockFile` and `VectorIndex` are for files no process
   is writing (closed day files, merged files, copies of a stopped node). Opening a running node's files
   with them can read half-written pages.
4. **Decode with the C++ record layout.** Objects come back as the bytes the C++ program stored. Get the
   struct from the program's header (field order, sizes, byte order, padding) and build the `struct`
   format or numpy dtype from it (user's manual, example 5). Never guess a layout from the bytes; ask the
   person for the header.
5. **Give `Reader` the node's own index spec** (`gxObjReplConfig::index`, the `index` line in
   `ROOT/objrepl.conf`), or since beta 2 none: `Reader(root)` reads it from the node's `.def` files
   (`r.index_spec`). Keys for `find_key()` and `find_range()` are the index's bytes, in the C++ field's
   size and byte order. Walk a large database in pages (`ids(db, after=last, limit=1000)`), not with one
   `ids()` call.
6. **Retry a refused call, do not work around it.** `datareel.Error` with "a full send is under way" or
   "cannot open ROOT/NAME.gxd" can come for a moment while the node changes; wait and call again. Any
   other error goes to the person with the full message.
7. **No package installs without approval.** Building needs only packages from the Red Hat repositories
   (`python/docs/building.md`); pip downloads nothing (`--no-build-isolation --no-deps`). Do not add
   third-party packages to the build. numpy is optional for the examples only.
8. **Never stop, restart or reconfigure the C++ node** to make a Python read work. Readers do not need it.
9. **Tests are never skipped.** `python/tests/run_python_tests.sh` must pass in full before a wheel is
   used. A test that fails once on a loaded machine is run again on its own and reported either way.

## 2. What the module is

| Class | Does | When |
| --- | --- | --- |
| `datareel.Primary(root, config=None, settings=None, quiet=False)` (beta 2) | writes a node as a gxObjRepl primary: transactions, `put`, `remove`, `purge`, `new_id`, `pump`, `drain`, and reads | when this Python process is the node's only writer |
| `datareel.Copy(root, ...)` (beta 2) | a copy node: `serve()` until `stop()`, reads | a copy this process runs |
| `datareel.Database`, `BTree`, `Journal`, vector writes (beta 2) | block files, B-tree indexes, journal transactions, `.vix` writes and merges | files this process owns |
| `datareel.Stream`, `StreamServer`, `ObjectClient` (beta 2) | gxStream over TLS (port 2073), the object server's object service | network peers with certificates |
| `datareel.Node(root, config, role=)` (beta 2) | a `Primary` or a `Copy` (`role="copy"`) | as those |
| `status`, `digest`, `takeover`, `detach`, `rejoin`, `check`, `clear_alarm`, `index_check` | node folder tools, as the `objrepl` tool | folders no process holds (check: any time) |
| `datareel.Reader(root, index="")` | a gxObjRepl node (primary or copy): `get`, `is_live`, `ids` (pages: `after`, `limit`), `databases`, `find_key`, `find_range`, `nearest`, `refresh`; `seq`, `epoch`, `role`, `index_spec` | while the node runs, or when none runs |
| `datareel.VectorIndex(path, spec=None)` | a `.vix` file: `search` (tags, exact or graph), `get`; `dim`, `count`, `spec`, `metric` | files no process is writing |
| `datareel.BlockFile(path)` | a `.gxd` file: `addresses`, `read` | files no process is writing |

Each `Reader` call reads under the node journal's shared lock, so it sees whole commits only. The
extension is plain CPython C API with the DataReel library inside it; it links the system OpenSSL 3.
Wheels: Linux x86_64, CPython 3.12, built on RHEL 9 and tested on RHEL 9 and 10.

## 3. Builds

Follow `python/docs/building.md`. The short form, from the top of the source tree (branch `7.5-beta`):

```
python3.12 -m pip wheel python/ --no-build-isolation --no-deps -w dist/
python/tests/run_python_tests.sh /tmp/drpy
```

On a host that should not get development packages, build in the UBI 9 container the build guide
describes. Show the person each command first (section 0).

## 4. Using it in a project

1. Install the wheel in the project's virtual environment:
   `python -m pip install --no-index datareel-7.5b3.post1-cp312-cp312-linux_x86_64.whl`.
2. Find the store's facts: the node folder, its index spec, the C++ record struct of each database.
3. Start from the user's manual examples: example 1 (open and list), 2 (decode), 3 (keys), 4 (vectors),
   5 (layout class and rows), 6 (follow a running node), 7 (vector files), 8 (block files), 9 (threads).
4. Keep each `Reader` in one thread; open one per thread.

## 5. Troubleshooting

Read-only checks first, shown to the person and run only with approval:

```
python -c "import datareel; print(datareel.__version__, datareel.library_version(), datareel.engine_bits())"
objrepl status ROOT        # the C++ tool: role, epoch, seqs, in_snapshot
objrepl check ROOT         # OK / WARN / FAIL
ls -l ROOT
```

The error table is in the user's manual, section 7. A build failure: the problems table in the build
guide. Anything else, or a wrong result (an object that decodes to nonsense with the right layout, a key
the C++ program finds and Python does not), stops the work: report it (section 6).

## 6. Report back

Problems the agent and the person cannot solve go to DataReel as described in the top `AGENTS.md`,
section 7.4, with: the module and library versions (section 5), the Python and OS versions, the exact
call and its full error, `objrepl status ROOT` output, and whether the C++ program reads the same object
correctly. Never send database files, keys or certificates.

## Map

| Need | Where |
| --- | --- |
| Build the wheel | `python/docs/building.md` |
| Use the module | `python/docs/users-manual.md`, `python/examples/` |
| Complete programs for an agent | `docs/ai/python.md` |
| The C++ side (gxObjRepl, indexes, journal) | top `AGENTS.md`, `docs/ai/object-database.md`, `docs/replication_admin_guide.html` |
| Release notes | `RELEASE_NOTES_7.5.txt` |
