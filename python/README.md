# datareel for Python (DataReel 7.5)

Read and write the object databases of the DataReel C++ library from Python, without a C++ helper
or an export file. A Python program can also write: it can be a gxObjRepl primary that replicates to a
C++ copy.

```
pip install datareel-7.5-cp312-cp312-linux_x86_64.whl
```

```python
import struct
import datareel

# A gxObjRepl node folder, while the node runs in another process (or none runs).
# index: the node's index spec, for find_key() and nearest().
with datareel.Reader("/data/osa/news", "k0:events:B:0:8,v0:events:V:8:96") as r:
    print(r.role, r.seq, r.databases())
    for oid in r.ids("events")[:10]:
        rec = r.get("events", oid)            # bytes, exactly as the C++ program stored them
    ids = r.find_key("k0", struct.pack("<Q", 20261120))
    hits = r.nearest("v0", query_vector, k=10) # [(id, distance)], closest first

# A .vix vector index file that no process is writing (a day file, a merged month)
with datareel.VectorIndex("/data/news_202611.vix") as v:
    hits = v.search(query_vector, k=20, tags={0: 3, 2: (20261101, 20261130)})

# A .gxd block file that no process is writing
with datareel.BlockFile("/data/old_day.gxd") as f:
    for a in f.addresses():
        rec = f.read(a)
```

## Documentation

- `python/docs/users-manual.md`: the user's manual: ideas, a demo node, simple examples (1 to 4),
  detailed examples (5 to 9), the reference, errors and limits.
- `python/docs/building.md`: the build guide: packages, the wheel, a container build, the tests.
- `python/examples/`: the example programs (`make_demo_node.sh` makes a node to run them on).
- `python/AGENTS.md` (with `CLAUDE.md` and `GEMINI.md`): AI-assisted builds, use and support;
  `docs/ai/python.md` is the task guide for AI tools (also in `llms-full.txt`).
- HTML pages: `docs/python_readme.html`, `docs/python_users_manual.html`, `docs/python_building.html`.

## What it does

- `Primary(root, config=None, settings=None, quiet=False)` opens a node as a gxObjRepl primary in
  this process: `open_database`, `begin`/`commit`/`rollback` (or `with p.transaction():`), `new_id`,
  `put`, `remove`, `purge`, `pump`, `drain`, and the reads below. Commits are durable when `commit()`
  returns; `pump()`/`drain()` send them to the copy.

- `Reader(root, index="")` reads a gxObjRepl node (a primary or a copy) from another process while
  the node runs. Each call reads under the node journal's shared lock, so it sees whole commits only:
  `get(db, id)` (None when the object is not live), `is_live`, `ids(db, tombstones=False)`,
  `databases()`, `find_key(index, key_bytes)`, `nearest(index, query, k=10)`, `refresh()`, and the
  `seq`, `epoch` and `role` of the commit read. A call can raise `datareel.Error` for a moment while
  the node creates a database file or a copy takes a full send: try it again.
- `VectorIndex(path, spec=None)` opens a `.vix` file read only: `search(query, k=10, ef=64,
  exact=False, tags=None)` returns `[(id, distance, tags)]`; `get(id)`, `dim`, `count`, `spec`,
  `metric`.
- `BlockFile(path)` opens a `.gxd` file read only: `addresses()` and `read(address)`.
- `Copy(root, ...)` runs a copy node (`serve()` in a thread, `stop()`); `takeover()`, `rejoin()`,
  `check()`, `clear_alarm()`, `index_check()`, `status()` and `digest()` are the `objrepl` tool's node
  commands.
- `Database`, `BTree`, `Journal`: gxDatabase block files, gxBtree index files of byte keys,
  gxdJournal transactions over them; `create_vector_index()`, `VectorIndex(path, write=True)` and
  `merge_vector_indexes()` write vector files.
- `Stream`, `StreamServer`, `ObjectClient`: gxStream over TLS on port 2073, and the object
  service of a DataReel object server (objects and vector searches).
- A query vector can be a list of numbers or any float32 buffer (`array('f')`, a numpy float32 array).
- Calls that read files release the GIL. Use one object per thread.

Objects come back as the bytes the C++ program stored. Decode them with `struct` or numpy using that
program's record layout. In a gxObjRepl node, `Reader.get()` returns the object's own bytes;
`BlockFile.read()` returns the whole block, with gxObjRepl's 40-byte record head in front.

## Limits

- Not wrapped: InfoHog and POD persistent objects, the 6.x-style gxStream block requests, and the
  network clients Python's standard library has (HTTPS, SMTP, POP3, IMAP, FTPS).
- The 64-bit database engine only (the engine nodes and OSA use).
- Linux x86_64 and CPython 3.12 wheels, built on RHEL 9 (UBI 9) and tested on RHEL 9 and 10. The
  wheel links the system OpenSSL 3 (`libssl.so.3`); the DataReel library is inside the extension. Windows: a build
  script only (`python\build_wheel_windows.bat`), not tested; no Windows wheel is distributed.
- `VectorIndex` and `BlockFile` are for files no process is writing. For a running node use `Reader`.

## Building

From the repository top, with a C++17 compiler, CMake 3.16+, the OpenSSL headers and the Python
headers (`python3.12-devel`):

```
python3.12 -m pip wheel python/ --no-build-isolation --no-deps -w dist/
python/tests/run_python_tests.sh /tmp/drpy      # builds, installs in a venv, runs the tests
```

`DATAREEL_JOBS=N` sets the build's parallel jobs (default 2).

## License

LGPL 3 or later, as the DataReel library (see LICENSE at the repository top).
