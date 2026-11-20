"""Example 7 (detailed): search a vector index file directly: graph or exact, tag filters, recall.

VectorIndex opens a .vix file that no process is writing (a day file, a merged month, a copy of a stopped node).
For a running node use Reader.nearest(). search() returns (id, distance, (tag0, tag1, tag2, tag3)), closest
first; tags filters on the 4 integer tags the C++ program stored with each vector: {tag: value} or
{tag: (lo, hi)}. Queries may be lists or float32 buffers (array('f'), numpy float32 arrays).

    python ex7_vector_files.py FILE.vix [SPEC]
"""
import array
import random
import sys
import datareel

path = sys.argv[1]
spec = sys.argv[2] if len(sys.argv) > 2 else None   # when given, a file with another spec is refused
with datareel.VectorIndex(path, spec) as v:
    print("dim", v.dim, "vectors", v.count, "metric", v.metric, "spec", v.spec)
    rnd = random.Random(7)
    queries = [array.array("f", [rnd.uniform(0, 300)] + [0.0] * (v.dim - 1)) for _ in range(20)]
    agree = 0
    for q in queries:
        graph = [h[0] for h in v.search(q, k=5)]
        exact = [h[0] for h in v.search(q, k=5, exact=True)]
        agree += len(set(graph) & set(exact))
    print("recall@5 of the graph search against exact: %.3f" % (agree / (5.0 * len(queries))))
    hit = v.search(queries[0], k=1)[0]
    vec, tags = v.get(hit[0])
    print("nearest", hit[0], "distance %.3f" % hit[1], "vector", vec, "tags", tags)
    print("only tag 0 = 0:", len(v.search(queries[0], k=5, tags={0: 0})), "hits")
    print("only tag 0 in 1..9:", len(v.search(queries[0], k=5, tags={0: (1, 9)})), "hits")
