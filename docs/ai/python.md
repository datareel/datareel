# The datareel Python module (7.5 beta 3)

The `datareel` pip package reads and writes, from Python, the stores of the DataReel C++ library:
gxObjRepl nodes while they run (`Reader`), vector index files (`VectorIndex`), block files (`BlockFile`),
and since beta 2 a node written from Python as a gxObjRepl primary (`Primary`). The rules for agents are in `python/AGENTS.md`; the user's manual is
`python/docs/users-manual.md` and the build guide `python/docs/building.md`.

## Build and install

```
python3.12 -m pip wheel python/ --no-build-isolation --no-deps -w dist/     # from the source top
python/tests/run_python_tests.sh /tmp/drpy                                  # every test must pass
python3.12 -m pip install --no-index dist/datareel-7.5b3.post2-cp312-cp312-linux_x86_64.whl
```

Build needs `gcc-c++ cmake openssl-devel python3.12-devel python3.12-setuptools python3.12-wheel`
(Red Hat repositories) and downloads nothing.

## Pick a class

| Task | Use |
| --- | --- |
| Read a gxObjRepl node, running or not | `Reader(root, index_spec)` |
| Objects of a database | `Reader.ids(db)`, `Reader.get(db, id)` (bytes or None) |
| Walk a large database in pages (beta 2) | `Reader.ids(db, after=last_id, limit=1000)` until `[]` |
| Objects by an index key | `Reader.find_key(index, key_bytes)` |
| A key range (beta 2) | `Reader.find_range(index, lo, hi=None, limit=0)` (bytes compared byte by byte) |
| The node's own index spec (beta 2) | `Reader(root)` with no spec; `r.index_spec` |
| Rejoin with only the changed objects (beta 3) | `datareel.rejoin(old_root)` (incremental when it can be), then serve it as a `Copy`; the person decides |
| A schema change (beta 3) | the `schema` setting; a refused rejoin means a rebuild into an empty folder, then `datareel.swap(old, new, index)` |
| A standalone test node from a stopped copy (beta 2) | file copy, then `datareel.detach(test_root)`, then `Primary(test_root, ...)` |
| Similar vectors in a running node | `Reader.nearest(index, query, k)` |
| Search a closed `.vix` file, with tag filters | `VectorIndex(path, spec).search(query, k, tags=...)` |
| Walk a closed `.gxd` file | `BlockFile(path).addresses()`, `.read(address)` |
| Write a node from Python (beta 2) | `Primary(root, settings=...)`, `with p.transaction(): p.put(...)`, `p.drain()` |
| Run a copy, fail over, rejoin (beta 2) | `Copy(root, settings=...).serve()` in a thread, `takeover()`, `rejoin()`, `check()` |
| Block files, B-trees, journals (beta 2) | `Database`, `BTree(path, key_size)`, `Journal` + `attach` + `datareel.transaction(j)` |
| Write vector files (beta 2) | `create_vector_index()`, `VectorIndex(path, spec, write=True).insert()`, `merge_vector_indexes()` |
| gxStream and the object server (beta 2) | `Stream`, `StreamServer`, `ObjectClient` |

## Open a node and list it

<!-- sample: python/examples/ex1_open_and_list.py -->
```python
# python/examples/ex1_open_and_list.py
"""Example 1 (simple): open a node and see what is in it.

    python ex1_open_and_list.py NODE
"""
import sys
import datareel

root = sys.argv[1]
with datareel.Reader(root) as r:                 # read only; the node may be running
    print("role", r.role, "seq", r.seq, "epoch", r.epoch)
    for db in r.databases():
        print("database", db, "objects", len(r.ids(db)), "tombstones", len(r.ids(db, tombstones=True)))
```
<!-- end sample -->

## Decode records with the C++ layout

The C++ record must be described exactly (order, sizes, little endian, padding). Ask for the header.

