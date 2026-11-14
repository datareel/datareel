# Per-object derived apply: status and handoff (11/14/2026)

Branch `gxrepl-derived-apply` on aws (based on rel-7.3.1-repl-f4 cb46f5d, the dead-folder fix). It has not
been rebased onto 7.3.2-beta yet: the plan is to rebase it onto `gxrepl-seal-per-folder` (see "Left", 1).
Design study: https://claude.ai/artifact/HbMq5JHi5JPsG4XrAuEMAZ.

## Done

Part 1 (reviewed: GO for vendoring with notes off):
- Object notes (`gxdonote.h`). gxDatabase records each add, update and remove. The notes are off by default
  (`gxdJournal::EnableObjectNotes()` or `GXDSHIP_OBJECTS=1`).
- Type 5 "objects" records. Each one is written with its commit in one write, so a segment change, purge or
  copy position never falls between them, and `Read()` never ends a batch on one. A trailing type 5 is held
  back for 10 s at most (the stall fix), and GetInfo agrees with Read. Older copies skip type 5.
- `<index>.stamp` sidecars (`gxdstamp.h`). Stamps count as derived files (`is_derived`), and `is_db_file` is
  false for them and for derived.stamp. A missing stamp is a warning only.
- Extractor ABI (`gxrextract.h`): `gxr_extractor()`, Count(), `gxrReadAfterBatch()`, and a built-in
  registry (`gxrRegisterExtractor`). A `.so` path is loaded with dlopen; gxrepl is built with
  ENABLE_EXPORTS.

Part 2, the apply path behind `derived_apply = incremental`:
- Staged scope (`gxdstage.h`): staged page caches never write to their files. The changed index and stamp
  pages, derived.stamp, the derived state, the data pages, the seal and the position all go into one
  ApplyPages batch.
- Changes are merged across the commits of a batch. Any gap means "rebuild needed": the stamps are dropped,
  and the derived pass rebuilds and writes fresh stamps.
- A fill by files (ApplyFiles) drops the stamps.
- Breaker: `incremental.try` and `incremental.breaker`, with an ALERT.
- `derived_apply_max_mb` (per index for now; see review item 5).
- If an extractor writes an index directly, gxrepl raises an ALERT and goes to rebuild needed.
- Tools: `gxrepl digest`, `gxrepl derived status`, `gxrepl derived rebuild`.
- Test extractor (`gxrepl_testextractor.cpp`), built in as "gxrepl_test" and as `gxrepl_testextractor.so`.

Tests:
- tests/derived_apply_test: 1,408 checks, 0 failures.
- run_tests: 50/50.
- gxrepl suite in both modes: 104 pass in each. The default run's one failure, the stamp test, is fixed
  and passes alone. In GXDSHIP_OBJECTS=1 mode, test_derived_index_rebuilt_on_copy failed once under load
  and passes alone.
- New suite tests: golden notes-off test, mixed versions against 44e2936 both ways, the stall, and 8
  incremental tests (builtin, .so, extractor error, direct write, kill -9 in the scratch phase, breaker,
  staged cap, orphan).
- Stress harness: `utils/gxrepl/stress_derived.py`, with `GXREPL_STATS_FILE` measurements, `testwriter
  lookup`, `--answers-log`, `--size-min/--size-max`, `--burst-*`, `--wait-at/--wait-file`.

## Quick stress results so far (burst profile, objects 1.5-3.5 KB, 4 MB max_message)

| load | method | apply lock (ms) | rebuild lock (ms) | rebuilds | caught up (s) | lookups wrong |
|---|---|---|---|---|---|---|
| 968 objects, 1 commit | rebuild | 24 | 17 | 1 | 3.7 | 0 |
| 9,680 objects, 7 commits (split) | rebuild | 144 (max 37) | 93 | 1 | 2.7 | 0 |
| 2,013 objects, 2 commits (split) | rebuild | 26 | 117 | 1 | 3.8 | 0 |
| 968 objects, 1 commit | incremental | 167 | 23 | 1 | 3.2 | 0 |

The incremental 968 row still includes one rebuild, which comes from the gated writer's restart. The next
incremental row (9,680) then hung: after a verify fill, `drop_stamps` dropped the stamps but left
derived.stamp and the derived state "ok". The derived pass therefore never rebuilt, and no stamps came
back for 10 minutes. This is review item 8, and it is needed for correctness, not only for promote-check.
The harness's gated writer should also wait until the rebuild count has been stable for a few seconds
before releasing the load.

## Review fixes (11/14/2026, branch gxrepl-derived-apply-732, on gxrepl-seal-per-folder)

The independent review of 19b261b found 10 fixes; its re-review of ff0ad0c found all of 2-8 and 10 FIXED
(GO to merge with derived_apply = rebuild the default) and asked for A-G, done here:

