"""Example 11 (detailed): an event log written from Python: batches, updates, removes, a rolled
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
