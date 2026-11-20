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
