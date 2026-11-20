"""DataReel 7.5 beta: tests of the datareel Python module against node folders, vector indexes and block files
that the C++ library writes. Run by python/tests/run_python_tests.sh, which sets DR_TEST_BIN to a folder holding
the C++ objrepl tool and test_gxobjrepl_reader (utils/objrepl), built with the 64-bit engine.

The C++ writer (test_gxobjrepl_reader write) makes N commits on a primary: each rewrites the one object in "big"
(12000 bytes: its commit number, then every byte from it) and adds one object to "vec" (the commit number as an
8-byte key, then the vector (k, 0, 0, 0)). The node's indexes are k0 (B-tree on the key) and v0 (the vector).
"""
import array
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

import datareel

BIN = os.environ.get("DR_TEST_BIN", "")
CERTS = os.environ.get("DR_TEST_CERTS_SH", "")
INDEX = "k0:vec:B:0:8,v0:vec:V:8:4"
BIG = 12000
OBJHEAD = struct.Struct("<QIIIIQQ")   # id, version, len, flags, reason, seen_seq, fwd (gxObjRepl's record head)


def prog(name):
    return os.path.join(BIN, name)


def write(root, pki, n, port=None):
    args = [prog("test_gxobjrepl_reader"), "write", root, pki, str(n)]
    if port:
        args.append(str(port))
    return subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def check_big(testcase, b):
    testcase.assertEqual(len(b), BIG)
    k = struct.unpack_from("<Q", b)[0]
    want = bytes(((k * 7 + i) & 0xFF) for i in range(8, BIG))
    testcase.assertEqual(b[8:], want, "big read half written at commit %d" % k)
    return k


