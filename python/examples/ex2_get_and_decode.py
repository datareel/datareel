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
