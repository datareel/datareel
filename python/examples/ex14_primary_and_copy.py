"""Example 14 (detailed): replication with both nodes in Python: a copy serving in a thread, a
primary writing and draining to it, then a failover (takeover) and the old primary brought back (rejoin).

    python ex14_primary_and_copy.py DIR PKI [PORT]
"""
import os
import struct
import sys
import threading
import time
import datareel

d, pki = sys.argv[1], sys.argv[2]
port = int(sys.argv[3]) if len(sys.argv) > 3 else 20741
k = lambda f: os.path.join(pki, f)
INDEX = "k0:ev:B:0:8"
primary = {"copy": "127.0.0.1:%d" % port, "ca": k("ca.pem"), "cert": k("primary.pem"), "key": k("primary.key"),
           "index": INDEX}
copy = {"listen": "127.0.0.1:%d" % port, "ca": k("ca.pem"), "cert": k("copy.pem"), "key": k("copy.key"),
        "allow-cn": "objrepl-primary", "index": INDEX}
a, b = os.path.join(d, "node_a"), os.path.join(d, "node_b")


def serve(root):
    c = datareel.Copy(root, settings=copy, quiet=True)
    t = threading.Thread(target=c.serve)
    t.start()
    time.sleep(0.3)
    return c, t


c, t = serve(b)                                        # B: the copy
with datareel.Primary(a, settings=primary, quiet=True) as p:   # A: the primary
    p.open_database("ev")
    for batch in range(5):
        with p.transaction():
            for i in range(10):
                n = batch * 10 + i
                p.put("ev", p.new_id(), struct.pack("<Qd", n, n * 1.5))
    print("A drained:", p.drain(30) == 0, "copy applied", p.copy_applied)
print("B holds", len(c.ids("ev")), "objects at seq", c.applied_seq)
c.stop(); t.join(); c.close()
print("A and B equal:", datareel.digest(a)[0] == datareel.digest(b)[0])

print(datareel.takeover(b))                            # A is gone: B becomes the primary
print(datareel.rejoin(a))                              # A comes back as B's copy
c, t = serve(a)
with datareel.Primary(b, settings=primary, quiet=True) as p:
    with p.transaction():
        p.put("ev", p.new_id(), struct.pack("<Qd", 100, 150.0))
    print("B epoch", p.epoch, "drained:", p.drain(30) == 0)
c.stop(); t.join(); c.close()
print("roles:", datareel.status(a)["role"], datareel.status(b)["role"],
      "equal:", datareel.digest(a)[0] == datareel.digest(b)[0])