class Base(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not BIN or not os.path.exists(prog("test_gxobjrepl_reader")):
            raise RuntimeError("DR_TEST_BIN must name the folder with objrepl and test_gxobjrepl_reader")
        cls.tmp = tempfile.mkdtemp(prefix="dr_pytest_")
        cls.pki = os.path.join(cls.tmp, "pki")
        subprocess.check_call([CERTS, cls.pki, "--copy-host", "127.0.0.1", "--days", "30", "--apply"],
                              stdout=subprocess.DEVNULL)
        cls.p = os.path.join(cls.tmp, "p")
        w = write(cls.p, cls.pki, 300)
        out = w.communicate()[0]
        if w.returncode != 0:
            raise RuntimeError("the writer failed: " + out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)


class ReaderTest(Base):
    def test_module(self):
        self.assertEqual(datareel.engine_bits(), 64)
        self.assertTrue(datareel.library_version().startswith("7."))
        self.assertTrue(datareel.__version__.startswith("7.5"))

    def test_state_and_objects(self):
        with datareel.Reader(self.p, INDEX) as r:
            self.assertEqual(r.role, "primary")
            self.assertEqual(r.seq, 300)
            self.assertEqual(r.epoch, 1)
            self.assertEqual(sorted(r.databases()), ["big", "vec"])
            big = r.ids("big")
            self.assertEqual(len(big), 1)
            self.assertEqual(check_big(self, r.get("big", big[0])), 300)
            vec = r.ids("vec")
            self.assertEqual(len(vec), 300)
            self.assertEqual(vec, sorted(vec))
            self.assertEqual(r.ids("vec", tombstones=True), [])
            self.assertTrue(r.is_live("vec", vec[0]))
            self.assertFalse(r.is_live("vec", 999999999))
            self.assertIsNone(r.get("vec", 999999999))
            self.assertEqual(r.ids("nosuchdb"), [])
        self.assertFalse(r.is_open)
        with self.assertRaises(datareel.Error):
            r.get("big", big[0])

    def test_keys_and_vectors(self):
        with datareel.Reader(self.p, INDEX) as r:
            for j in (1, 2, 150, 299, 300):
                ids = r.find_key("k0", struct.pack("<Q", j))
                self.assertEqual(len(ids), 1, "key %d" % j)
                b = r.get("vec", ids[0])
                self.assertEqual(struct.unpack_from("<Q", b)[0], j)
                self.assertEqual(struct.unpack_from("<4f", b, 8), (float(j), 0.0, 0.0, 0.0))
                hits = r.nearest("v0", [float(j), 0, 0, 0], k=3)
                self.assertEqual(hits[0], (ids[0], 0.0))
                self.assertEqual(len(hits), 3)
                self.assertEqual([h[1] for h in hits], sorted(h[1] for h in hits))
                # a float32 buffer (array('f'), as numpy float32 arrays are) is taken as is
                self.assertEqual(r.nearest("v0", array.array("f", [float(j), 0, 0, 0]), 1)[0][0], ids[0])
            self.assertEqual(r.find_key("k0", struct.pack("<Q", 301)), [])
            with self.assertRaises(datareel.Error):
                r.find_key("nosuch", b"x")
            with self.assertRaises(datareel.Error):
                r.nearest("k0", [1, 0, 0, 0])
            with self.assertRaises(datareel.Error):
                r.find_key("k0", b"x" * 33)
            r.release_vectors()
            self.assertEqual(len(r.nearest("v0", [1, 0, 0, 0], 1)), 1)

    def test_open_errors(self):
        with self.assertRaises(datareel.Error):
            datareel.Reader(os.path.join(self.tmp, "nosuch"))
        with self.assertRaises(datareel.Error):
            datareel.Reader(self.p, "bad index")

    def test_vector_index_file(self):
        with datareel.Reader(self.p, INDEX) as r:
            id5 = r.find_key("k0", struct.pack("<Q", 5))[0]
        path = os.path.join(self.p, "vec.v0.vix")
        with datareel.VectorIndex(path, "objrepl/v0/8/4") as v:
            self.assertEqual(v.dim, 4)
            self.assertEqual(v.count, 300)
            self.assertEqual(v.spec, "objrepl/v0/8/4")
            self.assertEqual(v.metric, "l2")
            hits = v.search([5, 0, 0, 0], k=3)
            self.assertEqual(hits[0][0], id5)
            self.assertEqual(hits[0][1], 0.0)
            self.assertEqual(len(hits[0][2]), 4)
            self.assertEqual(v.search([5, 0, 0, 0], k=3, exact=True), hits)
            self.assertEqual(v.search([5, 0, 0, 0], k=3, tags={0: (0, 0)}), hits)   # every tag is 0 here
            self.assertEqual(v.search([5, 0, 0, 0], k=3, tags={0: 1}), [])
            vec, tags = v.get(id5)
            self.assertEqual(vec, [5.0, 0.0, 0.0, 0.0])
            self.assertIsNone(v.get(123456789))
            with self.assertRaises(datareel.Error):
                v.search([1, 2], k=1)
        with self.assertRaises(datareel.Error):
            datareel.VectorIndex(path, "another/spec")

    def test_block_file(self):
        with datareel.BlockFile(os.path.join(self.p, "vec.gxd")) as f:
            keys = []
            for a in f.addresses():
                b = f.read(a)
                oid, ver, n, flags, reason, seen, fwd = OBJHEAD.unpack_from(b)
                if flags == 0:
                    keys.append(struct.unpack_from("<Q", b, OBJHEAD.size)[0])
            self.assertEqual(sorted(keys), list(range(1, 301)))
            with self.assertRaises(datareel.Error):
                f.read(1)
        with self.assertRaises(datareel.Error):
            datareel.BlockFile(os.path.join(self.tmp, "nosuch.gxd"))


class LiveTest(Base):
    """Reads while the C++ program writes, from Python threads (the GIL is released during reads)."""

    def read_while(self, root, proc, seconds=60):
        stats = {"reads": 0, "keys": 0, "bad": [], "last_k": 0, "last_seq": 0}
        r = None
        t0 = time.time()
        while r is None:
            try:
                r = datareel.Reader(root, INDEX)
            except datareel.Error:
                if time.time() - t0 > 30:
                    raise
                time.sleep(0.05)
        with r:
            while proc.poll() is None and time.time() - t0 < seconds:
                try:
                    big = r.ids("big")
                    if r.seq < stats["last_seq"]:
                        stats["bad"].append("seq went back")
                    stats["last_seq"] = r.seq
                    if len(big) != 1:
                        continue
                    b = r.get("big", big[0])
                    if b is None:
                        continue
                    k = check_big(self, b)
                    if k < stats["last_k"] or r.seq < k:
                        stats["bad"].append("commit %d after %d, seq %d" % (k, stats["last_k"], r.seq))
                    stats["last_k"] = k
                    stats["reads"] += 1
                    j = 1 + (stats["reads"] * 7919) % k
                    ids = r.find_key("k0", struct.pack("<Q", j))
                    if len(ids) != 1:
                        stats["bad"].append("key %d found %d times at commit %d" % (j, len(ids), k))
                        continue
                    if r.nearest("v0", [float(j), 0, 0, 0], 1)[0][0] != ids[0]:
                        stats["bad"].append("nearest to %d" % j)
                    stats["keys"] += 1
                except datareel.Error as e:
                    # refused for now (a database file being created): try again
                    if "cannot open" not in str(e):
                        stats["bad"].append(str(e))
        return stats

    def test_primary_while_writing(self):
        root = os.path.join(self.tmp, "live")
        w = write(root, self.pki, 2000)
        results = []
        threads = [threading.Thread(target=lambda: results.append(self.read_while(root, w))) for _ in range(2)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        out = w.communicate()[0]
        self.assertEqual(w.returncode, 0, out)
        self.assertEqual(len(results), 2, "a reader thread failed (see above)")
        for s in results:
            self.assertEqual(s["bad"], [])
            self.assertGreater(s["reads"], 10)
            self.assertGreater(s["keys"], 10)
        with datareel.Reader(root, INDEX) as r:
            self.assertEqual(check_big(self, r.get("big", r.ids("big")[0])), 2000)
            self.assertEqual(len(r.ids("vec")), 2000)

    def test_copy_while_applying(self):
        port = free_port()
        q, c = os.path.join(self.tmp, "q"), os.path.join(self.tmp, "c")
        os.makedirs(c)
        log = open(os.path.join(self.tmp, "copy.log"), "w")
        copy = subprocess.Popen([prog("objrepl"), "copy", c, "--listen", "127.0.0.1:%d" % port,
                                 "--ca", os.path.join(self.pki, "ca.pem"), "--cert", os.path.join(self.pki, "copy.pem"),
                                 "--key", os.path.join(self.pki, "copy.key"), "--allow-cn", "objrepl-primary",
                                 "--index", INDEX], stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                with open(os.path.join(self.tmp, "copy.log")) as f:
                    if "listening" in f.read():
                        break
                time.sleep(0.1)
            w = write(q, self.pki, 800, port)
            s = self.read_while(c, w)
            out = w.communicate()[0]
            self.assertEqual(w.returncode, 0, out)
            self.assertEqual(s["bad"], [])
            self.assertGreater(s["reads"], 10)
            with datareel.Reader(c, INDEX) as r:
                self.assertEqual(r.role, "copy")
                self.assertEqual(r.seq, 800)
                self.assertEqual(check_big(self, r.get("big", r.ids("big")[0])), 800)
                self.assertEqual(len(r.ids("vec")), 800)
        finally:
            copy.terminate()
            copy.wait(30)
            log.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
