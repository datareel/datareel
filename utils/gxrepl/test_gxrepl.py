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

    def start_server(self, extra_env=None):
        self.srv_log = open(os.path.join(self.dir, "server.log"), "a")
        e = dict(os.environ, **extra_env) if extra_env else None
        self.server = subprocess.Popen([os.path.join(BIN, "gxrepl"), "serve", "--config", self.srv_conf],
                                       stdout=self.srv_log, stderr=subprocess.STDOUT, env=e)
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", self.port), 0.2).close()
                return
            except OSError:
                time.sleep(0.05)
        raise RuntimeError("server did not start: " + open(os.path.join(self.dir, "server.log")).read())

    def start_client(self):
        self.cli_log = open(os.path.join(self.dir, "client.log"), "a")
        self.client = subprocess.Popen([os.path.join(BIN, "gxrepl"), "run", "--config", self.cli_conf],
                                       stdout=self.cli_log, stderr=subprocess.STDOUT)

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


def writer_bg(root, *args):
    return subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "write", root, *map(str, args)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


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
            if (lag and max(lag) == 0) or time.time() > end:
                break
            time.sleep(0.2)
        assert st["records"] > 0, st
        assert lag and max(lag) == 0, st
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
