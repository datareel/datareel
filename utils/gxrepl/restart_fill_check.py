#!/usr/bin/env python3
# 40 writer restarts on an incremental set (stamp-keeping fill check): each restart is a new writer process (an open
# record, so a verify) that commits right away and keeps committing, so the verify's fill races the next commits.
# Counts "copied by files" stamp drops and rebuilds per folder; checks every folder's index against a full rebuild.
#   python3 restart_fill_check.py utils/gxrepl BUILD/utils [RESTARTS]   (11/15/2026: base 3 rebuilds in 40, fixed 0)
import os, sys, tempfile, time, json
sys.path.insert(0, os.path.abspath(sys.argv[1]))
import stress_derived as S
import test_gxrepl as T
T.BIN = os.path.abspath(sys.argv[2])
N = int(sys.argv[3]) if len(sys.argv) > 3 else 40
T.TMP = tempfile.mkdtemp(prefix="gxrepl_restart40_")
T.make_pki(os.path.join(T.TMP, "pki"))
run = S.Run("restart40", "incremental")
run.start(("--commits", 20, "--per", 20, "--seed", 7))
log = os.path.join(run.env.dir, "server.log")
base_reb = run.rebuilds_of("d1")
base_cbf = open(log).read().count("copied by files")
t0 = time.time()
try:
    for k in range(N):
        w = S.writer_bg(os.path.join(run.ws, run.unit), run.ws, run.answers, "--commits", 25, "--per", 20,
                        "--sleep", 60, "--seed", 1000 + k)
        w.wait()
        assert w.returncode == 0, w.stdout.read()
    run.caught_up(900)
    time.sleep(5)
    run.wait_index()
    txt = open(log).read()
    res = {"restarts": N, "secs": round(time.time() - t0, 1),
           "copied_by_files": txt.count("copied by files") - base_cbf,
           "rebuilds_d1": run.rebuilds_of("d1") - base_reb,
           "rebuild_needed_lines": [l[20:160] for l in txt.split("\n") if "rebuild needed" in l][-5:],
           "wrong_lookups": run.sampler.wrong, "equal": run.sampler.equal, "stale": run.sampler.stale}
    ok, why = run.check()
    res["check"] = ok
    res["why"] = why
    res["index_matches_rebuild"] = run.index_matches_rebuild()
    print(json.dumps(res, indent=1))
finally:
    run.finish()
    print("kept", T.TMP)
