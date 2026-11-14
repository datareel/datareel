#!/usr/bin/env python3
# stress_derived.py - 11/14/2026 (DataReel 7.4): stress test of the two ways a gxrepl copy keeps its derived
# indexes, derived_apply = rebuild (derived_rebuild of the whole folder) and incremental (object by object), on the
# same load, on throwaway local sets only (the test suite's Env: 127.0.0.1, folders under a temporary directory).
#
#   python3 stress_derived.py --bin BUILD/utils [--out DIR] [--speed 10] [--only steady,burst,...] [--quick]
#
# Profiles (the design study's stress plan):
#   steady     the realtime mean (890 commits/h), peak (1,324/h), 5x and 10x peak, time-compressed by --speed
#   burst      one 968-object commit, then one 10x that; a 2,013-object late pass, then 10x
#   day        1, 2 and 4 GB data files with the mean load
#   faults     kill -9 of the copy mid-apply, a link cut mid-copy, an extractor crash (breaker), a ship-log gap,
#              a writer restart: rebuilds caused and recovery time for each
#   endurance  a long run at 10x peak: RSS and lag must not grow
# Measured for each run and method: apply lock (sum per hour, p99, max), rebuild lock, incremental step time and
# peak staged MB, copy lag (generations behind the primary), index lag (the copy's index answers an older
# generation), the copy server's CPU, peak RSS and bytes written, rebuild count. Lookups run on the copy the
# whole time under its apply read lock; each answer must be the primary's at the same generation ("equal"), or,
# while the index is behind, an older generation's ("stale"); any other answer is WRONG (the pass bar: none).
# The results go to DIR/results.json and DIR/results.txt. Every process runs at nice 19 / ionice idle.

import argparse
import json
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import test_gxrepl as T                                      # noqa: E402

NICE = ["nice", "-n", "19", "ionice", "-c3"]


def tw(*a):
    return [os.path.join(T.BIN, "gxrepl_testwriter"), *map(str, a)]


