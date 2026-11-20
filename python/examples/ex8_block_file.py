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
