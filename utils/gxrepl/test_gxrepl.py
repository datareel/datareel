#!/usr/bin/env python3
"""Tests for gxrepl, DataReel 7.3 database replication over gxStream.

    python3 test_gxrepl.py --bin DIR [-k NAME]

DIR holds gxrepl and gxrepl_testwriter. Needs openssl (the command) to
make a test CA and certificates. Everything runs on 127.0.0.1 in a
temporary folder. Each test starts its own server and client.
"""
import argparse
import glob
import hashlib
import json
import os
import random
import re
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
        self.server_bin = os.path.join(BIN, "gxrepl")   # 11/14/2026: another build's gxrepl (mixed versions)
        self.client_bin = os.path.join(BIN, "gxrepl")

    def start_server(self, extra_env=None, args=()):
        self.srv_log = open(os.path.join(self.dir, "server.log"), "a")
        e = dict(os.environ, **extra_env) if extra_env else None
        self.server = subprocess.Popen([self.server_bin, "serve", "--config", self.srv_conf, *args],
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
        self.client = subprocess.Popen([self.client_bin, "run", "--config", self.cli_conf],
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


def _vec_env(name, extra_global="", vec_dir=None, server_env=None):
    """11/14/2026 (7.4): a server whose [db news] answers vector searches from a 5-day .vixset"""
    env = Env(name, {"db": {}})
    vd = vec_dir or os.path.join(env.dir, "vec")
    os.makedirs(vd, exist_ok=True)
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "vmake", vd, "5", "400"])
    assert r.returncode == 0, r.stdout
    with open(env.srv_conf) as f:
        conf = f.read()
    k = conf.index("[set db]")
    conf = conf[:k] + f"{extra_global}\n[db news]\npath = {vd}/news.gxd\naccess = read\nvectors = {vd}/news.vixset\n\n" + conf[k:]
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    env.start_server(extra_env=server_env)
    return env, vd


def _vs(env, *a):
    return run([os.path.join(BIN, "gxrepl_testwriter"), "vsearch", env.cli_conf, "news", *map(str, a)], timeout=60)


def _vl(vd, *a):
    return run([os.path.join(BIN, "gxrepl_testwriter"), "vlocal", os.path.join(vd, "news.vixset"), *map(str, a)], timeout=60)


def test_vector_search_service():
    """11/14/2026 (7.4): op 7 VECTOR_SEARCH and op 8 VECTOR_INFO of the object service: the answers equal a local
    gxVectorSet search (ids, distances, order, file paths relative to the manifest), a day range skips the files
    outside it, every malformed request is refused (and the channel still answers), a missing day file is
    counted (strict: an error)."""
    env, vd = _vec_env("vecsvc")
    try:
        for qi in range(6):
            for extra in ((), (20261111, 20261112)):
                a, b = _vs(env, 10, qi, *extra), _vl(vd, 10, qi, *extra)
                assert a.returncode == 0 and b.returncode == 0, a.stdout + b.stdout + env.logs()
                assert a.stdout == b.stdout, "remote and local differ:\n" + a.stdout + "\n" + b.stdout
                assert len(a.stdout.strip().split("\n")) == 11, a.stdout
                if extra:
                    assert a.stdout.startswith("skipped 3 missing 0"), a.stdout
                    assert all(l.split()[2] in ("day1.vix", "day2.vix") for l in a.stdout.strip().split("\n")[1:]), a.stdout
        r = run([os.path.join(BIN, "gxrepl_testwriter"), "vinfo", env.cli_conf, "news"])
        assert r.returncode == 0 and "version 1 dim 16 element 1 metric 1 files 5 nodes 2000 deleted 0 spec vec-test" in r.stdout, r.stdout
        r = run([os.path.join(BIN, "gxrepl_testwriter"), "vbad", env.cli_conf, "news"])
        assert r.returncode == 0 and "vbad ok" in r.stdout, r.stdout + env.logs()
        # the example client
        pki = os.path.join(TMP, "pki")
        ex = [os.path.join(BIN, "object_client"), "127.0.0.1", str(env.port), "aws-test", f"{pki}/cli.pem",
              f"{pki}/cli.key", f"{pki}/ca.pem", "news"]
        r = run(ex + ["--vinfo"], timeout=30)
        assert r.returncode == 0 and "dimension 16" in r.stdout and "5 file(s), 2000 nodes" in r.stdout, r.stdout
        r = subprocess.run(ex + ["--vsearch", "3"], input=" ".join(["0.1"] * 16), stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, timeout=30)
        assert r.returncode == 0 and r.stdout.startswith("3 result(s)") and r.stdout.count("\nid ") == 3, r.stdout
        os.rename(os.path.join(vd, "day0.vix"), os.path.join(vd, "away.vix"))
        r = _vs(env, 10, 1)
        assert r.returncode == 0 and r.stdout.startswith("skipped 0 missing 1"), r.stdout
        assert "day0.vix" not in r.stdout
        r = _vs(env, 10, 1, 20261110, 20261114, "strict")
        assert r.returncode == 3 and "missing" in r.stdout, r.stdout
    finally:
        env.stop()


def test_vector_search_busy_cap():
    """11/14/2026 (7.4): vector_max_concurrent searches at once; one more waits up to 5 s, then is answered busy"""
    env, vd = _vec_env("vecbusy", extra_global="vector_max_concurrent = 1",
                       server_env={"GXDB_VECTOR_TEST_HOLD_MS": "7000"})
    try:
        p1 = subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "vsearch", env.cli_conf, "news", "5", "0"],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(1)
        t0 = time.time()
        r2 = _vs(env, 5, 1)
        waited = time.time() - t0
        o1 = p1.communicate(timeout=60)[0]
        assert p1.returncode == 0, o1
        assert r2.returncode == 3 and "busy" in r2.stdout, r2.stdout
        assert 4.5 <= waited <= 7.5, f"waited {waited:.1f} s before busy"
    finally:
        env.stop()


def test_vector_search_copy_waits_for_apply_lock():
    """11/14/2026 (7.4): vectors inside a set this server copies are searched under the set's apply lock: a search
    while an apply holds it waits, then answers"""
    import fcntl
    env = Env("veccopy", {"db": {"client": ["include = *.vix", "include = *.vixset"],
                                 "server": ["include = *.vix", "include = *.vixset"]}})
    ws, aws = env.roots["db"]
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "vmake", ws, "3", "300"])
    assert r.returncode == 0, r.stdout
    with open(env.srv_conf) as f:
        conf = f.read()
    k = conf.index("[set db]")
    conf = conf[:k] + f"[db news]\npath = {aws}/news.gxd\naccess = read\nvectors = {aws}/news.vixset\n\n" + conf[k:]
    with open(env.srv_conf, "w") as f:
        f.write(conf)
    env.start_server()
    try:
        assert env.once().returncode == 0, env.logs()
        assert os.path.exists(os.path.join(aws, "news.vixset")) and os.path.exists(os.path.join(aws, "day2.vix"))
        r = _vs(env, 5, 2)
        assert r.returncode == 0 and len(r.stdout.strip().split("\n")) == 6, r.stdout + env.logs()
        os.makedirs(os.path.join(aws, ".gxrepl"), exist_ok=True)
        lk = open(os.path.join(aws, ".gxrepl", "apply.jnl"), "a+")
        fcntl.flock(lk, fcntl.LOCK_EX)                      # an apply in progress
        p = subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "vsearch", env.cli_conf, "news", "5", "2"],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(2.5)
        assert p.poll() is None, "the search did not wait for the apply lock: " + p.stdout.read()
        fcntl.flock(lk, fcntl.LOCK_UN)
        lk.close()
        out = p.communicate(timeout=30)[0]
        assert p.returncode == 0 and out == r.stdout, out
    finally:
        env.stop()


def _vix_opts():
    """11/14/2026 (7.4): a set whose vectors.vix is derived from the records' vector members: rebuilt by the copy"""
    rb = (f"{os.path.join(BIN, 'vix_rebuild')} --data {{dir}}/data.gxd --index {{dir}}/vectors.vix --member infohog:1 "
          f"--tags -,-,infohog:2,- --dim 16 --m 8 --efc 100 --spec vtest --stamp-dir {{dir}} --lsn {{lsn}}")
    return {"client": CB_OPTS["client"] + ["derived = vectors.vix"],
            "server": CB_OPTS["server"] + ["derived = vectors.vix", f"derived_rebuild = {rb}", "derived_delay = 1"]}


def _gen_or_none(path):
    try:
        return gen_of(path)
    except OSError:
        return None


def _digest(path, kind="logical"):
    r = run([os.path.join(BIN, "vix_rebuild"), "--digest", path, "--digest-kind", kind])
    assert r.returncode == 0, r.stdout
    return r.stdout.split()[1], int(r.stdout.split()[2])


def test_derived_vix_rebuilt_on_copy():
    """11/14/2026 (7.4, phase D item 5): a vector index derived from the records' vector members. The writer keeps
    vectors.vix in its own journal transactions; no .vix bytes cross the wire (the ship log leaves the file out, the
    file sync never sends it); the copy rebuilds it (derived_rebuild running vix_rebuild); "gxrepl torn" says the index
    is behind (a warning), never TORN; the copy's rebuild holds the same live nodes as the primary's index after adds,
    vector updates and deletes with reused blocks (logical digest), and is byte for byte a rebuild of the primary's
    data; the rebuilt file is marked derived from members."""
    env = Env("vixderived", {"db": _vix_opts()})
    ws, aws = env.roots["db"]
    w = run([os.path.join(BIN, "gxrepl_testwriter"), "vwrite", ws, "--commits", "30", "--per", "12", "--seed", "3"])
    assert w.returncode == 0 and "vwrite ok" in w.stdout, w.stdout
    sl = run([os.path.join(BIN, "gxrepl"), "shiplog", os.path.join(ws, ".gxrepl", "log", os.listdir(os.path.join(ws, ".gxrepl", "log"))[0])])
    assert sl.returncode == 0 and "data.gxd" in sl.stdout and "vectors.vix" not in sl.stdout, sl.stdout[-2000:]
    env.start_server()
    env.start_client()
    try:
        end = time.time() + 60
        while time.time() < end and _gen_or_none(os.path.join(aws, "commit.bin")) != gen_of(os.path.join(ws, "commit.bin")):
            time.sleep(0.3)
        assert _gen_or_none(os.path.join(aws, "commit.bin")) == gen_of(os.path.join(ws, "commit.bin")), env.logs()
        # behind until the copy rebuilds it: a warning, never torn
        end = time.time() + 60
        saw_warning = False
        while time.time() < end:
            t = torn(env)
            assert t.returncode == 0 and "TORN" not in t.stdout, t.stdout
            if "INDEXES" in t.stdout:
                saw_warning = True
            elif os.path.exists(os.path.join(aws, "vectors.vix")):
                break
            time.sleep(0.3)
        assert os.path.exists(os.path.join(aws, "vectors.vix")), "never rebuilt: " + env.logs()
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "derived indexes rebuilt" in log, log
        # the same live nodes as the primary's own index (built in commit order, with tombstones and reused blocks)
        lp, np_ = _digest(os.path.join(ws, "vectors.vix"))
        lc, nc = _digest(os.path.join(aws, "vectors.vix"))
        assert lp == lc and np_ == nc and nc > 100, (lp, np_, lc, nc)
        # byte for byte a rebuild of the primary's data
        tmpix = os.path.join(env.dir, "primary_rebuild.vix")
        r = run([os.path.join(BIN, "vix_rebuild"), "--data", os.path.join(ws, "data.gxd"), "--index", tmpix,
                 "--member", "infohog:1", "--tags", "-,-,infohog:2,-", "--dim", "16", "--m", "8", "--efc", "100", "--spec", "vtest"])
        assert r.returncode == 0, r.stdout
        assert _digest(tmpix, "bytes")[0] == _digest(os.path.join(aws, "vectors.vix"), "bytes")[0], "rebuilds differ"
        assert _digest(tmpix)[0] == lp
        r = run([os.path.join(BIN, "vix_rebuild"), "--digest", os.path.join(aws, "vectors.vix")])
        assert " opt 3" in r.stdout, "the rebuilt index is not marked derived from members: " + r.stdout
        # more commits: behind (a warning) until rebuilt again, then equal again
        w = run([os.path.join(BIN, "gxrepl_testwriter"), "vwrite", ws, "--commits", "10", "--per", "12", "--seed", "9"])
        assert w.returncode == 0, w.stdout
        end = time.time() + 60
        while time.time() < end and (_digest(os.path.join(aws, "vectors.vix"))[0] != _digest(os.path.join(ws, "vectors.vix"))[0]):
            t = torn(env)
            assert "TORN" not in t.stdout, t.stdout
            time.sleep(0.5)
        assert _digest(os.path.join(aws, "vectors.vix"))[0] == _digest(os.path.join(ws, "vectors.vix"))[0], env.logs()
        assert torn(env).returncode == 0
    finally:
        env.stop()


def _vix_incr_opts(extra=()):
    """11/14/2026 (7.4 merge): the derived vectors.vix kept object by object with the built-in gxvmember extractor"""
    o = _vix_opts()
    o["server"] = o["server"] + ["derived_apply = incremental", "derived_extractor = gxvmember",
                                 "derived_extractor_config = member=infohog:1 tags=-,-,infohog:2,-",
                                 "derived_source = data.gxd", "derived_count = vectors.vix",
                                 "derived_delay = 0", "derived_busy_rebuild = 2"] + list(extra)
    return o


def _vwrite_bg(ws, *args):
    e = dict(os.environ, GXDSHIP_OBJECTS="1")
    return subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "vwrite", ws, *map(str, args)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)


def _vix_stamped(env, aws, secs=90):
    st = os.path.join(aws, "vectors.vix.stamp")
    end = time.time() + secs
    while time.time() < end and (not os.path.exists(st) or "INDEXES" in torn(env).stdout):
        time.sleep(0.3)
    assert os.path.exists(st), "the rebuilt vectors.vix was never stamped\n" + env.logs()


def _vix_settle(env, ws, aws, secs=120):
    """the copy at the primary's generation and its vectors.vix holding the primary's live nodes"""
    end = time.time() + secs
    while time.time() < end:
        if _gen_or_none(os.path.join(aws, "commit.bin")) == gen_of(os.path.join(ws, "commit.bin")) and \
           os.path.exists(os.path.join(aws, "vectors.vix")) and "INDEXES" not in torn(env).stdout and \
           _digest(os.path.join(aws, "vectors.vix"))[0] == _digest(os.path.join(ws, "vectors.vix"))[0]:
            return
        time.sleep(0.5)
    raise AssertionError("the copy's vectors.vix never matched the primary's: " + str(_digest(os.path.join(aws, "vectors.vix"))) +
                         " " + str(_digest(os.path.join(ws, "vectors.vix"))) + "\n" + env.logs())


