#!/usr/bin/env python3
"""Tests for gxrepl, DataReel 7.3 database replication over gxStream.

    python3 test_gxrepl.py --bin DIR [-k NAME]

DIR holds gxrepl and gxrepl_testwriter. Needs openssl (the command) to
make a test CA and certificates. Everything runs on 127.0.0.1 in a
temporary folder. Each test starts its own server and client.
"""
import argparse
import hashlib
import json
import os
import random
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import textwrap
import time

BIN = None
TMP = None
PORT = 0
RESULTS = []


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def make_pki(d):
    os.makedirs(d, exist_ok=True)
    def sh(*a):
        r = run(list(a), cwd=d)
        if r.returncode != 0:
            raise RuntimeError(" ".join(a) + "\n" + r.stdout)
    ec = ["-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes"]
    for ca in ("ca", "otherca"):
        sh("openssl", "req", "-x509", *ec, "-keyout", ca + ".key", "-out", ca + ".pem", "-days", "30",
           "-subj", "/CN=test-" + ca)
    def cert(name, cn, ca, ext):
        sh("openssl", "req", *ec, "-keyout", name + ".key", "-out", name + ".csr", "-subj", "/CN=" + cn)
        with open(os.path.join(d, name + ".ext"), "w") as f:
            f.write(ext)
        sh("openssl", "x509", "-req", "-in", name + ".csr", "-CA", ca + ".pem", "-CAkey", ca + ".key",
           "-CAcreateserial", "-out", name + ".pem", "-days", "30", "-extfile", name + ".ext")
        os.chmod(os.path.join(d, name + ".key"), 0o600)
    cert("srv", "aws-test", "ca", "subjectAltName=DNS:aws-test,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n")
    cert("cli", "ws-test", "ca", "extendedKeyUsage=clientAuth\n")
    cert("intruder", "intruder", "ca", "extendedKeyUsage=clientAuth\n")
    cert("foreign", "ws-test", "otherca", "extendedKeyUsage=clientAuth\n")


class Env:
    """A server config, a client config and their folders for one test."""

    def __init__(self, name, sets, client_cert="cli", extra_client="", extra_server=""):
        self.dir = os.path.join(TMP, name)
        shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(self.dir)
        self.port = free_port()
        pki = os.path.join(TMP, "pki")
        self.srv_conf = os.path.join(self.dir, "aws.conf")
        self.cli_conf = os.path.join(self.dir, "ws.conf")
        self.srv_status = os.path.join(self.dir, "aws.status.json")
        self.cli_status = os.path.join(self.dir, "ws.status.json")
        s = [f"node = aws-test", "role = server", f"listen = 127.0.0.1:{self.port}",
             f"cert = {pki}/srv.pem", f"key = {pki}/srv.key", f"ca = {pki}/ca.pem",
             "allow_cn = ws-test", f"status_file = {self.srv_status}", "max_message = 262144", extra_server]
        c = ["node = ws-test", "role = client", f"connect = 127.0.0.1:{self.port}", "server_name = aws-test",
             f"cert = {pki}/{client_cert}.pem", f"key = {pki}/{client_cert}.key", f"ca = {pki}/ca.pem",
             f"status_file = {self.cli_status}", "poll_ms = 100", "max_message = 262144", "timeout = 20",
             extra_client]
        self.roots = {}
        for name_, opts in sets.items():
            ws = os.path.join(self.dir, "ws", name_)
            aws = os.path.join(self.dir, "aws", name_)
            os.makedirs(ws, exist_ok=True)
            self.roots[name_] = (ws, aws)
            s += ["", f"[set {name_}]", f"root = {aws}", f"accept = {opts.get('accept', 'target')}"]
            s += opts.get("server", [])
            c += ["", f"[set {name_}]", f"root = {ws}", f"direction = {opts.get('direction', 'push')}",
                  "scan_interval = 1"]
            c += opts.get("client", [])
        with open(self.srv_conf, "w") as f:
            f.write("\n".join(s) + "\n")
        with open(self.cli_conf, "w") as f:
            f.write("\n".join(c) + "\n")
        self.server = None
        self.client = None

    def start_server(self, extra_env=None, args=()):
        self.srv_log = open(os.path.join(self.dir, "server.log"), "a")
        e = dict(os.environ, **extra_env) if extra_env else None
        self.server = subprocess.Popen([os.path.join(BIN, "gxrepl"), "serve", "--config", self.srv_conf, *args],
                                       stdout=self.srv_log, stderr=subprocess.STDOUT, env=e)
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", self.port), 0.2).close()
                return
            except OSError:
                time.sleep(0.05)
        raise RuntimeError("server did not start: " + open(os.path.join(self.dir, "server.log")).read())

    def start_client(self, extra_env=None):
        self.cli_log = open(os.path.join(self.dir, "client.log"), "a")
        e = dict(os.environ, **extra_env) if extra_env else None
        self.client = subprocess.Popen([os.path.join(BIN, "gxrepl"), "run", "--config", self.cli_conf],
                                       stdout=self.cli_log, stderr=subprocess.STDOUT, env=e)

    def once(self, conf=None, timeout=120):
        return run([os.path.join(BIN, "gxrepl"), "once", "--config", conf or self.cli_conf], timeout=timeout)

    def stop(self, sig=signal.SIGTERM):
        for p in (self.client, self.server):
            if p and p.poll() is None:
                p.send_signal(sig)
                try:
                    p.wait(10)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
        self.client = self.server = None

    def kill(self, which):
        p = getattr(self, which)
        if p and p.poll() is None:
            p.kill()
            p.wait()
        setattr(self, which, None)

    def status(self):
        try:
            with open(self.cli_status) as f:
                return json.load(f)
        except Exception:
            return {}

    def logs(self):
        out = ""
        for n in ("server.log", "client.log"):
            p = os.path.join(self.dir, n)
            if os.path.exists(p):
                out += f"--- {n}\n" + open(p).read()[-4000:]
        return out


SKIP = (".gxrepl",)


def tree(root):
    files = {}
    for dp, dn, fn in os.walk(root):
        rel = os.path.relpath(dp, root)
        if rel == ".gxrepl" or rel.startswith(".gxrepl" + os.sep):
            continue
        if any(c.startswith(".") for c in rel.split(os.sep) if c != "."):
            continue
        for f in fn:
            if f.endswith((".jnl", ".lock", ".tmp", ".gxrtmp")) or "databento" in os.path.join(rel, f).lower():
                continue
            # 11/08/2026: gxrepl carries database files only (and commit.bin where a set includes it)
            if not f.endswith((".gxd", ".btx")) and f != "commit.bin":
                continue
            if f.startswith("."):
                continue
            p = os.path.join(dp, f)
            if os.path.islink(p):
                continue
            with open(p, "rb") as fh:
                files[os.path.normpath(os.path.join(rel, f))] = hashlib.sha256(fh.read()).hexdigest()
    return files


def same(a, b):
    ta, tb = tree(a), tree(b)
    if ta == tb:
        return True, ""
    missing = sorted(set(ta) - set(tb))[:5]
    extra = sorted(set(tb) - set(ta))[:5]
    diff = sorted(k for k in set(ta) & set(tb) if ta[k] != tb[k])[:5]
    return False, f"missing {missing} extra {extra} differ {diff}"


def wait_same(env, name, timeout=60):
    ws, aws = env.roots[name]
    end = time.time() + timeout
    why = ""
    while time.time() < end:
        ok, why = same(ws, aws)
        if ok:
            return True
        time.sleep(0.25)
    raise AssertionError(f"set {name} not the same after {timeout}s: {why}\n{env.logs()}")


def writer(root, *args):
    return run([os.path.join(BIN, "gxrepl_testwriter"), "write", root, *map(str, args)])


def writer_bg(root, *args, env=None):
    e = dict(os.environ, **env) if env else None
    return subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "write", root, *map(str, args)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)


def check_db(root):
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "check", root])
    assert r.returncode == 0, r.stdout
    return r.stdout.strip()


