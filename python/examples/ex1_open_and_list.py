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