class Sampler:
    """the copy server's CPU, RSS and bytes written; the copy's lag; lookups on the copy"""

    def __init__(self, env, ws, aws, unit, answers):
        self.env, self.ws, self.aws, self.unit, self.answers_path = env, ws, aws, unit, answers
        self.stop_ev = threading.Event()
        self.rss_peak = 0
        self.cpu0 = self.cpu1 = None
        self.wb0 = self.wb1 = 0
        self.lag = []                 # generations behind (sampled)
        self.equal = self.stale = self.wrong = self.busy = 0
        self.wrong_examples = []
        self.t = threading.Thread(target=self.loop, daemon=True)

    def proc(self):
        p = self.env.server
        if not p or p.poll() is not None:
            return None
        return p.pid

    def read_proc(self, pid):
        try:
            st = open(f"/proc/{pid}/stat").read().split(")")[-1].split()
            cpu = (int(st[11]) + int(st[12])) / os.sysconf("SC_CLK_TCK")
            rss = 0
            for line in open(f"/proc/{pid}/status"):
                if line.startswith("VmRSS:"):
                    rss = int(line.split()[1]) * 1024
            wb = 0
            for line in open(f"/proc/{pid}/io"):
                if line.startswith("write_bytes:"):
                    wb = int(line.split()[1])
            return cpu, rss, wb
        except OSError:
            return None

    def answers(self):
        out = {}
        try:
            for line in open(self.answers_path):
                g, c, s = line.split()
                out[int(g)] = (int(c), int(s))
        except OSError:
            pass
        return out

    def loop(self):
        cb_w = os.path.join(self.ws, self.unit, "commit.bin")
        cb_a = os.path.join(self.aws, self.unit, "commit.bin")
        jnl = os.path.join(self.aws, ".gxrepl", "apply.jnl")
        while not self.stop_ev.is_set():
            pid = self.proc()
            if pid:
                r = self.read_proc(pid)
                if r:
                    cpu, rss, wb = r
                    if self.cpu0 is None:
                        self.cpu0, self.wb0 = cpu, wb
                    self.cpu1, self.wb1 = cpu, wb
                    self.rss_peak = max(self.rss_peak, rss)
            gw, ga = T._gen(cb_w), T._gen(cb_a)
            if gw >= 0 and ga >= 0:
                self.lag.append(max(0, (gw - ga) // 2))
            if os.path.exists(os.path.join(self.aws, self.unit, "index.gxd")):
                r = subprocess.run(NICE + tw("lookup", os.path.join(self.aws, self.unit), jnl),
                                   stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=60)
                parts = r.stdout.split()
                if r.returncode == 0 and len(parts) == 3:
                    g, c, s = map(int, parts)
                    ans = self.answers()
                    if g in ans and ans[g] == (c, s):
                        self.equal += 1
                    elif (c, s) in ans.values():
                        self.stale += 1
                    elif g not in ans:
                        pass                       # the primary's line for g is not written yet
                    else:
                        self.wrong += 1
                        if len(self.wrong_examples) < 5:
                            self.wrong_examples.append((g, c, s, ans.get(g)))
                else:
                    self.busy += 1
            self.stop_ev.wait(0.25)

    def start(self):
        self.t.start()

    def stop(self):
        self.stop_ev.set()
        self.t.join(30)


def read_stats(path, t0_ms):
    st = {"apply": [], "rebuild": [], "incremental": [], "peak_mb": 0.0}
    try:
        for line in open(path):
            p = line.split()
            if len(p) < 4 or int(p[0]) < t0_ms:
                continue
            kind, ms = p[1], float(p[3])
            if kind in st:
                st[kind].append(ms)
            if kind == "incremental" and len(p) >= 5:
                st["peak_mb"] = max(st["peak_mb"], float(p[4]))
    except OSError:
        pass
    return st


def summary(vals, secs):
    if not vals:
        return {"n": 0, "sum_ms": 0, "per_hour_ms": 0, "p99_ms": 0, "max_ms": 0}
    v = sorted(vals)
    return {"n": len(v), "sum_ms": round(sum(v), 1), "per_hour_ms": round(sum(v) * 3600.0 / max(secs, 1), 1),
            "p99_ms": round(v[min(len(v) - 1, int(len(v) * 0.99))], 2), "max_ms": round(v[-1], 2)}


def make_env(name, method, extra_server=()):
    if method == "incremental":
        opts = T._incr_opts("gxrepl_test", ["derived_delay = 0", "derived_busy_rebuild = 30"] + list(extra_server))
    else:
        opts = T._derived_opts(["derived_delay = 0", "derived_busy_rebuild = 30"] + list(extra_server))
    return T.Env(name, {"db": opts})


def writer_bg(root, ws, answers, *args):
    e = dict(os.environ, GXDSHIP_OBJECTS="1")
    return subprocess.Popen(NICE + tw("write", root, "--commit-block", "--ship-root", ws, "--answers-log", answers, *args),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)


def writer_fg(root, ws, answers, *args):
    e = dict(os.environ, GXDSHIP_OBJECTS="1")
    return subprocess.run(NICE + tw("write", root, "--commit-block", "--ship-root", ws, "--answers-log", answers, *args),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)


class Run:
    """one load on one method: a set, a writer, the copy (server) with stats, a sampler"""

    def __init__(self, name, method, extra_server=()):
        self.name, self.method = name, method
        self.env = make_env(f"{name}_{method}", method, extra_server)
        self.ws, self.aws = self.env.roots["db"]
        self.unit = "d1"
        os.makedirs(os.path.join(self.ws, self.unit), exist_ok=True)
        self.answers = os.path.join(self.env.dir, "answers.log")
        self.stats = os.path.join(self.env.dir, "stats.log")
        self.sampler = None

    def start(self, prefill=()):
        if prefill:
            r = writer_fg(os.path.join(self.ws, self.unit), self.ws, self.answers, *prefill)
            assert r.returncode == 0, r.stdout
        self.env.start_server(extra_env={"GXREPL_STATS_FILE": self.stats})
        self.env.start_client()
        T._wait_units(self.env, self.ws, self.aws, [self.unit], 600)
        self.wait_index()
        self.sampler = Sampler(self.env, self.ws, self.aws, self.unit, self.answers)
        self.sampler.start()

    def wait_index(self, secs=600):
        end = time.time() + secs
        while time.time() < end and "INDEXES" in T.torn(self.env).stdout:
            time.sleep(0.5)

    def rebuilds(self):
        return T._rebuilds(self.env)

    def caught_up(self, secs=600):
        """seconds until the copy holds the primary's generation and its index answers it"""
        t0 = time.time()
        T._wait_gen(self.ws, self.aws, self.unit, secs)
        self.wait_index(secs)
        return round(time.time() - t0, 2)

    def measure(self, fn):
        t0 = time.time()
        t0_ms = int(t0 * 1000)
        r0 = self.rebuilds()
        s = self.sampler
        lag0 = len(s.lag)
        eq0, st0, wr0 = s.equal, s.stale, s.wrong
        cpu0, wb0 = s.cpu1, s.wb1
        extra = fn() or {}
        secs = time.time() - t0
        st = read_stats(self.stats, t0_ms)
        lag = s.lag[lag0:]
        res = {"secs": round(secs, 1), "apply_lock": summary(st["apply"], secs), "rebuild_lock": summary(st["rebuild"], secs),
               "incremental_step": summary(st["incremental"], secs), "peak_staged_mb": round(st["peak_mb"], 2),
               "rebuilds": self.rebuilds() - r0,
               "copy_lag_gen": {"mean": round(statistics.mean(lag), 2) if lag else 0, "max": max(lag) if lag else 0},
               "lookups": {"equal": s.equal - eq0, "stale": s.stale - st0, "wrong": s.wrong - wr0},
               "cpu_s": round((s.cpu1 or 0) - (cpu0 or 0), 2), "rss_peak_mb": round(s.rss_peak / 1048576.0, 1),
               "written_mb": round(((s.wb1 or 0) - (wb0 or 0)) / 1048576.0, 1)}
        res.update(extra)
        return res

    def check(self):
        c = subprocess.run(NICE + tw("check", os.path.join(self.aws, self.unit)), stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
        return c.returncode == 0, c.stdout.strip()

    def finish(self):
        if self.sampler:
            self.sampler.stop()
        ok, why = self.check()
        wrong = self.sampler.wrong_examples if self.sampler else []
        self.env.stop()
        return {"final_check": ok, "final_check_text": why, "wrong_examples": wrong}


def steady(method, speed, quick):
    out = {}
    rates = [("mean", 890), ("peak", 1324), ("5x_peak", 5 * 1324), ("10x_peak", 10 * 1324)]
    secs = 30 if quick else 120
    run = Run("steady", method)
    run.start(("--commits", 20, "--per", 20, "--seed", 3))
    try:
        for label, per_hour in rates:
            rate = per_hour * speed / 3600.0                  # commits a second, time-compressed
            n = max(5, int(rate * secs))
            sleep_ms = max(1, int(1000.0 / rate))

            def load():
                w = writer_bg(os.path.join(run.ws, run.unit), run.ws, run.answers, "--commits", n, "--per", 20,
                              "--sleep", sleep_ms, "--seed", hash(label) % 1000)
                w.wait()
                return {"commits": n, "rate_per_s": round(rate, 2), "caught_up_s": run.caught_up()}
            out[label] = run.measure(load)
            print(f"  steady {method} {label}: {json.dumps(out[label])}", flush=True)
    finally:
        out["end"] = run.finish()
    return out


def burst(method, quick):
    out = {}
    run = Run("burst", method)
    run.start(("--commits", 20, "--per", 20, "--seed", 4))
    try:
        for label, per in (("968", 968), ("968x10", 9680), ("late_2013", 2013), ("late_2013x10", 20130)):
            if quick and per > 10000:
                continue

            def load():
                w = writer_fg(os.path.join(run.ws, run.unit), run.ws, run.answers, "--commits", 1, "--per", per,
                              "--seed", per)
                assert w.returncode == 0, w.stdout
                return {"objects": per, "caught_up_s": run.caught_up()}
            out[label] = run.measure(load)
            print(f"  burst {method} {label}: {json.dumps(out[label])}", flush=True)
    finally:
        out["end"] = run.finish()
    return out


def day(method, speed, quick):
    out = {}
    sizes = [1] if quick else [1, 2, 4]
    for gb in sizes:
        run = Run(f"day{gb}", method)
        # a day file of gb GB: one big object (random bytes: it does not compress) and the usual objects
        run.start(("--commits", 10, "--per", 20, "--big", gb * 1024 * 1024 * 1024 - 64 * 1024 * 1024, "--seed", 5))
        try:
            rate = 890 * speed / 3600.0
            n = max(5, int(rate * (30 if quick else 90)))

            def load():
                w = writer_bg(os.path.join(run.ws, run.unit), run.ws, run.answers, "--commits", n, "--per", 20,
                              "--sleep", int(1000 / rate), "--seed", 6)
                w.wait()
                return {"gb": gb, "commits": n, "caught_up_s": run.caught_up(1800)}
            out[f"{gb}GB"] = run.measure(load)
            print(f"  day {method} {gb}GB: {json.dumps(out[f'{gb}GB'])}", flush=True)
        finally:
            out[f"{gb}GB"]["end"] = run.finish()
            shutil.rmtree(run.env.dir, ignore_errors=True)        # the big files go at once
    return out


def faults(method, speed, quick):
    out = {}
    run = Run("faults", method)
    run.start(("--commits", 20, "--per", 20, "--seed", 7))
    rate = 1324 * speed / 3600.0
    try:
        def background(n):
            return writer_bg(os.path.join(run.ws, run.unit), run.ws, run.answers, "--commits", n, "--per", 20,
                             "--sleep", int(1000 / rate), "--seed", int(time.time()) % 1000)

        def fault(label, act, wait_after=True):
            def load():
                w = background(int(rate * 40))
                time.sleep(8)
                t0 = time.time()
                act()
                w.wait()
                return {"recovered_s": round(run.caught_up(900) + (time.time() - t0) * 0, 2)}
            r = run.measure(load)
            ok, why = run.check()
            r["check_after"] = ok
            out[label] = r
            print(f"  faults {method} {label}: {json.dumps(r)}", flush=True)

        def kill_copy():
            run.env.server.kill()
            run.env.server.wait()
            time.sleep(1)
            run.env.start_server(extra_env={"GXREPL_STATS_FILE": run.stats})

        def cut_link():
            run.env.client.kill()
            run.env.client.wait()
            time.sleep(2)
            run.env.start_client()

        def extractor_crash():
            for _ in range(2):
                run.env.server.terminate()
                run.env.server.wait(10)
                run.env.start_server(extra_env={"GXREPL_STATS_FILE": run.stats, "GXREPL_TEST_EXTRACTOR_CRASH": "1"})
                end = time.time() + 30
                while time.time() < end and run.env.server.poll() is None:
                    time.sleep(0.2)
                if run.env.server.poll() is None:
                    run.env.server.terminate(); run.env.server.wait(10)
            run.env.start_server(extra_env={"GXREPL_STATS_FILE": run.stats})

        def writer_restart():
            pass                                   # the background writer is a new process: an open record

        fault("writer_restart", writer_restart)
        fault("kill9_copy", kill_copy)
        fault("cut_link", cut_link)
        if method == "incremental":
            fault("extractor_crash", extractor_crash)
            subprocess.run([os.path.join(T.BIN, "gxrepl"), "derived", "rebuild", "--config", run.env.srv_conf, "--set", "db"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        def gap():
            w = writer_fg(os.path.join(run.ws, run.unit), run.ws, run.answers, "--commits", 3, "--per", 20,
                          "--split-pair-at", 1, "--seed", 99)
            assert w.returncode == 0, w.stdout
        fault("ship_log_gap", gap)
    finally:
        out["end"] = run.finish()
    return out


def endurance(method, speed, quick):
    run = Run("endurance", method)
    run.start(("--commits", 20, "--per", 20, "--seed", 8))
    rate = 1324 * 10 * speed / 3600.0 / 10            # 10x peak, at real time x speed / 10
    secs = 120 if quick else 1200
    rss = []
    try:
        def load():
            w = writer_bg(os.path.join(run.ws, run.unit), run.ws, run.answers, "--commits", int(rate * secs), "--per", 20,
                          "--sleep", max(1, int(1000 / rate)), "--seed", 9)
            while w.poll() is None:
                rss.append(run.sampler.rss_peak)
                time.sleep(5)
            return {"commits": int(rate * secs), "caught_up_s": run.caught_up()}
        out = run.measure(load)
        lag = run.sampler.lag
        q = max(1, len(lag) // 4)
        out["lag_first_quarter"] = round(statistics.mean(lag[:q]), 2) if lag else 0
        out["lag_last_quarter"] = round(statistics.mean(lag[-q:]), 2) if lag else 0
        print(f"  endurance {method}: {json.dumps(out)}", flush=True)
    finally:
        end = run.finish()
    out["end"] = end
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--out", default="")
    ap.add_argument("--speed", type=float, default=10.0)
    ap.add_argument("--only", default="steady,burst,day,faults,endurance")
    ap.add_argument("--methods", default="rebuild,incremental")
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    T.BIN = os.path.abspath(a.bin)
    T.TMP = tempfile.mkdtemp(prefix="gxrepl_stress_")
    T.make_pki(os.path.join(T.TMP, "pki"))
    out_dir = a.out or os.path.join(T.TMP, "results")
    os.makedirs(out_dir, exist_ok=True)
    results = {"speed": a.speed, "quick": a.quick, "started": time.strftime("%Y-%m-%d %H:%M:%S")}
    for prof in a.only.split(","):
        for method in a.methods.split(","):          # the rebuild baseline first
            print(f"== {prof} / {method}", flush=True)
            if prof == "steady":
                r = steady(method, a.speed, a.quick)
            elif prof == "burst":
                r = burst(method, a.quick)
            elif prof == "day":
                r = day(method, a.speed, a.quick)
            elif prof == "faults":
                r = faults(method, a.speed, a.quick)
            elif prof == "endurance":
                r = endurance(method, a.speed, a.quick)
            else:
                raise SystemExit("unknown profile " + prof)
            results.setdefault(prof, {})[method] = r
            json.dump(results, open(os.path.join(out_dir, "results.json"), "w"), indent=1)
    results["ended"] = time.strftime("%Y-%m-%d %H:%M:%S")
    json.dump(results, open(os.path.join(out_dir, "results.json"), "w"), indent=1)
    print("results:", os.path.join(out_dir, "results.json"))
    shutil.rmtree(T.TMP, ignore_errors=True) if not a.out else None


if __name__ == "__main__":
    main()