def put(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


# ---------------- Tests ----------------

def test_initial_copy():
    inc = ["include = commit.bin"]
    env = Env("initial", {"db": {"client": inc, "server": inc}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 60, "--per", 6).returncode == 0
    put(os.path.join(ws, "realtime/2026-11-06/commit.bin"), os.urandom(4096))
    put(os.path.join(ws, "big.gxd"), os.urandom(3 * 1024 * 1024 + 123))  # > max_message: in parts
    put(os.path.join(ws, "empty.gxd"), b"")
    put(os.path.join(ws, "x/y/z/deep.btx"), b'{"a": 1}\n')
    put(os.path.join(ws, "research/databento/bars.gxd"), b"internal only")
    put(os.path.join(ws, "ingest.lock"), b"")
    put(os.path.join(ws, "notes.txt"), b"a flat file: DRCM's rsync, never gxrepl")
    put(os.path.join(ws, ".patterns.tmp-old-1/patterns.gxd"), b"a publish leftover")
    os.chmod(os.path.join(ws, "x/y/z/deep.btx"), 0o640)
    env.start_server()
    try:
        r = env.once()
        assert r.returncode == 0, r.stdout + env.logs()
        ok, why = same(ws, aws)
        assert ok, why
        check_db(aws)
        assert not os.path.exists(os.path.join(aws, "research/databento")), "databento data was copied"
        assert not os.path.exists(os.path.join(aws, "ingest.lock"))
        assert not os.path.exists(os.path.join(aws, "writer.jnl"))
        assert oct(os.stat(os.path.join(aws, "x/y/z/deep.btx")).st_mode & 0o777) == "0o640"
        assert os.stat(os.path.join(aws, "big.gxd")).st_mtime_ns == os.stat(os.path.join(ws, "big.gxd")).st_mtime_ns
        assert not os.path.exists(os.path.join(aws, "notes.txt")), "a flat file was copied"
        assert not os.path.exists(os.path.join(aws, ".patterns.tmp-old-1")), "a hidden publish leftover was copied"
        # A second once has nothing to send
        r = env.once()
        assert r.returncode == 0, r.stdout
    finally:
        env.stop()


def test_live_changes():
    env = Env("live", {"db": {}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5).returncode == 0
    env.start_server()
    env.start_client()
    try:
        w = writer_bg(ws, "--commits", 400, "--per", 4, "--sleep", 5, "--seed", 7)
        # Other database files (no ship log) change while the writer runs
        for i in range(20):
            put(os.path.join(ws, f"side/f{i % 7}.gxd"), os.urandom(random.randint(10, 90000)))
            if i % 5 == 4:
                os.remove(os.path.join(ws, f"side/f{(i - 2) % 7}.gxd"))
            time.sleep(0.1)
        assert w.wait(120) == 0, w.stdout.read()
        wait_same(env, "db")
        check_db(aws)
        # The status file follows the copy within a pass or two
        end = time.time() + 10
        while True:
            st = env.status()["sets"]["db"]
            lag = [s["lag"] for s in st["streams"].values()]
            # 11/11/2026: lag is clamped at 0 (r4e), so a status written in the middle of a pass (a stale source
            # position below the copy's) already shows lag 0: wait for copy == source as well
            same = all(s["source"] > 0 and s["copy"] == s["source"] for s in st["streams"].values())
            if (lag and max(lag) == 0 and same) or time.time() > end:
                break
            time.sleep(0.2)
        # 11/10/2026: the copy reached the source's log position. (It was "records > 0": the records a pass
        # drained. In some full-suite runs a verify carried every change, which is correct, so no record
        # was ever drained and the count check failed.)
        assert lag and max(lag) == 0, st
        assert all(s["source"] > 0 and s["copy"] == s["source"] for s in st["streams"].values()), st
    finally:
        env.stop()


def test_writer_crash_and_reopen():
    env = Env("writer_crash", {"db": {}})
    ws, aws = env.roots["db"]
    env.start_server()
    env.start_client()
    try:
        for seed in range(6):
            w = writer_bg(ws, "--commits", 100000, "--per", 3, "--seed", seed + 11)
            time.sleep(random.uniform(0.2, 0.8))
            w.kill()  # kill -9 mid commit: the journal recovers it at the next open
            w.wait()
        assert writer(ws, "--commits", 3).returncode == 0  # recovers the journal, writes an open record
        wait_same(env, "db", 90)
        check_db(ws)
        check_db(aws)
    finally:
        env.stop()


def wait_status(env, pred, timeout=30):
    end = time.time() + timeout
    st = {}
    while time.time() < end:
        st = env.status()
        if st and pred(st):
            return st
        time.sleep(0.2)
    raise AssertionError(f"status never matched: {st}\n{env.logs()}")


def test_reconnect_does_not_rehash():
    """A reconnect verifies only the ship-log streams: unchanged plain files are not read and
    hashed again (size/mtime scan). A SIGHUP still forces a full verify, and the status file
    is written while the client runs."""
    env = Env("rehash", {"plain": {"client": ["verify_interval = 0", "full_verify_interval = 0"]}})
    ws, aws = env.roots["plain"]
    for i in range(40):
        put(os.path.join(ws, f"d{i % 4}/f{i}.gxd"), os.urandom(70 * 1024 + i))
    env.start_server()
    try:
        env.start_client()
        wait_same(env, "plain")
        env.kill("client")
        os.remove(env.cli_status)
        env.start_client()           # a reconnect with nothing changed
        st = wait_status(env, lambda s: s.get("connected") and s["sets"]["plain"]["last_scan"] > 0)
        time.sleep(1.5)              # a few more passes
        st = env.status()["sets"]["plain"]
        assert st["files_sent"] == 0, f"a reconnect rehashed unchanged files: {st}"
        put(os.path.join(ws, "d1/f1.gxd"), os.urandom(5000))   # one change
        wait_same(env, "plain")
        st = wait_status(env, lambda s: s["sets"]["plain"]["files_sent"] >= 1)["sets"]["plain"]
        assert st["files_sent"] == 1, f"only the changed file should go over: {st}"
        env.client.send_signal(signal.SIGHUP)                   # a full verify on request
        st = wait_status(env, lambda s: s["sets"]["plain"]["last_full_verify"] > 0)["sets"]["plain"]
        assert st["files_sent"] >= 41, f"SIGHUP did not verify every file: {st}"
    finally:
        env.stop()


def raw(env, mode, set_, path):
    return run([os.path.join(BIN, "gxrepl_testwriter"), mode, env.cli_conf, set_, path])


def test_flat_files_refused_both_sides():
    """gxrepl carries DataReel databases only. A flat file is never sent, and each side refuses one
    from a peer that skipped its own checks: the copy will not accept it, the source will not send it."""
    env = Env("flat", {"db": {}})
    ws, aws = env.roots["db"]
    put(os.path.join(ws, "a.gxd"), os.urandom(5000))
    put(os.path.join(ws, "notes.txt"), b"flat")
    env.start_server()
    try:
        assert env.once().returncode == 0
        assert os.path.exists(os.path.join(aws, "a.gxd"))
        assert not os.path.exists(os.path.join(aws, "notes.txt")), "a flat file was sent"
        for path in ("notes.txt", "state.json", "x/.hidden.gxd", ".gxrepl/x.gxd"):
            r = raw(env, "rawapply", "db", path)
            assert r.returncode == 2 and "error" in r.stdout, f"the copy accepted {path}: {r.stdout}"
            assert not os.path.exists(os.path.join(aws, path))
        r = raw(env, "rawapply", "db", "ok.gxd")   # the control: a database file is accepted
        assert r.returncode == 0, r.stdout
    finally:
        env.stop()
    env = Env("flat_src", {"back": {"direction": "pull", "accept": "source"}})
    ws, aws = env.roots["back"]
    os.makedirs(aws, exist_ok=True)
    put(os.path.join(aws, "notes.txt"), b"flat on the source")
    put(os.path.join(aws, "b.gxd"), os.urandom(3000))
    env.start_server()
    try:
        r = raw(env, "rawdiff", "back", "notes.txt")
        assert r.returncode == 2 and "not a database file" in r.stdout, f"the source sent a flat file: {r.stdout}"
        r = raw(env, "rawdiff", "back", "b.gxd")
        assert r.returncode == 0, r.stdout
        assert env.once().returncode == 0
        assert os.path.exists(os.path.join(ws, "b.gxd")) and not os.path.exists(os.path.join(ws, "notes.txt"))
    finally:
        env.stop()


def test_deletes_only_database_files():
    """deletes = yes removes database files the source no longer has, and never touches the copy's
    other files (DRCM's rsync owns those)."""
    env = Env("deldb", {"db": {"client": ["deletes = yes"]}})
    ws, aws = env.roots["db"]
    put(os.path.join(ws, "a.gxd"), b"a")
    put(os.path.join(aws, "old.gxd"), b"gone from the source")
    put(os.path.join(aws, "old.btx"), b"gone from the source")
    put(os.path.join(aws, "report.json"), b"rsync's file")
    env.start_server()
    try:
        assert env.once().returncode == 0
        assert not os.path.exists(os.path.join(aws, "old.gxd")) and not os.path.exists(os.path.join(aws, "old.btx"))
        assert open(os.path.join(aws, "report.json"), "rb").read() == b"rsync's file"
        assert os.path.exists(os.path.join(aws, "a.gxd"))
    finally:
        env.stop()


def test_live_db_without_ship_log():
    """A database written through its journal without a ship log is read under the journal's read
    lock while the writer commits: the copy is never torn and ends identical."""
    env = Env("noship", {"db": {"client": ["verify_interval = 0", "full_verify_interval = 0"]}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5, "--no-ship").returncode == 0
    env.start_server()
    env.start_client()
    try:
        w = writer_bg(ws, "--commits", 300, "--per", 4, "--sleep", 5, "--seed", 9, "--no-ship")
        assert w.wait(120) == 0, w.stdout.read()
        wait_same(env, "db", 90)
        check_db(aws)
        assert env.status()["sets"]["db"]["streams"] == {}, "expected no ship log stream"
    finally:
        env.stop()


def test_published_folder_swap():
    """A published database folder (no journal) replaced by a rename, as OSA publishes db/patterns:
    the copy follows each version whole, and the hidden leftovers are never copied."""
    env = Env("publish", {"db": {"client": ["verify_interval = 0", "full_verify_interval = 0"]}})
    ws, aws = env.roots["db"]
    def publish(n):
        tmp = os.path.join(ws, f".patterns.tmp-new-{n}")
        put(os.path.join(tmp, "patterns.gxd"), os.urandom(200000 + n))
        for i in range(3):
            put(os.path.join(tmp, f"ix{i}.btx"), os.urandom(50000 + n * 10 + i))
        cur = os.path.join(ws, "patterns")
        if os.path.exists(cur):
            os.rename(cur, os.path.join(ws, f".patterns.tmp-old-{n}"))
        os.rename(tmp, cur)
    publish(0)
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        for n in range(1, 6):
            publish(n)
            time.sleep(random.uniform(0.0, 0.4))
        wait_same(env, "db", 60)
        assert not any(d.startswith(".") for d in os.listdir(aws) if d != ".gxrepl"), os.listdir(aws)
    finally:
        env.stop()


def test_torn_ship_log_tail():
    """A power loss mid-append leaves half a record at the end of the ship log. The writer cuts it
    back to the last whole record when it opens the log again, and the copy stays whole and catches
    up with no gap."""
    env = Env("torn", {"db": {}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 30, "--per", 3).returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        logdir = os.path.join(ws, ".gxrepl", "log")
        stream = os.path.join(logdir, os.listdir(logdir)[0])
        segs = sorted(f for f in os.listdir(stream) if f.endswith(".gxl"))
        assert segs, os.listdir(stream)
        last = os.path.join(stream, segs[-1])
        size0 = os.path.getsize(last)
        with open(last, "ab") as f:      # half a record: a header start and some payload
            f.write(b"\x52\x45\x43\x31" + os.urandom(21))
        assert writer(ws, "--commits", 20, "--per", 3, "--seed", 4).returncode == 0   # reopens, cuts, appends
        assert env.once().returncode == 0, env.logs()
        ok, why = same(ws, aws)
        assert ok, why
        check_db(aws)
        with open(last, "rb") as f:
            assert f.read()[size0:size0 + 4] != b"\x52\x45\x43\x31", "the torn tail was not cut"
    finally:
        env.stop()


def test_server_unclaimed_first_block():
    """The database object server gives a session to the service that claims its first block. A
    first block no service claims (here an object-access 'A' block with no object service set up)
    closes that session only, and the server goes on serving."""
    env = Env("unclaimed", {"db": {}})
    ws, aws = env.roots["db"]
    put(os.path.join(ws, "a.gxd"), b"after")
    env.start_server()
    try:
        r = run([os.path.join(BIN, "gxrepl_testwriter"), "rawblock", env.cli_conf, "A"])
        assert r.stdout.strip() == "closed", r.stdout + env.logs()
        assert env.once().returncode == 0, env.logs()
        assert os.path.exists(os.path.join(aws, "a.gxd"))
        assert "no service for its first block" in env.logs(), env.logs()
    finally:
        env.stop()


def test_mux_unit():
    """The database object server's multiplex (gxdbmux_test): channels interleaved, a refused and
    an errored channel, a slow large channel next to a small one, a full queue, shutdown."""
    r = run([os.path.join(BIN, "gxdbmux_test")], timeout=300)
    for line in r.stdout.splitlines():
        print("    " + line, flush=True)
    assert r.returncode == 0, r.stdout


def server_status(env):
    try:
        with open(env.srv_status) as f:
            return json.load(f)
    except Exception:
        return {}


def test_many_sets_at_once():
    """Protocol 3: four sets over one connection, each on its own channel and thread, their writers
    committing at the same time. Each copy matches its own commits, and the server shows a channel
    for each set."""
    names = ["db1", "db2", "db3", "db4"]
    env = Env("manysets", {n: {} for n in names})
    env.start_server()
    env.start_client()
    try:
        ws_writers = [writer_bg(env.roots[n][0], "--commits", 250, "--per", 3, "--sleep", 4, "--seed", 20 + k)
                      for k, n in enumerate(names)]
        chans = set()
        end = time.time() + 30
        while time.time() < end and len(chans) < len(names):
            chans = {c["set"] for c in server_status(env).get("channels", [])}
            time.sleep(0.3)
        for w in ws_writers:
            assert w.wait(120) == 0, w.stdout.read()
        for n in names:
            wait_same(env, n, 90)
            check_db(env.roots[n][1])
        assert chans == set(names), f"server channels {chans}"
        ok = {n: env.status()["sets"][n]["ok"] for n in names}
        assert all(ok.values()), ok
    finally:
        env.stop()


def test_refused_set_others_continue():
    """A set the server refuses (unknown there, or the wrong role) closes only its own channel:
    the other set replicates, and the status says why the refused ones are not in step."""
    env = Env("refusedset", {"good": {}, "ghost": {}, "src": {"accept": "source"}})
    with open(env.srv_conf) as f:
        conf = f.read()
    k = conf.index("[set ghost]")
    k2 = conf.index("[set src]")
    with open(env.srv_conf, "w") as f:      # the server has no "ghost" set
        f.write(conf[:k] + conf[k2:])
    ws, aws = env.roots["good"]
    assert writer(ws, "--commits", 40).returncode == 0
    put(os.path.join(env.roots["ghost"][0], "g.gxd"), b"ghost")
    put(os.path.join(env.roots["src"][0], "s.gxd"), b"src")
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "good", 60)
        check_db(aws)
        st = wait_status(env, lambda s: s["sets"]["ghost"]["last_error"] and s["sets"]["src"]["last_error"]
                         and s["sets"]["good"]["ok"])["sets"]
        assert "no set ghost" in st["ghost"]["last_error"], st["ghost"]
        assert "source" in st["src"]["last_error"], st["src"]
        assert st["good"]["ok"], st["good"]
        assert not os.path.exists(os.path.join(env.roots["src"][1], "s.gxd"))
    finally:
        env.stop()


def test_slow_set_does_not_block_small():
    """A slow set (each request delayed on the server) next to a small live one: the small set stays
    real time while the slow one is still copying."""
    env = Env("slowset", {"big": {}, "small": {}})
    big_ws, big_aws = env.roots["big"]
    for i in range(150):
        put(os.path.join(big_ws, f"b{i}.gxd"), os.urandom(40000 + i))
    small_ws, small_aws = env.roots["small"]
    env.start_server(extra_env={"GXREPL_TEST_SLOW_SET": "big:60"})
    env.start_client()
    try:
        t0 = time.time()
        w = writer_bg(small_ws, "--commits", 60, "--per", 2, "--sleep", 20, "--seed", 3)
        assert w.wait(60) == 0, w.stdout.read()
        wait_same(env, "small", 30)
        t_small = time.time() - t0
        big_done = same(big_ws, big_aws)[0]
        check_db(small_aws)
        print(f"    small set in step after {t_small:.1f}s; big set done then: {big_done}", flush=True)
        assert not big_done, "the slow set finished too quickly to show anything"
        wait_same(env, "big", 120)
    finally:
        env.stop()


def test_retention_floor():
    """The ship log keeps what a copy has not acknowledged (the ACK floor, acked/<copy>). With the
    copy offline, old segments it already has are removed and the ones it still needs stay; the copy
    then catches up."""
    env = Env("floor", {"db": {}})
    ws, aws = env.roots["db"]
    opts = ["--segment", "6000", "--keep-days", "1"]
    assert writer(ws, "--commits", 60, "--per", 2, *opts).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        logdir = os.path.join(ws, ".gxrepl", "log")
        stream = os.path.join(logdir, os.listdir(logdir)[0])
        acked = os.path.join(stream, "acked")
        st = wait_status(env, lambda s: os.path.isdir(acked) and os.listdir(acked))
        floor = int(open(os.path.join(acked, os.listdir(acked)[0])).read())
        assert floor > 0, floor
        env.kill("client")                                # the copy goes offline
        # A floor write a crash stopped half way: not a copy, so it holds nothing back
        open(os.path.join(acked, os.listdir(acked)[0] + ".gxrtmp"), "w").write("")
        assert writer(ws, "--commits", 60, "--per", 2, "--seed", 8, *opts).returncode == 0
        segs = sorted(f for f in os.listdir(stream) if f.endswith(".gxl"))
        old = time.time() - 3 * 86400
        for f in segs:                                    # every segment is past keep-days now
            os.utime(os.path.join(stream, f), (old, old))
        assert writer(ws, "--commits", 10, "--per", 2, "--seed", 9, *opts).returncode == 0   # rolls, purges
        left = sorted(int(f[:16], 16) for f in os.listdir(stream) if f.endswith(".gxl"))
        assert left and left[0] <= floor + 1, f"a segment the copy needs (after lsn {floor}) was removed: {left[:5]}"
        assert left[0] > 1, "nothing the copy had was removed"
        # The copy comes back and catches up (the writer's restarts left open records, which a copy
        # answers with a hash verify of the stream's files; the log it still needed is all there)
        env.start_client()
        wait_same(env, "db", 60)
        check_db(aws)
    finally:
        env.stop()


def objects(env, db, mode, count=0):
    return run([os.path.join(BIN, "gxrepl_testwriter"), "objects", env.cli_conf, db, mode, str(count)], timeout=120)


def test_object_service():
    """Database objects over the network (the port's primary use): add, request, change, delete and
    walk a database on the server through an object channel; a 6.x style client (A/R blocks) works on
    the legacy database."""
    env = Env("objects", {"db": {}}, extra_server="")
    orders = os.path.join(env.dir, "srvdb", "orders.gxd")
    os.makedirs(os.path.dirname(orders))
    with open(env.srv_conf) as f:
        conf = f.read()
    k = conf.index("[set db]")
    conf = conf[:k] + f"legacy_db = orders\n[db orders]\npath = {orders}\naccess = write\n\n" + conf[k:]
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    env.start_server()
    try:
        r = objects(env, "orders", "write", 300)
        assert r.returncode == 0 and r.stdout.startswith("objects ok"), r.stdout + env.logs()
        r = run([os.path.join(BIN, "gxrepl_testwriter"), "legacyobj", env.cli_conf])
        assert r.stdout.strip() == "legacy ok", r.stdout + env.logs()
        r = objects(env, "nosuch", "read")
        assert r.returncode == 3 and "no database" in r.stdout, r.stdout
    finally:
        env.stop()


def test_object_copy_is_read_only():
    """A database inside a set the server receives is a replicated copy: object clients may read it
    (under the apply lock) but never write it."""
    env = Env("objcopy", {"db": {}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 30).returncode == 0
    with open(env.srv_conf) as f:
        conf = f.read()
    k = conf.index("[set db]")
    conf = conf[:k] + f"[db copy]\npath = {aws}/data.gxd\naccess = write\n\n" + conf[k:]
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    env.start_server()
    try:
        assert env.once().returncode == 0, env.logs()
        r = objects(env, "copy", "write", 5)
        assert r.returncode == 3 and "replicated copy" in r.stdout, r.stdout
        r = objects(env, "copy", "read")
        assert r.returncode == 0 and r.stdout.startswith("readonly ok"), r.stdout + env.logs()
        assert int(r.stdout.split()[2]) > 0, r.stdout
    finally:
        env.stop()


def test_objects_and_replication_together():
    """Object clients and replication share one server at once."""
    env = Env("objrepl", {"db": {}})
    ws, aws = env.roots["db"]
    orders = os.path.join(env.dir, "srvdb", "orders.gxd")
    os.makedirs(os.path.dirname(orders))
    with open(env.srv_conf) as f:
        conf = f.read()
    k = conf.index("[set db]")
    conf = conf[:k] + f"[db orders]\npath = {orders}\naccess = write\n\n" + conf[k:]
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    env.start_server()
    env.start_client()
    try:
        w = writer_bg(ws, "--commits", 200, "--per", 3, "--sleep", 5, "--seed", 12)
        outs = [subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "objects", env.cli_conf, "orders", "write", "150"],
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) for _ in range(2)]
        for o in outs:
            out = o.communicate(timeout=120)[0]
            assert o.returncode == 0 and out.startswith("objects ok"), out + env.logs()
        assert w.wait(120) == 0, w.stdout.read()
        wait_same(env, "db", 60)
        check_db(aws)
    finally:
        env.stop()


def test_object_examples():
    """The example programs (utils/dbserver): object_server serves a database, object_client stores
    an object and reads it back over mutual TLS."""
    d = os.path.join(TMP, "examples")
    os.makedirs(d, exist_ok=True)
    pki = os.path.join(TMP, "pki")
    port = free_port()
    srv = subprocess.Popen([os.path.join(BIN, "object_server"), str(port), f"{pki}/srv.pem", f"{pki}/srv.key",
                            f"{pki}/ca.pem", "ws-test", "notes", os.path.join(d, "notes.gxd")],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", port), 0.2).close()
                break
            except OSError:
                time.sleep(0.05)
        r = run([os.path.join(BIN, "object_client"), "127.0.0.1", str(port), "aws-test", f"{pki}/cli.pem",
                 f"{pki}/cli.key", f"{pki}/ca.pem", "notes", "hello from the example"], timeout=30)
        assert r.returncode == 0 and "read back: hello from the example" in r.stdout, r.stdout
    finally:
        srv.send_signal(signal.SIGTERM)
        try:
            srv.wait(10)
        except subprocess.TimeoutExpired:
            srv.kill()


def test_link_loss_and_catch_up():
    env = Env("link", {"db": {}})
    ws, aws = env.roots["db"]
    env.start_server()
    env.start_client()
    try:
        w = writer_bg(ws, "--commits", 1500, "--per", 3, "--sleep", 2, "--seed", 3)
        for i in range(4):
            time.sleep(random.uniform(0.3, 0.9))
            env.kill("server")      # the link drops (kill -9 of the server mid apply)
            time.sleep(random.uniform(0.2, 1.5))
            env.start_server()
            time.sleep(random.uniform(0.2, 0.6))
            env.kill("client")      # and the client dies too
            env.start_client()
        assert w.wait(180) == 0, w.stdout.read()
        wait_same(env, "db", 120)
        check_db(aws)
    finally:
        env.stop()


def test_log_lost_long_outage():
    env = Env("outage", {"db": {}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 30).returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        # Long outage: the ship log is purged (made again) while the link is down
        assert writer(ws, "--commits", 40, "--seed", 9).returncode == 0
        shutil.rmtree(os.path.join(ws, ".gxrepl", "log"))
        assert writer(ws, "--commits", 10, "--seed", 10).returncode == 0
        r = env.once()
        assert r.returncode == 0, r.stdout + env.logs()
        ok, why = same(ws, aws)
        assert ok, why
        check_db(aws)
    finally:
        env.stop()


def test_pull_direction():
    env = Env("pull", {"back": {"direction": "pull", "accept": "source"}})
    ws, aws = env.roots["back"]
    os.makedirs(aws, exist_ok=True)
    assert writer(aws, "--commits", 40).returncode == 0   # AWS was active during a fail over
    put(os.path.join(aws, "notes/day.gxd"), b"written on AWS\n")
    put(os.path.join(ws, "stale.gxd"), b"left on the workstation\n")
    env.start_server()
    try:
        r = env.once()
        assert r.returncode == 0, r.stdout + env.logs()
        ok, why = same(aws, ws)
        assert ok, why
        check_db(ws)
        assert not os.path.exists(os.path.join(ws, "stale.gxd"))
        assert writer(aws, "--commits", 25, "--seed", 5).returncode == 0
        r = env.once()
        assert r.returncode == 0, r.stdout
        ok, why = same(aws, ws)
        assert ok, why
    finally:
        env.stop()


def test_failback_pull_all():
    """11/11/2026: failback (gxrepl.sh failback): after a take over AWS's writer changed its copy (a commit_block set
    plus a plain database file); with the server started --accept-both, the client's once --pull-all brings every set
    back to this node, deletes nothing that only this node has, and the pulled copies pass promote-check here. A normal
    server (no --accept-both) refuses the pull; a writer holding its lock on this node refuses it too."""
    env = Env("failback", {"db": {"client": CB_OPTS["client"] + ["writer_lock = w.lock"],
                                  "server": CB_OPTS["server"]}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        ok, why = same(ws, aws)
        assert ok, why
        env.stop()
        # the take over: AWS's writer commits on its copy; this node keeps a file AWS never had
        assert writer(aws, "--commits", 7, "--no-ship", "--commit-block", "--seed", 81).returncode == 0
        os.makedirs(os.path.join(aws, "notes"), exist_ok=True)
        os.makedirs(os.path.join(ws, "keep"), exist_ok=True)
        assert writer(os.path.join(aws, "notes"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 82).returncode == 0
        assert writer(os.path.join(ws, "keep"), "--commits", 2, "--no-ship", "--commit-block", "--seed", 83).returncode == 0
        pull = [os.path.join(BIN, "gxrepl"), "once", "--config", env.cli_conf, "--pull-all"]
        # a normal server refuses to be a source
        env.start_server()
        r = run(pull)
        assert r.returncode != 0, r.stdout
        env.stop()
        # failback: the server for this run accepts both ways
        env.start_server(args=("--accept-both",))
        import fcntl
        fd = os.open(os.path.join(ws, "w.lock"), os.O_RDWR | os.O_CREAT, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX)                                   # a writer runs here: refused
        try:
            r = run(pull)
            assert r.returncode != 0, r.stdout
        finally:
            fcntl.flock(fd, fcntl.LOCK_UN)
            os.close(fd)
        r = run(pull)
        assert r.returncode == 0, r.stdout + env.logs()
        check_db(ws)                                                     # AWS's commits are here now
        check_db(os.path.join(ws, "notes"))                              # a folder only AWS had: pulled
        check_db(os.path.join(ws, "keep"))                               # a folder only this node had: kept
        for n in ("data.gxd", "index.gxd", "commit.bin"):
            with open(os.path.join(ws, n), "rb") as a, open(os.path.join(aws, n), "rb") as b:
                assert a.read() == b.read(), n
        t = run([os.path.join(BIN, "gxrepl"), "torn", "--config", env.cli_conf, "--copied-only"])
        print(f"    failback: pulled, promote-check here exit {t.returncode}: {t.stdout.strip()!r}", flush=True)
        assert t.returncode == 0 and "OWN keep" in t.stdout, t.stdout
        # without --copied-only the folder this node made is "never sealed": the failback check needs the flag
        assert run([os.path.join(BIN, "gxrepl"), "torn", "--config", env.cli_conf]).returncode == 1
    finally:
        env.stop()


def test_split_brain_guard():
    env = Env("guard", {"db": {"server": ["writer_lock = ingest.lock"]}})
    ws, aws = env.roots["db"]
    os.makedirs(aws, exist_ok=True)
    put(os.path.join(ws, "a.gxd"), b"one\n")
    import fcntl
    lockf = open(os.path.join(aws, "ingest.lock"), "w")
    fcntl.flock(lockf, fcntl.LOCK_EX)    # a writer is active on AWS
    env.start_server()
    try:
        r = env.once()
        assert r.returncode != 0, "applied while a local writer held the lock"
        assert not os.path.exists(os.path.join(aws, "a.gxd"))
        assert "local writer" in json.dumps(env.status()), env.status()
        fcntl.flock(lockf, fcntl.LOCK_UN)
        r = env.once()
        assert r.returncode == 0, r.stdout + env.logs()
        assert os.path.exists(os.path.join(aws, "a.gxd"))
    finally:
        lockf.close()
        env.stop()


def test_server_refuses_source_set():
    env = Env("refuse", {"db": {"accept": "source"}})
    ws, aws = env.roots["db"]
    put(os.path.join(ws, "a.gxd"), b"x")
    env.start_server()
    try:
        r = env.once()
        assert r.returncode != 0
        assert not os.path.exists(os.path.join(aws, "a.gxd"))
    finally:
        env.stop()


def test_tls_refusals():
    for cert, what in (("intruder", "a name not on the allow list"), ("foreign", "another CA")):
        env = Env("tls_" + cert, {"db": {}}, client_cert=cert)
        ws, aws = env.roots["db"]
        put(os.path.join(ws, "a.gxd"), b"x")
        env.start_server()
        try:
            r = env.once(timeout=60)
            assert r.returncode == 2, f"{what}: {r.returncode} {r.stdout}"
            assert not os.path.exists(os.path.join(aws, "a.gxd")), what
        finally:
            env.stop()
    # A client that expects another server name
    env = Env("tls_name", {"db": {}}, extra_client="")
    with open(env.cli_conf) as f:
        conf = f.read().replace("server_name = aws-test", "server_name = not-aws")
    with open(env.cli_conf, "w") as f:
        f.write(conf)
    env.start_server()
    try:
        r = env.once(timeout=60)
        assert r.returncode == 2, r.stdout
    finally:
        env.stop()
    # A plain TCP client gets nothing
    env = Env("tls_plain", {"db": {}})
    env.start_server()
    try:
        s = socket.create_connection(("127.0.0.1", env.port), 5)
        s.sendall(b"\xfe\xfe\x00\x00" + b"x" * 64)
        s.settimeout(5)
        try:
            data = s.recv(100)
        except OSError:
            data = b""
        assert b"GXREPL" not in data and b"aws-test" not in data
        s.close()
    finally:
        env.stop()


FAKE_SSH = r'''#!/usr/bin/env python3
# Stand-in for ssh -N -L 127.0.0.1:LPORT:HOST:PORT dest: forwards TCP.
import socket, sys, threading
args = sys.argv[1:]
spec = args[args.index("-L") + 1]
_, lport, host, port = spec.rsplit(":", 3)
open(sys.argv[0] + ".args", "w").write(" ".join(args))
ls = socket.socket(); ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ls.bind(("127.0.0.1", int(lport))); ls.listen(4)
def pipe(a, b):
    try:
        while True:
            d = a.recv(65536)
            if not d: break
            b.sendall(d)
    except OSError: pass
    for s in (a, b):
        try: s.shutdown(socket.SHUT_RDWR)
        except OSError: pass
while True:
    c, _ = ls.accept()
    u = socket.create_connection((host, int(port)))
    threading.Thread(target=pipe, args=(c, u), daemon=True).start()
    threading.Thread(target=pipe, args=(u, c), daemon=True).start()
'''


def test_idle_connections_do_not_block():
    """Scanners on an open port: connections that never start TLS must not keep the real client out."""
    env = Env("idle", {"db": {}}, extra_server="handshake_timeout = 3\nmax_handshakes = 4")
    ws, aws = env.roots["db"]
    put(os.path.join(ws, "a.gxd"), b"hello")
    env.start_server()
    idle = []
    try:
        # Three silent TCP connections (under max_handshakes): the client still gets in at once
        idle = [socket.create_connection(("127.0.0.1", env.port), 5) for _ in range(3)]
        t = time.time()
        r = env.once(timeout=60)
        assert r.returncode == 0, r.stdout
        assert time.time() - t < 3, f"the client waited {time.time() - t:.1f} s behind idle connections"
        assert open(os.path.join(aws, "a.gxd"), "rb").read() == b"hello"
        # The server closes them after handshake_timeout
        for c in idle:
            c.settimeout(8)
            try:
                assert c.recv(100) == b""
            except ConnectionResetError:
                pass
        # A flood over max_handshakes is dropped, and the server is fine once it passes
        idle = [socket.create_connection(("127.0.0.1", env.port), 5) for _ in range(12)]
        time.sleep(4)
        put(os.path.join(ws, "b.gxd"), b"after the flood")
        r = env.once(timeout=60)
        assert r.returncode == 0, r.stdout
        assert open(os.path.join(aws, "b.gxd"), "rb").read() == b"after the flood"
        assert env.server.poll() is None
    finally:
        for c in idle:
            c.close()
        env.stop()


def test_rsync_style_excludes():
    """The OSA sync list: anchored /var/ and /logs/, the Webull token folders, lock and tmp files."""
    ex = ["exclude = /var/, /proc/, /logs/, /trading/webull/*/token/, /trading/webull/rate/",
          "exclude = .*.lock, *.tmp*"]
    env = Env("excl", {"d": {"client": ex + ["deletes = yes"], "server": ex}})
    ws, aws = env.roots["d"]
    keep = ["db/a.gxd", "screeners/var/x.gxd", "screeners/logs/y.btx", "trading/webull/paper/ledger.gxd", "etc/k.btx"]
    skip = ["var/osa_sip_manager/state.gxd", "proc/p.gxd", "logs/run.gxd", "trading/webull/paper/token/t.gxd",
            "trading/webull/rate/r.gxd", "x/.y.lock", "x/z.tmp.1", "research_Databento/b.gxd", "etc/flat.json"]
    for f in keep + skip:
        put(os.path.join(ws, f), f.encode())
    # The copy's own files in excluded folders survive deletes
    put(os.path.join(aws, "var/osa_sip_manager/aws_state.gxd"), b"aws")
    env.start_server()
    try:
        r = env.once(timeout=60)
        assert r.returncode == 0, r.stdout
        for f in keep:
            assert os.path.exists(os.path.join(aws, f)), "missing " + f
        for f in skip:
            assert not os.path.exists(os.path.join(aws, f)), "copied " + f
        assert open(os.path.join(aws, "var/osa_sip_manager/aws_state.gxd"), "rb").read() == b"aws"
    finally:
        env.stop()


def test_ssh_tunnel():
    lport = free_port()
    env = Env("tunnel", {"db": {}})
    fake = os.path.join(env.dir, "fake_ssh.py")
    with open(fake, "w") as f:
        f.write(FAKE_SSH)
    os.chmod(fake, 0o755)
    with open(env.cli_conf) as f:
        conf = f.read().replace(f"connect = 127.0.0.1:{env.port}",
                                f"ssh_tunnel = osa@aws-test\nssh_command = {fake}\n"
                                f"ssh_local_port = {lport}\nssh_remote = 127.0.0.1:{env.port}\n"
                                "ssh_option = IdentitiesOnly=yes")
    with open(env.cli_conf, "w") as f:
        f.write(conf)
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 20).returncode == 0
    env.start_server()
    try:
        r = env.once()
        assert r.returncode == 0, r.stdout + env.logs()
        ok, why = same(ws, aws)
        assert ok, why
        args = open(fake + ".args").read()
        assert "-N" in args and "ExitOnForwardFailure=yes" in args and "osa@aws-test" in args, args
        assert "IdentitiesOnly=yes" in args, args
        assert "ssh" in env.status().get("via", ""), env.status()
    finally:
        env.stop()


def test_key_from_pipe():
    env = Env("keypipe", {"db": {}})
    with open(env.cli_conf) as f:
        conf = f.read()
    keyfile = os.path.join(TMP, "pki", "cli.key")
    conf = conf.replace(f"key = {keyfile}", "key = fd:3")
    with open(env.cli_conf, "w") as f:
        f.write(conf)
    ws, aws = env.roots["db"]
    put(os.path.join(ws, "a.gxd"), b"via a pipe\n")
    env.start_server()
    try:
        r_fd, w_fd = os.pipe()
        os.write(w_fd, open(keyfile, "rb").read())
        os.close(w_fd)
        p = subprocess.run([os.path.join(BIN, "gxrepl"), "once", "--config", env.cli_conf, "--key-fd", str(r_fd)],
                           pass_fds=(r_fd,), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=60)
        os.close(r_fd)
        assert p.returncode == 0, p.stdout + env.logs()
        assert open(os.path.join(aws, "a.gxd"), "rb").read() == b"via a pipe\n"
        # The env: form, removed from the environment after it is read
        e = dict(os.environ, GXREPL_TEST_KEY=open(keyfile).read())
        conf2 = conf.replace("key = fd:3", "key = env:GXREPL_TEST_KEY")
        with open(env.cli_conf, "w") as f:
            f.write(conf2)
        put(os.path.join(ws, "b.gxd"), b"via env\n")
        p = subprocess.run([os.path.join(BIN, "gxrepl"), "once", "--config", env.cli_conf], env=e,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=60)
        assert p.returncode == 0, p.stdout
        assert os.path.exists(os.path.join(aws, "b.gxd"))
    finally:
        env.stop()


def test_unsafe_records_refused():
    """A ship log record naming a file outside the set is skipped on the copy."""
    env = Env("unsafe", {"db": {}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5).returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        # Forge a stream whose files list and records name ../escape
        d = os.path.join(ws, ".gxrepl", "log", "evil")
        os.makedirs(d)
        open(os.path.join(d, "log.id"), "w").write("00000000deadbeef\n")
        open(os.path.join(d, "files"), "w").write("../escape.txt\n")
        r = env.once()
        assert not os.path.exists(os.path.join(os.path.dirname(aws), "escape.txt"))
        assert not os.path.exists(os.path.join(env.dir, "escape.txt"))
    finally:
        env.stop()

# ---------------- Power loss ----------------
# 11/09/2026: powerloss_shim.c (LD_PRELOAD) saves what a disk would still
# have after a power cut: a file's bytes when it is fsynced, a folder's
# entries when the folder is fsynced. A cut kills the processes with
# SIGKILL and rebuilds the folders from that image only (or, with "mix",
# lets each file keep its unsynced bytes at random, as the page cache may
# have written some of them back).

SHIM = None


def shim():
    global SHIM
    if SHIM is None:
        src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "powerloss_shim.c")
        SHIM = os.path.join(TMP, "powerloss_shim.so")
        r = run(["cc", "-O2", "-shared", "-fPIC", "-o", SHIM, src, "-ldl"])
        assert r.returncode == 0, r.stdout
    return SHIM


def ikey(st):
    return f"{st.st_dev}_{st.st_ino}"


class PowerImage:
    def __init__(self, d, roots):
        self.dir = d
        self.roots = [os.path.abspath(r) for r in roots]
        self.cuts = 0

    def env(self):
        return {"LD_PRELOAD": shim(), "PL_ROOTS": ":".join(self.roots), "PL_DURABLE": self.dir}

    def baseline(self):
        """Everything on disk now counts as durable (the test made it before the shim ran)."""
        shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(os.path.join(self.dir, "inodes"))
        os.makedirs(os.path.join(self.dir, "dirs"))
        self.root_keys = {}
        for r in self.roots:
            os.makedirs(r, exist_ok=True)
            self.root_keys[r] = ikey(os.stat(r))
            for dp, dn, fn in os.walk(r):
                lines = []
                for n in dn + fn:
                    st = os.lstat(os.path.join(dp, n))
                    ty = "d" if os.path.isdir(os.path.join(dp, n)) and not os.path.islink(os.path.join(dp, n)) else \
                         "f" if os.path.isfile(os.path.join(dp, n)) and not os.path.islink(os.path.join(dp, n)) else "o"
                    lines.append(f"{ty}\t{ikey(st)}\t{n}\n")
                    if ty == "f":
                        shutil.copyfile(os.path.join(dp, n), os.path.join(self.dir, "inodes", ikey(st)))
                with open(os.path.join(self.dir, "dirs", ikey(os.stat(dp))), "w") as f:
                    f.write("".join(lines))

    def cut(self, root, mix=None):
        """Rebuild ROOT as a power cut leaves it. The processes using it must be dead."""
        root = os.path.abspath(root)
        self.cuts += 1
        live = f"{root}.cut{self.cuts}"
        os.rename(root, live)
        live_map = {}
        if mix is not None:
            for dp, dn, fn in os.walk(live):
                for n in fn:
                    p = os.path.join(dp, n)
                    if os.path.isfile(p) and not os.path.islink(p):
                        live_map[ikey(os.stat(p))] = p
        stats = {"files": 0, "dirs": 0, "live": 0, "empty": 0}

        def build(path, key):
            os.mkdir(path)
            stats["dirs"] += 1
            lp = os.path.join(self.dir, "dirs", key)
            if not os.path.exists(lp):
                return  # Made after the start and never fsynced: no entries survive
            for line in open(lp).read().splitlines():
                ty, k, name = line.split("\t", 2)
                p = os.path.join(path, name)
                if ty == "d":
                    build(p, k)
                elif ty == "f":
                    stats["files"] += 1
                    src = os.path.join(self.dir, "inodes", k)
                    if mix is not None and k in live_map and mix.random() < 0.5:
                        shutil.copyfile(live_map[k], p)
                        stats["live"] += 1
                    elif os.path.exists(src):
                        shutil.copyfile(src, p)
                    else:
                        open(p, "wb").close()  # The name is durable, its bytes never were
                        stats["empty"] += 1
        build(root, self.root_keys[root])
        return stats


def read_num(p):
    try:
        return int(open(p).read().split()[0])
    except Exception:
        return None


def durable_positions(ws, aws):
    """(stream, floor the source kept, position the copy has) after a cut."""
    out = []
    logd = os.path.join(ws, ".gxrepl", "log")
    if not os.path.isdir(logd):
        return out
    for s in sorted(os.listdir(logd)):
        ad = os.path.join(logd, s, "acked")
        if not os.path.isdir(ad):
            continue
        for c in os.listdir(ad):
            if c.startswith(".") or c.endswith(".gxrtmp"):
                continue  # A write the cut stopped (purge skips it too)
            floor = read_num(os.path.join(ad, c))
            sp = os.path.join(aws, ".gxrepl", "state", s + ".state")
            pos = None
            try:
                pos = int(open(sp).read().split()[1])
            except Exception:
                pass
            out.append((s, floor, pos))
    return out


def power_loss_cycles(name, side, cycles, mix_every=2):
    """SIDE: both, source (the writer's node) or copy (the server's node)."""
    env = Env(name, {"db": {}})
    ws, aws = env.roots["db"]
    os.makedirs(aws, exist_ok=True)
    assert writer(ws, "--commits", 20, "--per", 3).returncode == 0
    img = PowerImage(os.path.join(env.dir, "image"), [ws, aws])
    rng = random.Random(1109 + len(name))
    notes = []
    try:
        env.start_server()
        env.start_client()
        wait_same(env, "db", 60)
        for c in range(cycles):
            env.stop()
            # A database file with no ship log, big enough to go in parts: a cut can land mid copy
            put(os.path.join(ws, f"side/big{c % 3}.gxd"), os.urandom(rng.randint(1, 5) * 1024 * 1024 + rng.randint(0, 9999)))
            img.baseline()
            pe = img.env()
            env.start_server(pe)
            env.start_client(pe)
            w = writer_bg(ws, "--commits", 100000, "--per", 3, "--seed", 100 + c, "--sleep", rng.choice([0, 2, 5]),
                          env=pe)
            time.sleep(rng.uniform(0.5, 2.5))
            # The cut: SIGKILL, every process on the side(s) at once
            procs = [w]
            if side in ("both", "source"):
                procs.append(env.client)
            if side in ("both", "copy"):
                procs.append(env.server)
            for p in procs:
                p.kill()
            for p in procs:
                p.wait()
            if side in ("both", "source"):
                env.client = None
            if side in ("both", "copy"):
                env.server = None
            if side == "copy":
                env.kill("client")  # The client is rebuilt below with no shim
            if side == "source":
                env.kill("server")
            mix = random.Random(c) if c % mix_every == 1 else None
            st = {}
            if side in ("both", "source"):
                st["ws"] = img.cut(ws, mix)
            if side in ("both", "copy"):
                st["aws"] = img.cut(aws, mix)
            # What the source kept as applied by the copy, the copy must still have
            dp = durable_positions(ws, aws)
            for s, floor, pos in dp:
                assert floor is None or (pos is not None and pos >= floor), \
                    f"cycle {c}: stream {s}: the source's floor is {floor} but the copy has {pos} after the cut"
            st["positions"] = dp
            notes.append(st)
            # Restart on the cut folders (no shim) and catch up
            r = writer(ws, "--commits", 3)
            assert r.returncode == 0, f"cycle {c}: the writer did not recover: {r.stdout}"
            check_db(ws)
            env.start_server()
            env.start_client()
            wait_same(env, "db", 120)
            check_db(aws)
            wait_status(env, lambda s: all(x["lag"] == 0 for x in s.get("sets", {}).get("db", {})
                                           .get("streams", {}).values()) and s["sets"]["db"]["streams"], 30)
    finally:
        env.stop()
    # Show what the cuts kept, and that the floor check had something to compare
    for c, st in enumerate(notes):
        print(f"  {name} cycle {c}: " + "; ".join(
            f"{k} {v['files']} files, {v['empty']} empty, {v['live']} kept unsynced bytes"
            for k, v in st.items() if k != "positions") +
            "; floor/copy " + ", ".join(f"{f}/{p}" for _, f, p in st["positions"]), flush=True)
    assert any(f and p for st in notes for _, f, p in st["positions"]), "no cut had a floor and a copy position"
    return notes


def test_power_loss_shim_model():
    """The shim keeps only what was fsynced: file bytes and folder entries."""
    d = os.path.join(TMP, "plmodel")
    shutil.rmtree(d, ignore_errors=True)
    root = os.path.join(d, "root")
    os.makedirs(root)
    open(os.path.join(root, "old"), "wb").write(b"old")
    os.makedirs(os.path.join(root, "keep"))
    img = PowerImage(os.path.join(d, "image"), [root])
    img.baseline()
    prog = r"""
import os, sys
r = sys.argv[1]
def w(n, b, sync):
    fd = os.open(os.path.join(r, n), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    os.write(fd, b)
    if sync: os.fsync(fd)
    os.close(fd)
def dsync(n):
    fd = os.open(os.path.join(r, n), os.O_RDONLY); os.fsync(fd); os.close(fd)
w("a", b"synced", True)
w("b", b"never synced", False)
dsync(".")                                                       # names a and b
w("a", b"synced, then more", False)                              # the new bytes are lost
os.rename(os.path.join(r, "old"), os.path.join(r, "renamed"))    # undone: no folder sync
w("c", b"synced, no folder sync", True)                          # the name is lost
os.makedirs(os.path.join(r, "sub")); w("sub/x", b"x", True); dsync("sub")   # sub itself is not named
w("keep/d.tmp", b"d", True)                                      # the safe way: sync, rename, sync the folder
os.rename(os.path.join(r, "keep/d.tmp"), os.path.join(r, "keep/d")); dsync("keep")
"""
    r = run([sys.executable, "-c", prog, root], env=dict(os.environ, **img.env()))
    assert r.returncode == 0, r.stdout
    img.cut(root)
    got = {}
    for dp, dn, fn in os.walk(root):
        for n in fn:
            got[os.path.relpath(os.path.join(dp, n), root)] = open(os.path.join(dp, n), "rb").read()
    want = {"a": b"synced", "b": b"", "old": b"old", "keep/d": b"d"}
    assert got == want, got


def purge(root, stream=None):
    args = [os.path.join(BIN, "gxrepl_testwriter"), "purge", root] + ([stream] if stream else [])
    r = run(args)
    assert r.returncode == 0, r.stdout
    return r.stdout


def streams_of(root):
    d = os.path.join(root, ".gxrepl", "log")
    return sorted(n for n in os.listdir(d) if not n.startswith(".")) if os.path.isdir(d) else []


def test_shiplog_purge_after_archive():
    """11/10/2026: osa_ingest's layout: one journal per day folder, the ship log at the set root. A day moved away
    (archived) leaves its stream; the purge removes it once the copy has ACKed all of it, keeps one no copy has
    ACKed or whose writer is open, never touches a live day's stream, and the next writer's open sweeps the rest."""
    env = Env("shiplog_purge", {"db": {"client": ["deletes = yes"]}})
    ws, aws = env.roots["db"]
    arc = os.path.join(env.dir, "archive")
    os.makedirs(arc)
    day = lambda d: os.path.join(ws, d)
    for d in ("2026-11-06", "2026-11-07"):
        os.makedirs(day(d))
        assert writer(day(d), "--commits", 5, "--ship-root", ws).returncode == 0
    s6, s7 = "2026-11-06_writer.jnl", "2026-11-07_writer.jnl"
    assert streams_of(ws) == [s6, s7], streams_of(ws)
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        wait_status(env, lambda s: all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values())
                    and len(s["sets"]["db"]["streams"]) == 2)
        # 1. Archive 11-06 (its folder moves out of the set): its stream goes; the live 11-07 one stays
        os.rename(day("2026-11-06"), os.path.join(arc, "2026-11-06"))
        out = purge(ws, s6)
        assert out.strip() == "removed " + s6, out
        assert purge(ws) == "", "the orphan sweep touched a live stream"
        assert streams_of(ws) == [s7], streams_of(ws)
        # 2. A stream whose writer is open is kept
        w = writer_bg(day("2026-11-07"), "--commits", 100000, "--sleep", 20, "--ship-root", ws)
        time.sleep(1.0)
        assert "its writer is open" in purge(ws, s7)
        w.kill(); w.wait()
        # 3. A day the copy never ACKed (the client is down) is kept, even after its folder moved away
        env.kill("client")
        os.makedirs(day("2026-11-08"))
        assert writer(day("2026-11-08"), "--commits", 3, "--ship-root", ws).returncode == 0
        s8 = "2026-11-08_writer.jnl"
        os.rename(day("2026-11-08"), os.path.join(arc, "2026-11-08"))
        out = purge(ws, s8)
        assert "no copy has acknowledged it yet" in out, out
        assert s8 in streams_of(ws)
        # 4. The copy comes back and applies all of it; the next writer's open (gxdShipLog::Open) sweeps it
        env.start_client()
        wait_status(env, lambda s: s["sets"]["db"]["streams"].get(s8, {}).get("lag", 1) == 0, 60)
        acked = os.path.join(ws, ".gxrepl", "log", s8, "acked")
        end = time.time() + 30
        while time.time() < end and not (os.path.isdir(acked) and os.listdir(acked)):
            time.sleep(0.2)
        os.makedirs(day("2026-11-09"))
        assert writer(day("2026-11-09"), "--commits", 2, "--ship-root", ws).returncode == 0
        assert s8 not in streams_of(ws), streams_of(ws)
        assert streams_of(ws) == [s7, "2026-11-09_writer.jnl"], streams_of(ws)
        wait_same(env, "db", 60)
        check_db(os.path.join(aws, "2026-11-07"))
        check_db(os.path.join(aws, "2026-11-09"))
    finally:
        env.stop()


def test_shiplog_purge_fails_closed():
    """11/10/2026: a stream whose log cannot be read (no log.id, a damaged segment) is kept, never taken as empty;
    dot-named folders in the log folder (.purged-*) are never streams."""
    env = Env("purge_closed", {"db": {"client": ["deletes = yes"]}})
    ws, aws = env.roots["db"]
    arc = os.path.join(env.dir, "archive")
    os.makedirs(arc)
    for d in ("2026-11-01", "2026-11-02", "2026-11-03"):
        os.makedirs(os.path.join(ws, d))
        assert writer(os.path.join(ws, d), "--commits", 3, "--ship-root", ws).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_status(env, lambda s: len(s["sets"]["db"]["streams"]) == 3 and
                    all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values()), 60)
        logd = os.path.join(ws, ".gxrepl", "log")
        for s in ("2026-11-01_writer.jnl", "2026-11-02_writer.jnl"):
            acked = os.path.join(logd, s, "acked")
            end = time.time() + 20
            while time.time() < end and not (os.path.isdir(acked) and os.listdir(acked)):
                time.sleep(0.2)
        env.kill("client")
        for d in ("2026-11-01", "2026-11-02"):
            os.rename(os.path.join(ws, d), os.path.join(arc, d))
        os.remove(os.path.join(logd, "2026-11-01_writer.jnl", "log.id"))
        out = purge(ws, "2026-11-01_writer.jnl")
        assert "kept" in out and "no log.id" in out, out
        seg = [f for f in os.listdir(os.path.join(logd, "2026-11-02_writer.jnl")) if f.endswith(".gxl")][0]
        with open(os.path.join(logd, "2026-11-02_writer.jnl", seg), "r+b") as f:
            f.write(b"\0" * 16)                                       # the segment header damaged
        out = purge(ws, "2026-11-02_writer.jnl")
        assert "kept" in out and "cannot be read" in out, out
        assert sorted(streams_of(ws)) == ["2026-11-01_writer.jnl", "2026-11-02_writer.jnl", "2026-11-03_writer.jnl"]
        # A dot-named copy of a live stream is not a stream
        shutil.copytree(os.path.join(logd, "2026-11-03_writer.jnl"), os.path.join(logd, ".purged-2026-11-03_writer.jnl-1"))
        env.start_client()
        st = wait_status(env, lambda s: s["sets"]["db"]["ok"] and "2026-11-03_writer.jnl" in s["sets"]["db"]["streams"], 60)
        assert not [n for n in st["sets"]["db"]["streams"] if n.startswith(".")], st["sets"]["db"]["streams"]
    finally:
        env.stop()


def test_ship_log_total_cap():
    """11/10/2026: the ship logs of a set are capped together (ship_log_max_total): the oldest segments go, the
    status warns and shows the .gxrepl and ship-log sizes, and the copy still ends the same."""
    cap = 400000
    env = Env("logcap", {"db": {"client": ["ship_log_max_total = %d" % cap]}})
    ws, aws = env.roots["db"]
    for d in ("a", "b"):
        os.makedirs(os.path.join(ws, d))
        assert writer(os.path.join(ws, d), "--commits", 80, "--per", 3, "--segment", 30000,
                      "--ship-root", ws).returncode == 0
    logd = os.path.join(ws, ".gxrepl", "log")
    total = lambda: sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fn in os.walk(logd) for f in fn
                        if f.endswith(".gxl"))
    assert total() > cap, total()
    env.start_server()
    env.start_client()
    try:
        st = wait_status(env, lambda s: s["sets"]["db"].get("warn", "").startswith("WARN"), 60)
        s = st["sets"]["db"]
        assert s["ship_log_bytes"] > 0 and s["gxrepl_bytes"] >= s["ship_log_bytes"], s
        newest = sum(max((os.path.getsize(os.path.join(logd, n, f)) for f in os.listdir(os.path.join(logd, n))
                          if f.endswith(".gxl")), default=0) for n in streams_of(ws))
        assert total() <= cap + newest, (total(), cap, newest)
        wait_same(env, "db", 90)
        check_db(os.path.join(aws, "a"))
        check_db(os.path.join(aws, "b"))
    finally:
        env.stop()


def test_writer_lock_refuses_changes():
    """11/10/2026 (review): while a local writer holds a set's writer_lock on the copy (this node took over), gxrepl
    changes nothing there (the pass is refused with "a local writer holds"); once the writer lets go the copy
    catches up. gxrepl holds the locks shared for the whole of each change, so a writer starting meanwhile waits."""
    import fcntl
    env = Env("wlock", {"db": {"client": ["writer_lock = w.lock"], "server": ["writer_lock = w.lock"]}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        # 11/11/2026 (review): the hold makes a lock file that did not exist yet (no writer has run on the copy)
        assert os.path.exists(os.path.join(aws, "w.lock"))
        before = {n: open(os.path.join(aws, n), "rb").read() for n in os.listdir(aws) if n.endswith(".gxd")}
        fd = os.open(os.path.join(aws, "w.lock"), os.O_RDWR | os.O_CREAT, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX)
        try:
            assert writer(ws, "--commits", 4, "--seed", 51).returncode == 0
            st = wait_status(env, lambda s: "a local writer holds" in (s["sets"]["db"].get("last_error") or ""), 60)
            after = {n: open(os.path.join(aws, n), "rb").read() for n in os.listdir(aws) if n.endswith(".gxd")}
            assert after == before, "the copy changed while its writer held the lock"
            print(f"    refused while held: {st['sets']['db']['last_error'][:90]}", flush=True)
        finally:
            fcntl.flock(fd, fcntl.LOCK_UN)
            os.close(fd)
        wait_same(env, "db", 90)
        check_db(aws)
    finally:
        env.stop()


def test_commit_block_copy_matches_one_commit():
    """11/10/2026: a live folder whose writer commits in place WITHOUT the journal write lock (no ship log), with a
    seqlock commit block (commit_block = commit.bin@8). Every copy gxrepl accepts is one whole commit: the files and
    commit.bin agree (gxrepl_testwriter check). A commit.bin changed in place with its size and mtime kept (as an
    mmap'd block does) is still found, by its generation."""
    opts = {"client": ["commit_block = commit.bin@8", "include = commit.bin", "verify_interval = 0",
                       "full_verify_interval = 0"],
            "server": ["commit_block = commit.bin@8", "include = commit.bin"]}
    env = Env("commit_block", {"db": opts})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    try:
        w = writer_bg(ws, "--commits", 100000, "--per", 2, "--sleep", 1000, "--no-ship", "--commit-block", "--seed", 3)
        good = 0
        for i in range(10):
            r = env.once()
            if r.returncode == 0:
                c = run([os.path.join(BIN, "gxrepl_testwriter"), "check", aws])
                assert c.returncode == 0, f"pass {i}: an accepted copy is not one commit: {c.stdout}"
                good += 1
        w.kill(); w.wait()
        assert good >= 3, f"only {good} of 10 passes ended in step"
        assert env.once().returncode == 0
        check_db(aws)
        # In place, size and mtime kept: found by the generation
        env.start_client()
        wait_status(env, lambda s: s["sets"]["db"]["ok"], 30)
        cb = os.path.join(ws, "commit.bin")
        st0 = os.stat(cb)
        with open(cb, "r+b") as f:
            f.seek(8)
            g = int.from_bytes(f.read(8), "little")
            f.seek(8)
            f.write((g + 2).to_bytes(8, "little"))
        os.utime(cb, ns=(st0.st_atime_ns, st0.st_mtime_ns))
        assert os.stat(cb).st_mtime_ns == st0.st_mtime_ns and os.stat(cb).st_size == st0.st_size
        want = open(cb, "rb").read()
        end = time.time() + 30
        while time.time() < end and open(os.path.join(aws, "commit.bin"), "rb").read() != want:
            time.sleep(0.3)
        assert open(os.path.join(aws, "commit.bin"), "rb").read() == want, "the in-place commit.bin change was missed"
    finally:
        env.stop()


def drop_cache(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.posix_fadvise(fd, 0, 0, os.POSIX_FADV_DONTNEED)
    finally:
        os.close(fd)


def test_verify_does_not_stall_the_writer():
    """11/10/2026: a full verify of a large live database (ship log on, so commits take the journal write lock)
    reads it in batches of at most 4 MB / 100 ms under the read lock, so the writer's commits wait well under 1 s,
    with a cold cache and other reads loading the disk."""
    big = 256 * 1024 * 1024
    env = Env("stall", {"db": {"client": ["verify_interval = 0", "full_verify_interval = 0"]}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 2, "--big", big).returncode == 0
    env.start_server()
    env.start_client(extra_env={k: os.environ[k] for k in ("GXREPL_TEST_WHOLE_FILE_LOCK",) if k in os.environ})
    load = None
    try:
        wait_same(env, "db", 300)
        loadfile = os.path.join(env.dir, "load.bin")
        with open(loadfile, "wb") as f:
            for _ in range(256):
                f.write(os.urandom(1024 * 1024))
        for pth in (os.path.join(ws, "data.gxd"), os.path.join(aws, "data.gxd"), loadfile):
            drop_cache(pth)
        load = subprocess.Popen(["bash", "-c", f"while :; do dd if={loadfile} of=/dev/null bs=1M iflag=direct status=none || sleep 1; done"])
        # 11/10/2026: CPU load as well (the review saw 16.8 s at load 35): GXREPL_TEST_CPU_LOAD busy processes
        cpu = [subprocess.Popen(["bash", "-c", "while :; do :; done"])
               for _ in range(int(os.environ.get("GXREPL_TEST_CPU_LOAD", "0")))]
        lat = os.path.join(env.dir, "latency.txt")
        rchar = lambda: int([ln.split()[1] for ln in open(f"/proc/{env.client.pid}/io") if ln.startswith("rchar")][0])
        # A commit every 0.4 s, like osa_ingest's bursts: the client drains the log between commits (a pass drains
        # first, so a writer that never pauses defers verifies until it does) and the verify runs meanwhile
        w = writer_bg(ws, "--commits", 100000, "--per", 30, "--sleep", 400, "--latency-out", lat, "--seed", 9)
        w_start = time.time()
        time.sleep(2.0)                                            # past the writer's own open-record verify
        for pth in (os.path.join(ws, "data.gxd"), os.path.join(aws, "data.gxd")):
            drop_cache(pth)
        r0 = rchar()
        env.client.send_signal(signal.SIGHUP)                      # a full verify while it commits
        # The writer runs until the verify has read most of the file (under heavy load that takes longer)
        end = time.time() + 600
        while time.time() < end and (rchar() - r0) < 0.9 * big and w.poll() is None:
            time.sleep(0.5)
        time.sleep(2)
        w.kill(); w.wait()
        w_end = time.time()
        read_mb = (rchar() - r0) / 1048576
        worst, n = open(lat).read().split()
        worst = float(worst)
        # The verify must have read the file while the writer committed (else the test proves nothing):
        # the client is the source here, so its reads are the verify's
        print(f"    writer: {n} commits in {w_end - w_start:.0f} s while a full verify read {read_mb:.0f} MB of a "
              f"{big // 1048576} MB live file (cold cache, disk load); slowest commit {worst:.0f} ms", flush=True)
        assert read_mb >= 0.8 * big / 1048576, f"the verify read only {read_mb:.0f} MB during the writer's run"
        assert worst < 1000, f"a commit waited {worst:.0f} ms"
        wait_same(env, "db", 300)
        check_db(aws)
    finally:
        if load:
            load.kill(); load.wait()
        for c in locals().get("cpu", []) or []:
            c.kill(); c.wait()
        env.stop()


def test_server_does_not_leak_descriptors():
    """11/10/2026: read_states() never closed the state folder, so each State() call leaked a descriptor (a large
    copy in parts ran the server out of them: "cannot lock apply.jnl"). The count stays flat over many passes."""
    env = Env("fdleak", {"db": {}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        fds = lambda: len(os.listdir(f"/proc/{env.server.pid}/fd"))
        n0 = fds()
        for i in range(20):
            assert writer(ws, "--commits", 2, "--seed", 50 + i).returncode == 0
            time.sleep(0.3)
        wait_same(env, "db", 60)
        n1 = fds()
        assert n1 <= n0 + 5, f"server descriptors {n0} -> {n1}"
    finally:
        env.stop()


def test_big_record_resyncs():
    """11/10/2026: a ship-log record bigger than max_message (an osa_ingest rebuild writes one of 500 MB) is never
    sent: the copy's position is reset, its files are resynced in pieces, and the records after it drain as usual."""
    env = Env("bigrec", {"db": {}})                      # max_message = 262144 in these tests
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        assert writer(ws, "--commits", 1, "--big", 1500000).returncode == 0     # one ~1.5 MB record
        assert writer(ws, "--commits", 6, "--seed", 77).returncode == 0
        wait_same(env, "db", 90)
        check_db(aws)
        st = wait_status(env, lambda s: s["sets"]["db"]["ok"] and
                         all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values()), 60)
        log = open(os.path.join(env.dir, "client.log")).read()
        assert "bigger than max_message" in log, log[-2000:]
        assert log.count("bigger than max_message") <= 3, "a resync loop: " + log[-2000:]
        n0 = st["sets"]["db"]["records"]
        assert writer(ws, "--commits", 4, "--seed", 78).returncode == 0          # and the log goes on
        wait_same(env, "db", 60)
        st = wait_status(env, lambda s: s["sets"]["db"]["records"] > n0, 30)
    finally:
        env.stop()


def test_big_compressed_record_counts_inflated():
    """11/10/2026 (review): a compressed record counts against max_message by its inflated size: one that is
    small on disk but inflates past max_message is never sent (the copy resyncs its files), so a receiver never
    inflates more than max_message; smaller compressed records still drain."""
    env = Env("bigz", {"db": {}})                        # max_message = 262144 in these tests
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        assert writer(ws, "--commits", 1, "--big-zero", 1500000).returncode == 0   # ~1.5 MB, a few KB stored
        r = run([os.path.join(BIN, "gxrepl"), "shiplog", os.path.join(ws, ".gxrepl", "log", "writer.jnl")])
        assert "commit(z)" in r.stdout, r.stdout[-1500:]
        assert writer(ws, "--commits", 6, "--seed", 79).returncode == 0
        wait_same(env, "db", 90)
        check_db(aws)
        wait_status(env, lambda s: s["sets"]["db"]["ok"] and
                    all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values()), 60)
        log = open(os.path.join(env.dir, "client.log")).read()
        print(f"    {log.count('bigger than max_message')} resync(s) for the record that inflates past max_message", flush=True)
        assert "bigger than max_message" in log, log[-2000:]
    finally:
        env.stop()


def torn(env):
    return run([os.path.join(BIN, "gxrepl"), "torn", "--config", env.srv_conf])


CB_OPTS = {"client": ["commit_block = commit.bin@8", "include = commit.bin", "verify_interval = 0",
                      "full_verify_interval = 0"],
           "server": ["commit_block = commit.bin@8", "include = commit.bin"]}
# 11/12/2026 (F7): the in-place copy (before F7, and F7's fallback), for the tests of its torn marking
CB_INPLACE = {k: v + ["staged_fill = no"] for k, v in CB_OPTS.items()}


def gen_of(path):
    with open(path, "rb") as f:
        f.seek(8)
        return int.from_bytes(f.read(8), "little")


def test_commit_block_marks_torn_copies():
    """11/10/2026: the copy's commit block goes odd before a unit's files change, so a copy that could not finish
    (the writer commits faster than a copy) is torn for "gxrepl torn" (exit 1, a fail over must not promote it);
    once the writer pauses the copy is whole again (exit 0). 11/12 (F7): the in-place copy (staged_fill = no, and
    F7's fallback); the staged copy is test_staged_fill_writer_faster_than_the_copy."""
    _marks_torn(staged=False)


def test_staged_fill_writer_faster_than_the_copy():
    """11/12/2026 (F7): a copy that cannot finish (the writer commits faster) leaves the copy whole at its last
    sealed commit: promote-check exit 0 (a STAGED warning), the copy's database checks out; once the writer pauses
    the copy reaches the source's commit."""
    _marks_torn(staged=True)


def _marks_torn(staged):
    env = Env("cb_torn_staged" if staged else "cb_torn", {"db": CB_OPTS if staged else CB_INPLACE})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        r = torn(env)
        assert r.returncode == 0 and "0 torn" in r.stdout, r.stdout
        w = writer_bg(ws, "--commits", 100000, "--per", 1, "--no-ship", "--commit-block", "--seed", 5)
        time.sleep(0.5)
        r = env.once()                                   # every try sees a new generation: gives up
        w.kill(); w.wait()
        if r.returncode != 0:
            t = torn(env)
            if staged:
                assert t.returncode == 0 and "0 torn" in t.stdout, t.stdout
                print(f"    gave up mid-copy: torn exit 0, {'a stage kept' if 'STAGED' in t.stdout else 'no stage'}",
                      flush=True)
                check_db(aws)                            # whole at its last sealed commit
            else:
                assert t.returncode == 1 and "TORN" in t.stdout and "odd" in t.stdout, t.stdout
        # The kill may have left the SOURCE in a batch (an odd generation): no copy is accepted until its
        # writer opens again and recovers (as OSA's writers do), so the copy stays torn meanwhile
        assert writer(ws, "--commits", 1, "--no-ship", "--commit-block", "--seed", 8).returncode == 0
        r = env.once()
        assert r.returncode == 0, r.stdout
        t = torn(env)
        assert t.returncode == 0 and "0 torn" in t.stdout, t.stdout
        check_db(aws)
    finally:
        env.stop()


def test_commit_block_heartbeat_is_not_a_change():
    """11/10/2026: a writer's heartbeat rewrites commit.bin (bytes after the generation, and its mtime) without a
    commit: the folder is not read again."""
    env = Env("cb_beat", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 20, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        time.sleep(3)
        st0 = env.status()["sets"]["db"]
        for i in range(4):
            with open(os.path.join(ws, "commit.bin"), "r+b") as f:
                f.seek(40)
                f.write(os.urandom(16))                  # a heartbeat
            time.sleep(1.2)
        time.sleep(3)
        st1 = env.status()["sets"]["db"]
        assert st1["files_sent"] == st0["files_sent"] and st1["pieces_sent"] == st0["pieces_sent"], (st0, st1)
    finally:
        env.stop()


def gen_log_map(path):
    out = {}
    try:
        for line in open(path):
            g, hd, hi = line.split()
            out[int(g)] = (hd, hi)
    except OSError:
        pass
    return out


def copy_verdict(aws, log):
    """The copy now: "whole" (even, its content the writer's at that generation), "torn" (even, other content),
    "odd" (marked in progress), or None (it moved while read, or a generation the log does not have)."""
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "hashes", aws]).stdout.split()
    if len(r) != 3:
        return None
    g = int(r[0])
    if g & 1:
        return "odd"
    if g not in log:
        return None
    return "whole" if (r[1], r[2]) == log[g] else "torn"


def sample_copy(aws, gen_log, seconds, period=0.2):
    counts = {"whole": 0, "torn": 0, "odd": 0, None: 0}
    wholes = set()
    end = time.time() + seconds
    while time.time() < end:
        log = gen_log_map(gen_log)
        v = copy_verdict(aws, log)
        counts[v] += 1
        if v == "whole":
            wholes.add(run([os.path.join(BIN, "gxrepl_testwriter"), "hashes", aws]).stdout.split()[0])
        time.sleep(period)
    return counts, wholes


def judge_copy(env, aws, gl):
    """The copy's verdict by content and "gxrepl torn", read under a shared lock on the copy's apply.jnl (as a
    reader of the copy does): an apply the server is still finishing for a killed client is never read half way."""
    import fcntl
    jp = os.path.join(aws, ".gxrepl", "apply.jnl")
    fd = os.open(jp, os.O_RDWR | os.O_CREAT, 0o600) if os.path.isdir(os.path.dirname(jp)) else None
    try:
        if fd is not None:
            fcntl.flock(fd, fcntl.LOCK_SH)
        v = copy_verdict(aws, gen_log_map(gl)) if os.path.exists(os.path.join(aws, "data.gxd")) else "empty"
        return v, torn(env)
    finally:
        if fd is not None:
            os.close(fd)


def test_commit_block_ship_log_copy_is_sealed():
    """11/10/2026: a commit_block folder fed by the ship log: each commit record carries the commit block
    (gxdShipLog::SetCommitBlock), the copy applies data, commit block and its seal "log G L" as one batch, and
    "gxrepl torn" passes at every record; the plain scan never writes commit.bin; a commit block changed after
    its seal is caught."""
    env = Env("cb_log", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3, "--commit-block").returncode == 0
    env.start_server()
    seal = os.path.join(aws, ".gxrepl", "units", "_root")
    try:
        env.start_client()
        wait_same(env, "db", 120)
        assert writer(ws, "--commits", 5, "--commit-block", "--seed", 41).returncode == 0
        wait_same(env, "db", 120)
        t = torn(env)
        s = open(seal).readline().split()
        g = gen_of(os.path.join(aws, "commit.bin"))
        print(f"    after 5 shipped commits: torn exit {t.returncode}, seal {s}, copy generation {g}", flush=True)
        assert t.returncode == 0, t.stdout
        assert s[0] == "log" and int(s[1]) == g, (s, g)
        env.stop()
        with open(os.path.join(aws, "commit.bin"), "r+b") as f:     # a commit block that is not the sealed one
            f.seek(8)
            f.write((g + 2).to_bytes(8, "little"))
        t = torn(env)
        assert t.returncode == 1 and "sealed by ship log record" in t.stdout, t.stdout
    finally:
        env.stop()


def test_commit_block_records_without_block_are_torn():
    """11/10/2026 (review r4c): a sealed ship-log copy then gets records WITHOUT the commit block (an older writer, or
    another journal writer of the folder): its data moves past the sealed generation, so the unit is "copying-log"
    and "gxrepl torn" fails, until a record carries the commit block again. A seal's lsn must equal the stream's
    applied lsn: a copy whose stream position is past its seal fails too."""
    env = Env("cb_nocb", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3, "--commit-block").returncode == 0
    env.start_server()
    seal = os.path.join(aws, ".gxrepl", "units", "_root")
    try:
        env.start_client()
        wait_same(env, "db", 120)
        assert writer(ws, "--commits", 2, "--commit-block", "--seed", 61).returncode == 0
        wait_same(env, "db", 120)
        t = torn(env)
        assert t.returncode == 0, t.stdout
        s = open(seal).read().split()
        assert s[0] == "log" and len(s) == 5, s   # 11/14/2026: "log G L S C" (seal per folder)
        # records without the commit block (the old format)
        assert writer(ws, "--commits", 3, "--commit-block", "--old-ship-format", "--seed", 62).returncode == 0
        wait_status(env, lambda st: st["sets"]["db"]["streams"] and
                    all(x["lag"] == 0 for x in st["sets"]["db"]["streams"].values()), 60)
        assert torn(env).returncode == 1                       # torn from here on (during the reopen's verify too)
        end = time.time() + 30
        while time.time() < end and not open(seal).read().startswith("copying-log"):
            assert torn(env).returncode == 1
            time.sleep(0.2)
        t = torn(env)
        print(f"    records without the commit block: torn exit {t.returncode}: {t.stdout.strip()[:300]!r}", flush=True)
        assert t.returncode == 1 and "without its commit block" in t.stdout, t.stdout
        # a record that carries it again: whole and sealed at its lsn
        assert writer(ws, "--commits", 2, "--commit-block", "--seed", 63).returncode == 0
        wait_same(env, "db", 120)
        t = torn(env)
        assert t.returncode == 0, t.stdout
        s = open(seal).read().split()
        env.stop()
        stf = os.path.join(aws, ".gxrepl", "state", s[3] + ".state")
        body = open(stf).read().split()
        assert int(body[1]) == int(s[2]), (body, s)
        with open(stf, "w") as f:                                    # the stream past its seal
            f.write(f"{body[0]} {int(body[1]) + 1}\n")
        t = torn(env)
        print(f"    stream past the seal: torn exit {t.returncode}: {t.stdout.strip()[:150]}", flush=True)
        assert t.returncode == 1 and "records past the seal" in t.stdout, t.stdout
    finally:
        env.stop()


def test_commit_block_old_ship_format_is_unsealed():
    """11/10/2026: records from a writer that does not ship its commit block (the old format) still apply, but the
    copy never gets a commit block from the scan (it would race the records): "gxrepl torn" reports it unsealed."""
    env = Env("cb_old", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3, "--commit-block", "--old-ship-format").returncode == 0
    env.start_server()
    try:
        env.start_client()
        wait_same_files = time.time() + 60
        while time.time() < wait_same_files and not os.path.exists(os.path.join(aws, "data.gxd")):
            time.sleep(0.5)
        assert writer(ws, "--commits", 4, "--commit-block", "--old-ship-format", "--seed", 42).returncode == 0
        st = wait_status(env, lambda s: s["sets"]["db"]["streams"] and
                         all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values()), 60)
        t = torn(env)
        print(f"    old format: torn exit {t.returncode}: {t.stdout.strip()[:160]}", flush=True)
        assert t.returncode == 1 and ("UNSEALED" in t.stdout or "never sealed" in t.stdout), t.stdout
    finally:
        env.stop()


def test_commit_block_ship_log_kill_storm():
    """11/10/2026 (review): 60 random kill -9 of the client while it drains the ship log of a writer committing
    about 6 times a second: by content (the writer's gen log), every even copy is one whole commit, and "gxrepl
    torn" refuses every copy that is not."""
    env = Env("cb_log_kills", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3, "--commit-block", "--big", 4 * 1024 * 1024).returncode == 0
    env.start_server()
    gl = os.path.join(env.dir, "gen.log")
    rng = random.Random(61)
    try:
        env.start_client()
        wait_same(env, "db", 120)
        env.kill("client")
        w = writer_bg(ws, "--commits", 100000, "--per", 3, "--sleep", 150, "--commit-block", "--seed", 8,
                      "--gen-log", gl)
        stats = {"whole": 0, "refused_whole": 0, "caught": 0, "missed": 0, "unknown": 0}
        for i in range(60):
            env.start_client()
            time.sleep(rng.uniform(0.05, 1.2))
            env.kill("client")
            v, tr = judge_copy(env, aws, gl)
            if v == "whole":
                stats["whole"] += 1
                if tr.returncode != 0:
                    stats["refused_whole"] += 1
            elif v in ("torn", "odd"):
                stats["caught" if tr.returncode == 1 else "missed"] += 1
            else:
                stats["unknown"] += 1
        print(f"    60 kills mid-drain: {stats['whole']} whole ({stats['refused_whole']} refused), {stats['caught']} "
              f"not whole and refused, {stats['missed']} not whole and PASSED, {stats['unknown']} unknown", flush=True)
        assert stats["missed"] == 0, stats
        assert stats["whole"] >= 30, stats
        # and 20 kills while a copy is filled from nothing (the stream verify, in pieces): half copies happen
        fill = {"whole": 0, "refused_whole": 0, "caught": 0, "missed": 0, "unknown": 0}
        for i in range(20):
            for n in os.listdir(aws):
                p2 = os.path.join(aws, n)
                shutil.rmtree(p2) if os.path.isdir(p2) else os.unlink(p2)
            env.start_client()
            time.sleep(rng.uniform(0.05, 0.8))
            env.kill("client")
            v, tr = judge_copy(env, aws, gl)
            if v == "empty":
                fill["unknown"] += 1
            elif v == "whole":
                fill["whole"] += 1
                if tr.returncode != 0:
                    fill["refused_whole"] += 1
            elif v in ("torn", "odd") or (v is None and os.path.exists(os.path.join(aws, "data.gxd"))):
                fill["caught" if tr.returncode == 1 else "missed"] += 1
                if tr.returncode != 1 and fill["missed"] <= 3:
                    hs = run([os.path.join(BIN, "gxrepl_testwriter"), "hashes", aws]).stdout.strip()
                    sp = os.path.join(aws, ".gxrepl", "units", "_root")
                    print(f"      missed: verdict {v}, hashes '{hs}', seal '{open(sp).read().strip() if os.path.exists(sp) else None}'"
                          f", torn: {tr.stdout.strip()[:200]}", flush=True)
            else:
                fill["unknown"] += 1
        w.kill(); w.wait()
        print(f"    20 kills mid-fill: {fill['whole']} whole ({fill['refused_whole']} refused), {fill['caught']} "
              f"not whole and refused, {fill['missed']} not whole and PASSED, {fill['unknown']} empty", flush=True)
        assert fill["missed"] == 0, fill
    finally:
        env.stop()


def test_commit_block_no_even_torn_copy():
    """11/10/2026 (review): sampled by content (the writer logs each commit's file hashes), a commit_block copy
    of a writer committing every 0.7 s (commits land mid-copy) is never even and torn: 121 samples, 0
    even-but-torn, and at least 10 of them even (so the even case is tested)."""
    env = Env("cb_samples", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    env.start_client()
    gl = os.path.join(env.dir, "gen.log")
    try:
        wait_same(env, "db", 120)
        w = writer_bg(ws, "--commits", 100000, "--per", 2, "--sleep", 700, "--no-ship", "--commit-block",
                      "--seed", 6, "--gen-log", gl)
        counts = {"whole": 0, "torn": 0, "odd": 0, None: 0}
        n = 0
        end = time.time() + 120
        while n < 121 and time.time() < end:
            v = copy_verdict(aws, gen_log_map(gl))
            if v is not None:
                counts[v] += 1
                n += 1
            time.sleep(0.15)
        w.kill(); w.wait()
        print(f"    8 MB, a commit every 0.7 s: {n} samples by content: {counts['whole']} whole, {counts['odd']} marked "
              f"in progress, {counts['torn']} even but torn", flush=True)
        assert n >= 121, n
        assert counts["torn"] == 0, counts
        assert counts["whole"] >= 10, counts  # the case under test: even copies of a busy writer
    finally:
        env.stop()


def test_commit_block_kill_storm():
    """11/10/2026 (review): 60 random kill -9 of the client while it copies units of a writer committing every
    0.7 s: every copy that is not whole by content fails "gxrepl torn" (promote-check), no whole copy does,
    and one byte changed under an even seal fails it too. 11/12 (F7): the in-place copy (staged_fill = no)."""
    _kill_storm(staged=False)


def test_staged_fill_kill_storm():
    """11/12/2026 (F7): the same 60 kills with staged copies: no torn copy passes, no whole copy is refused, and
    the kills leave the copy whole (almost never torn: only a kill that lands inside the server's swap)."""
    _kill_storm(staged=True)


def _kill_storm(staged):
    env = Env("cb_kills_staged" if staged else "cb_kills", {"db": CB_OPTS if staged else CB_INPLACE})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 2, "--big", 4 * 1024 * 1024, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    gl = os.path.join(env.dir, "gen.log")
    rng = random.Random(60)
    try:
        assert env.once().returncode == 0
        w = writer_bg(ws, "--commits", 100000, "--per", 2, "--sleep", 700, "--no-ship", "--commit-block",
                      "--seed", 7, "--gen-log", gl)
        stats = {"whole": 0, "refused_whole": 0, "caught": 0, "missed": 0, "unknown": 0}
        for i in range(60):
            env.start_client()
            time.sleep(rng.uniform(0.05, 1.5))
            env.kill("client")
            if staged:
                # 11/13/2026: the server may still be swapping in a stage the killed client finished: the verdict
                # and promote-check are read under a shared lock on apply.jnl (judge_copy, as a locked reader does),
                # never one before and one after that swap
                v, tr = judge_copy(env, aws, gl)
            else:
                v = copy_verdict(aws, gen_log_map(gl))
                tr = torn(env)
            if v in ("torn", "odd"):
                if tr.returncode == 1:
                    stats["caught"] += 1
                else:
                    stats["missed"] += 1
            elif v == "whole":
                stats["whole"] += 1
                if tr.returncode != 0:
                    stats["refused_whole"] += 1
            else:
                stats["unknown"] += 1
        w.kill(); w.wait()
        print(f"    60 kills: {stats['whole']} whole ({stats['refused_whole']} refused), {stats['caught']} torn and "
              f"refused, {stats['missed']} torn and PASSED promote-check, {stats['unknown']} unknown", flush=True)
        assert stats["missed"] == 0, stats
        assert stats["refused_whole"] == 0, stats
        if staged:
            assert stats["whole"] >= 50, stats          # F7: a kill leaves the last sealed commit, whole
        else:
            assert stats["whole"] >= 5 and stats["caught"] >= 5, stats  # both cases happened
        # 11/10/2026 (review): 20 kills while the copy is filled from nothing (a first copy of the unit)
        fill = {"whole": 0, "refused_whole": 0, "caught": 0, "missed": 0, "empty": 0}
        w = writer_bg(ws, "--commits", 100000, "--per", 2, "--sleep", 700, "--no-ship", "--commit-block",
                      "--seed", 9, "--gen-log", gl)
        for i in range(20):
            for n in os.listdir(aws):
                p2 = os.path.join(aws, n)
                shutil.rmtree(p2) if os.path.isdir(p2) else os.unlink(p2)
            env.start_client()
            time.sleep(rng.uniform(0.05, 1.5))
            env.kill("client")
            v, tr = judge_copy(env, aws, gl)
            if v == "empty":
                fill["empty"] += 1
            elif v == "whole":
                fill["whole"] += 1
                if tr.returncode != 0:
                    fill["refused_whole"] += 1
            elif v in ("torn", "odd"):
                fill["caught" if tr.returncode == 1 else "missed"] += 1
            else:
                fill["empty"] += 1
        w.kill(); w.wait()
        print(f"    20 kills mid-fill: {fill['whole']} whole ({fill['refused_whole']} refused), {fill['caught']} not whole "
              f"and refused, {fill['missed']} not whole and PASSED, {fill['empty']} empty or unknown", flush=True)
        assert fill["missed"] == 0 and fill["refused_whole"] == 0, fill
        # an even copy whose bytes differ from its seal (what a torn even copy would be) is refused
        env.start_client()
        wait_same(env, "db", 120)
        env.stop()
        assert torn(env).returncode == 0
        dn = [f for f in sorted(os.listdir(aws)) if f.endswith(".gxd")][0]
        with open(os.path.join(aws, dn), "r+b") as f:
            f.seek(4096)
            b = f.read(1)
            f.seek(4096)
            f.write(bytes([b[0] ^ 0xFF]))
        tr = torn(env)
        print(f"    one byte changed under an even seal: torn exit {tr.returncode}: {tr.stdout.strip()[:120]}", flush=True)
        assert tr.returncode == 1, tr
    finally:
        env.stop()


def test_commit_block_converges_slow_writer():
    """11/10/2026: by content, a copy keeps reaching whole commits of a writer that commits every 2 s (8 MB);
    frequent writers belong on the ship log (the review measured 5 commits/s: about 1 whole commit in 25 s)."""
    env = Env("cb_conv", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    env.start_client()
    gl = os.path.join(env.dir, "gen.log")
    try:
        wait_same(env, "db", 120)
        w = writer_bg(ws, "--commits", 20, "--per", 2, "--sleep", 2000, "--no-ship", "--commit-block", "--seed", 6,
                      "--gen-log", gl)
        counts, wholes = sample_copy(aws, gl, 40)
        w.wait(120)
        print(f"    8 MB, a commit every 2 s, by content: {len(wholes)} distinct whole commits in 40 s "
              f"({counts['torn']} even but torn)", flush=True)
        assert counts["torn"] == 0, counts
        assert len(wholes) >= 4, wholes
    finally:
        env.stop()


def test_purge_unreadable_ack_and_trim():
    """11/10/2026: an acked/<copy> file that cannot be read keeps the stream even when another copy ACKed it all;
    a writer that opens a stream whose one segment every copy has applied starts a new segment (trim)."""
    env = Env("purge_minor", {"db": {}})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d1"))
    assert writer(os.path.join(ws, "d1"), "--commits", 3, "--ship-root", ws).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_status(env, lambda s: all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values())
                    and s["sets"]["db"]["streams"], 60)
        s1 = os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl")
        acked = os.path.join(s1, "acked")
        end = time.time() + 20
        while time.time() < end and not (os.path.isdir(acked) and os.listdir(acked)):
            time.sleep(0.2)
        segs0 = sorted(f for f in os.listdir(s1) if f.endswith(".gxl"))
        assert writer(os.path.join(ws, "d1"), "--commits", 1, "--ship-root", ws, "--seed", 9).returncode == 0
        segs1 = sorted(f for f in os.listdir(s1) if f.endswith(".gxl"))
        assert len(segs1) == len(segs0) + 1, (segs0, segs1)      # rolled: the acked one can go by retention
        env.kill("client")
        open(os.path.join(s1, "acked", "other-copy"), "w").write("garbage\n")
        os.rename(os.path.join(ws, "d1"), os.path.join(env.dir, "d1_gone"))
        out = purge(ws, "d1_writer.jnl")
        assert "kept" in out and "cannot be read" in out, out
    finally:
        env.stop()


def test_version_names_the_build():
    """11/10/2026: "gxrepl version" names the protocol (gxrepl_build.sh checks it), the git head of the DataReel
    sources (+dirty, or unknown outside a git checkout) and the build time (or unknown)."""
    import re as _re
    r = run([os.path.join(BIN, "gxrepl"), "version"])
    assert r.returncode == 0, r.stdout
    m = _re.match(r"gxrepl \(DataReel [\d.]+\), protocol 3, default port 2073, zlib (yes|no), "
                  r"git ([0-9a-f]{7,40}(\+dirty)?|unknown), built (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d UTC|unknown)"
                  r"(, test hooks)?$",
                  r.stdout.strip())
    assert m, r.stdout


def test_compressed_log_records():
    """11/10/2026: commit records are zlib-compressed (type 4) when smaller; a log holding both kinds (older
    uncompressed records, GXDSHIP_COMPRESS=0) replicates whole."""
    env = Env("zlog", {"db": {}})
    ws, aws = env.roots["db"]
    e0 = dict(os.environ, GXDSHIP_COMPRESS="0")
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "write", ws, "--commits", "10", "--per", "4"], env=e0)
    assert r.returncode == 0, r.stdout
    assert writer(ws, "--commits", 10, "--per", 4, "--seed", 12).returncode == 0      # compressed
    dump = run([os.path.join(BIN, "gxrepl"), "shiplog", os.path.join(ws, ".gxrepl", "log", "writer.jnl")]).stdout
    plain = [int(l.split()[4]) for l in dump.splitlines() if " commit " in l]
    packed = [int(l.split()[4]) for l in dump.splitlines() if " commit(z) " in l]
    assert plain and packed, dump[-1500:]
    print(f"    commit records: {len(plain)} plain, mean {sum(plain) // len(plain)} bytes; {len(packed)} compressed, "
          f"mean {sum(packed) // len(packed)} bytes", flush=True)
    # (the test writer's objects are random bytes: a record is stored compressed only when that is smaller than
    # its own raw form; the size win is measured on osa_ingest's real pages, not here)
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        check_db(aws)
        assert writer(ws, "--commits", 5, "--seed", 13).returncode == 0
        wait_same(env, "db", 60)
        check_db(aws)
    finally:
        env.stop()


def test_derived_busy_writer_never_quiet():
    """11/11/2026 (r4e): a writer that never pauses (a commit every 4 s, ship log on, the realtime case): the copy's
    derived index never goes quiet for derived_delay, so derived_busy_rebuild rebuilds it anyway under the apply lock;
    for the whole run "gxrepl torn" exits 0 (the index is at worst a warning), the copy keeps up, and after the writer
    stops the index ends rebuilt at the copy's generation."""
    rebuild = f"{os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}"
    opts = {"client": CB_OPTS["client"] + ["derived = index.gxd"],
            "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 30",
                                           "derived_busy_rebuild = 40"]}
    env = Env("derived_busy", {"db": opts})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3, "--commit-block").returncode == 0
    env.start_server()
    env.start_client()
    secs = int(os.environ.get("GXREPL_BUSY_SECS", "300"))
    try:
        wait_status(env, lambda s: s["sets"]["db"]["streams"] and
                    all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values()), 120)
        w = writer_bg(ws, "--commits", 100000, "--per", 2, "--sleep", 4000, "--commit-block", "--seed", 71)
        rc1, warned, checks, lags = 0, 0, 0, []
        end = time.time() + secs
        while time.time() < end:
            t = torn(env)
            checks += 1
            if t.returncode != 0:
                rc1 += 1
                print(f"      rc {t.returncode}: {t.stdout.strip()[:400]!r}", flush=True)
            if "INDEXES" in t.stdout:
                warned += 1
            st = env.status() or {}
            lags.append(max([x["lag"] for x in st.get("sets", {}).get("db", {}).get("streams", {}).values()] or [0]))
            time.sleep(5)
        w.kill(); w.wait()
        log = open(os.path.join(env.dir, "server.log")).read()
        busy = log.count("(busy: never quiet)")
        print(f"    {secs} s of a commit every 4 s: {checks} promote-checks, {rc1} rc 1, {warned} INDEXES warnings, "
              f"{busy} busy rebuilds, worst lag {max(lags)} records", flush=True)
        assert rc1 == 0, rc1
        assert busy >= 2, busy
        # quiet now: the index ends rebuilt at the copy's generation
        end = time.time() + 120
        while time.time() < end and "INDEXES" in torn(env).stdout:
            time.sleep(1)
        t = torn(env)
        assert t.returncode == 0 and "INDEXES" not in t.stdout, t.stdout
    finally:
        env.stop()


def test_slow_rebuild_never_stalls_the_other_sets():
    """11/12/2026 (F4, the AWS incident): a derived index rebuild that takes minutes holds the copy's apply lock. An
    apply of that set used to block in flock while it held the set's mutex, the other requests piled up behind it and
    the multiplexed connection stopped every set ("no reply in 120 s", "bad reply"). Now: the other set keeps copying
    during the rebuild, the rebuilding set's applies answer "busy" (no stall) and it catches up after; gxrepl's own
    shared writer holds are never taken for a local writer; the rebuild child gets only stdin/out/err (no
    gxrepl.lock, state files, locks or the key memfd)."""
    probe = os.path.join(TMP, "slow_rebuild_probe.py")
    with open(probe, "w") as f:
        f.write(textwrap.dedent("""\
            import os, subprocess, sys, time
            out, slow = sys.argv[1], float(sys.argv[2])
            fds = {}
            for n in os.listdir("/proc/self/fd"):
                try:
                    fds[n] = os.readlink("/proc/self/fd/" + n)
                except OSError:
                    pass
            open(out + ".started", "w").write(repr(fds))
            time.sleep(slow)
            rc = subprocess.call(sys.argv[3:])
            open(out + ".done", "a").write("%d\\n" % rc)
            sys.exit(rc)
            """))
    mark = os.path.join(TMP, "slow_rebuild")
    for x in (".started", ".done"):
        if os.path.exists(mark + x):
            os.unlink(mark + x)
    rebuild = (f"python3 -I {probe} {mark} 20 {os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}")
    db = {"client": CB_OPTS["client"] + ["derived = index.gxd"],
          "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 1",
                                         "derived_busy_rebuild = 2"]}
    db2 = {"client": list(CB_OPTS["client"]), "server": list(CB_OPTS["server"])}
    env = Env("slow_rebuild", {"db": db, "db2": db2})
    ws, aws = env.roots["db"]
    ws2, aws2 = env.roots["db2"]
    assert writer(ws, "--commits", 3, "--commit-block").returncode == 0
    assert writer(ws2, "--commits", 3, "--commit-block").returncode == 0
    env.start_server(extra_env={"GXREPL_APPLY_LOCK_WAIT": "1"})
    env.start_client()
    try:
        wait_same(env, "db2", 60)
        end = time.time() + 60
        while time.time() < end and not os.path.exists(mark + ".started"):
            time.sleep(0.2)
        assert os.path.exists(mark + ".started"), "no rebuild started"
        t0 = time.time()
        # during the rebuild (20 s): both sets change; db2 must reach its source while the rebuild still runs
        assert writer(ws, "--commits", 2, "--commit-block", "--seed", 5).returncode == 0
        assert writer(ws2, "--commits", 4, "--commit-block", "--seed", 6).returncode == 0
        wait_same(env, "db2", 15)
        took = time.time() - t0
        assert not os.path.exists(mark + ".done"), "the rebuild ended before db2 was checked: make it slower"
        print(f"    db2 copied in {took:.1f} s while db's rebuild ran", flush=True)
        # after the rebuild db catches up; the copies end whole
        end = time.time() + 90
        while time.time() < end and not os.path.exists(mark + ".done"):
            time.sleep(0.5)
        wait_same(env, "db", 90)
        t = torn(env)
        assert t.returncode == 0, t.stdout
        slog = open(os.path.join(env.dir, "server.log")).read()
        clog = open(os.path.join(env.dir, "client.log")).read()
        assert "a local writer holds" not in slog + clog, "gxrepl's own hold was taken for a local writer"
        assert "no reply in" not in clog, "a request had no reply (the stall)"
        print(f"    db applies answered busy {clog.count('busy:')} time(s) during the rebuild", flush=True)
        # the rebuild child's fds: only stdio (and its own), nothing of gxrepl's
        fds = eval(open(mark + ".started").read())
        bad = {k: v for k, v in fds.items() if int(k) > 2 and (v.endswith(".lock") or ".state" in v or "apply.jnl" in v
                                                              or "memfd" in v or v.endswith(".gxl") or "/.gxrepl/" in v)}
        assert not bad, f"the rebuild child inherited {bad}"
    finally:
        env.stop()


def test_late_reply_never_desyncs_a_channel():
    """11/12/2026 (F4e, the AWS stall): the server stopped answering for longer than the client's timeout (on 11/12 the
    workstation lost its internet for 16 min, the TCP connection survived), then the late replies came. Each set's
    channel kept a late reply queued: every later request read the previous one's reply ("bad reply") and no set
    copied again, silently, while the connection stayed up. Now a channel is closed at its timeout and the set's thread
    opens a fresh one: the late replies are dropped and every set catches up."""
    env = Env("late_reply", {"db": dict(CB_OPTS), "db2": dict(CB_OPTS)}, extra_client="timeout = 5")
    ws, aws = env.roots["db"]
    ws2, aws2 = env.roots["db2"]
    assert writer(ws, "--commits", 3, "--commit-block").returncode == 0
    assert writer(ws2, "--commits", 3, "--commit-block").returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 30)
        wait_same(env, "db2", 30)
        # the outage: the server answers nothing for 9 s (> the client's 5 s timeout), then answers late
        env.server.send_signal(signal.SIGSTOP)
        time.sleep(9)
        env.server.send_signal(signal.SIGCONT)
        assert writer(ws, "--commits", 4, "--commit-block", "--seed", 9).returncode == 0
        assert writer(ws2, "--commits", 4, "--commit-block", "--seed", 10).returncode == 0
        wait_same(env, "db", 45)
        wait_same(env, "db2", 45)
        clog = open(os.path.join(env.dir, "client.log")).read()
        assert "no reply in 5 s" in clog, clog[-2000:]
        for name in ("db", "db2"):
            end = time.time() + 30
            st = {}
            while time.time() < end:
                st = env.status()["sets"][name]
                if st.get("ok"):
                    break
                time.sleep(0.5)
            assert st.get("ok"), (name, st)
        assert torn(env).returncode == 0
    finally:
        env.stop()