<!-- sample: python/examples/ex5_record_layout.py -->
```python
# python/examples/ex5_record_layout.py
"""Example 5 (detailed): a record layout class for a C++ struct, and a whole database as rows.

A C++ program stores fixed-size records (DataReel's rule: no pointers, no std::string). Describe the struct once,
with the same field order, sizes and byte order, and decode every object with it. With numpy installed the same
layout becomes a structured dtype and the rows one array; without numpy the example uses plain struct.

    C++:  struct VecRec { uint64_t key; float v[4]; };   // 24 bytes

    python ex5_record_layout.py NODE
"""
import struct
import sys
import datareel


class Layout:
    """Fields as (name, struct code, count); '<' little endian, no padding (pack the C++ struct the same way)."""

    def __init__(self, *fields):
        self.fields = fields
        self.st = struct.Struct("<" + "".join("%d%s" % (n, c) for _, c, n in fields))

    def decode(self, b):
        vals = list(self.st.unpack_from(b))
        row = {}
        for name, _, n in self.fields:
            row[name] = vals[0] if n == 1 else tuple(vals[:n])
            del vals[:n]
        return row

    def numpy_dtype(self):
        import numpy as np
        return np.dtype([(name, "<" + code, (n,)) if n > 1 else (name, "<" + code) for name, code, n in self.fields])


VEC = Layout(("key", "Q", 1), ("v", "f", 4))

with datareel.Reader(sys.argv[1]) as r:
    ids = r.ids("vec")
    blobs = [r.get("vec", oid) for oid in ids]
    seq = r.seq

blobs = [b for b in blobs if b is not None]       # an object removed between ids() and get()
rows = [VEC.decode(b) for b in blobs]
print("seq", seq, "rows", len(rows))
print("first", rows[0])
print("keys from", min(x["key"] for x in rows), "to", max(x["key"] for x in rows))
print("mean of v[0]", sum(x["v"][0] for x in rows) / len(rows))
try:
    import numpy as np
    a = np.frombuffer(b"".join(b[:VEC.st.size] for b in blobs), dtype=VEC.numpy_dtype())
    print("numpy rows", len(a), "mean of v[0]", float(a["v"][:, 0].mean()))
except ImportError:
    print("numpy is not installed: struct only")
```
<!-- end sample -->

## Follow a running node

<!-- sample: python/examples/ex6_follow_node.py -->
```python
# python/examples/ex6_follow_node.py
"""Example 6 (detailed): follow a node while its C++ writer runs, and read each new object once.

Each call sees whole commits only, and seq names the commit the call read, so a reader that remembers the ids it
has seen can pick up new objects as commits arrive. A call may be refused for a moment (datareel.Error) while
the node creates a database file or a copy takes a full send: wait and try again.

    python ex6_follow_node.py NODE [SECONDS]
"""
import struct
import sys
import time
import datareel

root = sys.argv[1]
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0


def open_reader(root, wait=30.0):
    t0 = time.time()
    while True:
        try:
            return datareel.Reader(root)
        except datareel.Error:                    # no node folder yet
            if time.time() - t0 > wait:
                raise
            time.sleep(0.1)


seen, last_seq, new_total = set(), -1, 0
with open_reader(root) as r:
    t0 = time.time()
    while time.time() - t0 < seconds:
        try:
            r.refresh()
            if r.seq == last_seq:
                time.sleep(0.2)                   # nothing committed since the last look
                continue
            ids = r.ids("vec")
            last_seq = r.seq
        except datareel.Error as e:
            print("refused for now:", e)
            time.sleep(0.2)
            continue
        new = [i for i in ids if i not in seen]
        for oid in new:
            b = r.get("vec", oid)
            if b is not None:
                seen.add(oid)
                new_total += 1
        if new:
            print("seq", last_seq, "new objects", len(new), "last key", struct.unpack_from("<Q", r.get("vec", new[-1]))[0])
print("followed", new_total, "objects up to seq", last_seq)
```
<!-- end sample -->

## Vector index files

