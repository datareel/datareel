# The datareel Python module: user's manual (DataReel 7.5 beta 2)

The `datareel` module reads and writes, from Python, the object databases that programs written with the
DataReel C++ library keep: gxObjRepl nodes (a primary or its copy), vector index files (`.vix`) and block
files (`.gxd`). Python code reads the same stores the production C++ programs write, with no C++ helper and
no export file in between, and since 7.5 beta 2 it can also be the primary that writes a node and
replicates it to a C++ copy.

Contents: 1. Install. 2. Ideas you need. 3. A demo node. 4. Simple examples (1 to 4). 5. Detailed examples
(5 to 9). 5a. Writing from Python (examples 10 and 11, beta 2). 6. Reference. 7. Errors and what to do.
8. Limits of beta 2.

## 1. Install

```
python3.12 -m pip install --no-index datareel-7.5b2-cp312-cp312-linux_x86_64.whl
python3.12 -c "import datareel; print(datareel.__version__, datareel.library_version())"
```

The wheel runs on RHEL 9 and RHEL 10 (x86_64, CPython 3.12) and needs only the system OpenSSL 3. To build
it, see `python/docs/building.md`.

## 2. Ideas you need

- **A node** is a folder that one C++ process keeps open with `gxObjRepl` (gxobjrepl.h): its databases
  (`NAME.gxd` with `NAME.id.btx`), its indexes, its journal (`node.jnl`) and its state. A node is a
  **primary** (the application writes to it) or a **copy** (it applies what its primary sends).
- **Objects** have a 64-bit id. The module returns an object as the bytes the C++ program stored. You
  decode them with `struct` or numpy, using the C++ program's record layout (example 5).
- **A removed object** stays as a **tombstone** with its id; `get()` returns `None` for it, and
  `ids(db, tombstones=True)` lists them.