1. Seal per folder: its own branch `gxrepl-seal-per-folder` (from 7.3.2-beta, for rel-7.3.2 alone). The seal
   is "log G L S C", C the last record that changed the folder; stamps, `{lsn}`, incremental_apply, `gxrepl
   derived status` and OSA's derived_problem use C. A verify that re-sends a folder as sealed keeps C (a writer
   restart costs no rebuild), one that repairs its data moves C. In OSA each day is its own stream, so the
   restarts' verifies were what rebuilt idle days.
2. Each `<index>.stamp` holds its own count; `derived_count` names the index whose count is derived.stamp's
   keys. Re-review B: incremental refuses a set without derived_count (stays on rebuild, says why).
3. gxdStagedScope: Rollback() no longer ends the scope; a closed file's changes are captured; a file opened
   again after closing with changes fails the capture; the extractor is closed before the capture.
4. Breaker: also 3 index step crashes in an hour, any batches (`incremental.crashes`).
5. The MB cap is per batch; `derived_apply_max_s` (checked between calls, after Flush and between indexes,
   re-review E) and a hard watchdog (exit 8: it stops the whole server, every set; the supervisor restarts it).
6. ApplyPages order data, indexes, stamps, commit blocks, seals; a batch over 1000 files carries no index pages.
7. Staged writes only to the current index and its stamp; anything else: rebuild needed + ALERT.
8. A fill by files that wrote derived_source drops every stamp, derived.stamp and the derived state.
   Re-review A: only derived_source (a journal piece or the re-sent commit block drops nothing; a rebuild set
   without derived_source drops nothing).
9. `__pycache__` removed and ignored (2fb698c).
10. status.json "derived" per incremental set (cached 30 s, re-review C), WARN when object notes look off.
    Nothing reads the server's status.json exactly (gxrepl_stall_check.py reads the client's, by key).
G. A failed extractor Open while stamping after a rebuild is logged (records 0).

Then: the OSA realtime extractor (open_stock `osa-realtime-extractor`): key for key equal to
`osa_ingest --rebuild-index` on copies of 10-30, 11-01, 11-06, 11-08, 11-09; the replayed 11-08 day
(osa_ingest --once per 100 lines, 92 writer restarts): the copy equal to the primary and a rebuild at
every check, 1 rebuild (per-object) against 5 (rebuild mode). The stress results: below.

Known costs, accepted: each writer restart (an open record leading to a verify fill) and each server death in
the middle of an apply cost one rebuild of the folder. Count both in the stress runs.

## Stress results (11/14/2026, workstation, nice 19 / ionice idle, shared with other sessions' builds and suites)

Harness: stress_derived.py, multi-folder (today's d1 takes the load, past days p1 and p2 take late lines on their own
streams), the gated writer waits for a stable rebuild count, every folder checked against a full rebuild at the end.
Full profiles (--speed 10). "Own" = the time the method itself holds the apply lock: the rebuild lock (rebuild mode)
or the incremental step (per-object). Apply lock = everything under the lock, including the pages' fsync.

Every run, both methods: each folder's copy index equal to a full rebuild (digest), 0 wrong lookups, final check OK.

| profile | rebuild: own ms / rebuilds | per-object: own ms / rebuilds |
|---|---|---|
| steady mean | 17,003 / 121 | 392 / 0 |
| steady peak | 7,688 / 133 | 419 / 0 |
| steady 5x peak | 3,516 / 135 | 1,761 / 0 |
| steady 10x peak | 20,333 / 108 | 3,592 / 2 |
| burst 968 | 1,231 / 133 | 70 / 0 |
| burst 9,680 | 1,567 / 133 | 189 / 0 |
| late 2,013 | 1,578 / 133 | 77 / 0 |
| late 20,130 | 12,399 / 122 | 405 / 0 |
| day 1 GB | 7,838 / 125 | 286 / 0 |
| day 2 GB | 1,282 / 135 | 209 / 0 |
| day 4 GB | 2,066 / 87 | 271 / 0 |
| faults: writer restart | 1,158 / 134 | 138 / 1 |
| faults: kill -9 of the copy | 1,649 / 134 | 148 / 1 |
| faults: link cut | 1,621 / 134 | 168 / 1 |
| faults: ship log gap | 1,881 / 134 | 404 / 1 |
| faults: extractor crash (breaker) | - | 6,151 / 125 (the breaker puts the set on rebuild) |
| endurance (20 min at 10x peak) | 16,113 / 175 | 4,434 / 0 |

Per-object's own lock time is lower at every full-run level, and it rebuilds only after a fault (one folder). The
incremental step never took more than 45 ms. The quick pass (--quick, small folders) had one level where it was
higher (steady 10x peak: 948 ms against 722 ms).

Not settled: the whole apply lock (the pages' fsync, the same work in both methods) is dominated by the shared
machine's I/O under ionice idle: the day 2 GB per-object run had an apply lock p99 of 21.9 s (max 39 s) while the
step itself was 3.9 ms at most, and the ship log gap fault 889 ms against 45 ms. Those runs overlapped other jobs.
A rerun of steady + day, both methods, on a quiet machine (night) decides whether per-object also wins on the whole
apply lock. Until then it stays an option (derived_apply = rebuild is the default).