<!-- sample: python/examples/ex7_vector_files.py -->
```python
# python/examples/ex7_vector_files.py
"""Example 7 (detailed): search a vector index file directly: graph or exact, tag filters, recall.

VectorIndex opens a .vix file that no process is writing (a day file, a merged month, a copy of a stopped node).
For a running node use Reader.nearest(). search() returns (id, distance, (tag0, tag1, tag2, tag3)), closest
first; tags filters on the 4 integer tags the C++ program stored with each vector: {tag: value} or
{tag: (lo, hi)}. Queries may be lists or float32 buffers (array('f'), numpy float32 arrays).

    python ex7_vector_files.py FILE.vix [SPEC]
"""
import array
import random
import sys
import datareel

path = sys.argv[1]
spec = sys.argv[2] if len(sys.argv) > 2 else None   # when given, a file with another spec is refused
with datareel.VectorIndex(path, spec) as v:
    print("dim", v.dim, "vectors", v.count, "metric", v.metric, "spec", v.spec)
    rnd = random.Random(7)
    queries = [array.array("f", [rnd.uniform(0, 300)] + [0.0] * (v.dim - 1)) for _ in range(20)]
    agree = 0
    for q in queries:
        graph = [h[0] for h in v.search(q, k=5)]
        exact = [h[0] for h in v.search(q, k=5, exact=True)]
        agree += len(set(graph) & set(exact))
    print("recall@5 of the graph search against exact: %.3f" % (agree / (5.0 * len(queries))))
    hit = v.search(queries[0], k=1)[0]
    vec, tags = v.get(hit[0])
    print("nearest", hit[0], "distance %.3f" % hit[1], "vector", vec, "tags", tags)
    print("only tag 0 = 0:", len(v.search(queries[0], k=5, tags={0: 0})), "hits")
    print("only tag 0 in 1..9:", len(v.search(queries[0], k=5, tags={0: (1, 9)})), "hits")
```
<!-- end sample -->

## Write a node from Python (beta 2)

<!-- sample: python/examples/ex11_event_log.py -->
```python
# python/examples/ex11_event_log.py
"""Example 11 (detailed, 7.5 beta 2): an event log written from Python: batches, updates, removes, a rolled
back batch, and replication to a copy.

Each batch is one transaction. A batch that fails half way is rolled back, so the node never holds part of
it. With COPY_PORT, a C++ copy (objrepl copy ... --listen 127.0.0.1:COPY_PORT) receives every commit:
pump() after a commit sends what the copy lacks, drain() waits until it has everything.

    python ex11_event_log.py ROOT PKI [COPY_PORT]
"""
import os
import struct
import sys
import datareel

root, pki = sys.argv[1], sys.argv[2]
port = int(sys.argv[3]) if len(sys.argv) > 3 else 1
INDEX = "k0:log:B:0:8,v0:log:V:8:2"                    # key at 0; a 2-float vector at 8
REC = struct.Struct("<Q2f8s")                          # C++: struct { uint64_t seq; float v[2]; char kind[8]; }
settings = {"copy": "127.0.0.1:%d" % port, "ca": os.path.join(pki, "ca.pem"),
            "cert": os.path.join(pki, "primary.pem"), "key": os.path.join(pki, "primary.key"), "index": INDEX}


class BadEvent(Exception):
    pass


def write_batch(p, start, events):
    """events: (x, y, kind) tuples; a kind longer than 8 bytes fails the whole batch"""
    ids = []
    with p.transaction():
        for i, (x, y, kind) in enumerate(events):
            if len(kind) > 8:
                raise BadEvent("kind %r is longer than 8 bytes" % kind)
            oid = p.new_id()
            p.put("log", oid, REC.pack(start + i, x, y, kind))
            ids.append(oid)
    return ids


with datareel.Primary(root, settings=settings, quiet=True) as p:
    p.open_database("log")
    ids = write_batch(p, 1, [(1.0, 1.0, b"trade"), (2.0, 1.5, b"quote"), (3.0, 0.5, b"trade")])
    ids += write_batch(p, 4, [(4.0, 2.0, b"quote")])
    try:
        write_batch(p, 5, [(5.0, 5.0, b"trade"), (6.0, 6.0, b"not-a-kind")])
    except BadEvent as e:
        print("rolled back:", e)
    with p.transaction():                              # an update and a remove in one commit
        seq, x, y, kind = REC.unpack(p.get("log", ids[1]))
        p.put("log", ids[1], REC.pack(seq, x, y + 10.0, kind))
        p.remove("log", ids[2])
    print("last_seq", p.last_seq, "live", len(p.ids("log")), "tombstones", len(p.ids("log", tombstones=True)))
    near = p.nearest("v0", [2.0, 11.0], k=1)[0]
    print("nearest to (2, 11): seq", REC.unpack(p.get("log", near[0]))[0])
    rc = p.drain(30)                                   # 0: the copy has every commit; 2: not in 30 s; 3: refused
    print("copy:", "up to date" if rc == 0 else "away (rc %d)" % rc, "applied", p.copy_applied)
```
<!-- end sample -->

