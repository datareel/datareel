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

## Left (the independent review of 19b261b: NO-GO for incremental until fixed)

1. Seal against stamp per folder: started on aws `gxrepl-seal-per-folder` (024654b, from 7.3.2-beta, WIP,
   no tests yet). The seal gains a 4th field: the last record that changed THIS folder.
   `derived_stamp_ok` and `{lsn}` use that field. Still to do: multi-folder tests in both modes (a
   testwriter that writes two folders through one journal: today plus late lines into a past day),
   OSA's `derived_problem` (the same compare), incremental_apply comparing against it, and then rebasing
   this branch on top of it.
2. Count each index for its own stamp, with a set option naming the index that is counted (OSA: by_time).
3. gxdstage: the Rollback between indexes sets done=1, so an index closed with changes is no longer caught.
4. Breaker: count crashes per set over a time window (3 in 1 h), with an ALERT.
5. A watchdog on the extractor step; the max_mb cap applies per batch.
6. ApplyPages order: data files, then indexes, then stamps. Batches over 1000 files must split at commit
   boundaries, or the limit is documented and tested.
7. Staged writes are allowed only to this folder's index and stamp paths; anything else is rebuild needed
   plus an ALERT.
8. A fill by files, in both modes, removes every `<index>.stamp`, derived.stamp and the derived state.
9. Remove the committed `utils/gxrepl/__pycache__` and add it to .gitignore (this commit removes them).
10. status.json shows the derived state per set, and a WARN when incremental is on with notes off.

Then:
- The OSA realtime extractor, on open_stock branch `osa-realtime-extractor`. It is built into
  cpp/gxrepl_build and registered as "osa_realtime", covers the 5 by_*.btx indexes with by_time counted,
  and is checked against derived_rebuild on read-only copies of real days.
- The full stress profiles, with multi-folder streams and the replayed real day.

Known costs, accepted: each writer restart (an open record leading to a verify fill) and each server death in
the middle of an apply cost one rebuild of the folder. Count both in the stress runs.
