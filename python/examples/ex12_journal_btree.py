"""Example 12 (detailed): a block file and a B-tree index written together in journal transactions.

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
