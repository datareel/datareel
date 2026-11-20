# datareel for Python (DataReel 7.5 beta 1)

Read the object databases that the DataReel C++ library writes, from Python, without a C++ helper
or an export file. This beta is read only.

```
pip install datareel-7.5b1-cp312-cp312-linux_x86_64.whl
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

## What it does

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
- A query vector can be a list of numbers or any float32 buffer (`array('f')`, a numpy float32 array).
- Calls that read files release the GIL. Use one object per thread.

Objects come back as the bytes the C++ program stored. Decode them with `struct` or numpy using that
program's record layout. In a gxObjRepl node, `Reader.get()` returns the object's own bytes;
`BlockFile.read()` returns the whole block, with gxObjRepl's 40-byte record head in front.

## Limits (beta 1)

- Read only. Writes (a gxObjRepl primary from Python) are planned for a later beta.
- The 64-bit database engine only (the engine nodes and OSA use).
- Linux x86_64 and CPython 3.12 wheels, built on RHEL 9 (UBI 9) and tested on RHEL 9 and 10. The
  wheel links the system OpenSSL 3 (`libssl.so.3`); the DataReel library is inside the extension.
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