def test_derived_vix_incremental_gxvmember():
    """11/14/2026 (7.4 merge): derived_apply = incremental with the built-in gxvmember extractor: after the first
    rebuild stamps vectors.vix, the copy applies its records' vector members object by object (no more rebuilds)
    and ends with the primary's live nodes (logical digest) and the same answers as a full rebuild of its own data"""
    env = Env("vixincr", {"db": _vix_incr_opts()})
    ws, aws = env.roots["db"]
    w = _vwrite_bg(ws, "--commits", 150, "--per", 12, "--sleep", 200, "--seed", 5)
    time.sleep(1.5)
    env.start_server()
    env.start_client()
    try:
        _vix_stamped(env, aws)
        r0 = _rebuilds(env)
        g0 = gen_of(os.path.join(aws, "commit.bin"))
        assert w.poll() is None, "the writer ended before the index was stamped"
        assert w.wait(timeout=180) == 0, w.stdout.read()
        _vix_settle(env, ws, aws)
        st = open(os.path.join(aws, ".gxrepl", "derived", "_root")).read()
        assert "ok incremental" in st, st
        assert gen_of(os.path.join(aws, "commit.bin")) > g0 + 20, "too few commits applied object by object"
        assert _rebuilds(env) == r0, "a rebuild ran after the stamp: " + env.logs()
        # the incremental index answers as a full rebuild of the copy's own data would
        rb = os.path.join(env.dir, "copy_rebuild.vix")
        r = run([os.path.join(BIN, "vix_rebuild"), "--data", os.path.join(aws, "data.gxd"), "--index", rb, "--member", "infohog:1",
                 "--tags", "-,-,infohog:2,-", "--dim", "16", "--m", "8", "--efc", "100", "--spec", "vtest"])
        assert r.returncode == 0, r.stdout
        assert _digest(rb)[0] == _digest(os.path.join(aws, "vectors.vix"))[0]
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_derived_vix_incremental_kill_in_scratch_and_cap():
    """11/14/2026 (7.4 merge): gxvmember under a kill -9 while a batch's .vix pages are staged (nothing of the batch
    on disk: the copy's index equals a rebuild of its own data; after the restart it matches the primary), and the
    staged cap (derived_apply_max_mb = 0: rebuild needed, then a rebuild matches the primary)"""
    env = Env("vixscratch", {"db": _vix_incr_opts()})
    ws, aws = env.roots["db"]
    w = _vwrite_bg(ws, "--commits", 200, "--per", 12, "--sleep", 200, "--seed", 6)
    time.sleep(1.5)
    env.start_server()
    env.start_client()
    try:
        _vix_stamped(env, aws)
        env.server.terminate(); env.server.wait(10)
        env.start_server(extra_env={"GXREPL_TEST_CRASH_IN_SCRATCH": "1"})
        end = time.time() + 60
        while time.time() < end and env.server.poll() is None:
            time.sleep(0.2)
        assert env.server.poll() == 9, "the scratch-phase hook never ran: " + env.logs()
        rb = os.path.join(env.dir, "copy_rebuild.vix")
        r = run([os.path.join(BIN, "vix_rebuild"), "--data", os.path.join(aws, "data.gxd"), "--index", rb, "--member", "infohog:1",
                 "--tags", "-,-,infohog:2,-", "--dim", "16", "--m", "8", "--efc", "100", "--spec", "vtest"])
        assert r.returncode == 0, r.stdout
        assert _digest(rb)[0] == _digest(os.path.join(aws, "vectors.vix"))[0], "the batch reached the copy's index"
        env.start_server()
        assert w.wait(timeout=240) == 0, w.stdout.read()
        _vix_settle(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()
    # the staged cap
    env = Env("vixcap", {"db": _vix_incr_opts(["derived_apply_max_mb = 0"])})
    ws, aws = env.roots["db"]
    w = _vwrite_bg(ws, "--commits", 80, "--per", 12, "--sleep", 200, "--seed", 7)
    time.sleep(1.5)
    env.start_server()
    env.start_client()
    try:
        end = time.time() + 90
        while time.time() < end and "passed derived_apply_max_mb" not in open(os.path.join(env.dir, "server.log")).read():
            time.sleep(0.5)
        assert "passed derived_apply_max_mb" in open(os.path.join(env.dir, "server.log")).read(), env.logs()
        assert w.wait(timeout=180) == 0, w.stdout.read()
        _vix_settle(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
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


def test_pull_reads_only_the_copys_log():
    """11/15/2026 (hardening, pull-mode F1): the source's log made again (purged and reopened under the same name)
    between a pull pass's listing and its read: the remote read carries the copy's log id, the 7.4 server answers
    "position gone" (a verify), never the new log's records as if they continued the old one; the copy ends equal"""
    env = Env("pull_logid", {"back": {"direction": "pull", "accept": "source"}})
    ws, aws = env.roots["back"]
    d1 = os.path.join(aws, "d1")
    os.makedirs(d1, exist_ok=True)
    assert writer(d1, "--commits", 20, "--ship-root", aws).returncode == 0
    hold = os.path.join(env.dir, "readlog_hold")
    env.start_server()
    env.start_client({"GXREPL_TEST_READLOG_HOLD": hold})
    try:
        wait_same(env, "back", 90)
        wait_status(env, lambda s: s["sets"]["back"]["ok"], 60)
        time.sleep(3)                                  # the first verifies done: the next records go by the feed
        log0 = len(open(os.path.join(env.dir, "client.log")).read())
        put(hold + ".arm", b"1\n")
        assert writer(d1, "--commits", 1, "--ship-root", aws, "--seed", 21).returncode == 0   # a record to read
        end = time.time() + 60
        while time.time() < end and not os.path.exists(hold + ".held"):
            time.sleep(0.1)
        assert os.path.exists(hold + ".held"), "the read was never held\n" + env.logs()
        # the log made again: the old one purged, a new writer from record 1 (more records than the copy has)
        shutil.rmtree(os.path.join(aws, ".gxrepl", "log", "d1_writer.jnl"))
        assert writer(d1, "--commits", 40, "--ship-root", aws, "--seed", 22).returncode == 0
        put(hold, b"go\n")
        wait_same(env, "back", 120)
        check_db(d1); check_db(os.path.join(ws, "d1"))
        tail = open(os.path.join(env.dir, "client.log")).read()[log0:]
        assert "position is gone" in tail or "not used" in tail, tail
    finally:
        env.stop()


def _late_pass_round(name, opts=None, writer_kind="write"):
    """11/15/2026 (the late-pass hollow seal): a day archived (its folder and stream gone from the source; the copy
    removes its folder), then reopened by a plain copy of the archived folder and written with a new log. Polls the
    copy: a unit sealed whole ("log ...") at the source's generation must hold the source's data byte for byte.
    Returns (problems, client log, server log)."""
    env = Env(name, {"db": opts or CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    arch = os.path.join(env.dir, "archive")
    os.makedirs(arch)
    tw = os.path.join(BIN, "gxrepl_testwriter")
    def write(*args, seed=1):
        if writer_kind == "vwrite":
            return subprocess.run([tw, "vwrite", d1, *map(str, args), "--seed", str(seed)], stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True)
        return subprocess.run([tw, "write", d1, *map(str, args), "--commit-block", "--ship-root", ws, "--seed", str(seed)],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    problems = []
    try:
        # a large day file (the late pass reopened files of 300 MB and more): the copy of it takes seconds
        r = write("--commits", 30, "--per", 6, "--big", 150000000, seed=3)
        assert r.returncode == 0, r.stdout
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        # the night: the day archived, its stream purged; the copy removes the folder
        shutil.move(d1, os.path.join(arch, "d1"))
        shutil.rmtree(os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl"), ignore_errors=True)
        end = time.time() + 60
        while time.time() < end and os.path.exists(os.path.join(aws, "d1", "data.gxd")):
            time.sleep(0.2)
        assert not os.path.exists(os.path.join(aws, "d1", "data.gxd")), "the copy kept the archived folder\n" + env.logs()
        # the late pass: a plain copy back, a new log, a few commits
        shutil.copytree(os.path.join(arch, "d1"), d1)
        # the writer opens (its new log's open record) and waits before its first commit, as the late pass did: the
        # client verifies the new log while it lists no file yet
        gate = os.path.join(env.dir, "first_commit")
        w = subprocess.Popen([tw, "write", d1, "--commits", "8", "--per", "3", "--sleep", "300", "--commit-block",
                              "--ship-root", ws, "--seed", "9", "--wait-at", "0", "--wait-file", gate]
                             if writer_kind == "write" else
                             [tw, "vwrite", d1, "--commits", "8", "--per", "3", "--sleep", "300", "--seed", "9"],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        log0 = len(open(os.path.join(env.dir, "client.log")).read())
        end = time.time() + 30
        while time.time() < end and "new log id" not in open(os.path.join(env.dir, "client.log")).read()[log0:]:
            time.sleep(0.1)
        time.sleep(2)
        put(gate, b"go\n")
        seal = os.path.join(aws, ".gxrepl", "units", "_d1")
        end = time.time() + 40
        while time.time() < end:
            try:
                sb = open(seal).read()
            except OSError:
                sb = ""
            if sb.startswith("log "):
                g = _gen_or_none(os.path.join(aws, "d1", "commit.bin"))
                if g is not None and g == _gen_or_none(os.path.join(d1, "commit.bin")) and \
                   not _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")):
                    a, b = os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")
                    zb = 0
                    try:
                        cb = open(b, "rb").read()
                        zb = sum(1 for k in range(0, len(cb), 4096) if not cb[k:k + 4096].strip(b"\0"))
                    except OSError:
                        pass
                    problems.append(f"sealed whole ({sb.strip()}) at generation {g} with a data.gxd that differs from "
                                    f"the source's ({os.path.getsize(b) if os.path.exists(b) else 0} bytes, {zb} zero pages)")
                    break
            if w.poll() is not None and time.time() > end - 25:
                pass
            time.sleep(0.1)
        w.wait(120)
        _wait_units(env, ws, aws, ["d1"], 120)
        return problems, open(os.path.join(env.dir, "client.log")).read(), open(os.path.join(env.dir, "server.log")).read()
    finally:
        env.stop()


def test_late_pass_reopened_day_never_sealed_hollow():
    """11/15/2026 (the late-pass hollow seal, beta3 production 11/15 20:30): an archived day reopened by a plain copy
    and written with a new log is never sealed whole on the copy while its data file differs from the source's"""
    problems, clog, slog = _late_pass_round("late_hollow")
    assert not problems, "; ".join(problems) + "\n" + clog[-2500:] + "\n" + slog[-2500:]


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


def test_a_segment_being_started_reads_as_nothing_yet():
    """11/15/2026 (F2): the writer's newest segment, truncated and its header not written yet (open_segment()), reads
    as nothing to send (status 0), never as a segment of another log (status 1: the copy's position gone, a full
    resync of the stream); a segment of another log still reads as status 1"""
    root = os.path.join(TMP, "seg_start")
    d1 = os.path.join(root, "d1")
    os.makedirs(d1)
    assert writer(d1, "--commits", 5, "--commit-block", "--ship-root", root).returncode == 0
    ld = os.path.join(root, ".gxrepl", "log", "d1_writer.jnl")
    head = run([os.path.join(BIN, "gxrepl"), "shiplog", ld]).stdout.split("\n")[0].split()
    nxt = int(head[head.index("next") + 1])
    seg = os.path.join(ld, "%016x.gxl" % nxt)
    open(seg, "wb").close()                            # the moment between O_TRUNC and the header write
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "readlog", ld, str(nxt)])
    assert r.stdout.startswith("status 0 count 0"), "a segment being started: " + r.stdout
    open(seg, "wb").write(b"GXRSEG01" + b"f" * 16 + b"\0" * 8)   # a whole header of another log id
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "readlog", ld, str(nxt)])
    assert r.stdout.startswith("status 1"), "a segment of another log: " + r.stdout
    os.remove(seg)


def test_big_record_resync_bound():
    """11/15/2026 (hardening): records bigger than max_message that never pass (GXREPL_TEST_STATUS2_ALWAYS) are not
    resynced forever: after GXREPL_STATUS2_MAX resyncs with no record drained, one ALERT and no more resyncs"""
    env = Env("big_bound", {"db": {}})
    ws, aws = env.roots["db"]
    assert writer(ws, "--commits", 5).returncode == 0
    env.start_server()
    try:
        wait_ok = time.time() + 30
        env.start_client({"GXREPL_TEST_STATUS2_ALWAYS": "1", "GXREPL_STATUS2_MAX": "3"})
        for i in range(12):                              # new records every second: each read answers status 2
            assert writer(ws, "--commits", 1, "--seed", 300 + i).returncode == 0
            time.sleep(1)
        log = open(os.path.join(env.dir, "client.log")).read()
        n = log.count("the copy resyncs its files")
        print(f"    {n} resyncs, {log.count('ALERT')} ALERT(s)", flush=True)
        assert log.count("ALERT") == 1 and "not resynced again" in log, log[-3000:]
        assert n <= 4, f"{n} resyncs past the bound of 3:\n" + log[-3000:]
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
        # 11/15/2026 (F3): "always", never ULLONG_MAX raw, in the log lines
        assert "18446744073709551615" not in log, log[-2000:]
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
            # 11/13/2026: the server may still be swapping in a stage the killed client finished: the verdict
            # and promote-check are read under a shared lock on apply.jnl (judge_copy, as a locked reader does),
            # never one before and one after that swap. 11/14/2026: in place too: promote-check now waits for an
            # apply in flight (its shared lock), so an unlocked verdict read before the killed client's last
            # request (a MarkGen making the unit odd) and a promote-check after it disagreed ("whole" refused)
            v, tr = judge_copy(env, aws, gl)
            if v in ("torn", "odd"):
                if tr.returncode == 1:
                    stats["caught"] += 1
                else:
                    stats["missed"] += 1
            elif v == "whole":
                stats["whole"] += 1
                if tr.returncode != 0:
                    stats["refused_whole"] += 1
                    print(f"      kill {i}: a whole copy refused: {tr.stdout.strip()[:300]!r}", flush=True)
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
    m = _re.match(r"gxrepl \(DataReel [\d.]+( (alpha|beta)\d*)?\), protocol 3, default port 2073, zlib (yes|no), "
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


def _seal_and_position(aws):
    """The root unit's "log G L stream" seal record L and its stream's applied position, or None."""
    try:
        s = open(os.path.join(aws, ".gxrepl", "units", "_root")).read().split()
        if len(s) < 4 or s[0] != "log":
            return None
        p = open(os.path.join(aws, ".gxrepl", "state", s[3] + ".state")).read().split()
        return int(s[2]), int(p[1])
    except (OSError, IndexError, ValueError):
        return None


def test_promote_check_reads_under_the_apply_lock():
    """11/14/2026 (promote-check rc 1 "sealed at ship log record 77, the copy applied up to 76" under heavy load):
    promote-check read the seal and the applied position unlocked, re-reading a split pair for 10 s; an apply whose
    position write lagged past that was reported TORN. Now each unit is read under a shared flock on apply.jnl (an
    apply holds it exclusively): (a) an apply held between its seal and its position (GXREPL_TEST_POSITION_DELAY_MS,
    4 s, with GXREPL_TORN_RECHECK_SEC=1) is waited for, rc 0; (b) a real split (no apply running) is still TORN,
    at once; (c) a lock held past GXREPL_TORN_LOCK_WAIT: the check still answers, unlocked, with a WARNING."""
    import fcntl
    env = Env("torn_lock", {"db": dict(CB_OPTS)})
    ws, aws = env.roots["db"]
    gx = os.path.join(BIN, "gxrepl")
    assert writer(ws, "--commits", 3, "--commit-block").returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        assert writer(ws, "--commits", 2, "--commit-block", "--seed", 41).returncode == 0
        wait_same(env, "db", 60)
        env.stop()
        sp = _seal_and_position(aws)
        assert sp and sp[0] == sp[1], f"no ship log seal in step: {sp}"
        t = torn(env)
        assert t.returncode == 0 and "0 torn" in t.stdout, t.stdout
        # (a) an apply inside its lock, the seal written, the position 4 s late. 11/14/2026 (7.3.4): every writer
        # run starts with an open record, which now always gets its stream verified (drain-verify), and the hook
        # delays the verify's file updates too: so the writer first opens and commits once, the open record's
        # verify ends, and only then does its second commit (records only) make the one delayed apply that the
        # check must wait for (a back-to-back run of delayed applies would rightly outlast GXREPL_TORN_LOCK_WAIT)
        env.start_server(extra_env={"GXREPL_TEST_POSITION_DELAY_MS": "4000"})
        n_log = len(open(os.path.join(env.dir, "client.log")).read()) if os.path.exists(os.path.join(env.dir, "client.log")) else 0
        env.start_client()
        go = os.path.join(env.dir, "torn_lock_go")
        w = writer_bg(ws, "--commits", 2, "--commit-block", "--seed", 43, "--wait-at", 1, "--wait-file", go)
        end = time.time() + 120
        while time.time() < end and "verified (" not in open(os.path.join(env.dir, "client.log")).read()[n_log:]:
            time.sleep(0.2)
        assert "verified (" in open(os.path.join(env.dir, "client.log")).read()[n_log:], "the open record was never verified\n" + env.logs()
        wait_same(env, "db", 120)                     # the first commit and the verify are in, nothing applying
        put(go, b"go\n")                              # the second commit: one delayed apply
        split = None
        end = time.time() + 60
        while time.time() < end and not split:
            sp = _seal_and_position(aws)
            if sp and sp[0] > sp[1]:
                split = sp
            else:
                time.sleep(0.02)
        assert split, "the delayed apply was never seen between its seal and its position\n" + env.logs()
        t0 = time.time()
        t = run([gx, "torn", "--config", env.srv_conf],
                env=dict(os.environ, GXREPL_TORN_RECHECK_SEC="1", GXREPL_TORN_LOCK_STATS="1"))
        el = time.time() - t0
        print(f"    (a) seal at record {split[0]}, position {split[1]}: torn exit {t.returncode} in {el:.1f} s: "
              f"{t.stdout.strip()[:300]!r}", flush=True)
        assert t.returncode == 0 and "0 torn" in t.stdout, t.stdout
        assert "without the apply lock" not in t.stdout, t.stdout
        assert "torn lock stats" in t.stdout, t.stdout
        assert w.wait(60) == 0, w.stdout.read()
        wait_same(env, "db", 60)
        env.stop()
        # (b) a real split, nothing applying: TORN under the lock, no 10 s of re-reads
        sp = _seal_and_position(aws)
        assert sp and sp[0] == sp[1], sp
        stream = open(os.path.join(aws, ".gxrepl", "units", "_root")).read().split()[3]
        stp = os.path.join(aws, ".gxrepl", "state", stream + ".state")
        keep = open(stp).read()
        f = keep.split()
        with open(stp, "w") as fh:
            fh.write(" ".join([f[0], str(int(f[1]) - 1)] + f[2:]) + "\n")
        try:
            t0 = time.time()
            t = torn(env)
            el = time.time() - t0
            print(f"    (b) a real split: torn exit {t.returncode} in {el:.1f} s", flush=True)
            assert t.returncode == 1 and "records past the seal" in t.stdout, t.stdout
            assert el < 5, f"{el:.1f} s: re-read under the lock"
        finally:
            with open(stp, "w") as fh:
                fh.write(keep)
        # (c) the lock held (as a long derived rebuild does) past GXREPL_TORN_LOCK_WAIT=1: unlocked, said
        fd = os.open(os.path.join(aws, ".gxrepl", "apply.jnl"), os.O_RDWR)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX)
            t0 = time.time()
            t = run([gx, "torn", "--config", env.srv_conf],
                    env=dict(os.environ, GXREPL_TORN_LOCK_WAIT="1", GXREPL_TORN_RECHECK_SEC="1",
                             GXREPL_TORN_LOCK_STATS="1"), timeout=60)
            el = time.time() - t0
        finally:
            os.close(fd)
        print(f"    (c) lock held: torn exit {t.returncode} in {el:.1f} s: {t.stdout.strip()[:400]!r}", flush=True)
        assert t.returncode == 0 and "0 torn" in t.stdout, t.stdout
        assert "WARNING the check ran without the apply lock" in t.stdout, t.stdout
        assert "(fell back)" in t.stdout, t.stdout
        assert 0.9 <= el < 10, el
        t = torn(env)
        assert t.returncode == 0 and "without the apply lock" not in t.stdout, t.stdout
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


def _incr_opts(extractor="gxrepl_test", extra_server=()):
    """11/14/2026: a set whose copy keeps index.gxd object by object (derived_apply = incremental)"""
    o = _derived_opts(["derived_apply = incremental", f"derived_extractor = {extractor}", "derived_source = data.gxd",
                       "derived_count = index.gxd"] + list(extra_server))
    return o


def _so():
    return os.path.join(BIN, "gxrepl_testextractor.so")


def _objects_writer(root, *args, extra_env=None):
    """the testwriter with object notes on (its ship log carries objects records)"""
    e = dict(os.environ, GXDSHIP_OBJECTS="1", **(extra_env or {}))
    return subprocess.run([os.path.join(BIN, "gxrepl_testwriter"), "write", root, *map(str, args)],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)


def _gen(path):
    try:
        with open(path, "rb") as f:
            f.seek(8)
            return int.from_bytes(f.read(8), "little")
    except OSError:
        return -1


def _wait_gen(ws, aws, unit, secs=60):
    end = time.time() + secs
    want = _gen(os.path.join(ws, unit, "commit.bin"))
    while time.time() < end and _gen(os.path.join(aws, unit, "commit.bin")) != want:
        time.sleep(0.2)
    assert _gen(os.path.join(aws, unit, "commit.bin")) == want, "the copy did not reach the source's generation"


def _wait_stamped(env, aws, unit, secs=60):
    st = os.path.join(aws, unit, "index.gxd.stamp")
    end = time.time() + secs
    while time.time() < end and (not os.path.exists(st) or "INDEXES" in torn(env).stdout):
        time.sleep(0.3)
    assert os.path.exists(st), "the rebuilt index was never stamped\n" + env.logs()
    t = torn(env)
    assert t.returncode == 0 and "INDEXES" not in t.stdout, t.stdout


def _rebuilds(env):
    try:
        return open(os.path.join(env.dir, "server.log")).read().count("derived indexes rebuilt")
    except OSError:
        return 0


def _objects_writer_bg(root, *args, extra_env=None):
    e = dict(os.environ, GXDSHIP_OBJECTS="1", **(extra_env or {}))
    return subprocess.Popen([os.path.join(BIN, "gxrepl_testwriter"), "write", root, *map(str, args)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)


def _incremental_round(extractor, name):
    # one writer process the whole time (a new writer's open record makes the copy verify: a fill by files)
    env = Env(name, {"db": _incr_opts(extractor, ["derived_delay = 0", "derived_busy_rebuild = 2"])})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d1"))
    w = _objects_writer_bg(os.path.join(ws, "d1"), "--commits", 120, "--per", 6, "--sleep", 250, "--commit-block",
                           "--ship-root", ws)
    time.sleep(1.5)
    env.start_server()
    env.start_client()
    try:
        _wait_stamped(env, aws, "d1")
        r0 = _rebuilds(env)
        assert r0 >= 1
        assert w.poll() is None, "the writer ended before the index was stamped: make it longer"
        g0 = _gen(os.path.join(aws, "d1", "commit.bin"))
        assert w.wait(timeout=180) == 0, w.stdout.read()
        _wait_gen(ws, aws, "d1")
        t = torn(env)                             # in step at once: no rebuild after the last commit
        assert t.returncode == 0 and "INDEXES" not in t.stdout, t.stdout + env.logs()
        check_db(os.path.join(aws, "d1"))         # the index agrees with the data and the commit block
        st = open(os.path.join(aws, ".gxrepl", "derived", "_d1")).read()
        assert "ok incremental" in st, st
        assert _gen(os.path.join(aws, "d1", "commit.bin")) > g0 + 20, "too few commits applied object by object"
        assert _rebuilds(env) == r0, "a rebuild ran after the stamps: " + env.logs()
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "rebuild needed" not in log.split("derived indexes rebuilt")[-1], log[-2000:]
    finally:
        if w.poll() is None:
            w.kill(); w.wait()
        env.stop()


def _stamped(name, extra_server=(), writer_args=(), commits=400):
    """an incremental set whose folder d1 is stamped while a writer keeps committing"""
    env = Env(name, {"db": _incr_opts("gxrepl_test", ["derived_delay = 0", "derived_busy_rebuild = 2"] + list(extra_server))})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d1"))
    w = _objects_writer_bg(os.path.join(ws, "d1"), "--commits", commits, "--per", 6, "--sleep", 250, "--commit-block",
                           "--ship-root", ws, *writer_args)
    time.sleep(1.5)
    env.start_server()
    env.start_client()
    _wait_stamped(env, aws, "d1")
    return env, ws, aws, w


def _restart_server(env, extra_env=None):
    if env.server and env.server.poll() is None:
        env.server.terminate()
        env.server.wait(10)
    env.start_server(extra_env=extra_env)


def _wait_log(env, text, secs=40, after=0):
    end = time.time() + secs
    while time.time() < end:
        log = open(os.path.join(env.dir, "server.log")).read()
        if text in log[after:]:
            return log
        time.sleep(0.3)
    raise AssertionError("never logged: " + text + "\n" + open(os.path.join(env.dir, "server.log")).read()[-3000:])


def _finish(env, ws, aws, w):
    """the writer stops; the copy reaches it; its indexes are good (rebuilt or kept) and check"""
    if w.poll() is None:
        w.terminate()
    w.wait(30)
    time.sleep(0.5)
    # a writer stopped in a batch leaves an odd commit block: one more commit makes it even
    assert _objects_writer(os.path.join(ws, "d1"), "--commits", 1, "--commit-block", "--ship-root", ws,
                           "--seed", 77).returncode == 0
    _wait_gen(ws, aws, "d1", 90)
    end = time.time() + 60
    while time.time() < end and "INDEXES" in torn(env).stdout:
        time.sleep(0.5)
    t = torn(env)
    assert t.returncode == 0 and "INDEXES" not in t.stdout, t.stdout + env.logs()
    check_db(os.path.join(aws, "d1"))


def test_incremental_extractor_error_means_rebuild():
    """11/14/2026: an extractor error is "rebuild needed": the stamps go, the folder is rebuilt, the index checks"""
    env, ws, aws, w = _stamped("incr_err")
    try:
        n = len(open(os.path.join(env.dir, "server.log")).read())
        _restart_server(env, {"GXREPL_TEST_EXTRACTOR_FAIL": "insert"})
        _wait_log(env, "rebuild needed: index.gxd: extractor insert: test: insert fails", after=n)
        _restart_server(env)
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_direct_write_is_caught():
    """11/14/2026: an extractor that writes its index outside the staged scope is caught (ALERT), rebuild needed"""
    env, ws, aws, w = _stamped("incr_direct")
    try:
        n = len(open(os.path.join(env.dir, "server.log")).read())
        _restart_server(env, {"GXREPL_TEST_EXTRACTOR_DIRECT": "1"})
        log = _wait_log(env, "outside the staged scope", after=n)
        assert "ALERT" in log[n:], log[n:]
        _restart_server(env)
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_kill_in_scratch_phase():
    """11/14/2026 (review): gxrepl killed (-9) while a batch's index pages are staged in memory: nothing of the
    batch is on disk (the copy's index matches its data at its generation, its stamp at the seal). After the
    restart the copy catches up by either of two correct paths, depending on where the kill landed against the
    client's read (11/14/2026, the 7.4 thread's 4-of-6): the client verifies (a server lost in an apply), a copy by
    files, so the folder is rebuilt once; or it drains the same records again and the index step applies them object
    by object. Either way: the crash is counted for the breaker, the index matches the data, and the folder then
    applies object by object."""
    env, ws, aws, w = _stamped("incr_scratch")
    try:
        _restart_server(env, {"GXREPL_TEST_CRASH_IN_SCRATCH": "1"})
        end = time.time() + 40
        while time.time() < end and env.server.poll() is None:
            time.sleep(0.2)
        assert env.server.poll() == 9, "the scratch-phase hook never ran"
        c = run([os.path.join(BIN, "gxrepl_testwriter"), "check", os.path.join(aws, "d1")])
        assert c.returncode == 0, "the copy's index is not its pre-batch state: " + c.stdout
        st = run([os.path.join(BIN, "gxrepl"), "derived", "status", "--config", env.srv_conf])
        assert "rebuild needed" not in st.stdout and "stamped at record" in st.stdout, st.stdout
        state = os.path.join(aws, ".gxrepl", "derived", "_d1")
        crashes = os.path.join(aws, ".gxrepl", "derived", "incremental.crashes")
        rd = lambda: open(state).read() if os.path.exists(state) else ""
        before = rd()
        env.start_server()
        # caught up by either path, and then object by object: a new "ok incremental" state after the restart
        end = time.time() + 60
        while time.time() < end and not ("ok incremental" in rd() and rd() != before and os.path.exists(crashes)):
            time.sleep(0.3)
        assert "ok incremental" in rd(), rd() + env.logs()
        assert len(open(crashes).read().split()) == 1, "the crash was not counted"
        # a verify after the reconnect may still fill the folder by pieces (a writer that keeps committing): one more
        # rebuild then is the accepted cost; the end state is what counts (_finish: the index checks and stamps)
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_breaker_after_two_crashes():
    """11/14/2026: the extractor crashing gxrepl twice on one batch trips the breaker: the next start puts the set
    back on rebuild with an ALERT (and rebuilds); gxrepl derived rebuild clears it"""
    env, ws, aws, w = _stamped("incr_breaker")
    try:
        for i in range(2):
            _restart_server(env, {"GXREPL_TEST_EXTRACTOR_CRASH": "1"})
            end = time.time() + 40
            while time.time() < end and env.server.poll() is None:
                time.sleep(0.2)
            assert env.server.poll() == 7, f"crash {i + 1} never happened"
        n = len(open(os.path.join(env.dir, "server.log")).read())
        env.start_server()
        log = _wait_log(env, "derived_apply back on rebuild", after=n)
        assert "ALERT" in log[n:]
        assert os.path.exists(os.path.join(aws, ".gxrepl", "derived", "incremental.breaker"))
        _finish(env, ws, aws, w)
        r = run([os.path.join(BIN, "gxrepl"), "derived", "rebuild", "--config", env.srv_conf, "--set", "db"])
        assert r.returncode == 0, r.stdout
        assert not os.path.exists(os.path.join(aws, ".gxrepl", "derived", "incremental.breaker"))
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_breaker_crashes_in_an_hour():
    """11/14/2026 (review 4): crashes of the index step on different batches count too: 3 in an hour (any batches)
    trip the breaker with an ALERT; a crash older than an hour does not count; gxrepl derived rebuild clears them"""
    env, ws, aws, w = _stamped("incr_window")
    crashes = os.path.join(aws, ".gxrepl", "derived", "incremental.crashes")
    try:
        now = int(time.time())
        with open(crashes, "w") as f:                    # two earlier crashes (other batches) and one too old
            f.write(f"{now - 7300}\n{now - 1800}\n{now - 600}\n")
        _restart_server(env, {"GXREPL_TEST_EXTRACTOR_CRASH": "1"})
        end = time.time() + 40
        while time.time() < end and env.server.poll() is None:
            time.sleep(0.2)
        assert env.server.poll() == 7, "the crash never happened"
        n = len(open(os.path.join(env.dir, "server.log")).read())
        env.start_server()
        log = _wait_log(env, "derived_apply back on rebuild", after=n)
        assert "ALERT" in log[n:] and "3 times in the last hour" in log[n:], log[n:]
        assert len(open(crashes).read().split()) == 3, open(crashes).read()   # the old one dropped
        st = run([os.path.join(BIN, "gxrepl"), "derived", "status", "--config", env.srv_conf, "--set", "db"])
        assert "BREAKER on" in st.stdout and "crashes in the last hour: 3" in st.stdout, st.stdout
        _finish(env, ws, aws, w)
        r = run([os.path.join(BIN, "gxrepl"), "derived", "rebuild", "--config", env.srv_conf, "--set", "db"])
        assert r.returncode == 0, r.stdout
        assert not os.path.exists(crashes)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_staged_cap():
    """11/14/2026: more staged index pages than derived_apply_max_mb: the batch's indexes are rebuilt instead"""
    env, ws, aws, w = _stamped("incr_cap", ["derived_apply_max_mb = 0"])
    try:
        _wait_log(env, "passed derived_apply_max_mb")
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_step_time_limit():
    """11/14/2026 (review 5): an index step slower than derived_apply_max_s (checked between extractor calls) stops
    and the batch's folders are rebuilt instead; an extractor call that never returns trips the hard watchdog:
    gxrepl stops with an ALERT (exit 8), and the crash is counted for the breaker"""
    env, ws, aws, w = _stamped("incr_time", ["derived_apply_max_s = 1"])
    try:
        n = len(open(os.path.join(env.dir, "server.log")).read())
        _restart_server(env, {"GXREPL_TEST_EXTRACTOR_SLOW": "400"})
        _wait_log(env, "passed derived_apply_max_s", after=n)
        n = len(open(os.path.join(env.dir, "server.log")).read())
        _restart_server(env, {"GXREPL_TEST_EXTRACTOR_HANG": "1", "GXREPL_TEST_WATCHDOG_S": "3"})
        end = time.time() + 60
        while time.time() < end and env.server.poll() is None:
            time.sleep(0.2)
        assert env.server.poll() == 8, "the watchdog never stopped gxrepl"
        log = open(os.path.join(env.dir, "server.log")).read()[n:]
        assert "ALERT" in log and "never returns" in log, log
        assert os.path.exists(os.path.join(aws, ".gxrepl", "derived", "incremental.try"))
        env.start_server()
        crashes = os.path.join(aws, ".gxrepl", "derived", "incremental.crashes")
        end = time.time() + 30
        while time.time() < end and not os.path.exists(crashes):
            time.sleep(0.2)
        assert len(open(crashes).read().split()) == 1
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_stray_write_is_refused():
    """11/14/2026 (review 7): an extractor that writes any file but its index and stamp inside the staged scope (here
    the data file's header): that write is never applied, the folder goes to rebuild needed with an ALERT, and the
    copy's data file stays the source's"""
    env, ws, aws, w = _stamped("incr_stray")
    try:
        n = len(open(os.path.join(env.dir, "server.log")).read())
        _restart_server(env, {"GXREPL_TEST_EXTRACTOR_STRAY": "1"})
        log = _wait_log(env, "not applied (rebuild needed)", after=n)
        assert "ALERT" in log[n:], log[n:]
        _restart_server(env)
        _finish(env, ws, aws, w)
        a = open(os.path.join(ws, "d1", "data.gxd"), "rb").read()
        assert open(os.path.join(aws, "d1", "data.gxd"), "rb").read() == a, "the copy's data file differs"
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_big_batch_has_no_index_pages():
    """11/14/2026 (review 6): a batch with more files than one transaction holds is applied in parts, which are not
    one transaction, so it carries no index pages: they, the stamps, derived.stamp and the derived state are
    dropped and the derived pass rebuilds them (the limit is 1000 files; 3 here, GXREPL_TEST_APPLY_PART_FILES)"""
    env, ws, aws, w = _stamped("incr_parts")
    try:
        n = len(open(os.path.join(env.dir, "server.log")).read())
        _restart_server(env, {"GXREPL_TEST_APPLY_PART_FILES": "3"})
        _wait_log(env, "applied in parts: no index pages in it", after=n)
        _restart_server(env)
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_status_shows_derived_state():
    """11/14/2026 (review 10): the server's status.json shows each derived set's state (mode, folders, failed, not
    built, rebuild needed, breaker, crashes in the last hour) and a WARN when incremental is on while the source's
    object notes are off (commits without objects records: every batch is rebuilt); it clears once the folder is
    applied object by object again"""
    env, ws, aws, w = _stamped("incr_status")
    try:
        def derived():
            try:
                return json.load(open(env.srv_status))["sets"]["db"]["derived"]
            except Exception:
                return {}
        end = time.time() + 30
        while time.time() < end and derived().get("folders") != 1:
            time.sleep(0.5)
        d = derived()
        assert d.get("mode") == "incremental" and d["folders"] == 1 and d["breaker"] is False and not d["warn"], d
        w.terminate(); w.wait(30)
        # the same folder written with notes off (GXDSHIP_OBJECTS taken out of the environment: the suite's objects
        # mode sets it for every writer)
        e = {k: v for k, v in os.environ.items() if k != "GXDSHIP_OBJECTS"}
        r = subprocess.run([os.path.join(BIN, "gxrepl_testwriter"), "write", os.path.join(ws, "d1"), "--commits", "3",
                            "--commit-block", "--ship-root", ws, "--seed", "81"], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, env=e)
        assert r.returncode == 0, r.stdout
        _wait_gen(ws, aws, "d1", 90)
        # 11/15/2026: the derived status is cached for 30 s: wait well past that (it failed once at load ~17)
        end = time.time() + 75
        while time.time() < end and "object notes are off" not in derived().get("warn", ""):
            time.sleep(0.5)
        assert "object notes are off" in derived().get("warn", ""), derived()
        # notes on again: object by object, the WARN goes
        w = _objects_writer_bg(os.path.join(ws, "d1"), "--commits", 40, "--per", 6, "--sleep", 250, "--commit-block",
                               "--ship-root", ws)
        end = time.time() + 90                       # the same 30 s cache before the WARN clears
        while time.time() < end and derived().get("warn"):
            time.sleep(0.5)
        assert not derived().get("warn"), derived()
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_needs_derived_count():
    """11/14/2026 (re-review B): derived_apply = incremental without derived_count (the index with one entry per
    record) never applies object by object: the set stays on rebuild and says why; its indexes stay good"""
    env = Env("incr_nocount", {"db": _derived_opts(["derived_apply = incremental", "derived_extractor = gxrepl_test",
                                                    "derived_source = data.gxd", "derived_delay = 0",
                                                    "derived_busy_rebuild = 2"])})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d1"))
    w = _objects_writer_bg(os.path.join(ws, "d1"), "--commits", 30, "--per", 6, "--sleep", 250, "--commit-block",
                           "--ship-root", ws)
    env.start_server()
    env.start_client()
    try:
        _wait_log(env, "incremental needs derived_count")
        _finish(env, ws, aws, w)
        assert "ok incremental" not in open(os.path.join(aws, ".gxrepl", "derived", "_d1")).read()
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_keeps_the_rebuilds_stamp_fields():
    """11/14/2026: derived.stamp fields after "keys K" are the rebuild's own (OSA: keys_version N, the key rules the
    indexes were made with): an object-by-object apply rewrites the counts and keeps them"""
    env = Env("incr_tail", {"db": _incr_opts("gxrepl_test", ["derived_delay = 0", "derived_busy_rebuild = 2"])})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d1"))
    w = _objects_writer_bg(os.path.join(ws, "d1"), "--commits", 40, "--per", 6, "--sleep", 250, "--commit-block",
                           "--ship-root", ws)
    time.sleep(1.5)
    env.start_server(extra_env={"GXREPL_TEST_STAMP_TAIL": " keys_version 7"})
    env.start_client()
    try:
        _wait_stamped(env, aws, "d1")
        st = os.path.join(aws, ".gxrepl", "derived", "_d1")
        end = time.time() + 60
        while time.time() < end and "incremental" not in (open(st).read() if os.path.exists(st) else ""):
            time.sleep(0.3)
        assert "incremental" in open(st).read(), open(st).read() + env.logs()
        body = open(os.path.join(aws, "d1", "derived.stamp")).read()
        assert body.strip().endswith("keys_version 7"), body
        _finish(env, ws, aws, w)
        assert open(os.path.join(aws, "d1", "derived.stamp")).read().strip().endswith("keys_version 7")
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_writer_restarts_keep_the_stamps():
    """11/14/2026 (the replay finding): a writer's restart (its open record: the copy verifies the stream) can bring
    the next records inside the verify's file update; those go through the same index step as ApplyLog's, so the
    folder's stamps stay current: no "stamped at L, the folder at L+n" over many restarts"""
    env, ws, aws, w = _stamped("incr_restarts", commits=20)
    try:
        w.wait(120)
        _finish(env, ws, aws, w)
        _wait_stamped(env, aws, "d1")
        def verifies():
            return open(os.path.join(env.dir, "client.log")).read().count("d1_writer.jnl verified (")
        # 11/14/2026 (review, drain-verify (c)): every writer open gets its verify (the protocol's rule), and the test
        # writer's open can change data.gxd outside its journal, so a verify may copy by files: one rebuild. One
        # restart: exactly one verify and at most one rebuild (none when the verify finds nothing to copy).
        time.sleep(3)
        v1, r1 = verifies(), _rebuilds(env)
        r = _objects_writer(os.path.join(ws, "d1"), "--commits", 3, "--per", 6, "--commit-block", "--ship-root", ws,
                            "--seed", 299)
        assert r.returncode == 0, r.stdout
        _wait_gen(ws, aws, "d1", 90)
        _wait_stamped(env, aws, "d1")
        time.sleep(3)
        assert verifies() - v1 == 1 and _rebuilds(env) - r1 <= 1, f"one restart: {verifies() - v1} verifies, " \
            f"{_rebuilds(env) - r1} rebuilds\n" + env.logs()
        n0 = _rebuilds(env)
        nv0 = verifies()
        for i in range(8):
            r = _objects_writer(os.path.join(ws, "d1"), "--commits", 3, "--per", 6, "--commit-block", "--ship-root", ws,
                                "--seed", 300 + i)
            assert r.returncode == 0, r.stdout
            _wait_gen(ws, aws, "d1", 90)
        _wait_stamped(env, aws, "d1")
        time.sleep(3)
        log = open(os.path.join(env.dir, "server.log")).read()
        print(f"    8 writer restarts: {_rebuilds(env) - n0} rebuilds, {log.count('rebuild needed')} rebuild needed", flush=True)
        assert "is stamped at" not in log, env.logs()                 # the records' index step ran
        # at most one verify and one rebuild per restart (8 restarts); osa_ingest's open never changes the files
        # outside its journal (the replayed real day: 1 rebuild in 92 restarts)
        assert verifies() - nv0 <= 8 and _rebuilds(env) - n0 <= 8, \
            f"8 restarts: {verifies() - nv0} verifies, {_rebuilds(env) - n0} rebuilds\n" + env.logs()
        check_db(os.path.join(aws, "d1"))
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def _index_equals_rebuild(env, folder):
    """11/15/2026: the copy folder's index.gxd against a full rebuild of a copy of its data (gxrepl digest)"""
    tmp = os.path.join(env.dir, "rebuild_" + os.path.basename(folder) + "_" + str(int(time.time() * 1000)))
    os.makedirs(tmp)
    for f in ("data.gxd", "commit.bin", "index.gxd"):
        shutil.copy2(os.path.join(folder, f), os.path.join(tmp, f))
    r = run([os.path.join(BIN, "gxrepl_testwriter"), "reindex", tmp])
    assert r.returncode == 0, r.stdout
    d = [run([os.path.join(BIN, "gxrepl"), "digest", os.path.join(x, "index.gxd")]).stdout.split()[:2] for x in (folder, tmp)]
    return d[0] == d[1] and bool(d[0]), d


def _recovered_round(name, second_args=()):
    """11/15/2026 (the recovery gap): writer A commits object X to its journal and dies before the ship log has it
    (GXDJRNL_TEST_KILL_BEFORE_SHIP); writer B's Open() recovers X into the files with no record (only B's open
    record) and commits again on the same tail page, so the copy gets X's bytes inside B's pages and no object note
    for X. The copy's index must still hold X: equal to a rebuild of its own data."""
    # adds only, small objects: the data file grows at its tail, with no freed block for X to land in, so B's
    # commits write X's page again (OSA's append-only realtime files)
    env, ws, aws, w = _stamped(name, commits=20, writer_args=("--adds-only", "--size-max", "64"))
    try:
        w.wait(120)
        _finish(env, ws, aws, w)
        _wait_stamped(env, aws, "d1")
        time.sleep(2)
        d1 = os.path.join(ws, "d1")
        # the client down while A and B run: B's open record and its commits then reach the copy together, the
        # commits applied object by object before the verify the open asks for compares the files (the order that
        # leaves X out of the index; with the client up, a verify racing B's commits copies by files and rebuilds)
        env.kill("client")
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 501,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, "writer A was not stopped before the ship log: " + a.stdout
        b = _objects_writer(d1, "--commits", 2, "--per", 1, "--commit-block", "--ship-root", ws, "--seed", 502,
                            "--adds-only", "--size-max", 64, *second_args)
        assert b.returncode == 0, b.stdout
        env.start_client()
        _wait_gen(ws, aws, "d1", 120)
        _wait_stamped(env, aws, "d1", 180)
        time.sleep(2)
        _wait_stamped(env, aws, "d1", 180)
        ok, d = _index_equals_rebuild(env, os.path.join(aws, "d1"))
        assert ok, f"the copy's index is not its data's (copy, rebuild): {d}\n" + env.logs()
        # 11/15/2026 (stamp-keeping fill review): the recovered open (or the gap) sent the folder to a rebuild
        log = open(os.path.join(env.dir, "server.log")).read()
        assert "rebuild needed: a writer's open record" in log or "rebuild needed: a gap record" in log, env.logs()
        check_db(os.path.join(aws, "d1"))
        check_db(d1)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_recovered_commit_is_indexed():
    _recovered_round("incr_recovered")


def test_incremental_recovered_commit_then_gap_is_indexed():
    """the same, and writer B's first commit loses its commit record (a gap follows it)"""
    _recovered_round("incr_recovered_gap", ("--split-pair-at", "0"))


def test_derived_vix_incremental_recovered_commit_is_indexed():
    """the recovery gap with the built-in gxvmember extractor: vectors.vix equal to a rebuild of the copy's data"""
    env = Env("vixrecovered", {"db": _vix_incr_opts()})
    ws, aws = env.roots["db"]
    w = _vwrite_bg(ws, "--commits", 30, "--per", 6, "--sleep", 100, "--seed", 5)
    time.sleep(1.5)
    env.start_server()
    env.start_client()
    try:
        assert w.wait(timeout=180) == 0, w.stdout.read()
        _vix_settle(env, ws, aws)
        e = dict(os.environ, GXDSHIP_OBJECTS="1", GXDJRNL_TEST_KILL_BEFORE_SHIP="1")
        a = subprocess.run([os.path.join(BIN, "gxrepl_testwriter"), "vwrite", ws, "--commits", "1", "--per", "3",
                            "--seed", "501"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)
        assert a.returncode == 9, "writer A was not stopped before the ship log: " + a.stdout
        b = _vwrite_bg(ws, "--commits", 2, "--per", 1, "--seed", 502)
        assert b.wait(timeout=180) == 0, b.stdout.read()
        _vix_settle(env, ws, aws, 240)
        rb = os.path.join(env.dir, "copy_rebuild.vix")
        r = run([os.path.join(BIN, "vix_rebuild"), "--data", os.path.join(aws, "data.gxd"), "--index", rb, "--member", "infohog:1",
                 "--tags", "-,-,infohog:2,-", "--dim", "16", "--m", "8", "--efc", "100", "--spec", "vtest"])
        assert r.returncode == 0, r.stdout
        assert _digest(rb)[0] == _digest(os.path.join(aws, "vectors.vix"))[0], \
            f"the copy's vectors.vix is not its data's: {_digest(rb)} {_digest(os.path.join(aws, 'vectors.vix'))}\n" + env.logs()
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def _recgap_env(name):
    """11/15/2026: an incremental set's folder d1 stamped, its adds-only writer finished (small objects at the tail)"""
    env, ws, aws, w = _stamped(name, commits=20, writer_args=("--adds-only", "--size-max", "64"))
    w.wait(120)
    _finish(env, ws, aws, w)
    _wait_stamped(env, aws, "d1")
    time.sleep(2)
    return env, ws, aws, w


def _recgap_index_ok(env, ws, aws):
    _wait_gen(ws, aws, "d1", 180)
    _wait_stamped(env, aws, "d1", 180)
    time.sleep(3)
    _wait_stamped(env, aws, "d1", 180)
    ok, d = _index_equals_rebuild(env, os.path.join(aws, "d1"))
    assert ok, f"the copy's index is not its data's (copy, rebuild): {d}\n" + env.logs()
    check_db(os.path.join(aws, "d1"))
    check_db(os.path.join(ws, "d1"))


def _adds(d1, ws, seed, *extra, env_extra=None):
    return _objects_writer_bg(d1, "--commits", 3, "--per", 1, "--commit-block", "--ship-root", ws, "--seed", seed,
                              "--adds-only", "--size-max", 64, *extra, extra_env=env_extra)


def test_incremental_gap_alone_is_indexed():
    """11/15/2026 (review 4): a gap record alone (no open record in its batch, no orphan objects record): writer B's
    first commit reaches the files and not the ship log (GXDSHIP_TEST_DROP_COMMIT), a gap record comes before its
    next; the next commits write the same tail page. The copy's index must hold the lost commit's objects."""
    env, ws, aws, w = _recgap_env("incr_gap_alone")
    try:
        d1 = os.path.join(ws, "d1")
        gate = os.path.join(env.dir, "go")
        b = _adds(d1, ws, 601, "--wait-at", 0, "--wait-file", gate, env_extra={"GXDSHIP_TEST_DROP_COMMIT": "1"})
        time.sleep(2)                                  # B's open record applied (and verified) on its own first
        _wait_stamped(env, aws, "d1", 180)
        time.sleep(3)
        put(gate, b"go\n")
        assert b.wait(timeout=120) == 0, b.stdout.read()
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_lone_open_then_commit_is_indexed():
    """11/15/2026 (review 2): writer B's open record reaches the copy alone (B waits before its first commit) and the
    stream's verify is held off (GXREPL_TEST_VERIFY_WAIT): the derived pass rebuilds the folder from data that lacks
    writer A's recovered commit, then B's commits (the same tail page) arrive. Until the verify is done the stream
    stays off object by object, so the index ends with A's objects.
    11/16/2026 (beta4 review L3): fails on 5e07237 with only the GXREPL_TEST_VERIFY_WAIT hook added (2 / 2: the copy's
    index is not its data's); it now checks that the hook held the verify, so a binary without it fails here."""
    env, ws, aws, w = _recgap_env("incr_lone_open")
    try:
        d1 = os.path.join(ws, "d1")
        vw = os.path.join(env.dir, "verify_go")
        env.kill("client")
        env.start_client({"GXREPL_TEST_VERIFY_WAIT": vw})
        c0 = len(open(os.path.join(env.dir, "client.log")).read())
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 611,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, "writer A was not stopped before the ship log: " + a.stdout
        gate = os.path.join(env.dir, "go")
        b = _adds(d1, ws, 612, "--wait-at", 0, "--wait-file", gate)
        time.sleep(3)                                  # B's lone open record applied
        _wait_stamped(env, aws, "d1", 180)             # rebuilt (or never dropped), stamped again
        time.sleep(2)
        put(gate, b"go\n")
        assert b.wait(timeout=120) == 0, b.stdout.read()
        time.sleep(5)                                  # B's commits applied while the verify waits
        # 11/16/2026 (beta4 review L3): the race needs the open's verify held until now; a gxrepl without the hook
        # (5e07237 itself) verifies at once and the test would pass there for the wrong reason
        cl = open(os.path.join(env.dir, "client.log")).read()[c0:]
        assert "needs a verify (a writer's open record)" in cl and "verified (" not in cl.split(
            "needs a verify (a writer's open record)", 1)[1], "the open's verify was not held (no hook?)\n" + env.logs()
        put(vw, b"go\n")
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def _holds(aws):
    return glob.glob(os.path.join(aws, ".gxrepl", "derived", "hold_*"))


def test_incremental_old_client_verify_keeps_the_hold():
    """11/16/2026 (beta4 review H1): a stream's hold goes only when the client says its verify compared files. An
    older client (GXREPL_TEST_OLD_VERIFY_DONE: the same position, no count, as 7.4 beta3 sends after any verify, a
    0-file one too) must not clear it: before the fix the copy took the same position as "verified past the hold".
    Writer A dies before the ship log, writer B's open record holds the stream; after the open's verify the hold is
    still there, the copy says why, and the index is still its data's."""
    env, ws, aws, w = _recgap_env("incr_old_verify")
    try:
        d1 = os.path.join(ws, "d1")
        env.kill("client")
        env.start_client({"GXREPL_TEST_OLD_VERIFY_DONE": "1"})
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 631,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, "writer A was not stopped before the ship log: " + a.stdout
        c0 = len(open(os.path.join(env.dir, "client.log")).read())
        b = _adds(d1, ws, 632)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        end = time.time() + 120
        while time.time() < end:
            cl = open(os.path.join(env.dir, "client.log")).read()[c0:]
            if re.search(r"verified \([1-9][0-9]* file", cl):
                break
            time.sleep(0.5)
        else:
            raise AssertionError("no verify of the open record\n" + env.logs())
        time.sleep(3)
        slog = open(os.path.join(env.dir, "server.log")).read()
        assert "verified past its open or gap record" not in slog, "an older client's verify cleared the hold\n" + env.logs()
        assert _holds(aws), "the stream's hold is gone\n" + env.logs()
        assert "its hold (an open or gap record at" in slog and "an older client" in slog, env.logs()
        _recgap_index_ok(env, ws, aws)
        # a 7.4 client: the next open's verify compares files, says so, and the hold goes
        env.kill("client")
        env.start_client()
        s0 = len(slog)
        a2 = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 633,
                             "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a2.returncode == 9, a2.stdout
        b2 = _adds(d1, ws, 634)
        assert b2.wait(timeout=120) == 0, b2.stdout.read()
        end = time.time() + 120
        while time.time() < end and not ("verified past its open or gap record" in
                                         open(os.path.join(env.dir, "server.log")).read()[s0:] and not _holds(aws)):
            time.sleep(0.5)
        assert not _holds(aws), "a 7.4 client's verify did not clear the hold\n" + env.logs()
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_hold_shown_and_unreadable_hold_cleared():
    """11/16/2026 (beta4 review L1): a stream's hold shows in "gxrepl derived status" (and status.json's "holds"); a
    hold that cannot be read is still cleared by the stream's next verify that compares files (before, an unreadable
    hold stayed for good and the stream never went object by object again)"""
    env, ws, aws, w = _recgap_env("incr_hold_status")
    try:
        d1 = os.path.join(ws, "d1")
        vw = os.path.join(env.dir, "verify_go")
        env.kill("client")
        env.start_client({"GXREPL_TEST_VERIFY_WAIT": vw})
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 641,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, a.stdout
        b = _adds(d1, ws, 642)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        end = time.time() + 60
        while time.time() < end and not _holds(aws):
            time.sleep(0.5)
        holds = _holds(aws)
        assert holds, "no hold after B's open record\n" + env.logs()
        st = run([os.path.join(BIN, "gxrepl"), "derived", "status", "--config", env.srv_conf])
        assert "HOLD stream" in st.stdout and "an open or gap record at" in st.stdout, st.stdout
        put(holds[0], b"garbage\n")
        st = run([os.path.join(BIN, "gxrepl"), "derived", "status", "--config", env.srv_conf])
        assert "an unreadable hold" in st.stdout, st.stdout
        put(vw, b"go\n")
        end = time.time() + 120
        while time.time() < end and _holds(aws):
            time.sleep(0.5)
        assert not _holds(aws), "the unreadable hold stayed after the verify\n" + env.logs()
        _recgap_index_ok(env, ws, aws)
        end = time.time() + 75                         # status.json's derived part is cached for 30 s
        while time.time() < end:
            try:
                d = json.load(open(env.srv_status))["sets"]["db"]["derived"]
                if "holds" in d and d["holds"] == []:
                    break
            except Exception:
                pass
            time.sleep(1)
        else:
            raise AssertionError("status.json has no empty \"holds\"\n" + open(env.srv_status).read())
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_hold_write_fail_turns_the_breaker_on():
    """11/16/2026 (beta4 review L2): when a stream's hold cannot be written (GXREPL_TEST_HOLD_WRITE_FAIL) the copy
    must not go on object by object as if there were none: the stream is held in memory, the breaker goes on (every
    batch rebuilt, across a restart too), an ALERT is logged, and the index stays its data's"""
    env, ws, aws, w = _recgap_env("incr_hold_write_fail")
    try:
        d1 = os.path.join(ws, "d1")
        env.kill("client")
        _restart_server(env, {"GXREPL_TEST_HOLD_WRITE_FAIL": "1"})
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 651,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, a.stdout
        b = _adds(d1, ws, 652)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        env.start_client()
        _wait_log(env, "ALERT set db: cannot write", secs=90)
        slog = open(os.path.join(env.dir, "server.log")).read()
        assert "ALERT set db: cannot write" in slog, env.logs()
        st = run([os.path.join(BIN, "gxrepl"), "derived", "status", "--config", env.srv_conf])
        assert "BREAKER on: the hold of stream" in st.stdout, st.stdout
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_fresh_log_recovered_open_is_indexed():
    """11/16/2026 (beta4 review H1 and M1, from the reviewer's reproducer): a fresh ship log (the log folder purged, as
    a late pass does) lists no file until its first commit. Writer A opens it and dies between its journal sync and
    the ship log; the copy's verify of the new log finds no file listed while d1 has its files. Writer B's Open()
    recovers A's commit (its open record) and commits. The hold must not go on a verify that compared nothing, d1
    must not stay sealed "copying", and the index must be its data's."""
    env, ws, aws, w = _recgap_env("incr_fresh_log")
    try:
        d1 = os.path.join(ws, "d1")
        env.kill("client")
        for p in glob.glob(os.path.join(ws, ".gxrepl", "log", "*")):
            if os.path.isdir(p):
                shutil.rmtree(p)
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 711,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, "writer A was not stopped before the ship log: " + a.stdout
        env.start_client()
        time.sleep(6)                                  # the new log seen, verified or waiting for its files
        clog = open(os.path.join(env.dir, "client.log")).read()
        assert clog.count("lists no file yet") <= 1, "said every pass\n" + env.logs()
        b = _adds(d1, ws, 712)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        _recgap_index_ok(env, ws, aws)
        end = time.time() + 120
        seal = os.path.join(aws, ".gxrepl", "units", "_d1")
        while time.time() < end and open(seal).read().startswith("copying"):
            time.sleep(0.5)
        assert not open(seal).read().startswith("copying"), "d1 stays sealed copying\n" + env.logs()
        assert _same_file(os.path.join(ws, "d1", "data.gxd"), os.path.join(aws, "d1", "data.gxd")), env.logs()
        slog = open(os.path.join(env.dir, "server.log")).read()
        cl = open(os.path.join(env.dir, "client.log")).read()
        assert "verified (0 file(s))" not in cl or "verified past its open or gap record" not in slog, \
            "a 0-file verify and a cleared hold\n" + env.logs()
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_kill_in_the_stamp_drop():
    """11/15/2026 (review 3/4): gxrepl killed -9 in the middle of an open record's stamp drop (after the derived state
    and derived.stamp, before the index stamps): after a restart the folder is rebuilt and its index is its data's"""
    env, ws, aws, w = _recgap_env("incr_kill_drop")
    try:
        d1 = os.path.join(ws, "d1")
        env.kill("client")
        _restart_server(env, {"GXREPL_TEST_KILL_IN_DROP": "1"})
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 621,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, a.stdout
        b = _adds(d1, ws, 622)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        env.start_client()
        end = time.time() + 60
        while time.time() < end and env.server.poll() is None:
            time.sleep(0.2)
        assert env.server.poll() == 9, "the server was not killed in the drop\n" + env.logs()
        _restart_server(env)
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_open_in_a_file_update_is_indexed():
    """11/15/2026 (review 4): writer B's open record and commits reach the copy inside a file update (the reconnect's
    file sync held while B runs, GXREPL_TEST_SYNC_FILE_HOLD), the ApplyFiles path: the copy's index holds writer A's
    recovered objects"""
    env, ws, aws, w = _recgap_env("incr_open_in_files")
    try:
        d1 = os.path.join(ws, "d1")
        env.kill("client")
        hold = os.path.join(env.dir, "sync_hold")
        env.start_client({"GXREPL_TEST_SYNC_FILE_HOLD": hold, "GXREPL_TEST_SYNC_FILE_HOLD_ON": "commit.bin"})
        end = time.time() + 60
        while time.time() < end and not os.path.exists(hold + ".held"):
            time.sleep(0.1)
        assert os.path.exists(hold + ".held"), "the reconnect's file sync never started\n" + env.logs()
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 631,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, a.stdout
        b = _adds(d1, ws, 632)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        put(hold, b"go\n")
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def _fill_without_records(name, server_env=None):
    """commits made without the ship log (--no-ship: pages no record explains) then a clean writer whose open record
    says it recovered nothing: only the fill check can send d1 to a rebuild. Returns (log since, rebuilds, index ok,
    detail, env, ws, aws, w)."""
    env, ws, aws, w = _stamped(name, commits=20)
    w.wait(120)
    _finish(env, ws, aws, w)
    _wait_stamped(env, aws, "d1")
    if server_env:
        _restart_server(env, server_env)
    time.sleep(3)
    r0 = _rebuilds(env)
    log0 = len(open(os.path.join(env.dir, "server.log")).read())
    r = subprocess.run([os.path.join(BIN, "gxrepl_testwriter"), "write", os.path.join(ws, "d1"), "--commits", "3",
                        "--per", "6", "--commit-block", "--no-ship", "--seed", "411"],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert r.returncode == 0, r.stdout
    r = _objects_writer(os.path.join(ws, "d1"), "--commits", 3, "--per", 6, "--commit-block", "--ship-root", ws,
                        "--seed", 412)
    assert r.returncode == 0, r.stdout
    _wait_gen(ws, aws, "d1", 90)
    _wait_stamped(env, aws, "d1", 120)
    time.sleep(3)
    _wait_stamped(env, aws, "d1", 120)
    log = open(os.path.join(env.dir, "server.log")).read()[log0:]
    ok, d = _index_equals_rebuild(env, os.path.join(aws, "d1"))
    return log, _rebuilds(env) - r0, ok, d, env, ws, aws, w


def test_incremental_fill_without_records_drops_the_stamps():
    """11/15/2026 (stamp-keeping fill): a fill keeps the folder's stamps only when every page it writes is what the
    update's records write or what the copy already holds. Commits made without the ship log (--no-ship: pages no
    record explains, like a recovered batch) reach the copy by files: the stamps go, the folder is rebuilt, and the
    copy's index agrees with its data.
    11/16/2026 (beta4 review L3): isolated again: the second writer's open is clean (it recovered nothing), so no
    open-record rule sends d1 to a rebuild; only the fill check does ("copied by files")"""
    log, reb, ok, d, env, ws, aws, w = _fill_without_records("incr_fill_norec")
    try:
        assert "rebuild needed: a writer's open record" not in log and "rebuild needed: a gap record" not in log, \
            "an open or gap record, not the fill check, sent d1 to a rebuild: the test isolates nothing\n" + env.logs()
        assert "copied by files" in log, "pages without records kept the stamps\n" + env.logs()
        assert reb > 0, env.logs()
        assert ok, f"the copy's index is not its data's (copy, rebuild): {d}\n" + env.logs()
        check_db(os.path.join(aws, "d1"))
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_fill_without_records_needs_the_check():
    """11/16/2026 (beta4 review L3): the same with the fill check switched off (GXREPL_TEST_FILL_TRUST_ALL): the stamps
    stay and the copy's index misses the commits made without the ship log. This shows the test above depends on
    the check alone."""
    log, reb, ok, d, env, ws, aws, w = _fill_without_records("incr_fill_trust", {"GXREPL_TEST_FILL_TRUST_ALL": "1"})
    try:
        assert "copied by files" not in log, "the hook did not switch the check off\n" + env.logs()
        assert not ok, "the index is still its data's with the check off: the fill test proves nothing\n" + env.logs()
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def _hup_verify_round(name, opts, rounds=12):
    """11/15/2026 (stamp-keeping fill): full verifies (SIGHUP) while a writer commits every 0.4 s: commits land
    while the verify compares the files, so the fills it sends are in step with the stream and carry the records
    from the copy's position to the pieces'. Returns (log text since the start,
    rebuilds during the rounds, env, ws, aws); the caller checks and stops env."""
    env = Env(name, {"db": opts})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d1"))
    # a commit every 0.4 s: the client drains the log between commits, so a verify runs while the writer commits
    # (a writer that never pauses defers verifies until it does)
    w = _objects_writer_bg(os.path.join(ws, "d1"), "--commits", 150, "--per", 6, "--sleep", 400, "--commit-block",
                           "--ship-root", ws, "--adds-only", "--size-max", 64)
    time.sleep(1.5)
    env.start_server()
    env.start_client()
    if any("incremental" in o for o in opts["server"]):
        _wait_stamped(env, aws, "d1", 120)
    else:                                     # rebuild mode: a first rebuild (derived.stamp) for the fills to drop
        _wait_units(env, ws, aws, ["d1"])
        end = time.time() + 120
        while time.time() < end and not os.path.exists(os.path.join(aws, "d1", "derived.stamp")):
            time.sleep(0.3)
        assert os.path.exists(os.path.join(aws, "d1", "derived.stamp")), "no first rebuild\n" + env.logs()
    time.sleep(3)
    log0 = len(open(os.path.join(env.dir, "server.log")).read())
    def pieces():                 # the client's pieces sent (a HUP verify logs nothing on the client when in step)
        try:
            return json.load(open(env.cli_status))["sets"]["db"].get("pieces_sent", 0)
        except (OSError, ValueError, KeyError):
            return 0
    p0 = pieces()
    r0 = _rebuilds(env)
    for _ in range(rounds):
        if w.poll() is not None: break
        env.client.send_signal(signal.SIGHUP)
        time.sleep(3)
    w.wait(300)
    _wait_gen(ws, aws, "d1", 120)
    time.sleep(3)
    return (open(os.path.join(env.dir, "server.log")).read()[log0:], _rebuilds(env) - r0, env, ws, aws,
            pieces() - p0)


def test_incremental_explained_fill_keeps_the_stamps():
    """11/15/2026 (stamp-keeping fill): on an incremental set, a verify's in-step fill whose pages are all its
    records' or the copy's own keeps the folder's stamps: no "copied by files", no rebuild, the index equal to a
    rebuild of the copy's data"""
    log, reb, env, ws, aws, fills = _hup_verify_round("explained_keep", _incr_opts("gxrepl_test", ["derived_delay = 0",
                                                                                           "derived_busy_rebuild = 2"]))
    try:
        _wait_stamped(env, aws, "d1", 180)
        print(f"    {fills} pieces sent by verifies during the writer's commits, {log.count('copied by files')} copied by files, "
              f"{reb} rebuilds", flush=True)
        assert fills >= 3, "too few fills: the test proves nothing\n" + env.logs()
        assert "copied by files" not in log and reb == 0, "an explained fill dropped the stamps:\n" + env.logs()
        ok, d = _index_equals_rebuild(env, os.path.join(aws, "d1"))
        assert ok, f"the copy's index is not its data's: {d}\n" + env.logs()
        check_db(os.path.join(aws, "d1"))
    finally:
        env.stop()


def test_rebuild_mode_fill_still_drops_the_stamps():
    """11/15/2026 (review HIGH): derived_apply = rebuild with derived_source set: a fill of the derived source drops
    the stamps (derived.stamp and the derived state) whatever its pages, and the copy's index ends equal to a rebuild"""
    log, reb, env, ws, aws, fills = _hup_verify_round("rebuild_mode_fill", _derived_opts(["derived_source = data.gxd",
                                                                                          "derived_busy_rebuild = 2"]))
    try:
        end = time.time() + 120
        while time.time() < end and "INDEXES" in torn(env).stdout:
            time.sleep(0.5)
        print(f"    {fills} pieces sent, {log.count('copied by files')} copied by files, {reb} rebuilds",
              flush=True)
        assert fills >= 3, "too few fills: the test proves nothing\n" + env.logs()
        assert log.count("copied by files") >= 1, "no fill dropped the stamps in rebuild mode:\n" + env.logs()
        ok, d = _index_equals_rebuild(env, os.path.join(aws, "d1"))
        assert ok, f"the copy's index is not its data's: {d}\n" + env.logs()
    finally:
        env.stop()


def test_incremental_clean_restarts_keep_the_stamps():
    """11/15/2026 (hardening): a writer's open record says what its journal recovered; a clean restart (nothing) is
    not a rebuild and no hold: three restarts, no "rebuild needed" for them, no rebuild, the index its data's"""
    env, ws, aws, w = _recgap_env("incr_clean_restarts")
    try:
        d1 = os.path.join(ws, "d1")
        r0 = _rebuilds(env)
        log0 = len(open(os.path.join(env.dir, "server.log")).read())
        for i in range(3):
            b = _adds(d1, ws, 700 + i)
            assert b.wait(timeout=120) == 0, b.stdout.read()
            _wait_gen(ws, aws, "d1", 120)
            time.sleep(2)
        log = open(os.path.join(env.dir, "server.log")).read()[log0:]
        assert "rebuild needed: a writer's open record" not in log and "waits for its verify" not in log, log[-2000:]
        assert _rebuilds(env) == r0, "a clean restart was rebuilt:\n" + log[-2000:]
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_static_recover_is_indexed():
    """11/15/2026 (hardening): the batch writer A died with is replayed by the static gxdJournal::Recover() before
    writer B opens (B's own Open() then recovers nothing): Recover()'s marker makes B's open record say it, so the
    copy rebuilds and its index holds A's objects"""
    env, ws, aws, w = _recgap_env("incr_static_recover")
    try:
        d1 = os.path.join(ws, "d1")
        env.kill("client")
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 711,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, a.stdout
        r = run([os.path.join(BIN, "gxrepl_testwriter"), "recover", d1])
        assert r.returncode == 0 and "recovered 1 batch" in r.stdout, r.stdout
        assert os.path.exists(os.path.join(d1, "writer.jnl.recovered")), "Recover() left no marker"
        b = _adds(d1, ws, 712)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        assert not os.path.exists(os.path.join(d1, "writer.jnl.recovered")), "the marker stayed after the open record"
        env.start_client()
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def _sha(path):
    with open(path, "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


def test_static_recover_refuses_without_its_marker():
    """11/16/2026 (beta4 review M2): the static Recover() replays a batch only after its marker is on the disk. When
    the marker cannot be written (here its temporary name is taken by a folder) it refuses: nothing replayed, the
    journal kept. Before the fix it replayed silently with no marker, so the next writer's open record said nothing
    was recovered. Once the marker can be written the same journal replays, and the copy's index holds A's objects."""
    env, ws, aws, w = _recgap_env("static_recover_marker")
    try:
        d1 = os.path.join(ws, "d1")
        env.kill("client")
        a = _objects_writer(d1, "--commits", 1, "--per", 3, "--commit-block", "--ship-root", ws, "--seed", 721,
                            "--adds-only", "--size-max", 64, extra_env={"GXDJRNL_TEST_KILL_BEFORE_SHIP": "1"})
        assert a.returncode == 9, a.stdout
        blocker = os.path.join(d1, "writer.jnl.recovered.tmp")
        os.mkdir(blocker)
        before = _sha(os.path.join(d1, "data.gxd"))
        r = run([os.path.join(BIN, "gxrepl_testwriter"), "recover", d1])
        assert r.returncode != 0 and "recovered 0 batch" in r.stdout, "replayed without its marker: " + r.stdout
        assert _sha(os.path.join(d1, "data.gxd")) == before, "the files changed though the replay was refused"
        assert not os.path.exists(os.path.join(d1, "writer.jnl.recovered")), "a marker after a refused replay"
        os.rmdir(blocker)
        r = run([os.path.join(BIN, "gxrepl_testwriter"), "recover", d1])
        assert r.returncode == 0 and "recovered 1 batch" in r.stdout, "the kept journal did not replay: " + r.stdout
        assert os.path.exists(os.path.join(d1, "writer.jnl.recovered")), "Recover() left no marker"
        b = _adds(d1, ws, 722)
        assert b.wait(timeout=120) == 0, b.stdout.read()
        env.start_client()
        _recgap_index_ok(env, ws, aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_orphan_means_rebuild():
    """11/14/2026: an objects record without its commit (a writer crash between them, here the record lost and a gap
    after it) is never applied object by object: rebuild needed, and the index checks after the rebuild"""
    env, ws, aws, w = _stamped("incr_orphan", writer_args=("--split-pair-at", "70"), commits=120)
    try:
        log = _wait_log(env, "rebuild needed", secs=90)
        _finish(env, ws, aws, w)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_incremental_derived_apply_builtin():
    """11/14/2026 (7.4): derived_apply = incremental with the built-in test extractor: the first fill (by files)
    drops the stamps, the derived pass rebuilds and stamps the folder, then every ship log batch updates index.gxd
    object by object in the apply's own transaction (no derived_delay, no rebuild), and the index checks."""
    _incremental_round("gxrepl_test", "incr_builtin")


def test_incremental_derived_apply_so():
    """11/14/2026 (7.4): the same with the extractor loaded from gxrepl_testextractor.so"""
    _incremental_round(_so(), "incr_so")


OLD_REV = "44e2936"   # 11/14/2026: the replication before objects records (F7), for the mixed-version tests


def old_bin():
    """gxrepl and gxrepl_testwriter built from OLD_REV (git archive of this repository), once, next to the build
    under test. A failure to build fails the test (never a skip)."""
    cache = os.path.join(os.path.dirname(os.path.abspath(BIN)), "old_" + OLD_REV)
    out = os.path.join(cache, "build", "utils")
    if all(os.path.exists(os.path.join(out, t)) for t in ("gxrepl", "gxrepl_testwriter")):
        return out
    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    src = os.path.join(cache, "src")
    shutil.rmtree(cache, ignore_errors=True)
    os.makedirs(src)
    arch = subprocess.run(f"git -C '{repo}' archive {OLD_REV} | tar -x -C '{src}'", shell=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert arch.returncode == 0, "cannot unpack " + OLD_REV + ": " + arch.stdout
    nice = ["nice", "-n", "19", "ionice", "-c3"]
    c = subprocess.run(nice + ["cmake", "-S", src, "-B", os.path.join(cache, "build"), "-DDATAREEL_64BIT_DATABASE=ON",
                               "-DCMAKE_BUILD_TYPE=Release"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert c.returncode == 0, c.stdout[-2000:]
    b = subprocess.run(nice + ["cmake", "--build", os.path.join(cache, "build"), "-j2", "--target", "gxrepl",
                               "gxrepl_testwriter"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert b.returncode == 0, b.stdout[-2000:]
    return out


def _log_records(root):
    """every ship log record under root: (stream, type, lsn, payload), without the times and log ids"""
    import struct
    out = []
    logdir = os.path.join(root, ".gxrepl", "log")
    for stream in sorted(os.listdir(logdir)):
        for seg in sorted(f for f in os.listdir(os.path.join(logdir, stream)) if f.endswith(".gxl")):
            b = open(os.path.join(logdir, stream, seg), "rb").read()
            o = 32
            while o + 32 <= len(b):
                _m, t, lsn, _ts, n, _ck = struct.unpack_from("<IIQQII", b, o)
                out.append((stream, t, lsn, b[o + 32:o + 32 + n]))
                o += 32 + n
    return out


def test_notes_off_output_is_unchanged():
    """11/14/2026 (review): with object notes off (the default), the writer's files and ship log records are byte
    for byte what the writer before objects records (OLD_REV) makes (times and log ids aside)."""
    ob = old_bin()
    base = os.path.join(TMP, "golden")
    shutil.rmtree(base, ignore_errors=True)
    outs = {}
    e = {k: v for k, v in os.environ.items() if k != "GXDSHIP_OBJECTS"}
    for name, b in (("old", ob), ("new", BIN)):
        d = os.path.join(base, name)
        os.makedirs(d)
        w = subprocess.run([os.path.join(b, "gxrepl_testwriter"), "write", d, "--commits", "12", "--per", "6",
                            "--seed", "5", "--commit-block"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, env=e)
        assert w.returncode == 0, w.stdout
        outs[name] = d
    for f in ("data.gxd", "index.gxd", "commit.bin"):
        a = open(os.path.join(outs["old"], f), "rb").read()
        n = open(os.path.join(outs["new"], f), "rb").read()
        assert a == n, f + " differs from the old writer's"
    ro, rn = _log_records(outs["old"]), _log_records(outs["new"])
    assert len(ro) == len(rn) and len(ro) > 12, (len(ro), len(rn))
    for x, y in zip(ro, rn):
        assert x == y, f"record {x[2]} differs (type {x[1]} / {y[1]})"
    assert not any(r[1] == 5 for r in rn), "an objects record with notes off"


def _mixed(old_side):
    env = Env("mixed_" + old_side, {"db": {}})
    ob = old_bin()
    setattr(env, old_side + "_bin", os.path.join(ob, "gxrepl"))
    ws, aws = env.roots["db"]
    assert _objects_writer(ws, "--commits", 6).returncode == 0
    env.start_server()
    env.start_client()
    try:
        wait_same(env, "db", 60)
        w = _objects_writer_bg(ws, "--commits", 30, "--per", 5, "--sleep", 100, "--seed", 9)
        assert w.wait(timeout=120) == 0
        wait_same(env, "db", 60)
        check_db(aws)
        assert any(r[1] == 5 for r in _log_records(ws)), "the writer wrote no objects records"
    finally:
        env.stop()


def test_mixed_version_old_copy_skips_objects_records():
    """11/14/2026 (review): a copy running the gxrepl before objects records (OLD_REV), fed by a new source whose
    writer writes them: the copy skips them and matches the source."""
    _mixed("server")


def test_mixed_version_old_source_sends_objects_records():
    """11/14/2026 (review): a source running OLD_REV's gxrepl (its reader may end a batch on an objects record)
    and a new copy: the copy applies everything and matches."""
    _mixed("client")


def test_trailing_orphan_objects_record_does_not_stall():
    """11/14/2026 (review: the stall): a writer that dies right after an objects record leaves it as the log's last
    record; it is held back only while its commit may still come (10 s), then let go: the copy gets in step with
    no record of lag instead of retrying the stream for ever."""
    env = Env("orphan_stall", {"db": {}})
    ws, aws = env.roots["db"]
    w = _objects_writer(ws, "--commits", 6, "--split-crash-at", 5)
    assert w.returncode == 9, w.stdout
    recs = _log_records(ws)
    assert recs[-1][1] == 5, "the log does not end on an objects record"
    env.start_server()
    env.start_client()
    try:
        wait_status(env, lambda s: s["sets"]["db"]["ok"] and not s["sets"]["db"].get("last_error")
                    and s["sets"]["db"].get("streams")
                    and all(x.get("lag", 1) == 0 for x in s["sets"]["db"]["streams"].values()), 45)
        wait_same(env, "db", 30)
        check_db(aws)
    finally:
        env.stop()


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
        # 11/14/2026 (review): one wait for all three (the folder, then its seal last, then the log line), so a pass
        # caught between the folder and its seal is not a failure
        seal = os.path.join(aws, ".gxrepl", "units", "_d1")
        end = time.time() + 30
        while time.time() < end and (os.path.exists(os.path.join(aws, "d1")) or os.path.exists(seal) or
                                     "removed the copy's folder d1: gone from the source" not in
                                     open(os.path.join(env.dir, "server.log")).read()):
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


def test_dead_copy_folder_abandoned_copying_is_swept():
    """11/15/2026 (hardening, PurgeOrphans): the source purged a stream with its folder while the copy's unit was
    still sealed "copying" (ACK is applied, not sealed): a "copying" seal unchanged past GXREPL_COPYING_ABANDONED_SEC
    with no database file in its folder is swept; a fresh "copying" seal is kept"""
    env = Env("dead_copying", {"db": _derived_opts()})
    ws, aws = env.roots["db"]
    os.makedirs(os.path.join(ws, "d2"))
    assert writer(os.path.join(ws, "d2"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 4).returncode == 0
    env.start_server()
    env.start_client()
    try:
        _wait_units(env, ws, aws, ["d2"])
        env.stop()
        units = os.path.join(aws, ".gxrepl", "units")
        for name, age in (("old_copy", 7200), ("new_copy", 0)):
            d = os.path.join(aws, name)
            os.makedirs(d, exist_ok=True)
            open(os.path.join(d, "index.gxd"), "wb").write(b"x" * 1024)
            seal = os.path.join(units, "_" + name)
            open(seal, "w").write("copying\n")
            t0 = time.time() - age
            os.utime(seal, (t0, t0))
            os.utime(os.path.join(d, "index.gxd"), (t0, t0))
        env.start_server(extra_env={"GXREPL_SWEEP_SEC": "1", "GXREPL_DEAD_QUIET_SEC": "0",
                                    "GXREPL_COPYING_ABANDONED_SEC": "3600"})
        end = time.time() + 20
        while time.time() < end and os.path.exists(os.path.join(aws, "old_copy")):
            time.sleep(0.3)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert not os.path.exists(os.path.join(aws, "old_copy")), "an abandoned copying folder stayed\n" + log[-1500:]
        assert not os.path.exists(os.path.join(units, "_old_copy")), "its seal stayed"
        assert os.path.isdir(os.path.join(aws, "new_copy")), "a copy in progress was swept"
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


def test_dead_copy_folder_stamps_swept():
    """11/14/2026 (7.4 per-object derived apply): a dead copy folder holding <index>.stamp sidecars (index.gxd.stamp
    of the derived index.gxd) is swept whole; a stamp of a file that is not derived (other.btx.stamp) is not gxrepl's
    to remove and keeps its folder."""
    env = Env("dead_stamps", {"db": _derived_opts()})
    ws, aws = env.roots["db"]
    tw = os.path.join(BIN, "gxrepl_testwriter")
    os.makedirs(os.path.join(ws, "d2"))
    assert writer(os.path.join(ws, "d2"), "--commits", 3, "--no-ship", "--commit-block", "--seed", 4).returncode == 0
    env.start_server()
    env.start_client()
    try:
        _wait_units(env, ws, aws, ["d2"])
        env.stop()
        units = os.path.join(aws, ".gxrepl", "units")
        for name, files in (("dead_s", ["index.gxd", "derived.stamp", "news.jnl"]), ("keep_s", ["index.gxd"])):
            d = os.path.join(aws, name)
            os.makedirs(d, exist_ok=True)
            for f in files:
                open(os.path.join(d, f), "wb").write(b"x" * 1024)
            open(os.path.join(units, "_" + name), "w").write("gen 6\n")
        assert run([tw, "stamp", os.path.join(aws, "dead_s", "index.gxd.stamp"), "cccccccccccccccc", "7"]).returncode == 0
        assert run([tw, "stamp", os.path.join(aws, "keep_s", "other.btx.stamp"), "cccccccccccccccc", "7"]).returncode == 0
        env.start_server(extra_env={"GXREPL_SWEEP_SEC": "1", "GXREPL_DEAD_QUIET_SEC": "0"})
        end = time.time() + 30
        while time.time() < end and os.path.exists(os.path.join(aws, "dead_s")):
            time.sleep(0.3)
        log = open(os.path.join(env.dir, "server.log")).read()
        assert not os.path.exists(os.path.join(aws, "dead_s")), ("the folder with a stamp stayed",
                                                               os.listdir(os.path.join(aws, "dead_s")), log[-1500:])
        assert not os.path.exists(os.path.join(units, "_dead_s")), "its seal stayed"
        time.sleep(3)                                                   # a few more sweeps
        assert os.path.exists(os.path.join(aws, "keep_s", "other.btx.stamp")), "a stamp of a non-derived file was removed"
        assert os.path.isdir(os.path.join(aws, "d2")) and os.path.exists(os.path.join(aws, "d2", "commit.bin"))
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
        # 11/14/2026 (review 8): a copy by files drops derived.stamp, so the indexes read as missing, not stale
        assert t.returncode == 0 and ("stale" in t.stdout or "missing" in t.stdout) and "INDEXES" in t.stdout, t.stdout
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
    while time.time() < end:
        t = torn(env)
        if t.returncode == 0 and "INDEXES" not in t.stdout:
            return
        time.sleep(0.5)
    t = torn(env)
    assert t.returncode == 0 and "INDEXES" not in t.stdout, f"rc {t.returncode}: " + t.stdout + env.logs()


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
        # re-review A: the restart's verify fill drops no derived.stamp (it never wrote the indexes' source)
        assert os.path.exists(os.path.join(aws, "d1", "derived.stamp")) and \
            os.path.exists(os.path.join(aws, "d2", "derived.stamp"))
        assert "rebuild needed" not in open(os.path.join(env.dir, "server.log")).read(), env.logs()
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


def test_delete_reseals_before_unlink():
    """11/14/2026 (seal per folder, re-review): deletes = yes removes a database file of a log-sealed folder (the same
    generation): its seal goes to C = L (its derived index rebuilt). The reseal is written under the apply lock BEFORE
    the unlink: a copy killed between them (GXREPL_TEST_DELETE_CRASH, test builds) has C = L with the file still
    there, never the old C with the file gone; after the restart the file goes and the index is rebuilt."""
    assert _test_hooks(), "needs a test build (-DGXREPL_TEST_HOOKS=ON)"
    rebuild = f"{os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}"
    opts = {"client": CB_OPTS["client"] + ["derived = index.gxd", "deletes = yes"],
            "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 1",
                                           "derived_busy_rebuild = 2"]}
    env = Env("del_reseal", {"db": opts})
    ws, aws = env.roots["db"]
    for d in ("d1", "d2"):
        os.makedirs(os.path.join(ws, d), exist_ok=True)
    w = lambda d, n, seed: writer(os.path.join(ws, d), "--commits", n, "--commit-block", "--ship-root", ws,
                                  "--stream", "shared", "--seed", seed)
    assert w("d1", 4, 1).returncode == 0
    assert w("d2", 4, 2).returncode == 0
    env.start_server()
    env.start_client()
    try:
        _wait_caught_up(env)
        _wait_indexes(env)
        l0 = int(_seal_fields(aws, "d1")[2])
        assert w("d2", 2, 3).returncode == 0              # the stream moves on: d1's L past its C
        end = time.time() + 30
        while time.time() < end and int(_seal_fields(aws, "d1")[2]) <= l0:
            time.sleep(0.2)
        _wait_indexes(env)
        s1 = _seal_fields(aws, "d1")
        assert s1[2] != s1[4], s1
        n1 = _rebuilds_of(env, "d1")
        env.kill("client")
        env.server.terminate(); env.server.wait(10)
        put(os.path.join(aws, "d1", "old.gxd"), b"gone from the source")
        env.start_server(extra_env={"GXREPL_TEST_DELETE_CRASH": "1"})
        env.start_client()
        end = time.time() + 60
        while time.time() < end and env.server.poll() is None:
            time.sleep(0.2)
        assert env.server.poll() == 9, "the delete never reached the hook"
        s2 = _seal_fields(aws, "d1")
        print(f"    killed between the reseal and the unlink: seal {s1} -> {s2}, old.gxd there: "
              f"{os.path.exists(os.path.join(aws, 'd1', 'old.gxd'))}", flush=True)
        assert s2[4] == s2[2] == s1[2], (s1, s2)          # C = L, written first
        assert os.path.exists(os.path.join(aws, "d1", "old.gxd"))
        env.start_server()
        end = time.time() + 60
        while time.time() < end and os.path.exists(os.path.join(aws, "d1", "old.gxd")):
            time.sleep(0.2)
        assert not os.path.exists(os.path.join(aws, "d1", "old.gxd"))
        end = time.time() + 30
        while time.time() < end and _rebuilds_of(env, "d1") == n1:
            time.sleep(0.5)
        _wait_indexes(env)
        assert _rebuilds_of(env, "d1") == n1 + 1, env.logs()
        check_db(os.path.join(aws, "d1"))
    finally:
        env.stop()


def test_verify_sees_files_named_during_its_pass():
    """11/14/2026 (the verify race, 7.3.2): a new stream's first record names only one file (the writer's fresh
    database init: no commit block yet); the copy's verify fills it out of step, so the unit is "copying"; the
    writer's next records name the commit block. The client read the stream's file list once per pass, so the
    commit block was never verified and the unit stayed "copying" until the next verify (records carrying the
    commit block never clear a fill's mark). Now the list is read again after the verify, and a stream with new
    files is verified again in the same pass. Deterministic: the writer waits after its first record, and the client
    (GXREPL_TEST_VERIFY_HOLD, test builds) holds after its first verify until the writer's next records are in."""
    assert _test_hooks(), "needs a test build (-DGXREPL_TEST_HOOKS=ON)"
    env = Env("verify_new_files", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    gate, hold = os.path.join(env.dir, "writer.go"), os.path.join(env.dir, "verify.go")
    w = writer_bg(ws, "--commits", 3, "--commit-block", "--wait-at", 0, "--wait-file", gate)
    end = time.time() + 30
    while time.time() < end and not streams_of(ws):
        time.sleep(0.1)
    time.sleep(1.0)                                    # the writer waits after its first record
    lf = os.path.join(ws, ".gxrepl", "log", streams_of(ws)[0], "files")
    first = open(lf).read().split()
    assert "commit.bin" not in first, first
    env.start_server()
    env.start_client(extra_env={"GXREPL_TEST_VERIFY_HOLD": hold})
    seal = os.path.join(aws, ".gxrepl", "units", "_root")
    try:
        end = time.time() + 30
        while time.time() < end and not os.path.exists(hold + ".held"):
            time.sleep(0.1)
        held = os.path.exists(hold + ".held")
        open(gate, "w").close()                         # the writer's next records name the commit block
        assert w.wait(60) == 0
        assert "commit.bin" in open(lf).read().split()
        open(hold, "w").close()
        end = time.time() + 30
        while time.time() < end and not (os.path.exists(seal) and open(seal).read().startswith("log ")):
            time.sleep(0.2)
        s = open(seal).read() if os.path.exists(seal) else "(no seal)"
        print(f"    first record named {first}; held after the first verify: {held}; seal now {s.strip()!r}", flush=True)
        assert s.startswith("log "), "the unit stayed " + s.strip() + " (its commit block never verified)\n" + env.logs()
        assert held, "the client never held after its verify (GXREPL_TEST_VERIFY_HOLD)"
        assert "names new file(s) since the pass began" in open(os.path.join(env.dir, "client.log")).read(), env.logs()
        wait_same(env, "db", 60)
        t = torn(env)
        assert t.returncode == 0, t.stdout
        check_db(aws)
    finally:
        if w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_derived_stamps_are_derived():
    """11/14/2026 (7.4 per-object derived apply): <index>.stamp matches the set's derived pattern, so a stamp on
    the source is never shipped (not even when an include pattern names it: never a data file), the copy's own
    stamp is never replaced or deleted (deletes = yes), and on an incremental set an index without a stamp is a
    warning in "gxrepl torn" (rebuild mode), never torn."""
    rebuild = f"{os.path.join(BIN, 'gxrepl_testwriter')} reindex {{dir}} {{lsn}}"
    opts = {"client": CB_OPTS["client"] + ["derived = index.gxd", "include = *.stamp", "deletes = yes"],
            "server": CB_OPTS["server"] + ["derived = index.gxd", f"derived_rebuild = {rebuild}", "derived_delay = 2",
                                           "include = *.stamp"]}
    # (rebuild mode while the copy syncs: an incremental set's rebuild replaces the stamps itself, part 2)
    env = Env("derived_stamps", {"db": opts})
    ws, aws = env.roots["db"]
    tw = os.path.join(BIN, "gxrepl_testwriter")
    assert writer(ws, "--commits", 12, "--no-ship", "--commit-block").returncode == 0
    assert run([tw, "stamp", os.path.join(ws, "index.gxd.stamp"), "aaaaaaaaaaaaaaaa", "999"]).returncode == 0
    os.makedirs(aws, exist_ok=True)
    assert run([tw, "stamp", os.path.join(aws, "index.gxd.stamp"), "bbbbbbbbbbbbbbbb", "5"]).returncode == 0
    env.start_server()
    try:
        assert env.once().returncode == 0
        r = run([tw, "readstamp", os.path.join(aws, "index.gxd.stamp")])
        assert r.returncode == 0 and "bbbbbbbbbbbbbbbb lsn 5" in r.stdout, "the copy's stamp changed: " + r.stdout
        assert writer(ws, "--commits", 3, "--no-ship", "--commit-block", "--seed", 41).returncode == 0
        assert env.once().returncode == 0
        r = run([tw, "readstamp", os.path.join(aws, "index.gxd.stamp")])
        assert r.returncode == 0 and "bbbbbbbbbbbbbbbb lsn 5" in r.stdout, "the copy's stamp changed: " + r.stdout
        end = time.time() + 30
        while time.time() < end and "INDEXES" in torn(env).stdout:
            time.sleep(0.5)
        t = torn(env)
        assert t.returncode == 0 and "has no stamp" not in t.stdout, t.stdout + env.logs()
        # the set goes incremental (torn reads the config)
        with open(env.srv_conf) as f:
            conf = f.read().replace("include = *.stamp", "include = *.stamp\nderived_apply = incremental", 1)
        with open(env.srv_conf, "w") as f:
            f.write(conf)
        t = torn(env)
        assert t.returncode == 0 and "has no stamp" not in t.stdout, t.stdout
        # No stamp (a copy on older code, or one never stamped): rebuild mode, a warning only
        os.remove(os.path.join(aws, "index.gxd.stamp"))
        t = torn(env)
        assert t.returncode == 0 and "index.gxd has no stamp (rebuild mode)" in t.stdout, t.stdout
        assert not os.path.exists(os.path.join(aws, "index.gxd.stamp")), "the source's stamp was shipped"
        # A damaged stamp: the same warning kind
        put(os.path.join(aws, "index.gxd.stamp"), os.urandom(200))
        t = torn(env)
        assert t.returncode == 0 and "index.gxd has a damaged stamp (rebuild mode)" in t.stdout, t.stdout
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


# ---------------- 11/14/2026: the drain-verify fix ----------------
# A writer killed after its batch is committed in its journal but before the ship log has it
# (GXDJRNL_TEST_KILL_BEFORE_SHIP); the next writer's journal recovery replays the batch into the files with no
# record, and its open record asks for a verify. If that open record is drained inside a file sync
# (GXREPL_TEST_SYNC_FILE_HOLD makes the records arrive while the reconnect's sync_file waits), the verify used
# to be dropped: the copy stayed "in step" at the source's generation with stale pages.

def _same_file(a, b):
    try:
        return open(a, "rb").read() == open(b, "rb").read()
    except OSError:
        return False


def _drain_verify_round(name, client_env=None, recovery=True, pre_open=False):
    """returns (data.gxd the same at the source's generation, client log)"""
    env = Env(name, {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    try:
        assert writer(d1, "--commits", 5, "--commit-block", "--ship-root", ws).returncode == 0
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        end = time.time() + 60                          # the first connect's own verify done before the restart
        while time.time() < end and "verified (" not in open(os.path.join(env.dir, "client.log")).read():
            time.sleep(0.2)
        env.kill("client")
        if pre_open:   # a writer opens and commits while the client is down: the reconnect verifies the stream first
            assert writer(d1, "--commits", 1, "--commit-block", "--ship-root", ws, "--seed", 40).returncode == 0
        hold = os.path.join(env.dir, "sync_hold")
        # held in the sync of index.gxd, after data.gxd was compared (as the 11/14 failure: the drain then
        # applies the records, and nothing compares data.gxd again unless the verify runs)
        e = {"GXREPL_TEST_SYNC_FILE_HOLD": hold, "GXREPL_TEST_SYNC_FILE_HOLD_ON": "index.gxd"}
        e.update(client_env or {})
        env.start_client(e)
        end = time.time() + 60
        while time.time() < end and not os.path.exists(hold + ".held"):
            time.sleep(0.1)
        assert os.path.exists(hold + ".held"), "the reconnect's file sync never started\n" + env.logs()
        if recovery:
            # writer A: killed with a batch in its journal and not in the ship log
            r = subprocess.run([os.path.join(BIN, "gxrepl_testwriter"), "write", d1, "--commits", "3", "--commit-block",
                                "--ship-root", ws, "--seed", "41"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, env=dict(os.environ, GXDJRNL_TEST_KILL_BEFORE_SHIP="1"))
            assert r.returncode == 9, "the kill hook did not stop writer A: " + str(r.returncode) + " " + r.stdout
        # writer B: recovers A's batch (no record), writes its open record and commits
        assert writer(d1, "--commits", 3, "--commit-block", "--ship-root", ws, "--seed", 42).returncode == 0
        put(hold, b"go\n")
        want = gen_of(os.path.join(d1, "commit.bin"))
        end = time.time() + 60
        while time.time() < end and not (gen_of(os.path.join(aws, "d1", "commit.bin")) == want and
                                         _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd"))):
            time.sleep(0.3)
        time.sleep(3)                                   # passes after it: nothing more may change
        same = gen_of(os.path.join(aws, "d1", "commit.bin")) == want and \
            _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd"))
        return same, open(os.path.join(env.dir, "client.log")).read()
    finally:
        env.stop()


def test_drain_verify_recovered_batch_reaches_the_copy():
    """the fix: the copy gets the pages the recovery wrote (the open record's verify, found in the file update)"""
    same, log = _drain_verify_round("dv_both")
    assert same, "the copy's data.gxd differs from the source at its generation\n" + log[-3000:]
    assert "needs a verify (a writer's open record in a file update)" in log, log[-3000:]


def test_drain_verify_sync_file_part_alone():
    """drain()'s part off (GXREPL_TEST_NO_DRAIN_VERIFY): sync_file()'s own check of the records it applies keeps
    the verify (this scenario's records come in the file update, not through a drain)"""
    same, log = _drain_verify_round("dv_sync", {"GXREPL_TEST_NO_DRAIN_VERIFY": "1"})
    assert same, "sync_file()'s part alone did not keep the verify\n" + log[-3000:]


def test_drain_verify_both_parts_off_loses_the_pages():
    """both parts off: the 7.3.3 behaviour, the copy keeps stale pages at the source's generation (proves the test
    reaches the bug)"""
    same, log = _drain_verify_round("dv_none", {"GXREPL_TEST_NO_CALLER_VERIFY": "1", "GXREPL_TEST_NO_DRAIN_VERIFY": "1"})
    assert not same, "the copy matched with both parts off: the test does not reach the bug\n" + log[-3000:]


def test_drain_verify_drain_part_alone():
    """sync_file()'s part off (GXREPL_TEST_NO_CALLER_VERIFY) and the source answering the reconnect's file sync
    "apply the log first" (GXREPL_TEST_DIFF_STATUS2): the records come through the drain inside sync_file(), and
    drain()'s own part of the fix keeps the verify"""
    same, log = _drain_verify_round("dv_drain", {"GXREPL_TEST_NO_CALLER_VERIFY": "1", "GXREPL_TEST_DIFF_STATUS2": "1"})
    assert same, "drain()'s part alone did not keep the verify\n" + log[-3000:]
    assert "needs a verify (a writer's open record)" in log, log[-3000:]


def test_drain_verify_drain_path_both_parts_off_loses_the_pages():
    """the same drain path with both parts off loses the pages (proves the drain test reaches the bug)"""
    same, log = _drain_verify_round("dv_drain_none", {"GXREPL_TEST_NO_CALLER_VERIFY": "1", "GXREPL_TEST_NO_DRAIN_VERIFY": "1",
                                                      "GXREPL_TEST_DIFF_STATUS2": "1"})
    assert not same, "the copy matched with both parts off on the drain path\n" + log[-3000:]


def test_diff_pieces_against_a_racing_commit():
    """11/14/2026 (hypothesis a): the source holds a file update after it read the position and the records (the
    journal read lock held, GXREPL_TEST_DIFF_HOLD) while a writer commits pages of that file: the commit waits for
    the lock, the pieces match the position, and the copy's data.gxd ends byte for byte the source's"""
    env = Env("diff_race", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    w = None
    try:
        assert writer(d1, "--commits", 5, "--commit-block", "--ship-root", ws).returncode == 0
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        env.kill("client")
        hold = os.path.join(env.dir, "diff_hold")
        env.start_client({"GXREPL_TEST_DIFF_HOLD": hold})
        end = time.time() + 60
        while time.time() < end and not os.path.exists(hold + ".held"):
            time.sleep(0.1)
        assert os.path.exists(hold + ".held"), "the file update never started\n" + env.logs()
        g0 = gen_of(os.path.join(d1, "commit.bin"))
        w = writer_bg(d1, "--commits", 6, "--per", 20, "--commit-block", "--ship-root", ws, "--seed", 43)
        time.sleep(2)
        assert w.poll() is None, "the writer committed while the source held the journal read lock: " + w.stdout.read()
        g1 = gen_of(os.path.join(d1, "commit.bin"))
        assert g1 == g0 or g1 == g0 + 1, f"the writer committed while the source held the journal read lock: gen {g0} -> {g1}"
        put(hold, b"go\n")
        assert w.wait(60) == 0, w.stdout.read()
        want = gen_of(os.path.join(d1, "commit.bin"))
        end = time.time() + 60
        while time.time() < end and not (gen_of(os.path.join(aws, "d1", "commit.bin")) == want and
                                         _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd"))):
            time.sleep(0.3)
        assert gen_of(os.path.join(aws, "d1", "commit.bin")) == want and \
            _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")), env.logs()
    finally:
        if w and w.poll() is None: w.kill(); w.wait()
        env.stop()


def test_position_gone_from_the_same_log_verifies():
    """11/14/2026 (review item 1): the copy offline while its next records leave the log (the same log id: the
    oldest segments removed, as ship_log_max_total does): at the reconnect the copy is verified and matches"""
    env = Env("pos_gone", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    try:
        assert writer(d1, "--commits", 5, "--commit-block", "--ship-root", ws, "--segment", 65536).returncode == 0
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        env.kill("client")
        env.kill("server")
        assert writer(d1, "--commits", 60, "--per", 8, "--commit-block", "--ship-root", ws, "--segment", 65536,
                      "--seed", 44).returncode == 0
        ld = os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl")
        segs = sorted(f for f in os.listdir(ld) if f.endswith(".gxl"))
        assert len(segs) >= 3, segs
        for f in segs[:2]:                              # the copy's next records go, the log id stays
            os.remove(os.path.join(ld, f))
        n = len(open(os.path.join(env.dir, "client.log")).read())
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"], 90)
        log = open(os.path.join(env.dir, "client.log")).read()[n:]
        assert "needs a verify" in log, log
        assert "gave up on" not in log, log
    finally:
        env.stop()


def test_drain_verify_open_during_its_own_verify():
    """the open record of a recovering writer arrives while that same stream is being verified (the file sync of
    the stream's verify, held): the stream is verified once more in the next pass, and the copy matches"""
    same, log = _drain_verify_round("dv_again", pre_open=True)
    assert same, "the copy's data.gxd differs after a verify that saw an open record\n" + log[-3000:]
    assert "needs a verify again after this one" in log, log[-3000:]


def test_drain_verify_restart_burst_one_verify():
    """11/14/2026 (review: no verify cascade): a burst of 6 writer restarts while the client is down gives one
    verify at the reconnect: the open records the verify catches up on never ask for it again"""
    env = Env("dv_burst", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    try:
        assert writer(d1, "--commits", 5, "--commit-block", "--ship-root", ws).returncode == 0
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        end = time.time() + 60
        while time.time() < end and "verified (" not in open(os.path.join(env.dir, "client.log")).read():
            time.sleep(0.2)
        env.kill("client")
        for i in range(6):                              # six writer restarts, each with its open record
            assert writer(d1, "--commits", 2, "--commit-block", "--ship-root", ws, "--seed", 50 + i).returncode == 0
        n = len(open(os.path.join(env.dir, "client.log")).read())
        env.start_client()
        _wait_units(env, ws, aws, ["d1"], 90)
        time.sleep(4)                                   # later passes: nothing more
        log = open(os.path.join(env.dir, "client.log")).read()[n:]
        assert log.count("d1_writer.jnl verified (") == 1, log
        assert "again after this one" not in log, log
        assert _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")), "data.gxd differs"
    finally:
        env.stop()


def _during_own_verify(name, action, client_env=None):
    """a reconnect verifies the stream (a writer's open while the client was down); its file sync of index.gxd is
    held; action(env, ws, d1) runs during the hold; returns (data.gxd the same at the end, client log)"""
    env = Env(name, {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    try:
        assert writer(d1, "--commits", 5, "--commit-block", "--ship-root", ws, "--segment", 65536).returncode == 0
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        end = time.time() + 60
        while time.time() < end and "verified (" not in open(os.path.join(env.dir, "client.log")).read():
            time.sleep(0.2)
        env.kill("client")
        assert writer(d1, "--commits", 1, "--commit-block", "--ship-root", ws, "--seed", 40, "--segment", 65536).returncode == 0
        hold = os.path.join(env.dir, "sync_hold")
        ce = {"GXREPL_TEST_SYNC_FILE_HOLD": hold, "GXREPL_TEST_SYNC_FILE_HOLD_ON": "index.gxd"}
        ce.update(client_env or {})
        env.start_client(ce)
        end = time.time() + 60
        while time.time() < end and not os.path.exists(hold + ".held"):
            time.sleep(0.1)
        assert os.path.exists(hold + ".held"), "the verify's file sync never started\n" + env.logs()
        action(env, ws, d1)
        put(hold, b"go\n")
        want = gen_of(os.path.join(d1, "commit.bin"))
        end = time.time() + 90
        while time.time() < end and not (gen_of(os.path.join(aws, "d1", "commit.bin")) == want and
                                         _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd"))):
            time.sleep(0.3)
        time.sleep(3)
        same = gen_of(os.path.join(aws, "d1", "commit.bin")) == want and \
            _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd"))
        return same, open(os.path.join(env.dir, "client.log")).read()
    finally:
        env.stop()


def test_drain_verify_position_gone_during_its_own_verify():
    """review: the copy's next records leave the log (the oldest segments removed) while the stream is being
    verified: the drain's "position gone" note asks for the verify again (lsn 0 / ULLONG_MAX = always)"""
    def act(env, ws, d1):
        assert writer(d1, "--commits", 40, "--per", 8, "--commit-block", "--ship-root", ws, "--seed", 45,
                      "--segment", 65536).returncode == 0
        ld = os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl")
        segs = sorted(f for f in os.listdir(ld) if f.endswith(".gxl"))
        assert len(segs) >= 3, segs
        for f in segs[:-1]:
            os.remove(os.path.join(ld, f))
    same, log = _during_own_verify("dv_gone_own", act)
    assert same, "the copy differs after its position left the log during its verify\n" + log[-3000:]
    tail = log[log.find("connected to", log.find("connected to") + 1):]
    assert "again after this one (the copy's position is gone" in tail or tail.count("verified (") >= 2, tail


def test_drain_verify_big_record_during_its_own_verify():
    """11/15/2026 (F3): a record bigger than max_message drained while the stream is being verified (its file sync
    held): the verify is asked for again (always), as for a position gone, and the copy ends equal; the log line says
    "always", never ULLONG_MAX raw"""
    def act(env, ws, d1):
        assert writer(d1, "--commits", 1, "--big", 1500000, "--commit-block", "--ship-root", ws, "--seed", 46,
                      "--segment", 65536).returncode == 0
        assert writer(d1, "--commits", 3, "--commit-block", "--ship-root", ws, "--seed", 47,
                      "--segment", 65536).returncode == 0
    same, log = _during_own_verify("dv_big_own", act)
    assert same, "the copy differs after a big record during its verify\n" + log[-3000:]
    tail = log[log.find("connected to", log.find("connected to") + 1):]
    assert "bigger than max_message" in tail, tail
    # verified again after the big record (the writer's own open asks for it too: a test writer always opens)
    assert "verified (" in tail[tail.find("bigger than max_message"):], tail
    assert "18446744073709551615" not in log, log[-3000:]


def test_drain_verify_log_remade_during_its_own_verify():
    """review: the stream's log is made again (new log id, lsns from 1) while the stream is being verified: its
    records are never taken for the old log's, and the copy ends byte for byte the source's"""
    def act(env, ws, d1):
        shutil.rmtree(os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl"))
        assert writer(d1, "--commits", 6, "--per", 8, "--commit-block", "--ship-root", ws, "--seed", 46).returncode == 0
    same, log = _during_own_verify("dv_relog_own", act)
    assert same, "the copy differs after the log was made again during its verify\n" + log[-3000:]


def test_log_remade_by_a_nightly_reopen():
    """11/14/2026 (review gap 2, osa_ingest's 20:30 late pass): a day folder archived (moved out of the set), its
    stream's log purged, the folder reopened under the same stream name (a new log, lsns from 1), late lines
    written, archived and purged again, with gxrepl and the writers running, two nights in a row: after each
    night's late lines the copy's data.gxd is byte for byte the source's"""
    env = Env("nightly_reopen", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    arch = os.path.join(env.dir, "archive")
    os.makedirs(d1)
    os.makedirs(arch)
    ld = os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl")
    def g(p):                                           # the copy's folder may be gone for a moment (dead copy)
        return gen_of(p) if os.path.exists(p) else -1
    try:
        assert writer(d1, "--commits", 8, "--commit-block", "--ship-root", ws).returncode == 0
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        for night, gap in enumerate((0, 0.5, 2, 0)):     # 0: reopened before the copy saw the folder go
            shutil.move(d1, os.path.join(arch, "d1"))     # archived: out of the set
            shutil.rmtree(ld, ignore_errors=True)         # its stream's log purged
            time.sleep(gap)
            shutil.move(os.path.join(arch, "d1"), d1)     # reopened for the late lines: the same stream name
            assert writer(d1, "--commits", 4, "--per", 6, "--commit-block", "--ship-root", ws,
                          "--seed", 60 + night).returncode == 0
            want = gen_of(os.path.join(d1, "commit.bin"))
            end = time.time() + 60
            while time.time() < end and not (g(os.path.join(aws, "d1", "commit.bin")) == want and
                                             _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd"))):
                time.sleep(0.3)
            time.sleep(2)
            assert _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")), \
                f"night {night + 1}: the copy's data.gxd differs from the source's\n" + env.logs()
    finally:
        env.stop()


def test_drain_never_reads_a_new_log_as_the_old_one():
    """11/14/2026 (review F1): the stream's log made again (new log id, more records than the copy's old position)
    while its verify is held, and the reconnect's file sync told "apply the log first" (GXREPL_TEST_DIFF_STATUS2):
    the drain reads only the log the copy's position is on, so the new log's records never continue the old
    position (position gone: resync); the copy ends byte for byte the source's"""
    def act(env, ws, d1):
        shutil.rmtree(os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl"))
        assert writer(d1, "--commits", 30, "--per", 8, "--commit-block", "--ship-root", ws, "--seed", 47).returncode == 0
    same, log = _during_own_verify("dv_newlog_drain", act, {"GXREPL_TEST_DIFF_STATUS2": "1"})
    assert same, "the copy differs after a drain across a log made again\n" + log[-3000:]


def _reopen_during_a_verify(name, client_env=None, sealed_within=60):
    """11/15/2026 (the nightly-reopen bug, .53 too): the stream's first verify synced data.gxd, then (its sync of
    index.gxd held) the day folder is archived, its log purged and the folder reopened with late lines (a new log).
    The later files' updates jump to the new log: the stream is verified again, and the seal guard never applies the
    folder's commit block across that jump. Invariant at every look: the copy at the source's generation means
    data.gxd byte for byte the source's; and so at the end"""
    env = Env(name, {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    arch = os.path.join(env.dir, "archive")
    os.makedirs(d1)
    os.makedirs(arch)
    def g(p):
        return gen_of(p) if os.path.exists(p) else -1
    try:
        assert writer(d1, "--commits", 8, "--commit-block", "--ship-root", ws).returncode == 0
        env.start_server()
        hold = os.path.join(env.dir, "sync_hold")
        ce = {"GXREPL_TEST_SYNC_FILE_HOLD": hold, "GXREPL_TEST_SYNC_FILE_HOLD_ON": "index.gxd"}
        ce.update(client_env or {})
        env.start_client(ce)
        end = time.time() + 60
        while time.time() < end and not os.path.exists(hold + ".held"):
            time.sleep(0.1)
        assert os.path.exists(hold + ".held"), "the first verify's file sync never started\n" + env.logs()
        shutil.move(d1, os.path.join(arch, "d1"))
        shutil.rmtree(os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl"), ignore_errors=True)
        shutil.move(os.path.join(arch, "d1"), d1)
        assert writer(d1, "--commits", 4, "--per", 6, "--commit-block", "--ship-root", ws, "--seed", 60).returncode == 0
        put(hold, b"go\n")
        want = g(os.path.join(d1, "commit.bin"))
        a, b = os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")
        t0 = time.time()
        end = t0 + sealed_within
        while time.time() < end:
            if g(os.path.join(aws, "d1", "commit.bin")) == want:
                assert _same_file(a, b), "the copy is at the source's generation with a stale data.gxd\n" + env.logs()
                break
            time.sleep(0.05)
        time.sleep(3)
        assert g(os.path.join(aws, "d1", "commit.bin")) == want and _same_file(a, b), env.logs()
    finally:
        env.stop()


def _gen_or(p):
    return gen_of(p) if os.path.exists(p) else -1


def _reopen_setup(name, client_env=None, opens=1):
    """the first verify held in its index.gxd sync while the day is archived, its log purged, reopened and written
    (opens writers, one after another); returns (env, ws, aws, d1, the source's generation) after the release"""
    env = Env(name, {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    arch = os.path.join(env.dir, "archive")
    os.makedirs(d1)
    os.makedirs(arch)
    assert writer(d1, "--commits", 8, "--commit-block", "--ship-root", ws).returncode == 0
    env.start_server()
    hold = os.path.join(env.dir, "sync_hold")
    ce = {"GXREPL_TEST_SYNC_FILE_HOLD": hold, "GXREPL_TEST_SYNC_FILE_HOLD_ON": "index.gxd"}
    ce.update(client_env or {})
    env.start_client(ce)
    end = time.time() + 60
    while time.time() < end and not os.path.exists(hold + ".held"):
        time.sleep(0.1)
    assert os.path.exists(hold + ".held"), "the first verify's file sync never started\n" + env.logs()
    shutil.move(d1, os.path.join(arch, "d1"))
    shutil.rmtree(os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl"), ignore_errors=True)
    shutil.move(os.path.join(arch, "d1"), d1)
    for k in range(opens):
        assert writer(d1, "--commits", 2, "--per", 6, "--commit-block", "--ship-root", ws, "--seed", 60 + k).returncode == 0
    put(hold, b"go\n")
    return env, ws, aws, d1, _gen_or(os.path.join(d1, "commit.bin"))


def test_seal_guard_repeated_opens_seal_in_time():
    """review: five writer opens during the held verify: the guard waits only for the stream's verifies, and the copy
    is sealed at the source's generation, byte for byte, within 20 s"""
    env, ws, aws, d1, want = _reopen_setup("reopen_opens", opens=5)
    try:
        end = time.time() + 20
        while time.time() < end and not (_gen_or(os.path.join(aws, "d1", "commit.bin")) == want and
                                         _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd"))):
            time.sleep(0.2)
        assert _gen_or(os.path.join(aws, "d1", "commit.bin")) == want and \
            _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")), env.logs()
    finally:
        env.stop()


def test_seal_guard_stream_back_after_a_pass_away():
    """review: a stream queued to verify again drops out of the source's list for a pass (its log moved aside) and
    comes back on the same log: no stale verify-again flag holds its commit block; sealed within 20 s"""
    def act(env, ws, d1):
        assert writer(d1, "--commits", 2, "--commit-block", "--ship-root", ws, "--seed", 48).returncode == 0
        ld = os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl")
        os.rename(ld, ld + ".away")
        time.sleep(2.5)
        os.rename(ld + ".away", ld)
    same, log = _during_own_verify("dv_back_after", act)
    assert same, "the copy is not sealed byte for byte after the stream came back\n" + log[-3000:]


def test_reopen_during_a_verify_never_seals_stale_data():
    """both parts: the log-jump requeue and the seal guard"""
    _reopen_during_a_verify("reopen_verify")


def test_reopen_during_a_verify_seal_guard_alone():
    """the requeue off (GXREPL_TEST_NO_LOGJUMP_REQUEUE): the seal guard alone never seals stale data, and the copy is
    sealed in time (the stream stays on the list: its next verify)"""
    _reopen_during_a_verify("reopen_guard", {"GXREPL_TEST_NO_LOGJUMP_REQUEUE": "1"})


def test_reopen_during_a_verify_both_off_seals_stale_data():
    """both off: once the copy reaches the source's generation (waited for, no poll race), its data.gxd is stale
    (proves the test reaches the bug)"""
    env, ws, aws, d1, want = _reopen_setup("reopen_none", {"GXREPL_TEST_NO_LOGJUMP_REQUEUE": "1", "GXREPL_TEST_NO_SEAL_GUARD": "1"})
    try:
        end = time.time() + 30
        while time.time() < end and _gen_or(os.path.join(aws, "d1", "commit.bin")) != want:
            time.sleep(0.2)
        assert _gen_or(os.path.join(aws, "d1", "commit.bin")) == want, "the copy never reached the generation\n" + env.logs()
        assert not _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")), \
            "both parts off and the copy still matched: the test does not reach the bug"
    finally:
        env.stop()


def test_seal_guard_never_livelocks():
    """the seal guard waits only for the stream's verify: the copy is sealed at the source's generation within 15 s
    of the hold's release"""
    _reopen_during_a_verify("reopen_bounded", sealed_within=15)


def test_open_without_a_commit_still_seals():
    """11/15/2026 (review F1): the stream's log made again by a writer that opens and closes without a commit (a day
    reopened by the late pass and archived again with no late line: the log holds only an open record, no commit
    block): the verify takes the copy's own block (Diff's "gone" for a block the log does not carry) and the unit
    stays sealed: torn rc 0 within 20 s, no endless verify"""
    env = Env("open_no_commit", {"db": CB_OPTS})
    ws, aws = env.roots["db"]
    d1 = os.path.join(ws, "d1")
    os.makedirs(d1)
    try:
        assert writer(d1, "--commits", 6, "--commit-block", "--ship-root", ws).returncode == 0
        env.start_server()
        env.start_client()
        _wait_units(env, ws, aws, ["d1"])
        shutil.rmtree(os.path.join(ws, ".gxrepl", "log", "d1_writer.jnl"))
        r = writer(d1, "--commits", 0, "--commit-block", "--ship-root", ws, "--seed", 70)
        assert r.returncode == 0, r.stdout
        n = len(open(os.path.join(env.dir, "client.log")).read())
        end = time.time() + 20
        ok = False
        while time.time() < end:
            t = torn(env)
            if t.returncode == 0 and "verified (" in open(os.path.join(env.dir, "client.log")).read()[n:]:
                ok = True
                break
            time.sleep(0.5)
        assert ok, torn(env).stdout + env.logs()
        time.sleep(3)
        log = open(os.path.join(env.dir, "client.log")).read()[n:]
        assert log.count("verified (") <= 2, log            # no endless verify
        assert _same_file(os.path.join(d1, "data.gxd"), os.path.join(aws, "d1", "data.gxd")), env.logs()
    finally:
        env.stop()


def test_drain_verify_once_per_stream():
    """note_verify() is idempotent: one "needs a verify" line and one verify for the stream, whichever drains
    (the drain's own, then the caller's) saw the open records of two writers"""
    same, log = _drain_verify_round("dv_once")
    assert same, log[-3000:]
    tail = log[log.find("connected to", log.find("connected to") + 1):]   # after the reconnect
    assert tail.count("d1_writer.jnl needs a verify") == 1, tail
    assert tail.count("d1_writer.jnl verified (") == 1, tail


def test_drain_verify_clean_restart_adds_no_verify():
    """a writer that stops cleanly and a new one: nothing to recover; the new open record gets its one verify,
    no more, and the copy is the same"""
    same, log = _drain_verify_round("dv_clean", recovery=False)
    assert same, log[-3000:]
    tail = log[log.find("connected to", log.find("connected to") + 1):]
    assert tail.count("d1_writer.jnl needs a verify") <= 1 and "again after this one" not in tail, tail


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