def test_seal_and_position_never_split():
    """11/12/2026 (the 7.4 thread's 1/60 and 12/60 promote-check rc 1): a ship log apply writes the records' pages
    and the unit's seal "log G L" in one transaction, and the stream's applied position after it. A promote-check
    read between the two saw "sealed at ship log record L, the copy applied up to L-1 (records past the seal)", a
    torn copy that was only an apply in flight. Now the position is written under the same apply lock and the
    check reads a split pair again before it reports. A busy writer, promote-check back to back: never that."""
    env = Env("seal_split", {"db": dict(CB_OPTS)})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 3, "--commit-block").returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        w = writer_bg(ws, "--commits", 100000, "--per", 1, "--sleep", 30, "--commit-block", "--seed", 41)
        split, other, checks = 0, 0, 0
        end = time.time() + int(os.environ.get("GXREPL_SPLIT_SECS", "60"))
        while time.time() < end:
            t = torn(env)
            checks += 1
            if t.returncode != 0:
                if "records past the seal" in t.stdout or "the commit block says" in t.stdout:
                    split += 1
                elif "is odd" not in t.stdout:
                    other += 1
                    print(f"      rc {t.returncode}: {t.stdout.strip()[:400]!r}", flush=True)
        w.kill(); w.wait()
        print(f"    {checks} promote-checks under a commit every 30 ms: {split} split (seal vs position or commit block), "
              f"{other} other", flush=True)
        assert split == 0, split
    finally:
        env.stop()


