"""Example 9 (detailed): read from several threads at once.

The module releases the GIL while it reads files, so threads read in parallel. Objects are not thread safe:
give each thread its own Reader.

    python ex9_threads.py NODE [THREADS]
"""
import concurrent.futures
import struct
import sys
import time
import datareel

root = sys.argv[1]
nthreads = int(sys.argv[2]) if len(sys.argv) > 2 else 4
INDEX = "k0:vec:B:0:8,v0:vec:V:8:4"

with datareel.Reader(root) as r:
    ids = r.ids("vec")


def work(part):
    found = 0
    with datareel.Reader(root, INDEX) as r:      # one Reader per thread
        for oid in part:
            key = struct.unpack_from("<Q", r.get("vec", oid))[0]
            if r.find_key("k0", struct.pack("<Q", key)) == [oid]:
                found += 1
    return found


parts = [ids[i::nthreads] for i in range(nthreads)]
t0 = time.time()
with concurrent.futures.ThreadPoolExecutor(nthreads) as pool:
    total = sum(pool.map(work, parts))
print("threads", nthreads, "objects checked", total, "of", len(ids), "in %.2f s" % (time.time() - t0))
