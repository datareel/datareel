"""Example 13 (detailed, 7.5 beta 2): write vector index files from Python: day files with tags, an update, a
delete, a check, and a merge into one month file.

    python ex13_vector_files_write.py DIR
"""
import os
import random
import sys
import datareel

d = sys.argv[1]
os.makedirs(d, exist_ok=True)
SPEC = "example/prices/v1"                             # files of another spec are refused when opened with this one
rnd = random.Random(13)
days = []
for day in (20261118, 20261119, 20261120):
    path = os.path.join(d, "day_%d.vix" % day)
    datareel.create_vector_index(path, 4, metric="cosine", spec=SPEC)
    with datareel.VectorIndex(path, SPEC, write=True) as v:
        for i in range(100):
            oid = day * 1000 + i                       # ids are unique across the day files
            v.insert(oid, [rnd.uniform(-1, 1) for _ in range(4)], tags=[i % 3, 0, day, 0])
        v.update(day * 1000, [1.0, 0.0, 0.0, 0.0])     # a new vector for an id
        v.delete(day * 1000 + 99)
        problems, report = v.verify()
        print(os.path.basename(path), "vectors", v.count, "problems", problems)
    days.append(path)

month = os.path.join(d, "month_202611.vix")
datareel.merge_vector_indexes(month, days, spec=SPEC)
with datareel.VectorIndex(month, SPEC) as v:
    print("month: vectors", v.count, "metric", v.metric)
    hits = v.search([1.0, 0.0, 0.0, 0.0], k=3)
    print("nearest to (1, 0, 0, 0):", [h[0] for h in hits])
    hits = v.search([1.0, 0.0, 0.0, 0.0], k=3, tags={2: 20261119})
    print("only 11/19:", sorted({h[2][2] for h in hits}))