def _staged_env(name, server_extra=(), slow=True):
    """A commit_block set with an 8 MB unit copied whole, then the source changed: returns (env, ws, aws) with the
    client running and the server slowed (each request of the set waits 150 ms) so a unit copy takes seconds."""
    opts = {"client": list(CB_OPTS["client"]), "server": list(CB_OPTS["server"]) + list(server_extra)}
    env = Env(name, {"db": opts})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 3).returncode == 0
    env.start_server()
    env.start_client()
    wait_same(env, "db", 120)
    env.stop()
    env.start_server(extra_env={"GXREPL_TEST_SLOW_SET": "db:150"} if slow else None)
    return env, ws, aws


def _stage_of(aws):
    return os.path.join(aws, ".gxrepl", "stage", "_root")


def _kill_mid_unit(env, ws, aws, seed):
    """Change the source unit, start the client, kill it (SIGKILL) while the unit copy is in progress."""
    assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", seed).returncode == 0
    env.start_client()
    stage, marked = _stage_of(aws), None
    end = time.time() + 60
    while time.time() < end:
        if os.path.exists(os.path.join(stage, ".stage")):
            marked = "stage"
            break
        try:
            g = gen_of(os.path.join(aws, "commit.bin"))
        except OSError:
            g = 0
        if g & 1:                              # the in-place copy (the old code) marks the unit odd
            marked = "odd"
            break
        time.sleep(0.05)
    assert marked, "no unit copy started"
    time.sleep(1.5)                            # part of the files are in
    env.kill("client")
    return marked


