"""Example 4 (simple): the objects whose vectors are nearest to a query.

    python ex4_nearest.py NODE X
"""
import sys
import datareel

root, x = sys.argv[1], float(sys.argv[2])
with datareel.Reader(root, "k0:vec:B:0:8,v0:vec:V:8:4") as r:
    for oid, distance in r.nearest("v0", [x, 0.0, 0.0, 0.0], k=3):
        print("id", oid, "distance", distance)