- **Indexes** belong to the node: B-tree indexes on up to 32 bytes at an offset of the object, and vector
  indexes on floats at an offset. The node's **index spec** names them, for example
  `k0:vec:B:0:8,v0:vec:V:8:4` ("k0 is a B-tree on 8 bytes at offset 0 of database vec; v0 is a vector
  index on 4 floats at offset 8"). Give `Reader` the same spec the node uses.
- **Whole commits only.** A `Reader` reads while the node runs. Each call takes the node journal's shared
  lock, so it sees only whole commits, never half of one. `reader.seq` names the commit the last call
  read (a primary's last commit, a copy's last applied one). Two calls may see different commits.
- **One process writes.** A node folder is held by one process at a time: the C++ program, or (beta 2)
  the Python program that opened it with `datareel.Primary`. Readers never block it for long and never
  write. Never open a running node's files with `BlockFile` or `VectorIndex`: use `Reader`.
- **Transactions** (beta 2). A `Primary` changes objects only inside a transaction: `begin()`, then
  `put`/`remove`/`purge`, then `commit()` (durable when it returns) or `rollback()`. `with p.transaction():`
  does that for you and rolls back when the block raises.

## 3. A demo node

Every example runs against a demo node that a C++ program writes. With a 64-bit engine build of DataReel
in `$BUILD` (`cmake -DDATAREEL_64BIT_DATABASE=ON`, which builds `utils/test_gxobjrepl_reader`):

```
python/examples/make_demo_node.sh $BUILD /tmp/demo 300
```

The node `/tmp/demo/node` has two databases. `big` holds one 12000-byte object that every commit rewrites.
`vec` gets one object per commit: `struct { uint64_t key; float v[4]; }` with key = the commit number and
v = (key, 0, 0, 0). Its index spec is `k0:vec:B:0:8,v0:vec:V:8:4`.

## 4. Simple examples

### Example 1: open a node and see what is in it

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

```
$ python3.12 ex1_open_and_list.py /tmp/demo/node
role primary seq 300 epoch 1
database big objects 1 tombstones 0
database vec objects 300 tombstones 0
```

### Example 2: read objects and decode them

<!-- sample: python/examples/ex2_get_and_decode.py -->
```python
# python/examples/ex2_get_and_decode.py
"""Example 2 (simple): read objects and decode their bytes with struct.

The C++ program stored each "vec" object as  struct { uint64_t key; float v[4]; }  (24 bytes, little endian).

    python ex2_get_and_decode.py NODE
"""
import struct
import sys
import datareel

VEC = struct.Struct("<Q4f")                      # the C++ record layout

with datareel.Reader(sys.argv[1]) as r:
    ids = r.ids("vec")
    for oid in ids[:5]:
        b = r.get("vec", oid)                    # bytes, or None when the object is not live
        key, *v = VEC.unpack_from(b)
        print("id", oid, "key", key, "vector", v)
    print("not there:", r.get("vec", 1))         # an id no object has: None
```
<!-- end sample -->

```
$ python3.12 ex2_get_and_decode.py /tmp/demo/node
id 1099511627778 key 1 vector [1.0, 0.0, 0.0, 0.0]
id 1099511627779 key 2 vector [2.0, 0.0, 0.0, 0.0]
...
not there: None
```

Ids come from the node: the epoch in the high bits, then a counter.

### Example 3: find objects by key

<!-- sample: python/examples/ex3_find_key.py -->
```python
# python/examples/ex3_find_key.py
"""Example 3 (simple): find objects by an index key.

find_key() takes the key as the bytes the C++ index holds (here an 8-byte little-endian number) and returns the
ids whose key starts with those bytes.

    python ex3_find_key.py NODE KEY
"""
import struct
import sys
import datareel

root, key = sys.argv[1], int(sys.argv[2])
with datareel.Reader(root, "k0:vec:B:0:8,v0:vec:V:8:4") as r:   # the node's index spec
    ids = r.find_key("k0", struct.pack("<Q", key))
    print("key", key, "ids", ids)
    for oid in ids:
        print("  object", struct.unpack_from("<Q4f", r.get("vec", oid)))
```
<!-- end sample -->

```
$ python3.12 ex3_find_key.py /tmp/demo/node 42
key 42 ids [1099511627819]
  object (42, 42.0, 0.0, 0.0, 0.0)
```

The key bytes must be the bytes the index holds: the same size and byte order as the C++ field.
A shorter key finds every key that starts with it (a prefix search).

### Example 4: nearest vectors

<!-- sample: python/examples/ex4_nearest.py -->
```python
# python/examples/ex4_nearest.py
"""Example 4 (simple): the objects whose vectors are nearest to a query.

    python ex4_nearest.py NODE X
"""
import sys
import datareel

root, x = sys.argv[1], float(sys.argv[2])
with datareel.Reader(root, "k0:vec:B:0:8,v0:vec:V:8:4") as r:
    for oid, distance in r.nearest("v0", [x, 0.0, 0.0, 0.0], k=3):
        print("id", oid, "distance", distance)
```
<!-- end sample -->

```
$ python3.12 ex4_nearest.py /tmp/demo/node 10.2
id 1099511627787 distance 0.03999992460012436
id 1099511627788 distance 0.6400002837181091
id 1099511627786 distance 1.4399995803833008
```

The distance is the index's metric: for `l2` the squared distance, (10.2 - 10)^2 = 0.04 here.

## 5. Detailed examples

### Example 5: a record layout class, and a whole database as rows

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

```
$ python3.12 ex5_record_layout.py /tmp/demo/node
seq 300 rows 300
first {'key': 1, 'v': (1.0, 0.0, 0.0, 0.0)}
keys from 1 to 300
mean of v[0] 150.5
numpy rows 300 mean of v[0] 150.5          (or: numpy is not installed: struct only)
```

Match the C++ struct exactly: field order, sizes, byte order and padding. A C++ struct with padding
(an `int32_t` before a `double`, for example) needs the pad bytes in the layout too (`"4x"` in struct).
DataReel records never hold pointers or `std::string`, so every record is fixed size.

### Example 6: follow a running node

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

Run it while a C++ writer commits (here the demo writer making 1500 commits in another shell):

```
$ python3.12 ex6_follow_node.py /tmp/live 25
seq 68 new objects 68 last key 68
seq 71 new objects 3 last key 71
seq 72 new objects 1 last key 72
...
followed 1500 objects up to seq 1500
```

The counts per line depend on timing. `refresh()` reads the node's state; a new `seq` means new commits.
For large databases, keep your own position (the last key or time you read) in an index and use
`find_key()` instead of listing every id.

### Example 7: vector index files, tag filters and recall

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

```
$ python3.12 ex7_vector_files.py /tmp/demo/node/vec.v0.vix objrepl/v0/8/4
dim 4 vectors 300 metric l2 spec objrepl/v0/8/4
recall@5 of the graph search against exact: 1.000
nearest 1099511627874 distance 0.022 vector [97.0, 0.0, 0.0, 0.0] tags (0, 0, 0, 0)
only tag 0 = 0: 5 hits
only tag 0 in 1..9: 0 hits
```

Indexes of up to 4096 vectors, and very selective filters, are always searched exactly. A gxObjRepl node
names its vector index specs `objrepl/NAME/OFFSET/DIM`. The demo node is stopped here; for a running node
use `Reader.nearest()`.

### Example 8: walk a block file

<!-- sample: python/examples/ex8_block_file.py -->
```python
# python/examples/ex8_block_file.py
"""Example 8 (detailed): walk a .gxd block file and read gxObjRepl records with their record head.

BlockFile opens a .gxd file that no process is writing. read(address) returns the whole block. In a gxObjRepl
node every block starts with a 40-byte record head, then the object's bytes:

    struct ObjHead { uint64_t id; uint32_t version, len, flags, reason; uint64_t seen_seq, fwd; };
    flags: 1 = tombstone, 2 = forwarding stub (an object that moved)

    python ex8_block_file.py FILE.gxd
"""
import struct
import sys
import datareel

HEAD = struct.Struct("<QIIIIQQ")
counts = {"live": 0, "tombstone": 0, "stub": 0}
with datareel.BlockFile(sys.argv[1]) as f:
    addrs = f.addresses()
    for a in addrs:
        b = f.read(a)
        oid, version, n, flags, reason, seen_seq, fwd = HEAD.unpack_from(b)
        kind = "stub" if flags & 2 else "tombstone" if flags & 1 else "live"
        counts[kind] += 1
        if kind == "live" and counts["live"] <= 3:
            body = b[HEAD.size:HEAD.size + n]
            print("address", a, "id", oid, "version", version, "bytes", n, "first 8", body[:8].hex())
print("blocks", len(addrs), counts)
```
<!-- end sample -->

```
$ python3.12 ex8_block_file.py /tmp/demo/node/vec.gxd
address 136 id 1099511627778 version 1 bytes 24 first 8 0100000000000000
address 232 id 1099511627779 version 1 bytes 24 first 8 0200000000000000
address 328 id 1099511627780 version 1 bytes 24 first 8 0300000000000000
blocks 300 {'live': 300, 'tombstone': 0, 'stub': 0}
```

`BlockFile` works on any DataReel `.gxd` file, not only gxObjRepl databases: other programs' blocks
start with their own records. Use it on files no process is writing (a copied or archived file, a day
file that is closed).

### Example 9: several threads

<!-- sample: python/examples/ex9_threads.py -->
```python
# python/examples/ex9_threads.py
"""Example 9 (detailed): read from several threads at once.

The module releases the GIL while it reads files, so threads read in parallel. Objects are not thread safe:
give each thread its own Reader.

    python ex9_threads.py NODE [THREADS]
"""
import concurrent.futures
import struct
import sys
import time
import datareel

root = sys.argv[1]
nthreads = int(sys.argv[2]) if len(sys.argv) > 2 else 4
INDEX = "k0:vec:B:0:8,v0:vec:V:8:4"

with datareel.Reader(root) as r:
    ids = r.ids("vec")


def work(part):
    found = 0
    with datareel.Reader(root, INDEX) as r:      # one Reader per thread
        for oid in part:
            key = struct.unpack_from("<Q", r.get("vec", oid))[0]
            if r.find_key("k0", struct.pack("<Q", key)) == [oid]:
                found += 1
    return found


parts = [ids[i::nthreads] for i in range(nthreads)]
t0 = time.time()
with concurrent.futures.ThreadPoolExecutor(nthreads) as pool:
    total = sum(pool.map(work, parts))
print("threads", nthreads, "objects checked", total, "of", len(ids), "in %.2f s" % (time.time() - t0))
```
<!-- end sample -->

```
$ python3.12 ex9_threads.py /tmp/demo/node 4
threads 4 objects checked 300 of 300 in 0.05 s
```

## 5a. Writing from Python (7.5 beta 2)

`datareel.Primary(root, config=None, settings=None, quiet=False)` opens (or makes) a node folder as a
gxObjRepl primary in this Python process. `settings` is a dict of the node's configuration keys, the same
ones `objrepl.conf` holds (`copy`, `ca`, `cert`, `key`, `index`, `links`, `name`, `peer-name`, `trim`,
`idle-timeout`, `ack-timeout`); `config` names an `objrepl.conf` file instead, and `settings` wins over it.
A primary always needs its certificates, even before any copy listens. `quiet=True` drops the node's
messages, which otherwise go to stderr.

### Example 10: write objects, then read them back

<!-- sample: python/examples/ex10_write_objects.py -->
```python
# python/examples/ex10_write_objects.py
"""Example 10 (simple, 7.5 beta 2): write objects from Python as a gxObjRepl primary, then read them back.

A primary needs the node's certificates even when no copy is listening (it connects when you pump()). The
demo node's certificates are in the folder make_demo_node.sh made (PKI). ROOT is a new folder for this node.

    python ex10_write_objects.py ROOT PKI
"""
import os
import struct
import sys
import datareel

root, pki = sys.argv[1], sys.argv[2]
settings = {
    "copy": "127.0.0.1:2073",                          # the copy (nobody listens in this example)
    "ca": os.path.join(pki, "ca.pem"),
    "cert": os.path.join(pki, "primary.pem"),
    "key": os.path.join(pki, "primary.key"),
    "index": "k0:events:B:0:8",                        # a B-tree on the 8-byte key at offset 0
}
EVENT = struct.Struct("<Qd16s")                        # C++: struct { uint64_t key; double value; char name[16]; }

with datareel.Primary(root, settings=settings, quiet=True) as p:
    p.open_database("events")                          # before the first transaction
    with p.transaction():                              # one commit: all of it or none of it
        for key, value, name in [(1, 10.5, b"open"), (2, 11.25, b"high"), (3, 9.75, b"low")]:
            p.put("events", p.new_id(), EVENT.pack(key, value, name))
    print("committed: last_seq", p.last_seq, "objects", len(p.ids("events")))
    oid = p.find_key("k0", struct.pack("<Q", 2))[0]
    key, value, name = EVENT.unpack(p.get("events", oid))
    print("key 2:", value, name.rstrip(b"\0").decode())

with datareel.Reader(root, "k0:events:B:0:8") as r:     # any other process can read it the same way
    print("reader: seq", r.seq, "objects", len(r.ids("events")))
```
<!-- end sample -->

```
$ python3.12 ex10_write_objects.py /tmp/ex10 /tmp/demo/pki
committed: last_seq 1 objects 3
key 2: 11.25 high
reader: seq 1 objects 3
```

### Example 11: an event log with updates, removes, a rolled back batch and a copy

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

With a C++ copy listening (`objrepl copy /tmp/copy --listen 127.0.0.1:20731 --ca ... --allow-cn
objrepl-primary`, see the replication administrator's guide):

```
$ python3.12 ex11_event_log.py /tmp/ex11 /tmp/demo/pki 20731
rolled back: kind b'not-a-kind' is longer than 8 bytes
last_seq 3 live 3 tombstones 1
nearest to (2, 11): seq 2
copy: up to date applied 3
```

Without a copy, `drain(30)` waits the 30 seconds and the last line says `copy: away (rc 2) applied 0`;
the commits stay in the node's outbox until a copy takes them.

What to know when Python writes:

- Encode objects with the same layout the C++ readers use (`struct.pack`, or numpy `tobytes()` with the
  dtype of example 5). Index fields must be at the offsets the index spec names.
- `new_id()` gives ids no database of the node has used; keep the ids you need to update or remove
  objects later, or find them through an index.
- `commit()` is durable when it returns. A Python program killed mid-transaction loses only that
  transaction: the next open replays the journal (the tests kill one with KILL and check).
- `pump()` after commits, or `drain(seconds)` at the end, sends the copy what it lacks. Commits are durable
  on the primary whether or not a copy is there.
- A copy's folder is never opened as a primary (`datareel.Error`). After a failover, the old primary
  comes back as a copy with `objrepl rejoin`.

## 6. Reference

### datareel.Reader(root, index="")

A gxObjRepl node folder, read only, while the node runs in another process (or none runs). `index` is the
node's index spec, needed for `find_key()` and `nearest()`. Raises `datareel.Error` when the folder is not
a node or the spec is bad. A context manager (`with`), or call `close()`.

| Call | Returns |
| --- | --- |
| `get(db, id)` | the object's bytes, or `None` when it is not live (removed or never there) |
| `is_live(db, id)` | `True` or `False` |
| `ids(db, tombstones=False)` | the sorted ids of the live objects (or of the tombstones); `[]` for a database that is not there |
| `databases()` | the database names |
| `find_key(index, key)` | ids whose key in B-tree index `index` starts with the bytes `key` (at most 32) |
| `nearest(index, query, k=10)` | `[(id, distance)]`, closest first, from vector index `index` |
| `refresh()` | reads the node's state now (`seq`, `epoch`, `role`) |
| `release_vectors()` | closes the vector indexes `nearest()` loaded |
| `close()` | |
| `seq`, `epoch`, `role`, `is_open` | the commit the last call read; the epoch; `'primary'`, `'copy'` or `'none'` |

`nearest()` keeps the vector index in memory and reads it again when the node has moved past the commit
it was loaded at. On a busy node with a large index, search in batches between commits, or search a
merged `.vix` file with `VectorIndex`.

### datareel.Primary(root, config=None, settings=None, quiet=False) (beta 2)

A gxObjRepl primary in this process (see section 5a). A context manager (`with`), or call `close()`.

| Call | Does |
| --- | --- |
| `open_database(db)` | opens or makes database `db` (outside a transaction) |
| `begin()`, `commit()`, `rollback()` | a transaction; `commit()` is durable when it returns |
| `with p.transaction():` | `begin()`, then `commit()`, or `rollback()` when the block raises |
| `new_id()` | an object id no database of the node has used |
| `put(db, id, data)` | adds or updates an object (bytes-like, at most 32 MB; a commit at most 60 MB) |
| `remove(db, id)` | the object becomes a tombstone |
| `purge(db, id)` | frees a tombstone for good (refused while a live link names it) |
| `pump()` | sends the copy what it lacks: 0 it has every commit, 1 behind or away, 3 refused (an alarm) |
| `drain(seconds=60)` | pumps until the copy has every commit: 0, 2 (not in time) or 3 |
| `get`, `is_live`, `ids`, `databases`, `find_key`, `nearest` | as `Reader`, and they see the open transaction's own changes |
| `close()` | checkpoints and closes (an open transaction is rolled back) |
| `last_seq`, `epoch`, `copy_applied`, `is_open` | the last commit, the epoch, the copy's applied commit as last heard |

### datareel.VectorIndex(path, spec=None)

A `.vix` file opened read only. With `spec`, a file whose spec differs is refused.

| Call | Returns |
| --- | --- |
| `search(query, k=10, ef=64, exact=False, tags=None)` | `[(id, distance, (t0, t1, t2, t3))]`, closest first. `ef`: the graph search width. `exact=True`: a full scan. `tags`: `{tag: value}` or `{tag: (lo, hi)}` for tags 0 to 3 |
| `get(id)` | `(vector, tags)`, or `None` |
| `close()` | |
| `dim`, `count`, `spec`, `metric` | values per vector, live vectors, the spec text, `'l2'`, `'cosine'` or `'ip'` |

### datareel.BlockFile(path)

A `.gxd` file opened read only: `addresses()` (every object's address, in file order), `read(address)`
(the whole block's bytes), `close()`.

### Module

`datareel.__version__` (`"7.5b2"`), `datareel.library_version()` (the C++ library, `"7.5 beta2"`),
`datareel.engine_bits()` (64), `datareel.Error` (a `RuntimeError`).

Query vectors are lists or tuples of numbers, or any float32 buffer (`array('f')`, a numpy `float32` array).
Calls that read files release the GIL, so threads read in parallel. A `Reader`, `VectorIndex` or `BlockFile`
is not thread safe: give each thread its own.

## 7. Errors and what to do

| Message | Meaning | What to do |
| --- | --- | --- |
| `no state in ROOT (not a node folder)` | the folder is not a gxObjRepl node | check the path |
| `a full send is under way` | the copy is receiving a full send | wait and try again |
| `cannot open ROOT/NAME.gxd` | the node is creating that database right now, or the file is damaged | try again; if it stays, run `objrepl check ROOT` |
| `no B-tree index NAME`, `no vector index NAME` | the name is not in the spec you gave `Reader` | give the node's index spec |
| `the index NAME is being built` | the node is building a new index | wait for the build |
| `cannot open FILE: the spec differs` | the `.vix` file was made with another spec | use the right file or spec |
| `the query has N values, the index M` | the query's length is not the index's `dim` | |
| `Put() outside a transaction` | `put`/`remove`/`purge` without `begin()` | use `with p.transaction():` |
| `ROOT is in use by another process` | another process (or another `Primary`) holds the node | one writer per node; readers use `Reader` |
| `ROOT is a copy: a copy is not written to` | the folder is a copy | write to the primary; after a failover use takeover/rejoin |
| `id 0 is not an object id` | `put` with id 0 | use `new_id()` |

## 8. Limits of beta 2

- Python writes as a gxObjRepl primary. A copy (`Serve()`), takeover and rejoin from Python are not in
  beta 2: run the C++ `objrepl` tool for those.
- The 64-bit database engine only (the one gxObjRepl nodes use).
- Linux x86_64 and CPython 3.12 wheels. Windows wheels are not built yet.
- `VectorIndex` and `BlockFile` are for files no process is writing.