def test_staged_fill_link_lost_mid_unit():
    """11/12/2026 (F7, test a): the link is lost in the middle of a unit copy (the client killed). Before F7 the unit
    was copied in place, marked odd: promote-check refused it for good (the source being gone, it never finishes).
    Now the copy goes into a stage: the unit stays whole at its sealed commit, promote-check exits 0 with a STAGED
    warning, the old content checks clean; when the link comes back the copy finishes and the stage is gone."""
    env, ws, aws = _staged_env("staged_lost")
    try:
        marked = _kill_mid_unit(env, ws, aws, 11)
        t = torn(env)
        print(f"    copy marked by: {marked}; promote-check rc {t.returncode}: {t.stdout.strip()[:160]}", flush=True)
        assert t.returncode == 0, t.stdout              # the old code: rc 1 ("generation N is odd")
        assert "STAGED" in t.stdout, t.stdout
        check_db(aws)                                   # the copy's old commit, whole
        env.start_client()
        wait_same(env, "db", 120)
        end = time.time() + 30
        while time.time() < end and os.path.exists(_stage_of(aws)):
            time.sleep(0.2)
        assert not os.path.exists(_stage_of(aws)), "the stage outlived the copy"
        t = torn(env)
        assert t.returncode == 0 and "STAGED" not in t.stdout, t.stdout
        check_db(aws)
    finally:
        env.stop()


