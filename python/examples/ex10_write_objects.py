"""Example 10 (simple): write objects from Python as a gxObjRepl primary, then read them back.

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
