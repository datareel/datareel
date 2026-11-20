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
