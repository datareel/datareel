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
        self.assertTrue(issubclass(datareel.Primary, object))

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


class PrimaryTest(Base):
    """7.5 beta 2: a primary written from Python, checked by the C++ library and a C++ copy."""

    def settings(self, port=1):
        return {"copy": "127.0.0.1:%d" % port, "ca": os.path.join(self.pki, "ca.pem"),
                "cert": os.path.join(self.pki, "primary.pem"), "key": os.path.join(self.pki, "primary.key"),
                "index": INDEX}

    def write_like_cpp(self, p, n, start=1):
        """The C++ writer's layout (test_gxobjrepl_reader write): one big object rewritten by each commit and one
        vec object per commit, so the C++ checks apply to what Python wrote."""
        big = p.ids("big")
        bid = big[0] if big else 0
        for k in range(start, n + 1):
            with p.transaction():
                if not bid:
                    bid = p.new_id()
                p.put("big", bid, struct.pack("<Q", k) + bytes(((k * 7 + i) & 0xFF) for i in range(8, BIG)))
                p.put("vec", p.new_id(), struct.pack("<Q4f", k, float(k), 0, 0, 0))

    def test_write_then_cpp_checks(self):
        root = os.path.join(self.tmp, "pyp")
        with datareel.Primary(root, settings=self.settings(), quiet=True) as p:
            p.open_database("big")
            p.open_database("vec")
            self.write_like_cpp(p, 200)
            self.assertEqual(p.last_seq, 200)
            self.assertEqual(p.epoch, 1)
            self.assertEqual(len(p.ids("vec")), 200)
            ids = p.find_key("k0", struct.pack("<Q", 77))
            self.assertEqual(len(ids), 1)
            self.assertEqual(p.nearest("v0", [77, 0, 0, 0], 1)[0], (ids[0], 0.0))
            self.assertEqual(p.pump(), 1)                       # no copy at port 1: away
        out = subprocess.run([prog("test_gxobjrepl_reader"), "final", root, "200"], stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
        self.assertEqual(out.returncode, 0, out.stdout)        # the C++ reader: whole commits, 200 of them
        ix = subprocess.run([prog("objrepl"), "ixcheck", root, "--index", INDEX], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
        self.assertEqual(ix.returncode, 0, ix.stdout)          # every index entry matches the objects
        self.assertEqual(ix.stdout.count("consistent"), 2, ix.stdout)

    def test_killed_python_primary(self):
        """A Python primary killed (KILL) in the middle of its commits: the next open replays the journal, and
        what the files hold is whole commits ("big" names as many commits as "vec" holds)."""
        root = os.path.join(self.tmp, "pykill")
        code = ("import datareel, struct, sys\n"
                "st = %r\n"
                "p = datareel.Primary(%r, settings=st, quiet=True)\n"
                "p.open_database('big'); p.open_database('vec')\n"
                "bid = 0\n"
                "for k in range(1, 100000):\n"
                "    p.begin()\n"
                "    bid = bid or p.new_id()\n"
                "    p.put('big', bid, struct.pack('<Q', k) + bytes(((k * 7 + i) & 0xFF) for i in range(8, %d)))\n"
                "    p.put('vec', p.new_id(), struct.pack('<Q4f', k, float(k), 0, 0, 0))\n"
                "    p.commit()\n"
                "    if k == 300: print('300', flush=True)\n") % (self.settings(), root, BIG)
        w = subprocess.Popen([sys.executable, "-I", "-c", code], stdout=subprocess.PIPE, text=True)
        self.assertEqual(w.stdout.readline().strip(), "300")
        time.sleep(0.5)
        w.kill()
        w.wait()
        w.stdout.close()
        out = subprocess.run([prog("test_gxobjrepl_reader"), "final", root], stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
        self.assertEqual(out.returncode, 0, out.stdout)
        with datareel.Primary(root, settings=self.settings(), quiet=True) as p:   # and it opens to write again
            n = p.last_seq
            self.assertGreaterEqual(n, 300)
            self.write_like_cpp(p, n + 5, start=n + 1)
            self.assertEqual(p.last_seq, n + 5)

    def test_transactions(self):
        root = os.path.join(self.tmp, "pyt")
        with datareel.Primary(root, settings=self.settings(), quiet=True) as p:
            p.open_database("ev")
            with p.transaction():
                a, b = p.new_id(), p.new_id()
                p.put("ev", a, b"first")
                p.put("ev", b, b"second")
            with self.assertRaises(ValueError):
                with p.transaction():
                    p.put("ev", a, b"changed")
                    raise ValueError("drop it")
            self.assertEqual(p.get("ev", a), b"first")          # rolled back
            self.assertEqual(p.last_seq, 1)
            p.begin()
            p.put("ev", a, b"changed")
            self.assertEqual(p.get("ev", a), b"changed")        # the open transaction's own change
            p.rollback()
            self.assertEqual(p.get("ev", a), b"first")
            with p.transaction():
                p.remove("ev", b)
            self.assertFalse(p.is_live("ev", b))
            self.assertIsNone(p.get("ev", b))
            self.assertEqual(p.ids("ev", tombstones=True), [b])
            with p.transaction():
                p.purge("ev", b)
            self.assertEqual(p.ids("ev", tombstones=True), [])
            with self.assertRaises(datareel.Error):
                p.put("ev", a, b"x")                            # outside a transaction
            with self.assertRaises(datareel.Error):
                with p.transaction():
                    p.put("ev", 0, b"x")                        # id 0
            self.assertEqual(p.last_seq, 3)
        with self.assertRaises(datareel.Error):
            p.get("ev", a)                                      # closed
        with datareel.Reader(root) as r:
            self.assertEqual(r.seq, 3)
            self.assertEqual(r.get("ev", a), b"first")
            self.assertEqual(r.ids("ev"), [a])

    def test_settings_and_refusals(self):
        with self.assertRaises(datareel.Error):
            datareel.Primary(os.path.join(self.tmp, "bad"), settings={"nosuchkey": 1}, quiet=True)
        with self.assertRaises(datareel.Error):
            datareel.Primary(os.path.join(self.tmp, "bad2"), settings={"index": "not a spec"}, quiet=True)
        conf = os.path.join(self.tmp, "objrepl.conf")
        with open(conf, "w") as f:
            f.write("".join("%s %s\n" % kv for kv in self.settings().items()))
        root = os.path.join(self.tmp, "pyconf")
        with datareel.Primary(root, config=conf, settings={"name": "pyprimary"}, quiet=True) as p:
            p.open_database("ev")
            with p.transaction():
                p.put("ev", p.new_id(), b"x")
            with self.assertRaises(datareel.Error):
                datareel.Primary(root, settings=self.settings(), quiet=True)   # one process (object) per node
        # a copy's folder is never written as a primary
        c = os.path.join(self.tmp, "acopy")
        port = free_port()
        log = open(os.path.join(self.tmp, "acopy.log"), "w")
        cp = subprocess.Popen([prog("objrepl"), "copy", c, "--listen", "127.0.0.1:%d" % port,
                               "--ca", os.path.join(self.pki, "ca.pem"), "--cert", os.path.join(self.pki, "copy.pem"),
                               "--key", os.path.join(self.pki, "copy.key"), "--allow-cn", "objrepl-primary"],
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                if os.path.exists(os.path.join(c, "state.gxd")):
                    break
                time.sleep(0.1)
        finally:
            cp.terminate()
            cp.wait(30)
            log.close()
        with self.assertRaises(datareel.Error) as e:
            datareel.Primary(c, settings=self.settings(), quiet=True)
        self.assertIn("copy", str(e.exception))

    def test_replicates_to_cpp_copy(self):
        port = free_port()
        root, c = os.path.join(self.tmp, "pyrep"), os.path.join(self.tmp, "pyrep_copy")
        os.makedirs(c)
        log = open(os.path.join(self.tmp, "pyrep_copy.log"), "w")
        cp = subprocess.Popen([prog("objrepl"), "copy", c, "--listen", "127.0.0.1:%d" % port,
                               "--ca", os.path.join(self.pki, "ca.pem"), "--cert", os.path.join(self.pki, "copy.pem"),
                               "--key", os.path.join(self.pki, "copy.key"), "--allow-cn", "objrepl-primary",
                               "--index", INDEX], stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                with open(os.path.join(self.tmp, "pyrep_copy.log")) as f:
                    if "listening" in f.read():
                        break
                time.sleep(0.1)
            with datareel.Primary(root, settings=self.settings(port), quiet=True) as p:
                p.open_database("big")
                p.open_database("vec")
                self.write_like_cpp(p, 150)
                self.assertEqual(p.drain(60), 0)
                self.assertEqual(p.copy_applied, 150)
                with p.transaction():
                    p.remove("vec", p.ids("vec")[0])
                self.assertEqual(p.drain(60), 0)
            with datareel.Reader(c, INDEX) as r:
                self.assertEqual(r.role, "copy")
                self.assertEqual(r.seq, 151)
                self.assertEqual(len(r.ids("vec")), 149)
                self.assertEqual(check_big(self, r.get("big", r.ids("big")[0])), 150)
                self.assertEqual(len(r.find_key("k0", struct.pack("<Q", 99))), 1)
        finally:
            cp.terminate()
            cp.wait(30)
            log.close()
        dig = lambda d: subprocess.run([prog("objrepl"), "digest", d], stdout=subprocess.PIPE, text=True).stdout
        self.assertEqual(dig(root), dig(c))                    # the C++ copy equals the Python primary
        self.assertEqual(datareel.digest(root)[0], datareel.digest(c)[0])
        self.assertEqual(datareel.digest(root)[0] + "\n", dig(root))   # the same digest as the C++ tool
        # a standalone test node made from the (stopped) copy: a file copy of the folder, then takeover, then
        # Python writes to it; the copy it came from is not changed
        st = datareel.status(c)
        self.assertEqual((st["role"], st["applied_seq"]), ("copy", 151))
        test = os.path.join(self.tmp, "pyrep_test")
        shutil.copytree(c, test)
        with self.assertRaises(datareel.Error):
            datareel.Primary(test, settings=self.settings(), quiet=True)   # still a copy
        msg = datareel.takeover(test)
        self.assertIn("epoch", msg)
        st = datareel.status(test)
        self.assertEqual((st["role"], st["epoch"]), ("primary", 2))
        before = datareel.digest(c)[0]
        with datareel.Primary(test, settings=self.settings(), quiet=True) as p:
            self.assertEqual(len(p.ids("vec")), 149)
            with p.transaction():
                p.put("vec", p.new_id(), struct.pack("<Q4f", 999, 999.0, 0, 0, 0))
            self.assertEqual(p.find_key("k0", struct.pack("<Q", 999)) != [], True)
        self.assertNotEqual(datareel.digest(test)[0], before)
        self.assertEqual(datareel.digest(c)[0], before)
        with self.assertRaises(datareel.Error):
            datareel.status(os.path.join(self.tmp, "nosuchnode"))


class ReplicationTest(Base):
    """7.5 beta 2: both nodes in Python (a Primary and a Copy), a failover and a rejoin, and the folder tools."""

    def node_settings(self, role, port):
        k = lambda f: os.path.join(self.pki, f)
        if role == "primary":
            return {"copy": "127.0.0.1:%d" % port, "ca": k("ca.pem"), "cert": k("primary.pem"),
                    "key": k("primary.key"), "index": INDEX}
        return {"listen": "127.0.0.1:%d" % port, "ca": k("ca.pem"), "cert": k("copy.pem"), "key": k("copy.key"),
                "allow-cn": "objrepl-primary", "index": INDEX}

    def start_copy(self, root, port):
        c = datareel.Copy(root, settings=self.node_settings("copy", port), quiet=True)
        t = threading.Thread(target=c.serve)
        t.start()
        time.sleep(0.3)
        return c, t

    def write(self, p, keys):
        with p.transaction():
            for k in keys:
                p.put("vec", p.new_id(), struct.pack("<Q4f", k, float(k), 0, 0, 0))

    def test_python_primary_and_copy_failover_rejoin(self):
        port = free_port()
        a, b = os.path.join(self.tmp, "rep_a"), os.path.join(self.tmp, "rep_b")
        c, t = self.start_copy(b, port)
        try:
            with datareel.Primary(a, settings=self.node_settings("primary", port), quiet=True) as p:
                p.open_database("vec")
                for i in range(20):
                    self.write(p, range(i * 10 + 1, i * 10 + 11))
                self.assertEqual(p.drain(60), 0)
                self.assertEqual(p.copy_applied, 20)
            self.assertEqual(c.applied_seq, 20)
            self.assertEqual(len(c.ids("vec")), 200)
            self.assertEqual(len(c.find_key("k0", struct.pack("<Q", 150))), 1)
            self.assertFalse(c.in_snapshot)
            with self.assertRaises(datareel.Error):
                datareel.Copy(b, settings=self.node_settings("copy", free_port()), quiet=True)   # held
        finally:
            c.stop()
            t.join(30)
            c.close()
        self.assertFalse(t.is_alive())
        self.assertEqual(datareel.digest(a)[0], datareel.digest(b)[0])
        ok, report = datareel.index_check(b, INDEX)
        self.assertTrue(ok, report)
        level, line = datareel.check(b, settings={"max-behind": "100000"})
        self.assertIn(level, (0, 1), line)
        # failover: B takes over, A rejoins as B's copy and gets a full send
        datareel.takeover(b)
        msg = datareel.rejoin(a)
        self.assertIn("full send", msg)
        port2 = free_port()
        ca, ta = self.start_copy(a, port2)
        try:
            with datareel.Primary(b, settings=self.node_settings("primary", port2), quiet=True) as p:
                self.assertEqual(p.epoch, 2)
                self.write(p, [9001, 9002])
                self.assertEqual(p.drain(60), 0)
        finally:
            ca.stop()
            ta.join(30)
            ca.close()
        self.assertEqual(datareel.status(a)["role"], "copy")
        self.assertEqual(datareel.status(b)["role"], "primary")
        self.assertEqual(datareel.digest(a)[0], datareel.digest(b)[0])
        with datareel.Reader(a, INDEX) as r:
            self.assertEqual(len(r.ids("vec")), 202)
        self.assertEqual(datareel.clear_alarm(a) is not None, True)

    def test_links(self):
        root = os.path.join(self.tmp, "links")
        st = dict(self.node_settings("primary", 1))
        st["links"] = "orders:0:items"
        st["index"] = ""
        with datareel.Primary(root, settings=st, quiet=True) as p:
            p.open_database("items")
            p.open_database("orders")
            with p.transaction():
                item = p.new_id()
                p.put("items", item, b"widget" + bytes(10))
            with p.transaction():
                order = p.new_id()
                p.put("orders", order, struct.pack("<Q", p.address_of("items", item)) + b"order-1")
            a = struct.unpack_from("<Q", p.get("orders", order))[0]
            self.assertNotEqual(a, 0)
            self.assertEqual(p.id_at("items", a), item)
            self.assertEqual(p.id_at("items", 12345), 0)


class DatabaseLayerTest(unittest.TestCase):
    """7.5 beta 2: gxDatabase, gxBtree with byte keys, vector index writes and gxdJournal from Python."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="dr_pydb_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def path(self, name):
        return os.path.join(self.tmp, name)

    def test_database(self):
        with datareel.Database(self.path("a.gxd"), create=True) as db:
            a = db.alloc(100)
            db.write(a, b"hello")
            db.write(a, b"world", 5)
            self.assertEqual(db.read(a, 10), b"helloworld")
            self.assertGreaterEqual(db.length(a), 100)
            b = db.alloc(50)
            self.assertEqual(db.addresses(), [a, b])
            db.delete(b)
            self.assertEqual(db.addresses(), [a])
            c = db.alloc(40)
            self.assertEqual(db.addresses(), [a, c])
            with self.assertRaises(datareel.Error):
                db.write(a, b"x" * 1000)                        # past the object
        with datareel.Database(self.path("a.gxd"), readonly=True) as db:
            self.assertEqual(db.read(db.addresses()[0], 10), b"helloworld")
            with self.assertRaises(datareel.Error):
                db.write(db.addresses()[0], b"no")
        # BlockFile (beta 1) reads the same file
        with datareel.BlockFile(self.path("a.gxd")) as f:
            self.assertEqual(f.read(f.addresses()[0])[:10], b"helloworld")

    def test_btree(self):
        with datareel.BTree(self.path("k.btx"), 8, create=True) as t:
            for i in range(1, 501):
                self.assertTrue(t.insert(struct.pack(">Q", i), i * 10))
            self.assertFalse(t.insert(struct.pack(">Q", 5), 50))  # the same key and value
            self.assertTrue(t.insert(struct.pack(">Q", 5), 51))   # the same key, another value
            self.assertEqual(t.count(), 501)
            self.assertEqual(t.find(struct.pack(">Q", 5)), [50, 51])
            self.assertEqual(t.find(struct.pack(">Q", 999)), [])
            self.assertTrue(t.contains(struct.pack(">Q", 7), 70))
            r = t.range(struct.pack(">Q", 10), struct.pack(">Q", 13))
            self.assertEqual([v for _, v in r], [100, 110, 120])
            self.assertEqual(len(t.range(limit=4)), 4)
            self.assertTrue(t.delete(struct.pack(">Q", 5), 51))
            self.assertFalse(t.delete(struct.pack(">Q", 5), 51))
        with datareel.BTree(self.path("k.btx"), 8, readonly=True) as t:
            self.assertEqual(t.count(), 500)
            self.assertEqual(t.key_size, 8)
        with datareel.BTree(self.path("s.btx"), 16, create=True) as t:
            for name in [b"apple", b"apricot", b"banana", b"avocado"]:
                t.insert(name, len(name))
            self.assertEqual([k.rstrip(b"\0") for k, _ in t.prefix(b"ap")], [b"apple", b"apricot"])
            with self.assertRaises(datareel.Error):
                t.insert(b"x" * 17)

    def test_btree_reads_a_node_index(self):
        # a gxObjRepl B-tree index file is 32 key bytes + the object id: BTree(path, 32) reads it
        pki = os.path.join(self.tmp, "pki")
        subprocess.check_call([CERTS, pki, "--copy-host", "127.0.0.1", "--days", "30", "--apply"], stdout=subprocess.DEVNULL)
        node = self.path("node")
        subprocess.check_call([prog("test_gxobjrepl_reader"), "write", node, pki, "50"], stdout=subprocess.DEVNULL)
        with datareel.Reader(node, INDEX) as r:
            want = r.find_key("k0", struct.pack("<Q", 20))
        with datareel.BTree(os.path.join(node, "vec.k0.ix.btx"), 32, readonly=True) as t:
            self.assertEqual(t.count(), 50)
            self.assertEqual(t.find(struct.pack("<Q", 20)), want)

    def test_vector_index_writes(self):
        p = self.path("v.vix")
        datareel.create_vector_index(p, 3, metric="l2", spec="test/v1")
        with datareel.VectorIndex(p, "test/v1", write=True) as v:
            for i in range(1, 201):
                v.insert(i, [float(i), 0, 0], tags=[i % 4, 0, 0, i])
            self.assertEqual(v.count, 200)
            self.assertEqual(v.search([10.2, 0, 0], k=1)[0][0], 10)
            self.assertEqual(v.search([10.2, 0, 0], k=3, tags={0: 3})[0][0], 11)
            v.update(10, [500.0, 0, 0])
            self.assertEqual(v.search([500, 0, 0], k=1)[0][0], 10)
            v.delete(11)
            self.assertNotIn(11, [h[0] for h in v.search([11, 0, 0], k=5)])
            with self.assertRaises(datareel.Error):
                v.insert(12, [1, 2, 3])                         # a duplicate id
            with self.assertRaises(datareel.Error):
                v.insert(999, [1, 2])                           # the wrong dimension
            v.repair()
            self.assertEqual(v.verify()[0], 0)
            v.flush()
        with datareel.VectorIndex(p, "test/v1") as v:
            self.assertEqual(v.count, 199)
            with self.assertRaises(datareel.Error):
                v.insert(1000, [1, 2, 3])                       # read only
        # merge two day files
        q = self.path("w.vix")
        datareel.create_vector_index(q, 3, spec="test/v1")
        with datareel.VectorIndex(q, "test/v1", write=True) as v:
            for i in range(1001, 1051):
                v.insert(i, [float(i), 1, 0])
        out = self.path("merged.vix")
        datareel.merge_vector_indexes(out, [p, q], spec="test/v1")
        with datareel.VectorIndex(out, "test/v1") as v:
            self.assertEqual(v.count, 249)
            self.assertEqual(v.search([1020, 1, 0], k=1)[0][0], 1020)

    def test_journal(self):
        j = datareel.Journal(self.path("j.jnl"))
        db = datareel.Database(self.path("j.gxd"), create=True)
        t = datareel.BTree(self.path("j.btx"), 8, create=True)
        datareel.create_vector_index(self.path("j.vix"), 2)
        v = datareel.VectorIndex(self.path("j.vix"), write=True)
        for f in (db, t, v):
            j.attach(f)
        with datareel.transaction(j):
            a = db.alloc(16)
            db.write(a, b"committed")
            t.insert(struct.pack(">Q", 1), a)
            v.insert(1, [1.0, 1.0])
        with self.assertRaises(KeyError):
            with datareel.transaction(j):
                db.write(a, b"rolledbak")
                t.insert(struct.pack(">Q", 2), a)
                v.insert(2, [2.0, 2.0])
                raise KeyError("undo")
        self.assertEqual(db.read(a, 9), b"committed")
        self.assertEqual(t.count(), 1)
        self.assertEqual(v.count, 1)                           # the index was read again after the rollback
        with self.assertRaises(datareel.Error):
            db.close()                                          # attached: close the journal first
        j.begin()
        with self.assertRaises(datareel.Error):
            j.attach(t)                                         # attach inside a transaction
        j.rollback()
        j.close()
        for f in (v, t, db):
            f.close()
        with datareel.Database(self.path("j.gxd"), readonly=True) as d2:
            self.assertEqual(d2.read(a, 9), b"committed")

    def test_journal_crash(self):
        """A Python writer killed (KILL) mid-transaction: the next Journal() replays what was committed, and the
        data file and the B-tree agree."""
        d = self.path("crash")
        os.makedirs(d)
        code = ("import datareel, struct\n"
                "j = datareel.Journal(%r)\n"
                "db = datareel.Database(%r, create=True)\n"
                "t = datareel.BTree(%r, 8, create=True)\n"
                "j.attach(db); j.attach(t)\n"
                "for k in range(1, 1000000):\n"
                "    j.begin()\n"
                "    for i in range(10):\n"
                "        n = k * 10 + i\n"
                "        a = db.alloc(64); db.write(a, struct.pack('>Q', n) * 8); t.insert(struct.pack('>Q', n), a)\n"
                "    j.commit()\n"
                "    if k == 50: print('50', flush=True)\n") % (os.path.join(d, "c.jnl"), os.path.join(d, "c.gxd"),
                                                               os.path.join(d, "c.btx"))
        w = subprocess.Popen([sys.executable, "-I", "-c", code], stdout=subprocess.PIPE, text=True)
        self.assertEqual(w.stdout.readline().strip(), "50")
        time.sleep(0.3)
        w.kill()
        w.wait()
        w.stdout.close()
        j = datareel.Journal(os.path.join(d, "c.jnl"))         # replays the committed transactions
        db = datareel.Database(os.path.join(d, "c.gxd"))
        t = datareel.BTree(os.path.join(d, "c.btx"), 8)
        j.attach(db)
        j.attach(t)
        addrs = db.addresses()
        entries = t.range()
        self.assertEqual(len(addrs) % 10, 0)                    # whole transactions only
        self.assertEqual(len(entries), len(addrs))
        self.assertGreaterEqual(len(addrs), 500)
        for k, a in entries:
            self.assertEqual(db.read(a, 8), k)                  # each key names its own record
        j.close()
        t.close()
        db.close()


class StreamTest(Base):
    """7.5 beta 2: gxStream over TLS between Python and Python, from a C++ program, and the C++ object server."""

    def k(self, f):
        return os.path.join(self.pki, f)

    def test_python_server_and_client(self):
        port = free_port()
        got = {}
        with datareel.StreamServer(port, self.k("copy.pem"), self.k("copy.key"), self.k("ca.pem"),
                                   allow_cn="objrepl-primary") as srv:
            def serve():
                s = srv.accept(timeout=20)
                got["peer"] = s.peer
                b = s.read_block()
                s.write_block(b[::-1])
                s.write_block(b"x" * 300000)
                s.close()
                got["refused"] = None
                try:
                    srv.accept(timeout=20)
                except datareel.Error as e:
                    got["refused"] = str(e)
            t = threading.Thread(target=serve)
            t.start()
            with datareel.Stream("127.0.0.1", port, ca=self.k("ca.pem"), cert=self.k("primary.pem"),
                                 key=self.k("primary.key")) as c:
                c.write_block(b"hello gxstream")
                self.assertEqual(c.read_block(), b"maertsxg olleh")
                self.assertEqual(len(c.read_block()), 300000)
            with self.assertRaises(datareel.Error):
                # the copy's certificate is not in the allow list: the server closes it
                c2 = datareel.Stream("127.0.0.1", port, ca=self.k("ca.pem"), cert=self.k("copy.pem"),
                                     key=self.k("copy.key"))
                c2.write_block(b"x")
                c2.read_block()
            t.join(30)
        self.assertEqual(got["peer"][0], "objrepl-primary")
        self.assertIsNotNone(got["refused"])
        with self.assertRaises(datareel.Error):
            datareel.Stream("127.0.0.1", free_port(), ca=self.k("ca.pem"), timeout=3)     # nothing listening

    def test_cpp_primary_to_python_server(self):
        # a C++ objrepl primary connects to a Python gxStream server: its first block (the hello) arrives whole
        port = free_port()
        got = {}
        with datareel.StreamServer(port, self.k("copy.pem"), self.k("copy.key"), self.k("ca.pem"),
                                   allow_cn="objrepl-primary") as srv:
            def serve():
                s = srv.accept(timeout=30)
                got["peer"] = s.peer[0]
                got["hello"] = s.read_block()
                s.close()
            t = threading.Thread(target=serve)
            t.start()
            pp = subprocess.Popen([prog("objrepl"), "primary", os.path.join(self.tmp, "cpp_p"), "--copy",
                                   "127.0.0.1:%d" % port, "--ca", self.k("ca.pem"), "--cert", self.k("primary.pem"),
                                   "--key", self.k("primary.key"), "--commits", "1", "--name", "cppP"],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            t.join(60)
            pp.terminate()                                      # it would wait for an answer to its hello
            pp.wait(30)
        self.assertEqual(got["peer"], "objrepl-primary")
        self.assertIn(b"cppP", got["hello"])                    # the primary's name travels in its hello

    def test_object_client(self):
        port = free_port()
        d = os.path.join(self.tmp, "objsrv")
        os.makedirs(d)
        vix = os.path.join(d, "notes.vix")
        datareel.create_vector_index(vix, 2, spec="notes/v1")
        with datareel.VectorIndex(vix, write=True) as v:
            for i in range(1, 51):
                v.insert(i, [float(i), 0.0], tags=[i % 2, 0, 0, 0])
        log = open(os.path.join(d, "server.log"), "w")
        srv = subprocess.Popen([os.path.join(BIN, "object_server"), str(port), self.k("copy.pem"), self.k("copy.key"),
                                self.k("ca.pem"), "objrepl-primary", "notes", os.path.join(d, "notes.gxd"),
                                "--stop-file", os.path.join(d, "stop.now"), "--vectors", vix],
                               stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                try:
                    socket.create_connection(("127.0.0.1", port), 0.2).close()
                    break
                except OSError:
                    time.sleep(0.1)
            with datareel.ObjectClient("127.0.0.1", port, "notes", self.k("ca.pem"), self.k("primary.pem"),
                                       self.k("primary.key"), write=True) as oc:
                a = oc.add(b"first note")
                b = oc.add(b"second note")
                self.assertEqual(oc.request(a), b"first note")
                oc.change(a, b"FIRST NOTE")
                self.assertEqual(oc.request(a), b"FIRST NOTE")
                with self.assertRaises(datareel.Error):
                    oc.change(a, b"a different length")
                self.assertEqual(oc.addresses(), [a, b])
                oc.delete(b)
                self.assertEqual(oc.addresses(), [a])
                with self.assertRaises(datareel.Error):
                    oc.request(b)
                info = oc.vector_info()
                self.assertEqual((info["dim"], info["nodes"], info["spec"]), (2, 50, "notes/v1"))
                hits = oc.vector_search([10.2, 0.0], k=3)
                self.assertEqual([h[0] for h in hits], [10, 11, 9])
                self.assertEqual(oc.vector_search([10.2, 0.0], k=1, tags={0: 1})[0][0], 11)
            with self.assertRaises(datareel.Error):
                datareel.ObjectClient("127.0.0.1", port, "notes", self.k("ca.pem"), self.k("copy.pem"),
                                      self.k("copy.key"))        # not the allowed client
        finally:
            open(os.path.join(d, "stop.now"), "w").close()
            try:
                srv.wait(30)
            except subprocess.TimeoutExpired:
                srv.kill()
            log.close()


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

PY_WRITER = r"""
import struct, sys, datareel
root, pki, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
st = {"copy": "127.0.0.1:1", "ca": pki + "/ca.pem", "cert": pki + "/primary.pem", "key": pki + "/primary.key",
      "index": "k0:vec:B:0:8,v0:vec:V:8:4"}
with datareel.Node(root, st, role="primary", quiet=True) as p:
    p.open_database("big")
    p.open_database("vec")
    big = p.ids("big")
    bid = big[0] if big else 0
    for k in range(len(p.ids("vec")) + 1, n + 1):
        with p.transaction():
            if not bid:
                bid = p.new_id()
            p.put("big", bid, struct.pack("<Q", k) + bytes(((k * 7 + i) & 0xFF) for i in range(8, 12000)))
            p.put("vec", p.new_id(), struct.pack("<Q4f", k, float(k), 0, 0, 0))
print("write: done %d" % n)
"""

PY_COPY = r"""
import signal, sys, threading, datareel
root, pki, port = sys.argv[1], sys.argv[2], sys.argv[3]
st = {"listen": "127.0.0.1:" + port, "ca": pki + "/ca.pem", "cert": pki + "/copy.pem", "key": pki + "/copy.key",
      "allow-cn": "objrepl-primary", "index": "k0:vec:B:0:8,v0:vec:V:8:4"}
c = datareel.Node(root, st, role="copy", quiet=True)
signal.signal(signal.SIGTERM, lambda *a: c.stop())
t = threading.Thread(target=c.serve)
t.start()
print("listening", flush=True)
while t.is_alive():
    t.join(0.2)           # back in Python often, so SIGTERM's handler runs
c.close()
"""


def applied(root):
    """the copy's applied_seq from its objrepl.status file (the copy is running)"""
    try:
        with open(os.path.join(root, "objrepl.status")) as f:
            for line in f:
                w = line.split()
                if len(w) == 2 and w[0] == "applied_seq":
                    return int(w[1])
    except (OSError, ValueError):
        pass
    return 0


class HandoffTest(Base):
    """7.5 beta 2 (the OSA handoff): paging and range reads, the node's own index spec, lock-wait, Node()/detach(),
    a C++ primary to a Python copy (full send, incremental, a killed copy), C++ and Python writers taking turns on one
    folder, and a Reader while a Python writer commits."""

    def settings(self, extra=None):
        st = {"copy": "127.0.0.1:1", "ca": os.path.join(self.pki, "ca.pem"),
              "cert": os.path.join(self.pki, "primary.pem"), "key": os.path.join(self.pki, "primary.key"),
              "index": INDEX}
        st.update(extra or {})
        return st

    def test_paging_range_and_spec(self):
        with datareel.Reader(self.p) as r:            # no spec: read from the node's .def files
            self.assertEqual(sorted(r.index_spec.split(",")), sorted(INDEX.split(",")))
            all_ids = r.ids("vec")
            self.assertEqual(len(all_ids), 300)
            pages, after = [], 0
            while True:
                page = r.ids("vec", after=after, limit=37)
                if not page:
                    break
                self.assertLessEqual(len(page), 37)
                pages += page
                after = page[-1]
            self.assertEqual(pages, all_ids)
            self.assertEqual(r.ids("vec", after=all_ids[-1]), [])
            self.assertEqual(r.ids("vec", limit=5), all_ids[:5])
            self.assertEqual(r.ids("vec", tombstones=True, limit=10), [])
            # the keys are compared as bytes (these 8-byte keys are little-endian, so by their low byte first)
            self.assertEqual(len(r.find_range("k0", b"")), 300)
            self.assertEqual(len(r.find_range("k0", b"", limit=7)), 7)
            got = r.find_range("k0", b"\x10", b"\x20")             # low byte 16..31: keys 16..31 and 272..287
            keys = sorted(struct.unpack_from("<Q", r.get("vec", i))[0] for i in got)
            self.assertEqual(keys, sorted(list(range(16, 32)) + list(range(272, 288))))
            self.assertEqual(r.find_range("k0", b"\x10", b"\x10"), [])
            with self.assertRaises(datareel.Error):
                r.find_range("v0", b"")                            # not a B-tree index
            with self.assertRaises(datareel.Error):
                r.find_range("k0", bytes(9))                       # longer than the key
            self.assertEqual(datareel.Reader(self.p, INDEX).index_spec, INDEX)

    def test_lock_wait_node_detach(self):
        root = os.path.join(self.tmp, "lockw")
        with datareel.Node(root, self.settings(), quiet=True) as p:
            self.assertEqual((p.role, p.seq), ("primary", 0))
            p.open_database("vec")
            with p.transaction():
                for k in range(1, 51):
                    p.put("vec", p.new_id(), struct.pack("<Q4f", k, float(k), 0, 0, 0))
            self.assertEqual(p.seq, 1)
            self.assertEqual(p.ids("vec", limit=3), p.ids("vec")[:3])
            self.assertEqual(p.ids("vec", after=p.ids("vec")[47]), p.ids("vec")[48:])
            t0 = time.time()
            with self.assertRaises(datareel.Error):
                datareel.Primary(root, settings=self.settings(), quiet=True)      # lock-wait 0: at once
            self.assertLess(time.time() - t0, 1.0)
            t0 = time.time()
            with self.assertRaises(datareel.Error):
                datareel.Primary(root, settings=self.settings({"lock-wait": "2"}), quiet=True)
            self.assertGreaterEqual(time.time() - t0, 1.8)
            with self.assertRaises(datareel.Error):
                datareel.Node(root, self.settings(), role="witness")
        # a Python copy of it, stopped, then a file copy of that detached into an independent writable node
        port = free_port()
        c = os.path.join(self.tmp, "lockw_c")
        k = lambda f: os.path.join(self.pki, f)
        cc = datareel.Node(c, {"listen": "127.0.0.1:%d" % port, "ca": k("ca.pem"), "cert": k("copy.pem"),
                               "key": k("copy.key"), "allow-cn": "objrepl-primary", "index": INDEX}, role="copy",
                           quiet=True)
        t = threading.Thread(target=cc.serve)
        t.start()
        time.sleep(0.3)
        try:
            with datareel.Node(root, self.settings({"copy": "127.0.0.1:%d" % port}), quiet=True) as p:
                self.assertEqual(p.drain(60), 0)
            self.assertEqual((cc.role, cc.seq), ("copy", 1))
        finally:
            cc.stop()
            t.join(30)
            cc.close()
        test = os.path.join(self.tmp, "lockw_test")
        shutil.copytree(c, test)
        self.assertEqual(datareel.status(test)["role"], "copy")
        self.assertIn("epoch", datareel.detach(test))
        self.assertEqual(datareel.status(test)["role"], "primary")
        with datareel.Node(test, self.settings(), quiet=True) as p:
            with p.transaction():
                p.put("vec", p.new_id(), struct.pack("<Q4f", 77, 77.0, 0, 0, 0))
            self.assertEqual(len(p.ids("vec")), 51)
        self.assertEqual(datareel.digest(c)[0], datareel.digest(root)[0])     # the copy it came from is unchanged

    def start_py_copy(self, root, port, log):
        with open(log.name) as f:
            seen = f.read().count("listening")        # the log is shared by every start: wait for a new line
        cp = subprocess.Popen([sys.executable, "-I", "-c", PY_COPY, root, self.pki, str(port)],
                              stdout=log, stderr=subprocess.STDOUT)
        for _ in range(300):
            with open(log.name) as f:
                if f.read().count("listening") > seen:
                    break
            time.sleep(0.1)
        time.sleep(0.3)
        return cp

    def test_cpp_primary_to_python_copy(self):
        port = free_port()
        p, c = os.path.join(self.tmp, "c2p"), os.path.join(self.tmp, "c2p_copy")
        log = open(os.path.join(self.tmp, "c2p_copy.log"), "a")
        cp = self.start_py_copy(c, port, log)
        try:
            # 1. a full send to a new copy: 100 commits made before the copy was there, then 100 more
            out = subprocess.run([prog("test_gxobjrepl_reader"), "write", p, self.pki, "100"],
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(out.returncode, 0, out.stdout)
            w = write(p, self.pki, 200, port)
            self.assertEqual(w.wait(120), 0, w.communicate()[0])
            cp.terminate()
            self.assertEqual(cp.wait(30), 0)
            with datareel.Reader(c) as r:
                self.assertEqual((r.role, r.seq), ("copy", 200))
            with datareel.Node(c, {"listen": "127.0.0.1:%d" % free_port(), "ca": os.path.join(self.pki, "ca.pem"),
                                   "cert": os.path.join(self.pki, "copy.pem"), "key": os.path.join(self.pki, "copy.key"),
                                   "allow-cn": "objrepl-primary", "index": INDEX}, role="copy", quiet=True) as cc:
                self.assertEqual((cc.role, cc.seq), ("copy", 200))
            self.assertEqual(datareel.digest(p)[0], datareel.digest(c)[0])
            # 2. incremental: the copy restarts and gets only the new commits
            cp = self.start_py_copy(c, port, log)
            w = write(p, self.pki, 300, port)
            self.assertEqual(w.wait(120), 0, w.communicate()[0])
            self.assertEqual(datareel.digest(p)[0], datareel.digest(c)[0])
            # 3. the copy is killed while it applies, restarted, and catches up
            w = write(p, self.pki, 900, port)
            for _ in range(300):
                if applied(c) >= 400:
                    break
                time.sleep(0.05)
            cp.kill()
            cp.wait(30)
            cp = self.start_py_copy(c, port, log)
            self.assertEqual(w.wait(300), 0, w.communicate()[0])
            cp.terminate()
            self.assertEqual(cp.wait(30), 0)
        finally:
            if cp.poll() is None:
                cp.kill()
                cp.wait(30)
            log.close()
        with open(log.name) as f:
            copylog = f.read()
        self.assertEqual(datareel.digest(p)[0], datareel.digest(c)[0], copylog[-2000:])
        ok, report = datareel.index_check(c, INDEX)
        self.assertTrue(ok, report)
        fin = subprocess.run([prog("test_gxobjrepl_reader"), "final", c, "900"], stdout=subprocess.PIPE, text=True)
        self.assertEqual(fin.returncode, 0, fin.stdout)

    def test_cpp_and_python_writers_take_turns(self):
        root = os.path.join(self.tmp, "turns")
        for n, who in ((50, "cpp"), (100, "py"), (150, "cpp"), (200, "py")):
            if who == "cpp":
                out = subprocess.run([prog("test_gxobjrepl_reader"), "write", root, self.pki, str(n)],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            else:
                out = subprocess.run([sys.executable, "-I", "-c", PY_WRITER, root, self.pki, str(n)],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(out.returncode, 0, out.stdout)
            fin = subprocess.run([prog("test_gxobjrepl_reader"), "final", root, str(n)], stdout=subprocess.PIPE,
                                 text=True)
            self.assertEqual(fin.returncode, 0, "%s to %d: %s" % (who, n, fin.stdout))
            ok, report = datareel.index_check(root, INDEX)
            self.assertTrue(ok, report)
        dig = subprocess.run([prog("objrepl"), "digest", root], stdout=subprocess.PIPE, text=True).stdout
        self.assertEqual(datareel.digest(root)[0] + "\n", dig)

    def test_reader_while_python_writes(self):
        root = os.path.join(self.tmp, "pylive")
        w = subprocess.Popen([sys.executable, "-I", "-c", PY_WRITER, root, self.pki, "1500"],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        s = LiveTest.read_while(self, root, w)
        out = w.communicate()[0]
        self.assertEqual(w.returncode, 0, out)
        self.assertEqual(s["bad"], [])
        self.assertGreater(s["reads"], 10)
        self.assertGreater(s["keys"], 10)
        fin = subprocess.run([prog("test_gxobjrepl_reader"), "final", root, "1500"], stdout=subprocess.PIPE, text=True)
        self.assertEqual(fin.returncode, 0, fin.stdout)


class RejoinTest(Base):
    """7.5 beta 3: the incremental rejoin between C++ and Python nodes (the emergency path and back), the schema
    refusal, and the rebuild and swap."""
    IX = "k0:db0:B:0:8,k1:db1:B:4:6"

    def k(self, f):
        return os.path.join(self.pki, f)

    def py_copy(self, root, port, schema=None):
        st = {"listen": "127.0.0.1:%d" % port, "ca": self.k("ca.pem"), "cert": self.k("copy.pem"),
              "key": self.k("copy.key"), "allow-cn": "objrepl-primary", "index": self.IX}
        if schema:
            st["schema"] = schema
        c = datareel.Copy(root, settings=st, quiet=True)
        t = threading.Thread(target=c.serve)
        t.start()
        time.sleep(0.3)
        return c, t

    def py_primary(self, root, port, schema=None):
        st = {"copy": "127.0.0.1:%d" % port, "ca": self.k("ca.pem"), "cert": self.k("primary.pem"),
              "key": self.k("primary.key"), "index": self.IX}
        if schema:
            st["schema"] = schema
        return datareel.Primary(root, settings=st, quiet=True)

    def cpp_primary(self, root, port, commits, seed, drain=60, extra=()):
        args = [prog("objrepl"), "primary", root, "--copy", "127.0.0.1:%d" % port, "--ca", self.k("ca.pem"),
                "--cert", self.k("primary.pem"), "--key", self.k("primary.key"), "--index", self.IX, "--dbs", "2",
                "--commits", str(commits), "--per", "4", "--seed", str(seed), "--drain-secs", str(drain)] + list(extra)
        return subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    def cpp_copy(self, root, port, log):
        os.makedirs(root, exist_ok=True)
        with open(log.name) as f:
            seen = f.read().count("listening")
        cp = subprocess.Popen([prog("objrepl"), "copy", root, "--listen", "127.0.0.1:%d" % port, "--ca", self.k("ca.pem"),
                               "--cert", self.k("copy.pem"), "--key", self.k("copy.key"), "--allow-cn", "objrepl-primary",
                               "--index", self.IX], stdout=log, stderr=subprocess.STDOUT)
        for _ in range(100):
            with open(log.name) as f:
                if f.read().count("listening") > seen:
                    break
            time.sleep(0.1)
        return cp

    def py_writes(self, root, n, seed):
        """N commits from Python on primary ROOT (nothing follows it): updates of existing objects, adds, a remove"""
        import random
        rnd = random.Random(seed)
        with self.py_primary(root, 1) as p:
            for db in ("db0", "db1"):
                p.open_database(db)
            for _ in range(n):
                with p.transaction():
                    for db in ("db0", "db1"):
                        ids = p.ids(db)
                        if ids and rnd.random() < 0.6:
                            p.put(db, rnd.choice(ids), bytes(rnd.randrange(256) for _ in range(60)))
                        else:
                            p.put(db, p.new_id(), bytes(rnd.randrange(256) for _ in range(60)))
                    ids = p.ids("db0")
                    if ids and rnd.random() < 0.3:
                        p.remove("db0", rnd.choice(ids))

    def status_file(self, root):
        out = {}
        with open(os.path.join(root, "objrepl.status")) as f:
            for line in f:
                k, _, v = line.rstrip("\n").partition(" ")
                out[k] = v
        return out

    def objects(self, root):
        return int(datareel.digest(root)[0].split()[1])

    def test_emergency_cpp_primary_python_copy(self):
        """C++ production primary P, Python DEV copy D; D detaches and writes; P writes on; P rejoins under D
        (incremental), then the roles go back (D rejoins under P with no full send)"""
        P, D = os.path.join(self.tmp, "em_p"), os.path.join(self.tmp, "em_d")
        port = free_port()
        c, t = self.py_copy(D, port)
        try:
            out = self.cpp_primary(P, port, 300, 11)
            self.assertEqual(out.returncode, 0, out.stdout)
        finally:
            c.stop(); t.join(30); c.close()
        self.assertEqual(datareel.digest(P)[0], datareel.digest(D)[0])
        self.assertIn("epoch", datareel.detach(D))
        self.py_writes(D, 10, 12)                                   # DEV's emergency changes
        out = self.cpp_primary(P, free_port(), 15, 13, drain=0)     # production kept writing (nobody follows)
        self.assertNotEqual(out.returncode, 64, out.stdout)
        msg = subprocess.run([prog("objrepl"), "rejoin", P], stdout=subprocess.PIPE, text=True).stdout
        self.assertIn("incremental rejoin", msg)
        port2 = free_port()
        log = open(os.path.join(self.tmp, "em_p.copy.log"), "a")
        cp = self.cpp_copy(P, port2, log)
        try:
            with self.py_primary(D, port2) as p:
                self.assertEqual(p.drain(60), 0)
            st = self.status_file(D)
            self.assertEqual(st.get("last_rejoin"), "incremental", st)
            self.assertLess(int(st["last_rejoin_objects"]), self.objects(D))
            self.assertGreater(int(st["last_rejoin_objects"]), 0)
        finally:
            cp.terminate(); cp.wait(30); log.close()
        self.assertEqual(datareel.digest(P)[0], datareel.digest(D)[0])
        ok, report = datareel.index_check(P, self.IX)
        self.assertTrue(ok, report)
        # back: P takes over (it has every commit D made), D rejoins with no full send
        datareel.takeover(P)
        self.assertIn("no full send", datareel.rejoin(D))
        port3 = free_port()
        c, t = self.py_copy(D, port3)
        try:
            out = self.cpp_primary(P, port3, 5, 14)
            self.assertEqual(out.returncode, 0, out.stdout)
        finally:
            c.stop(); t.join(30); c.close()
        self.assertEqual(datareel.digest(P)[0], datareel.digest(D)[0])
        self.assertEqual((datareel.status(P)["role"], datareel.status(D)["role"]), ("primary", "copy"))

    def test_python_primary_cpp_copy_rejoin(self):
        """the other way: Python primary P, C++ copy D; D takes over and writes (C++); P writes on (Python);
        datareel.rejoin(P) is incremental; P as a Python copy takes it from the C++ primary"""
        P, D = os.path.join(self.tmp, "rv_p"), os.path.join(self.tmp, "rv_d")
        port = free_port()
        log = open(os.path.join(self.tmp, "rv_d.copy.log"), "a")
        cp = self.cpp_copy(D, port, log)
        try:
            with self.py_primary(P, port) as p:
                for db in ("db0", "db1"):
                    p.open_database(db)
                for i in range(200):
                    with p.transaction():
                        p.put("db0", p.new_id(), struct.pack("<Q", i) + bytes(40))
                        p.put("db1", p.new_id(), bytes(4) + struct.pack("<6s", b"k%05d" % i) + bytes(30))
                self.assertEqual(p.drain(60), 0)
        finally:
            cp.terminate(); cp.wait(30); log.close()
        subprocess.run([prog("objrepl"), "takeover", D], stdout=subprocess.DEVNULL, check=True)
        out = self.cpp_primary(D, free_port(), 8, 21, drain=0)
        self.assertNotEqual(out.returncode, 64, out.stdout)
        self.py_writes(P, 6, 22)
        self.assertIn("incremental rejoin", datareel.rejoin(P))
        self.assertEqual(datareel.status(P)["in_snapshot"], 2)
        port2 = free_port()
        c, t = self.py_copy(P, port2)
        try:
            out = self.cpp_primary(D, port2, 1, 23)
            self.assertEqual(out.returncode, 0, out.stdout)
        finally:
            c.stop(); t.join(30); c.close()
        self.assertEqual(self.status_file(D).get("last_rejoin"), "incremental")
        self.assertEqual(datareel.digest(P)[0], datareel.digest(D)[0])
        ok, report = datareel.index_check(P, self.IX)
        self.assertTrue(ok, report)

    def test_schema_refusal_rebuild_swap(self):
        """a different schema setting refuses the incremental rejoin (P untouched); a rebuild into an empty folder and
        datareel.swap() put the new node in place and keep the old one"""
        P, D = os.path.join(self.tmp, "sc_p"), os.path.join(self.tmp, "sc_d")
        port = free_port()
        c, t = self.py_copy(D, port, schema="layout-1")
        try:
            out = self.cpp_primary(P, port, 200, 31, extra=["--schema", "layout-1"])
            self.assertEqual(out.returncode, 0, out.stdout)
        finally:
            c.stop(); t.join(30); c.close()
        datareel.detach(D)
        out = self.cpp_primary(P, free_port(), 5, 32, drain=0, extra=["--schema", "layout-1"])
        datareel.rejoin(P)
        before = datareel.digest(P)[0]
        port2 = free_port()
        log = open(os.path.join(self.tmp, "sc_p.copy.log"), "a")
        cp = self.cpp_copy(P, port2, log)                           # P has no schema setting: layout-1 vs none
        try:
            with self.py_primary(D, port2, schema="layout-2") as p:
                self.assertEqual(p.drain(10), 3)                    # refused
        finally:
            cp.terminate(); cp.wait(30); log.close()
        with open(log.name) as f:
            self.assertIn("schema differs", f.read())
        self.assertEqual(datareel.digest(P)[0], before)
        self.assertEqual(datareel.status(P)["in_snapshot"], 2)
        new = os.path.join(self.tmp, "sc_p_new")
        port3 = free_port()
        c, t = self.py_copy(new, port3, schema="layout-2")
        try:
            with self.py_primary(D, port3, schema="layout-2") as p:
                self.assertEqual(p.drain(60), 0)
        finally:
            c.stop(); t.join(30); c.close()
        msg = datareel.swap(P, new, self.IX)
        self.assertIn("kept at", msg)
        kept = msg.rsplit("kept at ", 1)[1].strip()
        self.assertEqual(datareel.digest(kept)[0], before)
        self.assertEqual(datareel.digest(P)[0], datareel.digest(D)[0])
        self.assertFalse(os.path.exists(new))
        with self.assertRaises(datareel.Error):
            datareel.swap(P, P)


if __name__ == "__main__":
    unittest.main(verbosity=2)
