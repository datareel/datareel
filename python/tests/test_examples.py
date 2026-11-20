"""DataReel 7.5 beta: runs every program in python/examples against a demo node, as the user's manual shows them,
and checks what they print. Run by python/tests/run_python_tests.sh (DR_TEST_BIN: the folder with
test_gxobjrepl_reader; DR_TEST_CERTS_SH: utils/objrepl/objrepl_certs.sh).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
EX = os.path.join(os.path.dirname(HERE), "examples")
BIN = os.environ.get("DR_TEST_BIN", "")
CERTS = os.environ.get("DR_TEST_CERTS_SH", "")


def run(name, *args, timeout=120):
    p = subprocess.run([sys.executable, os.path.join(EX, name)] + [str(a) for a in args],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=timeout)
    return p.returncode, p.stdout


class Examples(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="dr_pyex_")
        cls.pki = os.path.join(cls.tmp, "pki")
        subprocess.check_call([CERTS, cls.pki, "--copy-host", "127.0.0.1", "--days", "30", "--apply"],
                              stdout=subprocess.DEVNULL)
        cls.node = os.path.join(cls.tmp, "node")
        subprocess.check_call([os.path.join(BIN, "test_gxobjrepl_reader"), "write", cls.node, cls.pki, "300"],
                              stdout=subprocess.DEVNULL)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def ok(self, name, *args, **kw):
        rc, out = run(name, *args, **kw)
        self.assertEqual(rc, 0, out)
        return out

    def test_make_demo_node(self):
        d = os.path.join(self.tmp, "demo")
        p = subprocess.run([os.path.join(EX, "make_demo_node.sh"), os.path.dirname(BIN), d, "50"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertIn("demo node: " + os.path.join(d, "node"), p.stdout)
        self.assertIn("database vec objects 50 tombstones 0", self.ok("ex1_open_and_list.py", os.path.join(d, "node")))

    def test_ex1(self):
        out = self.ok("ex1_open_and_list.py", self.node)
        self.assertIn("role primary seq 300 epoch 1", out)
        self.assertIn("database big objects 1 tombstones 0", out)
        self.assertIn("database vec objects 300 tombstones 0", out)

    def test_ex2(self):
        out = self.ok("ex2_get_and_decode.py", self.node)
        self.assertEqual(len(re.findall(r"^id \d+ key \d+ vector \[", out, re.M)), 5)
        self.assertIn("key 1 vector [1.0, 0.0, 0.0, 0.0]", out)
        self.assertIn("not there: None", out)

    def test_ex3(self):
        out = self.ok("ex3_find_key.py", self.node, 42)
        self.assertRegex(out, r"key 42 ids \[\d+\]")
        self.assertIn("object (42, 42.0, 0.0, 0.0, 0.0)", out)
        self.assertIn("key 999 ids []", self.ok("ex3_find_key.py", self.node, 999))

    def test_ex4(self):
        out = self.ok("ex4_nearest.py", self.node, 10.2)
        lines = re.findall(r"^id \d+ distance ([0-9.e-]+)$", out, re.M)
        self.assertEqual(len(lines), 3)
        self.assertAlmostEqual(float(lines[0]), 0.04, places=4)          # (10.2 - 10)^2

    def test_ex5(self):
        out = self.ok("ex5_record_layout.py", self.node)
        self.assertIn("seq 300 rows 300", out)
        self.assertIn("first {'key': 1, 'v': (1.0, 0.0, 0.0, 0.0)}", out)
        self.assertIn("keys from 1 to 300", out)
        self.assertIn("mean of v[0] 150.5", out)
        self.assertTrue("numpy rows 300 mean of v[0] 150.5" in out or "numpy is not installed" in out, out)

    def test_ex6_follows_a_running_writer(self):
        live = os.path.join(self.tmp, "live")
        w = subprocess.Popen([os.path.join(BIN, "test_gxobjrepl_reader"), "write", live, self.pki, "1500"],
                             stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        try:
            out = self.ok("ex6_follow_node.py", live, 25)
        finally:
            w.wait(120)
        self.assertEqual(w.returncode, 0)
        m = re.search(r"followed (\d+) objects up to seq (\d+)", out)
        self.assertIsNotNone(m, out)
        self.assertGreater(len(re.findall(r"^seq \d+ new objects", out, re.M)), 1, out)   # it saw several commits
        # the writer ended before the 25 s were up, so the follower saw every object
        self.assertEqual(m.group(1), "1500", out)

    def test_ex7(self):
        vix = os.path.join(self.node, "vec.v0.vix")
        out = self.ok("ex7_vector_files.py", vix, "objrepl/v0/8/4")
        self.assertIn("dim 4 vectors 300 metric l2 spec objrepl/v0/8/4", out)
        self.assertIn("recall@5 of the graph search against exact: 1.000", out)   # exact below 4096 vectors
        self.assertIn("only tag 0 = 0: 5 hits", out)
        self.assertIn("only tag 0 in 1..9: 0 hits", out)
        rc, out = run("ex7_vector_files.py", vix, "another/spec")
        self.assertNotEqual(rc, 0)
        self.assertIn("the spec differs", out)

    def test_ex8(self):
        out = self.ok("ex8_block_file.py", os.path.join(self.node, "vec.gxd"))
        self.assertIn("'live': 300", out)
        self.assertEqual(len(re.findall(r"^address \d+ id \d+ version \d+ bytes 24 first 8 ", out, re.M)), 3)

    def test_ex9(self):
        out = self.ok("ex9_threads.py", self.node, 4)
        self.assertIn("threads 4 objects checked 300 of 300", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
