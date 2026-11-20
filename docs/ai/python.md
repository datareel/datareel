# The datareel Python module (7.5 beta 1)

The `datareel` pip package reads, from Python, the stores the DataReel C++ library writes: gxObjRepl
nodes while they run (`Reader`), vector index files (`VectorIndex`) and block files (`BlockFile`). It is
read only in beta 1. The rules for agents are in `python/AGENTS.md`; the user's manual is
`python/docs/users-manual.md` and the build guide `python/docs/building.md`.

## Build and install

```
python3.12 -m pip wheel python/ --no-build-isolation --no-deps -w dist/     # from the source top
python/tests/run_python_tests.sh /tmp/drpy                                  # every test must pass
python3.12 -m pip install --no-index dist/datareel-7.5b1-cp312-cp312-linux_x86_64.whl
```

Build needs `gcc-c++ cmake openssl-devel python3.12-devel python3.12-setuptools python3.12-wheel`
(Red Hat repositories) and downloads nothing.

## Pick a class

| Task | Use |
| --- | --- |
| Read a gxObjRepl node, running or not | `Reader(root, index_spec)` |
| Objects of a database | `Reader.ids(db)`, `Reader.get(db, id)` (bytes or None) |
| Objects by an index key | `Reader.find_key(index, key_bytes)` |
| Similar vectors in a running node | `Reader.nearest(index, query, k)` |
| Search a closed `.vix` file, with tag filters | `VectorIndex(path, spec).search(query, k, tags=...)` |
| Walk a closed `.gxd` file | `BlockFile(path).addresses()`, `.read(address)` |

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

## Mistakes to avoid

- Opening a running node's `.gxd` or `.vix` with `BlockFile` or `VectorIndex`: use `Reader`.
- Guessing a record layout from the bytes: get the C++ struct.
- A key of the wrong size or byte order for `find_key()`: pack it like the C++ field (`struct.pack("<Q", n)`
  for a little-endian `uint64_t`).
- Sharing one `Reader` between threads: open one per thread.
- Treating `seq` as a count of objects: it is the node's commit number.
- Writing to DataReel files from Python: beta 1 is read only; writes go through the C++ program.
- Looping on `datareel.Error`: retry a "full send" or "cannot open" refusal a few times, then report it.