def test_staged_fill_promote_discards_and_says_what_was_lost():
    """11/12/2026 (F7, test b): a take over after a lost link: gxrepl promote --apply discards the unfinished stage,
    says what was lost (the unit, since when, the commit it stays at, the files the stage had changed), leaves the
    copy whole at its sealed commit (it checks clean, promote-check 0), and its dry run changes nothing."""
    env, ws, aws = _staged_env("staged_promote")
    try:
        _kill_mid_unit(env, ws, aws, 12)
        env.kill("server")                              # the take over: nothing copies any more
        assert os.path.exists(os.path.join(_stage_of(aws), ".stage"))
        dry = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf])
        assert dry.returncode == 0 and "(dry run)" in dry.stdout and "would discard" in dry.stdout, dry.stdout
        assert os.path.exists(os.path.join(_stage_of(aws), ".stage")), "the dry run changed something"
        p = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"])
        print(f"    {p.stdout.strip()[:300]}", flush=True)
        assert p.returncode == 0, p.stdout
        assert "discarded an unfinished copy" in p.stdout and "LOST" in p.stdout, p.stdout
        assert not os.path.exists(_stage_of(aws)), "the stage is still there"
        check_db(aws)
        assert torn(env).returncode == 0
    finally:
        env.stop()


def _test_hooks():
    return ", test hooks" in run([os.path.join(BIN, "gxrepl"), "version"]).stdout


def test_staged_fill_crash_in_the_swap_is_finished():
    """11/12/2026 (F7, test c): the copy side stops in the middle of swapping a whole stage in (after 1 file): the
    unit is half old, half new, so promote-check is 1 ("being swapped in"); gxrepl promote finishes the swap (the
    stage was whole), or the next start does (RecoverAll): the unit is the new commit, whole. Needs a build with
    -DGXREPL_TEST_HOOKS=ON (the crash hook is not in production builds)."""
    if not _test_hooks():
        print("    SKIP: gxrepl was built without GXREPL_TEST_HOOKS", flush=True)
        return
    for how in ("promote", "restart"):
        env, ws, aws = _staged_env("staged_swap_" + how, slow=False)
        try:
            env.stop()
            env.start_server(extra_env={"GXREPL_TEST_SWAP_CRASH": "1"})
            assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block",
                          "--seed", 13).returncode == 0
            env.start_client()
            end = time.time() + 120
            while time.time() < end and env.server.poll() is None:
                time.sleep(0.2)
            assert env.server.poll() == 9, "the swap crash hook did not fire"
            env.kill("client")
            t = torn(env)
            assert t.returncode == 1 and "being swapped in" in t.stdout, t.stdout
            if how == "promote":
                p = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"])
                assert p.returncode == 0 and "finished the swap" in p.stdout, p.stdout
            else:
                env.server = None
                env.start_server()
                end = time.time() + 30
                while time.time() < end and os.path.exists(_stage_of(aws)):
                    time.sleep(0.2)
                assert "finished the swap of a staged copy" in open(os.path.join(env.dir, "server.log")).read()
            assert not os.path.exists(_stage_of(aws))
            assert torn(env).returncode == 0
            check_db(aws)
            env.start_server() if env.server is None else None
            env.start_client()
            wait_same(env, "db", 120)
        finally:
            env.stop()


def test_staged_fill_damaged_sealed_unit_still_refused():
    """11/12/2026 (F7, test d): promote never hides a real fault: a whole, sealed copy whose file was damaged
    afterwards (its hash no longer the seal's) is torn, and stays torn after gxrepl promote --apply."""
    env, ws, aws = _staged_env("staged_damaged", slow=False)
    try:
        env.stop()
        big = [n for n in os.listdir(aws) if n.endswith(".gxd")][0]
        with open(os.path.join(aws, big), "r+b") as f:
            f.seek(4096 + 17)
            b = f.read(1)
            f.seek(4096 + 17)
            f.write(bytes([b[0] ^ 0xFF]))
        t = torn(env)
        assert t.returncode == 1, t.stdout
        p = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"])
        assert p.returncode == 1, p.stdout
    finally:
        env.stop()


def test_staged_fill_falls_back_in_place_over_the_cap():
    """11/12/2026 (F7, test e): a unit over stage_max_bytes (here 1 MB) is copied in place as before, with one
    WARNING in the copy side's log, and ends whole."""
    env, ws, aws = _staged_env("staged_cap", server_extra=("stage_max_bytes = 1048576",), slow=False)
    try:
        env.stop()
        env.start_server()
        assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 14).returncode == 0
        env.start_client()
        wait_same(env, "db", 120)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "copied in place, not staged (the unit is" in log and "over stage_max_bytes" in log, log[-1500:]
        assert not os.path.exists(_stage_of(aws))
        assert torn(env).returncode == 0
        check_db(aws)
    finally:
        env.stop()


def _set_line(env, key, value):
    """Set (or add) one "key = value" line of the server's [set db]."""
    lines = open(env.srv_conf).read().splitlines()
    out, done, in_db = [], False, False
    for l in lines:
        if l.strip().startswith("["):
            if in_db and not done:
                out.append(f"{key} = {value}"); done = True
            in_db = l.strip() == "[set db]"
        if in_db and l.split("=")[0].strip() == key:
            l = f"{key} = {value}"; done = True
        out.append(l)
    if in_db and not done:
        out.append(f"{key} = {value}")
    open(env.srv_conf, "w").write("\n".join(out) + "\n")


def test_staged_fill_failback_after_a_takeover_with_a_stage():
    """11/13/2026 (F7 review 1): a take over left a stage on AWS (the gate ran the read-only promote-check). (a) no
    writer changed the copy: the failback server (serve --accept-both, the source now) finishes or discards every
    stage first and says so, and the pull brings the copy back whole. (b) the take over's writer committed on the
    copy: the stage is stale (never read: before the fix AWS served the stage's odd commit block as the source's
    and the pull gave up on the unit for good), it is discarded, and the pull brings the writer's commits back."""
    for how in ("no_writer", "writer"):
        env, ws, aws = _staged_env("staged_fb_" + how)
        try:
            _kill_mid_unit(env, ws, aws, 21)
            env.kill("server")
            assert os.path.exists(os.path.join(_stage_of(aws), ".stage"))
            if how == "writer":
                assert writer(aws, "--commits", 4, "--no-ship", "--commit-block", "--seed", 22).returncode == 0
                t = torn(env)
                assert "stale stage" in t.stdout, t.stdout
            env.start_server(args=("--accept-both",))
            r = run([os.path.join(BIN, "gxrepl"), "once", "--config", env.cli_conf, "--pull-all"])
            log = open(os.path.join(env.dir, "server.log")).read()
            print(f"    {how}: pull rc {r.returncode}; " +
                  " | ".join(l[:110] for l in log.splitlines() if "accept-both" in l), flush=True)
            assert r.returncode == 0, r.stdout + env.logs()
            assert "serve --accept-both" in log and ("discarded a stale stage" if how == "writer" else
                                                     "discarded an unfinished copy") in log, log[-1500:]
            assert not os.path.exists(_stage_of(aws)), "a stage outlived the failback server's start"
            ok, why = same(ws, aws)
            assert ok, why
            check_db(ws)
        finally:
            env.stop()


def test_staged_fill_low_disk_after_a_stage_copies_in_place_with_no_stage_left():
    """11/13/2026 (F7 review 2): a stage is left (a lost link, or ENOSPC part way: the apply failed); then the next
    copy cannot be staged (the cap, or the disk short of room). Before the fix the old stage stayed: the unit was
    marked in place (odd, "copying") while every write still went to the stage. Now the old stage goes first: when
    the live unit is marked odd for the in-place copy no stage exists, and the copy ends whole with the WARNING."""
    variants = [("cap", None)]
    if _test_hooks():
        variants.append(("disk", {"GXREPL_TEST_FREE_BYTES": "1"}))
    else:
        print("    the low-disk variant needs GXREPL_TEST_HOOKS: only the cap variant", flush=True)
    for how, extra in variants:
        env, ws, aws = _staged_env("staged_low_" + how)
        try:
            _kill_mid_unit(env, ws, aws, 31)
            env.kill("server")
            assert os.path.exists(os.path.join(_stage_of(aws), ".stage"))
            if how == "cap":
                _set_line(env, "stage_max_bytes", "1048576")
            env.start_server(extra_env=extra)
            env.start_client()
            seen_odd, stage_then = False, None
            end = time.time() + 120
            while time.time() < end and not seen_odd:
                try:
                    g = gen_of(os.path.join(aws, "commit.bin"))
                except OSError:
                    g = 0
                if g & 1:
                    seen_odd = True
                    stage_then = os.path.exists(_stage_of(aws))
                time.sleep(0.01)
            assert seen_odd, "the in-place copy never marked the unit"
            assert stage_then is False, "a stage was still there while the unit was copied in place"
            wait_same(env, "db", 120)
            log = open(os.path.join(env.dir, "server.log")).read()
            want = "over stage_max_bytes" if how == "cap" else "the disk has 1 bytes free"
            assert "copied in place, not staged" in log and want in log, log[-1500:]
            assert not os.path.exists(_stage_of(aws))
            t = torn(env)
            assert t.returncode == 0 and "STAGED" not in t.stdout, t.stdout
            check_db(aws)
            print(f"    {how}: no stage when marked in place, whole after", flush=True)
        finally:
            env.stop()


def test_staged_fill_promote_while_the_server_runs():
    """11/13/2026 (F7 review 3): gxrepl promote --apply while the server is still copying into a stage (a false
    fail over, the link up): the stage is discarded under the apply lock; the server's next batch for it is dropped
    (never written in place under the unit's even commit block); promote-check never fails meanwhile, and the copy
    then starts again and ends whole."""
    env, ws, aws = _staged_env("staged_promote_live")
    try:
        assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 41).returncode == 0
        env.start_client()
        end = time.time() + 60
        while time.time() < end and not os.path.exists(os.path.join(_stage_of(aws), ".stage")):
            time.sleep(0.05)
        assert os.path.exists(os.path.join(_stage_of(aws), ".stage")), "no stage began"
        time.sleep(1.0)
        p = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"])
        print(f"    promote: rc {p.returncode}: {p.stdout.strip()[:200]}", flush=True)
        assert "discarded an unfinished copy" in p.stdout, p.stdout
        fails = []
        for _ in range(10):
            t = torn(env)
            if t.returncode != 0:
                fails.append(t.stdout.strip()[:200])
            time.sleep(0.3)
        assert not fails, fails
        wait_same(env, "db", 180)
        log = open(os.path.join(env.dir, "server.log")).read() + open(os.path.join(env.dir, "client.log")).read()
        assert "was discarded (gxrepl promote)" in log or "changed during this apply" in log, log[-2000:]
        end = time.time() + 30
        while time.time() < end and os.path.exists(_stage_of(aws)):
            time.sleep(0.2)
        assert not os.path.exists(_stage_of(aws))
        t = torn(env)
        assert t.returncode == 0, t.stdout
        check_db(aws)
    finally:
        env.stop()


def test_staged_fill_promote_after_a_crash_with_a_committed_batch():
    """11/13/2026 (F7 review 4): the server dies inside an apply into a stage, its batch committed to apply.jnl and
    not yet in the files. gxrepl promote --apply recovers the journal first (into the stage), then discards the
    stage; the server starts again (before the fix promote removed the stage under the committed batch, and the
    start's journal recovery failed: gxrepl would not start), and the copy ends whole. Needs GXREPL_TEST_HOOKS."""
    if not _test_hooks():
        print("    SKIP: gxrepl was built without GXREPL_TEST_HOOKS", flush=True)
        return
    env, ws, aws = _staged_env("staged_crash_jnl", slow=False)
    try:
        env.stop()
        env.start_server(extra_env={"GXREPL_TEST_APPLY_CRASH": "1"})
        assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 51).returncode == 0
        env.start_client()
        end = time.time() + 60
        while time.time() < end and env.server.poll() is None:
            time.sleep(0.05)
        assert env.server.poll() is not None, "the server did not stop inside an apply"
        env.kill("client")
        env.server = None
        jnl = os.path.join(aws, ".gxrepl", "apply.jnl")
        assert os.path.getsize(jnl) > 0, "no committed batch left in the journal"
        assert os.path.exists(os.path.join(_stage_of(aws), ".stage"))
        p = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"])
        print(f"    promote: rc {p.returncode}: {p.stdout.strip()[:240]}", flush=True)
        assert "finished 1 unfinished apply batch(es)" in p.stdout and "discarded" in p.stdout, p.stdout
        assert not os.path.exists(_stage_of(aws))
        env.start_server()                                # raises when it does not start
        env.start_client()
        wait_same(env, "db", 120)
        t = torn(env)
        assert t.returncode == 0, t.stdout
        check_db(aws)
    finally:
        env.stop()


def _hashes(root):
    return run([os.path.join(BIN, "gxrepl_testwriter"), "hashes", root]).stdout.split()


def _takeover_writer(aws, seed):
    """A take over's writer on the copy: its open drops the unit's seal (osa_repl_copy.h forget_seal), then it
    commits."""
    seal = os.path.join(aws, ".gxrepl", "units", "_root")
    if os.path.exists(seal):
        os.unlink(seal)
    assert writer(aws, "--commits", 3, "--no-ship", "--commit-block", "--seed", seed).returncode == 0


def _server_dies(env, secs=60):
    end = time.time() + secs
    while time.time() < end and env.server.poll() is None:
        time.sleep(0.05)
    assert env.server.poll() is not None, "the server did not stop at the test hook"
    env.kill("client")
    env.server = None


def test_staged_fill_apply_failure_keeps_the_journal_and_the_stage():
    """11/13/2026 (F7 review round 2, 1): a batch into a stage is committed to apply.jnl but its pages cannot be
    written (an ENOSPC: GXREPL_TEST_APPLY_FAIL); the journal is kept. The retry recovers it before the stage is
    touched (before the fix the stage was discarded under it: every later apply of the set failed and the next
    start's recovery stopped the whole server). The copy ends whole, the server keeps running and starts again."""
    if not _test_hooks():
        print("    SKIP: gxrepl was built without GXREPL_TEST_HOOKS", flush=True)
        return
    env, ws, aws = _staged_env("staged_apply_fail", slow=False)
    try:
        env.stop()
        env.start_server(extra_env={"GXREPL_TEST_APPLY_FAIL": "1"})
        assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 61).returncode == 0
        env.start_client()
        wait_same(env, "db", 120)
        assert env.server.poll() is None, "the server stopped"
        log = open(os.path.join(env.dir, "client.log")).read() + open(os.path.join(env.dir, "server.log")).read()
        assert "apply failed" in log, log[-1500:]          # the injected failure happened
        assert not os.path.exists(_stage_of(aws))
        assert os.path.getsize(os.path.join(aws, ".gxrepl", "apply.jnl")) == 0
        assert torn(env).returncode == 0
        check_db(aws)
        env.stop()
        env.start_server()                                 # raises when it does not start
    finally:
        env.stop()


def test_staged_fill_a_complete_stage_never_goes_over_a_writers_data():
    """11/13/2026 (F7 review round 2, 3): the copy side stops after a stage's last commit arrived whole but before
    its swap (GXREPL_TEST_CRASH_BEFORE_SWAP). (a) nothing else changed: the next start swaps it in (whole, the
    source's). (b) a take over's writer opened the copy and committed: the stage is stale and the next start
    discards it; the writer's data stays (before the fix the older stage was swapped in over it)."""
    if not _test_hooks():
        print("    SKIP: gxrepl was built without GXREPL_TEST_HOOKS", flush=True)
        return
    for how in ("no_writer", "writer"):
        env, ws, aws = _staged_env("staged_complete_" + how, slow=False)
        try:
            env.stop()
            env.start_server(extra_env={"GXREPL_TEST_CRASH_BEFORE_SWAP": "1"})
            assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 71).returncode == 0
            env.start_client()
            _server_dies(env)
            sd = _stage_of(aws)
            assert os.path.exists(os.path.join(sd, ".seal")) and not os.path.exists(os.path.join(sd, ".swap"))
            if how == "writer":
                _takeover_writer(aws, 72)
                mine = _hashes(aws)
            env.start_server()
            log = open(os.path.join(env.dir, "server.log")).read()
            assert not os.path.exists(sd), "the stage is still there"
            if how == "writer":
                assert "discarded a stale stage" in log, log[-1500:]
                assert _hashes(aws) == mine, "the writer's data was replaced"
            else:
                assert "finished the swap of a staged copy" in log, log[-1500:]
                ok, why = same(ws, aws)
                assert ok, why
                assert torn(env).returncode == 0
            print(f"    {how}: ok", flush=True)
        finally:
            env.stop()


def test_staged_fill_crash_after_the_commit_block_moved():
    """11/13/2026 (F7 review round 2, 4): the copy side stops right after the swap renamed the commit block in,
    before the new seal (GXREPL_TEST_SWAP_CRASH_AFTER_CB). (a) the next start writes the seal (the unit is the new
    commit, whole). (b) a take over's writer took the folder first (dropped the seal, committed): the next start
    writes no seal over its folder and removes the stage (before the fix the old stage seal was written over it)."""
    if not _test_hooks():
        print("    SKIP: gxrepl was built without GXREPL_TEST_HOOKS", flush=True)
        return
    for how in ("no_writer", "writer"):
        env, ws, aws = _staged_env("staged_after_cb_" + how, slow=False)
        try:
            env.stop()
            env.start_server(extra_env={"GXREPL_TEST_SWAP_CRASH_AFTER_CB": "1"})
            assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 81).returncode == 0
            env.start_client()
            _server_dies(env)
            sd = _stage_of(aws)
            assert os.path.exists(os.path.join(sd, ".swap")) and not os.path.exists(os.path.join(sd, "commit.bin"))
            seal = os.path.join(aws, ".gxrepl", "units", "_root")
            if how == "writer":
                _takeover_writer(aws, 82)
                mine = _hashes(aws)
            env.start_server()
            assert not os.path.exists(sd), "the stage is still there"
            if how == "writer":
                assert not os.path.exists(seal), "a seal was written over the writer's folder"
                assert _hashes(aws) == mine
            else:
                assert open(seal).read().startswith("gen "), open(seal).read()[:80]
                ok, why = same(ws, aws)
                assert ok, why
                assert torn(env).returncode == 0
            print(f"    {how}: ok", flush=True)
        finally:
            env.stop()


