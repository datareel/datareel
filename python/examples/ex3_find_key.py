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