## Block files and a B-tree in journal transactions (beta 2)

<!-- sample: python/examples/ex12_journal_btree.py -->
```python
# python/examples/ex12_journal_btree.py
"""Example 12 (detailed, 7.5 beta 2): a block file and a B-tree index written together in journal transactions.

The C++ pattern of a gxDatabase data file with a gxBtree index, from Python: each record is a block in the data
file, its key goes in the B-tree with the block's address, and a gxdJournal makes both changes one transaction.
A transaction that fails is rolled back in both files; a crash leaves whole transactions only.

    python ex12_journal_btree.py DIR
"""
import os
import struct
import sys
import datareel

d = sys.argv[1]
os.makedirs(d, exist_ok=True)
REC = struct.Struct("<16sQd")                          # C++: struct { char sym[16]; uint64_t day; double close; }
KEY = 24                                               # B-tree key: the symbol (16 bytes), then the day (8, big endian)


def key(sym, day):
    return sym.ljust(16, b"\0") + struct.pack(">Q", day)   # big endian: bytes sort as numbers


j = datareel.Journal(os.path.join(d, "prices.jnl"))   # open the journal first: it replays a crashed run
db = datareel.Database(os.path.join(d, "prices.gxd"), create=not os.path.exists(os.path.join(d, "prices.gxd")))
ix = datareel.BTree(os.path.join(d, "prices.btx"), KEY, create=not os.path.exists(os.path.join(d, "prices.btx")))
j.attach(db)
j.attach(ix)

with datareel.transaction(j):
    for sym, day, close in [(b"ACME", 20261118, 10.5), (b"ACME", 20261119, 11.0), (b"ACME", 20261120, 10.75),
                            (b"BETA", 20261119, 99.0), (b"BETA", 20261120, 101.5)]:
        a = db.alloc(REC.size)
        db.write(a, REC.pack(sym, day, close))
        ix.insert(key(sym, day), a)
try:
    with datareel.transaction(j):
        a = db.alloc(REC.size)
        db.write(a, REC.pack(b"GAMMA", 20261120, 1.0))
        ix.insert(key(b"GAMMA", 20261120), a)
        raise ValueError("a bad batch")
except ValueError:
    print("rolled back: GAMMA is", "there" if ix.prefix(b"GAMMA") else "not there")

print("records", len(db.addresses()), "keys", ix.count())
for k, a in ix.prefix(b"ACME".ljust(16, b"\0")):       # ACME, every day, in day order
    sym, day, close = REC.unpack(db.read(a, REC.size))
    print(sym.rstrip(b"\0").decode(), day, close)
rows = ix.range(key(b"ACME", 20261119), key(b"ACME", 20261121))
print("ACME 11/19 to 11/20:", [REC.unpack(db.read(a, REC.size))[2] for _, a in rows])
j.close()                                              # checkpoint, then close the files
ix.close()
db.close()
```
<!-- end sample -->

## Mistakes to avoid

- Opening a running node's `.gxd` or `.vix` with `BlockFile` or `VectorIndex`: use `Reader`.
- Guessing a record layout from the bytes: get the C++ struct.
- A key of the wrong size or byte order for `find_key()`: pack it like the C++ field (`struct.pack("<Q", n)`
  for a little-endian `uint64_t`).
- Sharing one `Reader` between threads: open one per thread.
- Treating `seq` as a count of objects: it is the node's commit number.
- Writing DataReel files by any means but `datareel.Primary`, or opening as a `Primary` a node another
  process writes (or a copy): one writer per node.
- Changes outside a transaction (`Put() outside a transaction`): use `with p.transaction():`.
- A journal opened after its files, a file attached after `begin()`, or files closed before the journal.
- B-tree keys with little-endian numbers when they must sort as numbers: pack them big endian.
- Calling `Copy.serve()` on the main thread with nothing to call `stop()`: run it in a thread.
- Looping on `datareel.Error`: retry a "full send" or "cannot open" refusal a few times, then report it.