def test_staged_fill_promote_waits_for_the_apply_lock_then_lets_promote_check_decide():
    """11/13/2026 (F7 review round 2, 5): a derived rebuild holds the apply lock while the DRCM gate runs promote
    --apply. It waits up to GXREPL_PROMOTE_LOCK_WAIT (480 s; 2 here), then leaves the stages and lets the
    promote-check decide: exit 0 for a whole copy with a stage (before: exit 2 after 60 s, the takeover refused)."""
    import fcntl
    env, ws, aws = _staged_env("staged_promote_wait")
    try:
        _kill_mid_unit(env, ws, aws, 91)
        env.kill("server")
        fd = os.open(os.path.join(aws, ".gxrepl", "apply.jnl"), os.O_RDWR | os.O_CREAT, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX)
        try:
            t0 = time.time()
            p = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"],
                    env=dict(os.environ, GXREPL_PROMOTE_LOCK_WAIT="2"))
            took = time.time() - t0
        finally:
            fcntl.flock(fd, fcntl.LOCK_UN)
            os.close(fd)
        print(f"    rc {p.returncode} in {took:.1f} s: {p.stdout.strip()[:200]}", flush=True)
        assert p.returncode == 0, p.stdout
        assert "stayed locked for 2 s" in p.stdout and "promote-check decides" in p.stdout, p.stdout
        assert os.path.exists(os.path.join(_stage_of(aws), ".stage"))
        p = run([os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"])
        assert p.returncode == 0 and "discarded" in p.stdout, p.stdout
    finally:
        env.stop()


def test_staged_fill_warnings_from_many_sets_at_once():
    """11/13/2026 (F7 review round 2, 6): six sets copy in place at once (stage_max_bytes 1: every unit over it),
    their threads all reaching the shared once-per-unit-and-reason WARNING list: the server keeps running, and no
    WARNING line is logged twice (a smoke test of the mutex; the race itself is undefined behaviour)."""
    names = [f"s{i}" for i in range(6)]
    opts = {"client": list(CB_OPTS["client"]), "server": list(CB_OPTS["server"]) + ["stage_max_bytes = 1"]}
    env = Env("staged_warn_race", {n: opts for n in names})
    try:
        for n in names:
            assert writer(env.roots[n][0], "--commits", 2, "--big", 1024 * 1024, "--no-ship", "--commit-block").returncode == 0
        env.start_server()
        env.start_client()
        for n in names:
            wait_same(env, n, 120)
        ws_ = [writer_bg(env.roots[n][0], "--commits", 8, "--per", 2, "--sleep", 300, "--no-ship", "--commit-block",
                         "--seed", 5 + i) for i, n in enumerate(names)]
        for w in ws_:
            w.wait(120)
        for n in names:
            wait_same(env, n, 120)
        assert env.server.poll() is None, "the server stopped"
        log = open(os.path.join(env.dir, "server.log")).read()
        for n in names:
            mine = [l.split("gxrepl: ", 1)[-1] for l in log.splitlines() if f"WARNING set {n}: .: copied in place" in l]
            assert mine, n
            assert len(mine) == len(set(mine)), (n, mine)  # once per unit and reason (a new size is a new reason)
    finally:
        env.stop()


def test_staged_fill_no_reflink_shared_free_space_and_reuse():
    """11/13/2026 (F7 review round 2, 7): (a) a file system that cannot clone (GXREPL_TEST_NO_REFLINK): in place,
    one WARNING, no full-copy stage. (b) two sets' stages share one file system: a second stage that would not fit
    beside the first (GXREPL_TEST_FREE_BYTES) is copied in place, the WARNING naming the other stages. (c) a retry
    of the same commit after a lost link reuses its stage."""
    if not _test_hooks():
        print("    SKIP: gxrepl was built without GXREPL_TEST_HOOKS", flush=True)
        return
    # (a)
    env, ws, aws = _staged_env("staged_noreflink", slow=False)
    try:
        env.stop()
        env.start_server(extra_env={"GXREPL_TEST_NO_REFLINK": "1"})
        assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 101).returncode == 0
        env.start_client()
        wait_same(env, "db", 120)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "cannot clone files (no reflink)" in log, log[-1500:]
        assert not os.path.exists(_stage_of(aws)) and torn(env).returncode == 0
    finally:
        env.stop()
    # (b)
    opts = {"client": list(CB_OPTS["client"]), "server": list(CB_OPTS["server"])}
    env = Env("staged_shared_free", {"a": opts, "b": opts})
    try:
        for n in ("a", "b"):
            assert writer(env.roots[n][0], "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block",
                          "--seed", 3).returncode == 0
        env.start_server()
        env.start_client()
        wait_same(env, "a", 120); wait_same(env, "b", 120)
        env.stop()
        env.start_server(extra_env={"GXREPL_TEST_SLOW_SET": "a:150"})
        wsa, awsa = env.roots["a"]
        _kill_mid_unit(env, wsa, awsa, 111)                # a's stage stays (about 8 MB)
        env.kill("server")
        unit = sum(os.path.getsize(os.path.join(env.roots["b"][1], f)) for f in os.listdir(env.roots["b"][1])
                   if os.path.isfile(os.path.join(env.roots["b"][1], f)))
        room = unit + 256 * 1024 * 1024 + 2 * 1024 * 1024   # b fits alone, not beside a's stage
        env.start_server(extra_env={"GXREPL_TEST_FREE_BYTES": str(room), "GXREPL_TEST_SLOW_SET": "a:150"})
        assert writer(env.roots["b"][0], "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block",
                      "--seed", 112).returncode == 0
        env.start_client()
        wait_same(env, "b", 120)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "for the other open stages there" in log, log[-2000:]
        print("    (b) " + [l for l in log.splitlines() if "for the other open stages" in l][0][:200], flush=True)
    finally:
        env.stop()
    # (c)
    env, ws, aws = _staged_env("staged_reuse")
    try:
        _kill_mid_unit(env, ws, aws, 121)
        env.start_client()                                 # the same commit again
        wait_same(env, "db", 120)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "its stage is reused" in log, log[-1500:]
        assert not os.path.exists(_stage_of(aws)) and torn(env).returncode == 0
        check_db(aws)
    finally:
        env.stop()


def _wait_stage(path, secs=60):
    end = time.time() + secs
    while time.time() < end and not os.path.exists(os.path.join(path, ".stage")):
        time.sleep(0.05)
    assert os.path.exists(os.path.join(path, ".stage")), "no stage began"


def test_staged_fill_a_source_never_reads_a_stage():
    """11/13/2026 (F7 review round 3, 1): a failback pull (this node the copy) is cut mid-unit: a stage is left here.
    This node then pushes again (the source): it never reads the stage (before: it served the stage's odd
    generation as the source's, and the unit never reached AWS again); its start discards it and says so; the
    unit's next commit reaches AWS."""
    pull = {"direction": "pull", "accept": "source", "client": list(CB_OPTS["client"]), "server": list(CB_OPTS["server"])}
    env = Env("staged_source_side", {"db": pull})
    ws, aws = env.roots["db"]
    os.makedirs(aws, exist_ok=True)
    try:
        assert writer(aws, "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 3).returncode == 0
        env.start_server()
        env.start_client()
        wait_same(env, "db", 120)
        env.stop()
        env.start_server(extra_env={"GXREPL_TEST_SLOW_SET": "db:400"})
        assert writer(aws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 131).returncode == 0
        env.start_client()
        _wait_stage(_stage_of(ws))
        time.sleep(0.2)
        env.kill("client")                              # the link lost mid-unit
        env.stop()
        assert os.path.exists(os.path.join(_stage_of(ws), ".stage")), "the pull left no stage"
        # back to normal: this node pushes, AWS is the copy
        for conf, a_, b_ in ((env.cli_conf, "direction = pull", "direction = push"),
                             (env.srv_conf, "accept = source", "accept = target")):
            t = open(conf).read()
            assert a_ in t, (conf, a_)
            open(conf, "w").write(t.replace(a_, b_))
        env.start_server()
        env.start_client()
        end = time.time() + 30
        while time.time() < end and os.path.exists(_stage_of(ws)):
            time.sleep(0.2)
        log = open(os.path.join(env.dir, "client.log")).read()
        assert not os.path.exists(_stage_of(ws)), "the source kept its stage"
        assert "discarded a stage on this set's source side" in log, log[-1500:]
        assert writer(ws, "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 132).returncode == 0
        wait_same(env, "db", 120)
        assert torn(env).returncode == 0
    finally:
        env.stop()


def test_staged_fill_promote_with_a_writer_running():
    """11/13/2026 (F7 review round 3, 2): promote --apply while the set's writer holds its lock (a gate re-run
    after the managers started): (a) a stage is discarded (the writer never reads it) and the exit is the
    promote-check's (0), not 2; (b) a batch pending in apply.jnl is not replayed over the writer's files: exit 3,
    the journal and the stage left; (c) a begun swap is not finished under the writer: TORN, exit 1."""
    import fcntl
    opts = {"client": list(CB_OPTS["client"]), "server": list(CB_OPTS["server"]) + ["writer_lock = w.lock"]}
    env = Env("staged_promote_writer", {"db": opts})
    ws, aws = env.roots["db"]
    try:
        assert writer(ws, "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 3).returncode == 0
        env.start_server(); env.start_client(); wait_same(env, "db", 120); env.stop()
        env.start_server(extra_env={"GXREPL_TEST_SLOW_SET": "db:150"})
        _kill_mid_unit(env, ws, aws, 141)
        env.kill("server")
        sd = _stage_of(aws)
        promote = [os.path.join(BIN, "gxrepl"), "promote", "--config", env.srv_conf, "--apply"]
        fd = os.open(os.path.join(aws, "w.lock"), os.O_RDWR | os.O_CREAT, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX)                 # the writer runs
        try:
            jnl = os.path.join(aws, ".gxrepl", "apply.jnl")
            open(jnl, "wb").write(b"\x01" * 512)        # (b) a pending batch (its bytes are never replayed here)
            p = run(promote)
            print(f"    (b) rc {p.returncode}: {p.stdout.strip()[:200]}", flush=True)
            assert p.returncode == 3 and "is not replayed" in p.stdout, p.stdout
            assert open(jnl, "rb").read() == b"\x01" * 512 and os.path.exists(os.path.join(sd, ".stage"))
            open(jnl, "wb").close()
            p = run(promote)                           # (a)
            print(f"    (a) rc {p.returncode}: {p.stdout.strip()[:200]}", flush=True)
            assert p.returncode == 0 and "discarded" in p.stdout, p.stdout
            assert not os.path.exists(sd)
        finally:
            fcntl.flock(fd, fcntl.LOCK_UN)
            os.close(fd)
        if _test_hooks():                              # (c) a begun swap, then the writer
            env.stop()
            env.start_server(extra_env={"GXREPL_TEST_SWAP_CRASH": "1"})
            assert writer(ws, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 142).returncode == 0
            env.start_client()
            _server_dies(env)
            assert os.path.exists(os.path.join(sd, ".swap"))
            fd = os.open(os.path.join(aws, "w.lock"), os.O_RDWR | os.O_CREAT, 0o600)
            fcntl.flock(fd, fcntl.LOCK_EX)
            try:
                p = run(promote)
                print(f"    (c) rc {p.returncode}: {p.stdout.strip()[:200]}", flush=True)
                assert p.returncode == 1 and "is not finished" in p.stdout, p.stdout
                assert os.path.exists(os.path.join(sd, ".swap"))
            finally:
                fcntl.flock(fd, fcntl.LOCK_UN)
                os.close(fd)
            p = run(promote)                           # the writer stopped: finished
            assert p.returncode == 0 and "finished the swap" in p.stdout, p.stdout
    finally:
        env.stop()


def test_staged_fill_a_lingering_stage_never_wedges_a_ship_log_set():
    """11/13/2026 (F7 review round 3, 3): a stage is left by a lost link; then the writer ships its commits on the
    ship log (the unit is now applied from log records, no MarkGen). Updates of the unit go in place (the stage
    nobody fills is discarded, or goes stale): the set stays in step (before: the commit block went into the stage
    and its swap failed for lack of a seal on every pass), promote-check passes, and the next start leaves no stage."""
    env, ws, aws = _staged_env("staged_then_log")
    try:
        _kill_mid_unit(env, ws, aws, 151)
        env.stop()
        env.start_server()
        assert writer(ws, "--commits", 5, "--commit-block", "--seed", 152).returncode == 0     # shipped
        env.start_client()
        wait_same(env, "db", 120)
        log = open(os.path.join(env.dir, "server.log")).read() + open(os.path.join(env.dir, "client.log")).read()
        assert "has no seal yet" not in log, log[-1500:]
        assert torn(env).returncode == 0
        env.stop()
        env.start_server()
        assert not os.path.exists(_stage_of(aws)), "a stage is still there after a start"
        check_db(aws)
    finally:
        env.stop()


def test_staged_fill_a_ship_log_after_a_cut_copy_never_splits_the_unit():
    """11/13/2026 (F7 focused review, 1): a staged copy is cut (the client dies) while the server keeps running (the
    unit stays this server's staged copy); then the writer ships on the ship log. The stream's pieces of the unit go
    in place with the records (the stage is discarded under the lock), never half into the stage: the copy ends the
    source's, promote-check passes and no stage is left."""
    env, ws, aws = _staged_env("staged_cut_then_log")
    try:
        _kill_mid_unit(env, ws, aws, 181)              # the server keeps running
        assert writer(ws, "--commits", 5, "--commit-block", "--seed", 182).returncode == 0   # shipped
        env.start_client()
        wait_same(env, "db", 180)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "has no seal yet" not in log, log[-1500:]
        assert torn(env).returncode == 0
        end = time.time() + 30
        while time.time() < end and os.path.exists(_stage_of(aws)):
            time.sleep(0.2)
        assert not os.path.exists(_stage_of(aws)), "a stage is left"
        check_db(aws)
        ok, why = same(ws, aws)
        assert ok, why
    finally:
        env.stop()


def test_staged_fill_a_unit_deleted_on_the_source_takes_its_stage():
    """11/13/2026 (F7 review round 3, 4): a unit's copy is cut mid-unit (its stage stays), then the source deletes
    the unit (retention). The delete goes in place and the stage is discarded (before: the delete went only into
    the stage, the copy kept the unit for good and the stage counted against free space)."""
    env = Env("staged_delete", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    try:
        assert writer(d1, "--commits", 2, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 3).returncode == 0
        env.start_server(); env.start_client(); wait_same(env, "db", 120); env.stop()
        env.start_server(extra_env={"GXREPL_TEST_SLOW_SET": "db:150"})
        assert writer(d1, "--commits", 3, "--big", 8 * 1024 * 1024, "--no-ship", "--commit-block", "--seed", 161).returncode == 0
        env.start_client()
        sd = os.path.join(aws, ".gxrepl", "stage", "_d1")
        _wait_stage(sd)
        time.sleep(1.5)
        env.kill("client")
        shutil.rmtree(d1)                              # the source drops the unit
        env.start_client()
        end = time.time() + 60
        while time.time() < end and (os.path.exists(sd) or any(os.path.isfile(os.path.join(aws, "d1", f))
                                                               for f in (os.listdir(os.path.join(aws, "d1"))
                                                                         if os.path.isdir(os.path.join(aws, "d1")) else []))):
            time.sleep(0.2)
        left = os.listdir(os.path.join(aws, "d1")) if os.path.isdir(os.path.join(aws, "d1")) else []
        assert not os.path.exists(sd), "the stage outlived the unit"
        assert not [f for f in left if f.endswith(".gxd") or f == "commit.bin"], left
    finally:
        env.stop()


def test_staged_fill_promote_check_never_fails_a_whole_copy_mid_swap():
    """11/13/2026 (F7 review round 3, 6): promote-check (no lock) run again and again while staged copies keep
    swapping in (a writer commits every 0.5 s, the link up): a unit whose files do not match its seal while the
    generation moved during the check is read again; no run reports a whole copy TORN."""
    env, ws, aws = _staged_env("staged_torn_reread", slow=False)
    try:
        w = writer_bg(ws, "--commits", 60, "--per", 2, "--big", 2 * 1024 * 1024, "--sleep", 500, "--no-ship",
                      "--commit-block", "--seed", 171)
        env.start_client()
        fails, runs = [], 0
        end = time.time() + 25
        while time.time() < end:
            t = torn(env)
            runs += 1
            if t.returncode != 0:
                fails.append(t.stdout.strip()[:200])
        w.kill(); w.wait()
        print(f"    {runs} promote-checks during swaps, {len(fails)} failed", flush=True)
        assert not fails, fails[:3]
    finally:
        env.stop()


def test_derived_no_rebuild_while_writer_active():
    """11/11/2026 (review r4e): the copy's derived rebuild never runs while a local writer holds its writer_lock (a
    take over: the writer owns its indexes, its own open rebuilds them): no rebuild, no new file, the skip logged;
    once the writer lets go the index is rebuilt. A rebuild that exits 3 (the folder moved) is behind, not failed."""
    import fcntl
    rebuild = f"{os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}"
    opts = {"client": CB_OPTS["client"] + ["derived = index.gxd"],
            "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 1",
                                           "derived_busy_rebuild = 2", "writer_lock = w.lock"]}
    env = Env("derived_writer", {"db": opts})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 6, "--no-ship", "--commit-block").returncode == 0
    os.makedirs(aws, exist_ok=True)
    fd = os.open(os.path.join(aws, "w.lock"), os.O_RDWR | os.O_CREAT, 0o600)
    env.start_server()
    try:
        assert env.once().returncode == 0
        assert not os.path.exists(os.path.join(aws, "index.gxd"))
        fcntl.flock(fd, fcntl.LOCK_EX)                     # the take over: a writer runs on the copy's node
        time.sleep(12)                                     # many derived_delay / busy periods
        log = open(os.path.join(env.dir, "server.log")).read()
        print(f"    writer active 12 s: index.gxd {'MADE' if os.path.exists(os.path.join(aws, 'index.gxd')) else 'not made'}, "
              f"{log.count('derived indexes rebuilt')} rebuilds, skip logged {'not rebuilt: ' in log}", flush=True)
        assert not os.path.exists(os.path.join(aws, "index.gxd"))
        assert "derived indexes rebuilt" not in log
        assert "derived indexes not rebuilt" in log and "a local writer holds" in log
        fcntl.flock(fd, fcntl.LOCK_UN)
        end = time.time() + 30
        while time.time() < end and not os.path.exists(os.path.join(aws, "index.gxd")):
            time.sleep(0.5)
        assert os.path.exists(os.path.join(aws, "index.gxd"))
        end = time.time() + 30
        while time.time() < end and "INDEXES" in torn(env).stdout:
            time.sleep(0.5)
        assert torn(env).returncode == 0
        check_db(aws)
    finally:
        os.close(fd)
        env.stop()
    # a rebuild that exits 3 (the folder moved while it read): behind, tried again, never "failed"
    with open(env.srv_conf) as f:
        conf = f.read().replace(f"derived_rebuild = {rebuild}", "derived_rebuild = sh -c 'exit 3'")
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    assert writer(ws, "--commits", 2, "--no-ship", "--commit-block", "--seed", 91).returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        time.sleep(6)
        t = torn(env)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "moved during the rebuild" in log, log[-1500:]
        assert t.returncode == 0 and "INDEXES" in t.stdout and "failed" not in t.stdout, t.stdout
    finally:
        env.stop()


def _derived_opts(extra_server=()):
    rebuild = f"{os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}"
    return {"client": CB_OPTS["client"] + ["derived = index.gxd"],
            "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 1"]
            + list(extra_server)}


def _wait_units(env, ws, aws, units, secs=120):
    """Each unit's copy at the source's commit (the commit block's generation and data.gxd the same)."""
    def done():
        for u in units:
            a, b = os.path.join(ws, u), os.path.join(aws, u)
            try:
                if gen_of(os.path.join(a, "commit.bin")) != gen_of(os.path.join(b, "commit.bin")):
                    return False
                if open(os.path.join(a, "data.gxd"), "rb").read() != open(os.path.join(b, "data.gxd"), "rb").read():
                    return False
            except OSError:
                return False
        return True
    end = time.time() + secs
    while time.time() < end and not done():
        time.sleep(0.3)
    assert done(), f"units {units} not copied after {secs}s\n{env.logs()}"


def _wait_rebuilt(env, aws, unit, secs=60):
    end = time.time() + secs
    while time.time() < end and not os.path.exists(os.path.join(aws, unit, "index.gxd")):
        time.sleep(0.3)
    assert os.path.exists(os.path.join(aws, unit, "index.gxd")), "the derived index was never rebuilt"


def test_dead_copy_folder_goes_with_its_source():
    """11/13/2026 (dead copy folders): the source drops a unit folder (osa_ingest archives a day at 20:30). The copy
    deletes what gxrepl copied AND the files it never copies (the rebuilt derived index, a writer's journal), then
    the folder, its seal and derived state (before: they stayed, promote-check said UNSEALED and refused every take
    over). The other unit is untouched; the folder coming back on the source is copied whole again."""
    env = Env("dead_folder", {"db": _derived_opts()})
    ws, aws = env.roots["db"]
    for u, seed in (("d1", 3), ("d2", 4)):
        os.makedirs(os.path.join(ws, u))
        assert writer(os.path.join(ws, u), "--commits", 4, "--no-ship", "--commit-block", "--seed", seed).returncode == 0
    env.start_server(extra_env={"GXREPL_SWEEP_SEC": "3600"})      # the Delete path alone (no minute sweep)
    env.start_client()
    try:
        _wait_units(env, ws, aws, ["d1", "d2"])
        _wait_rebuilt(env, aws, "d1")
        open(os.path.join(aws, "d1", "news.jnl"), "wb").write(b"\0" * 4096)        # a journal gxrepl never copies
        assert os.path.exists(os.path.join(aws, ".gxrepl", "units", "_d1"))
        shutil.rmtree(os.path.join(ws, "d1"))                                     # the source archives d1
        end = time.time() + 20
        while time.time() < end and os.path.exists(os.path.join(aws, "d1")):
            time.sleep(0.3)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert not os.path.exists(os.path.join(aws, "d1")), "the dead copy folder stayed: " + str(os.listdir(os.path.join(aws, "d1")))
        assert not os.path.exists(os.path.join(aws, ".gxrepl", "units", "_d1")), "its seal stayed"
        assert "removed the copy's folder d1: gone from the source" in log, log[-1500:]
        t = torn(env)
        assert t.returncode == 0 and "UNSEALED" not in t.stdout, t.stdout
        check_db(os.path.join(aws, "d2"))
        # the source makes d1 again: a whole new copy
        os.makedirs(os.path.join(ws, "d1"))
        assert writer(os.path.join(ws, "d1"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 9).returncode == 0
        _wait_units(env, ws, aws, ["d1"])
        _wait_rebuilt(env, aws, "d1")
        assert torn(env).returncode == 0
        check_db(os.path.join(aws, "d1"))
    finally:
        env.stop()


def test_dead_copy_folder_swept_after_a_stop():
    """11/13/2026 (dead copy folders): a dead copy folder left by a stop in the middle of its removal, or by a gxrepl
    from before this change (the 11/12-11/13 leftovers: only by_*.btx, derived.stamp, news.jnl), is swept at the
    next start and by the server's minute sweep (GXREPL_SWEEP_SEC, a folder made while it runs, its stage and
    derived state too). Never swept: a folder still holding a file gxrepl copies (a first copy in progress), a folder
    sealed "copying", a folder with a sub-folder, a folder with a file gxrepl does not copy and does not own (a node's
    own excluded database file), the set's root."""
    env = Env("dead_sweep", {"db": _derived_opts()})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d2"))
    assert writer(os.path.join(ws, "d2"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 4).returncode == 0
    env.start_server()
    env.start_client()
    try:
        _wait_units(env, ws, aws, ["d2"])
        env.stop()
        units = os.path.join(aws, ".gxrepl", "units")
        def leftover(name, files, seal, sub=False):
            d = os.path.join(aws, name)
            os.makedirs(d, exist_ok=True)
            for f in files:
                open(os.path.join(d, f), "wb").write(b"x" * 1024)
            if sub:
                os.makedirs(os.path.join(d, "inner"), exist_ok=True)
            if seal is not None:
                open(os.path.join(units, "_" + name), "w").write(seal)
        leftover("dead_a", ["index.gxd", "derived.stamp", "news.jnl"], "gen 6\n")   # the 11/13 case
        os.makedirs(os.path.join(aws, ".gxrepl", "derived"), exist_ok=True)
        open(os.path.join(aws, ".gxrepl", "derived", "_dead_a"), "w").write("6 ok\n")   # its derived state
        leftover("own_f", ["index.gxd", "x_databento.gxd"], "gen 3\n")            # a node's own excluded db file
        leftover("dead_h", ["index.gxd"], "copying-log\n")                        # a dead ship-log unit (no copied file)
        open(os.path.join(units, "_d2.gxrtmp"), "w").write("gen 1\n")             # a seal being written: never touched
        leftover("dead_b", [], "log 8 3 writer.jnl\n")                             # an empty leftover
        open(os.path.join(units, "_gone"), "w").write("gen 2\n")                   # a seal whose folder is gone
        leftover("live_c", ["data.gxd", "index.gxd"], "gen 4\n")                   # holds a file gxrepl copies
        leftover("copy_d", ["index.gxd"], "copying\n")                             # a copy in progress
        leftover("sub_e", ["index.gxd"], "gen 2\n", sub=True)                       # a folder with a sub-folder
        t = torn(env)
        assert t.returncode == 1 and "dead_a: UNSEALED" in t.stdout, t.stdout      # the refusal before the sweep
        env.start_server(extra_env={"GXREPL_SWEEP_SEC": "1", "GXREPL_DEAD_QUIET_SEC": "0"})
        end = time.time() + 30
        while time.time() < end and (os.path.exists(os.path.join(aws, "dead_a")) or os.path.exists(os.path.join(units, "_gone"))):
            time.sleep(0.3)
        log = open(os.path.join(env.dir, "server.log")).read()
        for gone in ("dead_a", "dead_b"):
            assert not os.path.exists(os.path.join(aws, gone)), (gone, log[-1500:])
            assert not os.path.exists(os.path.join(units, "_" + gone)), gone
        assert not os.path.exists(os.path.join(units, "_gone"))
        assert not os.path.exists(os.path.join(aws, "dead_h")), "a dead copying-log unit stayed"
        assert os.path.exists(os.path.join(units, "_d2.gxrtmp")), "a seal temp file was removed"
        os.unlink(os.path.join(units, "_d2.gxrtmp"))
        assert "removed the seal of gone: the folder is gone" in log, log[-1500:]
        assert not os.path.exists(os.path.join(aws, ".gxrepl", "derived", "_dead_a")), "its derived state stayed"
        for kept in ("live_c", "copy_d", "sub_e", "own_f"):
            assert os.path.isdir(os.path.join(aws, kept)), (kept, "was swept")
        # the minute sweep (GXREPL_SWEEP_SEC=1) with the server up: a dead folder made now, one with a stage
        leftover("dead_g", ["index.gxd", "news.jnl"], "gen 9\n")
        sg = os.path.join(aws, ".gxrepl", "stage", "_dead_g")
        os.makedirs(sg, exist_ok=True)
        open(os.path.join(sg, ".stage"), "w").write("unit dead_g\nsince 0\nfrom 8\n")
        end = time.time() + 20
        while time.time() < end and os.path.exists(os.path.join(aws, "dead_g")):
            time.sleep(0.3)
        assert not os.path.exists(os.path.join(aws, "dead_g")), "the minute sweep did not run"
        assert not os.path.exists(sg), "its stage stayed"
        assert os.path.isdir(os.path.join(aws, "d2")) and os.path.exists(os.path.join(aws, "d2", "commit.bin"))
        for kept in ("live_c", "copy_d", "sub_e", "own_f"):
            shutil.rmtree(os.path.join(aws, kept))
            os.unlink(os.path.join(units, "_" + kept))
        t = torn(env)
        assert t.returncode == 0, t.stdout
    finally:
        env.stop()


def test_dead_copy_folder_sweep_guards():
    """11/13/2026 (dead copy folders, review): never swept while the set's writer holds its lock (swept once it lets
    go); a live unit whose folder name has '#' (its seal key looks like a nested path) keeps its seal; a server whose
    set is accept = both (a source too) never sweeps."""
    import fcntl
    opts = _derived_opts(extra_server=("writer_lock = w.lock",))
    env = Env("dead_guards", {"db": opts})
    ws, aws = env.roots["db"]
    for u, seed in (("d2", 4), ("h#x", 5)):
        os.makedirs(os.path.join(ws, u))
        assert writer(os.path.join(ws, u), "--commits", 3, "--no-ship", "--commit-block", "--seed", seed).returncode == 0
    env.start_server(extra_env={"GXREPL_SWEEP_SEC": "1", "GXREPL_DEAD_QUIET_SEC": "0"})
    env.start_client()
    units = os.path.join(aws, ".gxrepl", "units")
    try:
        _wait_units(env, ws, aws, ["d2", "h#x"])
        assert os.path.exists(os.path.join(units, "_h#x"))
        time.sleep(3)                                                            # several sweeps
        assert os.path.exists(os.path.join(units, "_h#x")), "the '#' unit lost its seal"
        assert torn(env).returncode == 0
        fd = os.open(os.path.join(aws, "w.lock"), os.O_RDWR | os.O_CREAT, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX)                                           # the writer runs
        try:
            d = os.path.join(aws, "dead_w")
            os.makedirs(d)
            open(os.path.join(d, "index.gxd"), "wb").write(b"x" * 1024)
            open(os.path.join(units, "_dead_w"), "w").write("gen 2\n")
            time.sleep(4)
            assert os.path.isdir(d), "swept while the writer held its lock"
        finally:
            fcntl.flock(fd, fcntl.LOCK_UN)
            os.close(fd)
        end = time.time() + 20
        while time.time() < end and os.path.isdir(d):
            time.sleep(0.3)
        assert not os.path.isdir(d), "not swept once the writer let go"
    finally:
        env.stop()
    # the default quiet time (600 s): a dead folder whose files changed just now is kept by the sweep
    env = Env("dead_quiet", {"db": _derived_opts()})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d2"))
    assert writer(os.path.join(ws, "d2"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 4).returncode == 0
    env.start_server(extra_env={"GXREPL_SWEEP_SEC": "1"})
    env.start_client()
    try:
        _wait_units(env, ws, aws, ["d2"])
        d = os.path.join(aws, "dead_q")
        os.makedirs(d)
        open(os.path.join(d, "index.gxd"), "wb").write(b"x" * 1024)
        open(os.path.join(aws, ".gxrepl", "units", "_dead_q"), "w").write("gen 2\n")
        time.sleep(4)
        assert os.path.isdir(d), "a folder changed just now was swept"
    finally:
        env.stop()
    both = _derived_opts()
    both["accept"] = "both"
    env = Env("dead_both", {"db": both})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d2"))
    assert writer(os.path.join(ws, "d2"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 4).returncode == 0
    env.start_server(extra_env={"GXREPL_SWEEP_SEC": "1", "GXREPL_DEAD_QUIET_SEC": "0"})
    env.start_client()
    try:
        _wait_units(env, ws, aws, ["d2"])
        d = os.path.join(aws, "dead_b")
        os.makedirs(d)
        open(os.path.join(d, "index.gxd"), "wb").write(b"x" * 1024)
        units = os.path.join(aws, ".gxrepl", "units")
        open(os.path.join(units, "_dead_b"), "w").write("gen 2\n")
        time.sleep(4)
        assert os.path.isdir(d), "a server with accept = both swept"
    finally:
        env.stop()


def test_dead_copy_folder_never_swept_on_the_source():
    """11/13/2026 (dead copy folders): a pushing client (the source) never sweeps: a folder there that looks dead
    (a derived index, no commit block, a whole seal left by an old failback pull) stays."""
    env = Env("dead_source", {"db": _derived_opts()})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d2"))
    assert writer(os.path.join(ws, "d2"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 4).returncode == 0
    d9 = os.path.join(ws, "d9")
    os.makedirs(d9)
    open(os.path.join(d9, "index.gxd"), "wb").write(b"x" * 1024)
    os.makedirs(os.path.join(ws, ".gxrepl", "units"), exist_ok=True)
    open(os.path.join(ws, ".gxrepl", "units", "_d9"), "w").write("gen 2\n")
    env.start_server()
    env.start_client()
    try:
        _wait_units(env, ws, aws, ["d2"])
        time.sleep(3)
        assert os.path.isdir(d9) and os.path.exists(os.path.join(d9, "index.gxd")), "the source swept its own folder"
    finally:
        env.stop()


def test_derived_index_rebuilt_on_copy():
    """11/10/2026: derived = index.gxd: the sender never ships it, the copy rebuilds it (derived_rebuild) once a
    folder's generation is quiet for derived_delay; "gxrepl torn" warns (INDEXES, exit 0) while the derived index is
    missing or stale (the take over rebuilds it) and refuses a failed rebuild (exit 1); after the rebuild the copy
    reads the same as the source."""
    rebuild = f"{os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}"
    opts = {"client": CB_OPTS["client"] + ["derived = index.gxd"],
            "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 3"]}
    env = Env("derived", {"db": opts})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 20, "--no-ship", "--commit-block").returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        assert not os.path.exists(os.path.join(aws, "index.gxd")), "the derived index was shipped"
        t = torn(env)
        # 11/11/2026 (r4e): indexes behind are a warning, not torn (the take over's writer open rebuilds them)
        assert t.returncode == 0 and "INDEXES" in t.stdout and "derived indexes missing" in t.stdout, t.stdout
        end = time.time() + 30
        while time.time() < end and "INDEXES" in torn(env).stdout:
            time.sleep(0.5)
        t = torn(env)
        assert t.returncode == 0 and "INDEXES" not in t.stdout, t.stdout + env.logs()
        check_db(aws)                                      # the rebuilt index agrees with the data
        # A new commit: stale until the copy rebuilds it again
        assert writer(ws, "--commits", 3, "--no-ship", "--commit-block", "--seed", 31).returncode == 0
        assert env.once().returncode == 0
        t = torn(env)
        assert t.returncode == 0 and "stale" in t.stdout and "INDEXES" in t.stdout, t.stdout
        end = time.time() + 30
        while time.time() < end and "INDEXES" in torn(env).stdout:
            time.sleep(0.5)
        t = torn(env)
        assert t.returncode == 0 and "INDEXES" not in t.stdout, t.stdout
        check_db(aws)
        assert "derived indexes rebuilt" in open(os.path.join(env.dir, "server.log")).read()
    finally:
        env.stop()
    # A rebuild that fails is a failed state, never promoted
    with open(env.srv_conf) as f:
        conf = f.read().replace(f"derived_rebuild = {rebuild}", "derived_rebuild = false")
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    assert writer(ws, "--commits", 2, "--no-ship", "--commit-block", "--seed", 32).returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        end = time.time() + 30
        while time.time() < end and "failed" not in torn(env).stdout:
            time.sleep(0.5)
        t = torn(env)
        assert t.returncode == 1 and "failed" in t.stdout, t.stdout
    finally:
        env.stop()


def _seal_fields(aws, unit):
    with open(os.path.join(aws, ".gxrepl", "units", "_" + unit.replace("/", "#"))) as f:
        return f.read().split()


def _rebuilds_of(env, unit):
    log = open(os.path.join(env.dir, "server.log")).read()
    return log.count(f": {unit}: derived indexes rebuilt")


def _wait_caught_up(env, timeout=60):
    wait_status(env, lambda s: s["sets"]["db"]["streams"] and
                all(x["lag"] == 0 for x in s["sets"]["db"]["streams"].values()), timeout)


def _wait_indexes(env, timeout=60):
    end = time.time() + timeout
    while time.time() < end and "INDEXES" in torn(env).stdout:
        time.sleep(0.5)
    t = torn(env)
    assert t.returncode == 0 and "INDEXES" not in t.stdout, t.stdout + env.logs()


def test_derived_seal_per_folder():
    """11/14/2026 (seal per folder, review finding 1 on the per-object apply): one ship log stream feeds two
    folders (--stream). A record that changes only d2 moves the stream's position in d1's seal as well (promote-check
    compares it with the applied position), but d1's own last change, the seal's 4th field, stays: d1's derived index,
    stamped at that record, stays current, so the idle folder is never rebuilt (before: every write to any folder of
    the stream rebuilt them all). A seal from before the 4th field reads as changed at its lsn: one rebuild, then stable
    again. A writer's restart (its open record: a verify of the stream) that finds a folder as sealed keeps its
    indexes; one that repairs a folder's data rebuilds only that folder.
    "gxrepl torn" stays clean throughout."""
    rebuild = f"{os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}"
    opts = {"client": CB_OPTS["client"] + ["derived = index.gxd"],
            "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 1",
                                           "derived_busy_rebuild = 2"]}
    env = Env("seal_folder", {"db": opts})
    ws, aws = env.roots["db"]
    for d in ("d1", "d2"):
        os.makedirs(os.path.join(ws, d), exist_ok=True)
    w = lambda d, n, seed: writer(os.path.join(ws, d), "--commits", n, "--commit-block", "--ship-root", ws,
                                  "--stream", "shared", "--seed", seed)
    assert w("d1", 4, 1).returncode == 0
    assert w("d2", 4, 2).returncode == 0
    assert streams_of(ws) == ["shared"], streams_of(ws)
    env.start_server()
    env.start_client()
    try:
        _wait_caught_up(env)
        _wait_indexes(env)
        time.sleep(3)
        stamp = open(os.path.join(aws, "d1", "derived.stamp")).read()
        s1 = _seal_fields(aws, "d1")
        assert s1[0] == "log" and len(s1) == 5 and s1[3] == "shared", s1
        n1, n2 = _rebuilds_of(env, "d1"), _rebuilds_of(env, "d2")
        assert n1 >= 1 and n2 >= 1, env.logs()
        assert stamp.split()[3] == s1[4], (stamp, s1)          # stamped at d1's own last change
        # Writes to d2 only: the stream's position moves on in d1's seal, its own last change stays
        for i in range(3):
            assert w("d2", 2, 10 + i).returncode == 0
            _wait_caught_up(env)
            _wait_indexes(env)
        time.sleep(4)                                          # several derived_delay and busy periods
        _wait_indexes(env)
        s1b, s2b = _seal_fields(aws, "d1"), _seal_fields(aws, "d2")
        print(f"    d1 seal {s1} -> {s1b}, d2 seal {s2b}; rebuilds d1 {n1} -> {_rebuilds_of(env, 'd1')}, "
              f"d2 {n2} -> {_rebuilds_of(env, 'd2')}", flush=True)
        assert int(s1b[2]) > int(s1[2]) and s1b[2] == s2b[2], (s1b, s2b)   # both at the stream's position
        assert s1b[4] == s1[4], (s1, s1b)                                  # d1 unchanged since
        assert int(s2b[4]) > int(s1[4]), s2b
        assert _rebuilds_of(env, "d1") == n1, env.logs()                   # the idle folder: never rebuilt
        assert _rebuilds_of(env, "d2") > n2
        assert open(os.path.join(aws, "d1", "derived.stamp")).read() == stamp
        check_db(os.path.join(aws, "d1"))
        check_db(os.path.join(aws, "d2"))
        # A seal from before the 4th field (an older gxrepl wrote it): changed at most at its lsn
        sp = os.path.join(aws, ".gxrepl", "units", "_d1")
        with open(sp, "w") as f:
            f.write(" ".join(s1b[:4]) + "\n")
        end = time.time() + 30
        while time.time() < end and _rebuilds_of(env, "d1") == n1:
            time.sleep(0.5)
        assert _rebuilds_of(env, "d1") == n1 + 1, env.logs()
        _wait_indexes(env)
        assert open(os.path.join(aws, "d1", "derived.stamp")).read().split()[3] == s1b[2]
        assert w("d2", 2, 30).returncode == 0
        _wait_caught_up(env)
        time.sleep(4)
        _wait_indexes(env)
        s1c = _seal_fields(aws, "d1")
        assert len(s1c) == 5 and s1c[4] == s1b[2], s1c                    # carried as its lsn from then on
        assert _rebuilds_of(env, "d1") == n1 + 1, env.logs()
        # A writer's restart on d1 (an open record: the stream's verify) finds both folders as sealed: neither is
        # rebuilt (review 4: the verify must have run)
        m2 = _rebuilds_of(env, "d2")
        v0 = open(os.path.join(env.dir, "client.log")).read().count("stream shared verified")
        assert w("d1", 0, 40).returncode == 0
        _wait_caught_up(env)
        end = time.time() + 30
        while time.time() < end and open(os.path.join(env.dir, "client.log")).read().count("stream shared verified") == v0:
            time.sleep(0.2)
        assert open(os.path.join(env.dir, "client.log")).read().count("stream shared verified") > v0, env.logs()
        time.sleep(4)
        _wait_indexes(env)
        print(f"    writer restart on d1: d1 rebuilds {n1 + 1} -> {_rebuilds_of(env, 'd1')}, d2 {m2} -> "
              f"{_rebuilds_of(env, 'd2')}", flush=True)
        assert _rebuilds_of(env, "d2") == m2, env.logs()
        assert _rebuilds_of(env, "d1") == n1 + 1, env.logs()       # the verify re-sent d1 as sealed: kept
        # A verify that repairs d1's data (a damaged copy, same generation): that is a change, rebuilt
        dp = os.path.join(aws, "d1", "data.gxd")
        with open(dp, "r+b") as f:
            f.seek(os.path.getsize(dp) // 2)
            b = f.read(1)
            f.seek(-1, 1)
            f.write(bytes([b[0] ^ 0xFF]))
        assert w("d2", 0, 50).returncode == 0                      # a restart: the stream's verify
        _wait_caught_up(env)
        end = time.time() + 30
        while time.time() < end and _rebuilds_of(env, "d1") == n1 + 1:
            time.sleep(0.5)
        _wait_indexes(env)
        print(f"    repaired d1: d1 seal {_seal_fields(aws, 'd1')}, rebuilds d1 {_rebuilds_of(env, 'd1')}, "
              f"d2 {_rebuilds_of(env, 'd2')}", flush=True)
        assert _rebuilds_of(env, "d1") == n1 + 2, env.logs()
        assert _rebuilds_of(env, "d2") == m2, env.logs()
        assert open(os.path.join(ws, "d1", "data.gxd"), "rb").read() == open(dp, "rb").read()
        # Review 1: a verify that only cuts d1's data file (the copy's is longer, the same generation): a change too
        with open(dp, "ab") as f:
            f.write(os.urandom(5000))
        n_now = _rebuilds_of(env, "d1")
        assert w("d2", 0, 60).returncode == 0
        _wait_caught_up(env)
        end = time.time() + 30
        while time.time() < end and _rebuilds_of(env, "d1") == n_now:
            time.sleep(0.5)
        _wait_indexes(env)
        assert _rebuilds_of(env, "d1") == n_now + 1, env.logs()
        assert _rebuilds_of(env, "d2") == m2, env.logs()
        assert open(os.path.join(ws, "d1", "data.gxd"), "rb").read() == open(dp, "rb").read()
        check_db(os.path.join(aws, "d1"))
        check_db(os.path.join(aws, "d2"))
    finally:
        env.stop()


def test_missing_root_is_empty():
    """11/10/2026: a set whose root does not exist yet (db/news/oodb before its first nightly publish) is an
    empty set on both nodes, not an error; once the source makes the folder, its first publish replicates."""
    env = Env("missing_root", {"db": {"client": ["deletes = yes", "include = commit.bin"],
                                      "server": ["include = commit.bin"]}})
    ws, aws = env.roots["db"]
    shutil.rmtree(ws)
    shutil.rmtree(aws, ignore_errors=True)
    env.start_server()
    env.start_client()
    try:
        r = env.once()
        assert r.returncode == 0, "once with a missing root: " + r.stdout + env.logs()
        wait_status(env, lambda s: s.get("sets", {}).get("db", {}).get("ok") is True
                    and not s["sets"]["db"].get("last_error"), 30)
        assert not os.path.exists(aws) or not tree(aws), "files on the copy of a missing root"
        # The first publish: a day folder swapped in by a rename, as the nightly does
        tmp = ws + ".tmp"
        os.makedirs(os.path.join(tmp, "2026/11/09"))
        assert writer(os.path.join(tmp, "2026/11/09"), "--commits", 4).returncode == 0
        put(os.path.join(tmp, "2026/11/09/commit.bin"), os.urandom(64))
        os.rename(tmp, ws)
        wait_same(env, "db", 60)
        check_db(os.path.join(aws, "2026/11/09"))
        st = env.status()["sets"]["db"]
        assert st["ok"] and not st["last_error"], st
    finally:
        env.stop()


def test_power_loss_object_service():
    """A power cut on the server while a client adds and changes objects: after the restart every
    object the server answered is there with its last answered bytes, and the database walks."""
    env = Env("pl_obj", {"db": {}})
    srvdb = os.path.join(env.dir, "srvdb")
    os.makedirs(srvdb)
    orders = os.path.join(srvdb, "orders.gxd")
    with open(env.srv_conf) as f:
        conf = f.read()
    k = conf.index("[set db]")
    conf = conf[:k] + f"[db orders]\npath = {orders}\naccess = write\n\n" + conf[k:]
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    img = PowerImage(os.path.join(env.dir, "image"), [srvdb])
    answered = os.path.join(env.dir, "answered.txt")
    open(answered, "w").close()
    rng = random.Random(4)
    counts = []
    try:
        for c in range(5):
            img.baseline()
            env.start_server(img.env())
            out = open(os.path.join(env.dir, f"stream{c}.txt"), "w")
            s = subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "objstream", env.cli_conf, "orders", str(c * 100000 + 1000000)],
                                 stdout=out, stderr=subprocess.STDOUT)
            time.sleep(rng.uniform(0.6, 2.0))
            env.kill("server")              # the cut, on the server's node
            s.kill()
            s.wait()
            out.close()
            lines = [ln for ln in open(out.name).read().splitlines() if ln[:2] in ("A ", "C ")]
            assert lines, f"cycle {c}: no object was answered: " + open(out.name).read()[-500:]
            with open(answered, "a") as f:
                f.write("\n".join(lines) + "\n")
            img.cut(srvdb, random.Random(c) if c % 2 else None)
            env.start_server()
            r = run([os.path.join(BIN, "gxrepl_testwriter"), "objverify", env.cli_conf, "orders", answered], timeout=120)
            assert r.returncode == 0 and r.stdout.startswith("objverify ok"), f"cycle {c}: {r.stdout}\n{env.logs()}"
            counts.append(r.stdout.split()[2])
            env.stop()
    finally:
        env.stop()
    print("  pl_obj answered after each cut: " + ", ".join(counts), flush=True)


def test_power_loss_both_nodes():
    power_loss_cycles("pl_both", "both", 6)


def test_power_loss_source_node():
    power_loss_cycles("pl_source", "source", 4)


def test_power_loss_copy_node():
    power_loss_cycles("pl_copy", "copy", 4)


TESTS = [v for k, v in sorted(globals().items()) if k.startswith("test_")]


def main():
    global BIN, TMP
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("-k", default="")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    BIN = os.path.abspath(a.bin)
    TMP = tempfile.mkdtemp(prefix="gxrepl_test_")
    make_pki(os.path.join(TMP, "pki"))
    failed = 0
    for t in TESTS:
        if a.k and a.k not in t.__name__:
            continue
        t0 = time.time()
        try:
            t()
            print(f"PASS {t.__name__} ({time.time() - t0:.1f}s)", flush=True)
        except Exception as e:
            failed += 1
            print(f"FAIL {t.__name__} ({time.time() - t0:.1f}s): {e}", flush=True)
    if not a.keep and not failed:
        shutil.rmtree(TMP, ignore_errors=True)
    else:
        print("kept", TMP)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
