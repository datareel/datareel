# DataReel Replication Administrator's Guide (gxrepl)

This guide is for the person who runs database replication on two nodes: setting it up, watching it, taking
over when the primary fails, and getting back. It covers gxrepl as shipped in DataReel 7.4 beta 2. The design
and every config key are in `utils/gxrepl/README.md`, `include/gxrepl.h` and `RELEASE_NOTES_7.4.txt`.

Every example uses throwaway folders under `/tmp/repl_demo`, two node names (`primary` and `copy`) and the
loopback address. Never try the commands here for the first time on production nodes: run them on a local
throwaway setup first. **Test status:** each command block is marked **(tested)** or **(test pending)**. A
pending block has not yet been run on a throwaway setup for this guide. Treat it as a description until it is.

## 1. What gxrepl replicates

- **Committed database pages.** A program writing through a `gxdJournal` with the ship log on
  (`EnableShipLog(set_root)`) appends each commit's pages to `<root>/.gxrepl/log/<stream>/`. gxrepl sends those
  records to the copy over a mutual-TLS gxStream connection (TCP 2073), and the copy applies them through its own
  journal (`<root>/.gxrepl/apply.jnl`). The copy is always one whole commit, even after a crash in the middle of
  an apply.
- **Database files without a ship log** (published databases, writers without the log) go by a scan: changed
  files are compared in 64 KB pieces and only the pieces that differ are sent. Live writers' files are read under
  `gxdJournalReadLock`.
- **Database files only.** `.gxd`, `.btx` and the files a set names with `include` (a commit block, a manifest).
  Flat files (logs, CSV, reports) are not gxrepl's job; copy them with your usual file sync.
- **Derived indexes are never sent.** An index the copy can make from the data (`derived = by_*.btx, vectors.vix`)
  is left out of the ship log and the file sync, and the copy keeps it itself. Each set chooses how:
  - `derived_apply = rebuild` (the default): the copy runs `derived_rebuild` for a folder once its data is quiet
    (`derived_delay`), or after `derived_busy_rebuild` seconds on a folder that never goes quiet.
  - `derived_apply = incremental` (7.4): the copy updates each index object by object, in the same atomic batch
    as the data, from the "objects" records the writer adds to the ship log (object notes, below). A gap, an
    error or a crash it cannot be sure of puts that folder back on a full rebuild ("rebuild needed").
- **Commit blocks.** A folder whose writer keeps a commit block (`commit_block = commit.bin@8`, a generation that
  is odd while a batch writes) is copied as one unit and sealed at a whole commit. `gxrepl torn` (promote-check)
  says whether every copy is whole.

## 2. Setting up a primary and a copy

### 2.1 Certificates

Both nodes authenticate with certificates from a private CA. The server lets in only the client names in
`allow_cn` (or fingerprints in `allow_fingerprint`), and the client checks that the server's certificate names
`server_name`.

(test pending)

```
mkdir -p /tmp/repl_demo/pki && cd /tmp/repl_demo/pki
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes \
    -keyout ca.key -out ca.pem -days 30 -subj /CN=demo-ca
# the server: the copy, which listens
openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout copy.key -out copy.csr -subj /CN=copy
printf 'subjectAltName=DNS:copy,IP:127.0.0.1\nextendedKeyUsage=serverAuth\n' > copy.ext
openssl x509 -req -in copy.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out copy.pem -days 30 -extfile copy.ext
# the client: the primary, which dials
openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout primary.key -out primary.csr -subj /CN=primary
printf 'extendedKeyUsage=clientAuth\n' > primary.ext
openssl x509 -req -in primary.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out primary.pem -days 30 -extfile primary.ext
chmod 600 *.key
gxrepl fingerprint primary.pem          # for allow_fingerprint, if you pin it
```

Keep the CA key off both nodes. On a real node, give gxrepl its key on a descriptor (`key = fd:3` or
`--key-fd 3`) or from the environment (`env:VAR`) instead of a file. gxrepl then keeps it only in an anonymous
memory file.

### 2.2 Configs

Usually the node that cannot take inbound connections is the client (`gxrepl run`) and the other is the server
(`gxrepl serve`). Direction is per set: `push` copies the client's folder to the server, `pull` the reverse.
In this demo the primary pushes to the copy.

`/tmp/repl_demo/primary.conf` (the client):

```
node = primary
role = client
connect = 127.0.0.1:12073
server_name = copy
cert = /tmp/repl_demo/pki/primary.pem
key = /tmp/repl_demo/pki/primary.key
ca = /tmp/repl_demo/pki/ca.pem
status_file = /tmp/repl_demo/primary.status.json

[set db]
root = /tmp/repl_demo/primary/db
direction = push
commit_block = commit.bin@8
include = commit.bin
derived = by_*.btx
deletes = yes
```

`/tmp/repl_demo/copy.conf` (the server):

```
node = copy
role = server
listen = 127.0.0.1:12073
allow_cn = primary
cert = /tmp/repl_demo/pki/copy.pem
key = /tmp/repl_demo/pki/copy.key
ca = /tmp/repl_demo/pki/ca.pem
status_file = /tmp/repl_demo/copy.status.json

[set db]
root = /tmp/repl_demo/copy/db
accept = target
commit_block = commit.bin@8
include = commit.bin
derived = by_*.btx
derived_rebuild = /path/to/your_rebuild_tool {dir} {lsn}
derived_delay = 30
```

The patterns:

- `include`: extra files carried with the databases (only database files go by default).
- `exclude`: files and folders never sent. `/x/` matches only x under the root; `name/` matches a folder of that
  name anywhere.
- `derived`: the indexes the copy rebuilds. Name the same patterns on both nodes. `<index>.stamp` files belong to
  their index and are never sent.
- `writer_lock`: lock files a node's own writer holds while it is the active node. While one is held, gxrepl
  refuses to change that node's copy (the split-brain guard).

`derived_rebuild` runs on the copy under the apply lock, with `{dir}` (the folder) and `{lsn}` (the ship log
record the folder was last changed at). It must rebuild the folder's derived indexes and write
`<folder>/derived.stamp` (`gen G lsn L records N keys K`). For vector indexes made from record members,
`utils/vix_rebuild` is such a tool:

```
derived = vectors.vix
derived_rebuild = vix_rebuild --data {dir}/data.gxd --index {dir}/vectors.vix --member infohog:1 \
    --dim 96 --spec news/v1 --stamp-dir {dir} --lsn {lsn}
```

### 2.3 Check and start

(test pending)

```
gxrepl check --config /tmp/repl_demo/copy.conf          # certificates, sets, folders
gxrepl check --config /tmp/repl_demo/primary.conf
gxrepl serve --config /tmp/repl_demo/copy.conf &        # the copy listens
gxrepl once  --config /tmp/repl_demo/primary.conf       # one pass with a full verify; 0 = every set the same
gxrepl run   --config /tmp/repl_demo/primary.conf &     # replicate until stopped
```

`once` exits 0 when every set is the same, 1 when a set is not in step, 2 on an error. `TERM` stops either side
after the current message. `HUP` on the client forces a full verify at its next pass.

### 2.4 Per-object derived apply (incremental)

Turn it on per set, on the copy, only when both nodes run DataReel 7.4 beta 2 or later (section 6):

```
derived_apply = incremental
derived_extractor = <a built-in name, or /path/to/extractor.so>
derived_extractor_config = <the extractor's settings>
derived_source = data.gxd          # the folder's data file the indexes come from
derived_count = by_time.btx        # the index whose entry count is derived.stamp's "keys"
derived_apply_max_mb = 256         # most staged index pages a batch may hold
derived_apply_max_s = 30           # longest index step of a batch
```

and on the writer, turn the object notes on (`gxdJournal::EnableObjectNotes(1)`, or `GXDSHIP_OBJECTS=1` in its
environment before it calls `EnableShipLog`). Without the notes, an incremental set stays on rebuild.

An extractor turns one record into its index entries (`include/gxrextract.h`). The built-in `gxvmember` keeps a
vector index made from the records' vector members. An application builds its own into its gxrepl and registers
it with `gxrRegisterExtractor()`. `examples/replication/per_object` shows one end to end.

Rules worth knowing:

- The first incremental batch of a folder needs a stamped index, so the folder is rebuilt once first.
- A writer restart (its open record makes the copy verify), a copy by files, a ship log gap, an extractor error,
  or a staged batch over the cap each cost one rebuild of the folders it touched.
- Two crashes of the index step on one batch, or three in an hour on one set, trip the breaker: the set goes
  back to rebuild with an ALERT in the log. `gxrepl derived rebuild --config FILE --set NAME` clears it.
- Rolling back is one line: `derived_apply = rebuild`, then restart the server.

## 3. Watching it

(test pending)

```
gxrepl status --config /tmp/repl_demo/primary.conf      # the status JSON; exit 1 if older than 30 s
gxrepl derived status --config /tmp/repl_demo/copy.conf # derived mode, stamps, breaker, per folder
gxrepl torn --config /tmp/repl_demo/copy.conf           # promote-check: is every copy whole?
gxrepl shiplog /tmp/repl_demo/primary/db/.gxrepl/log/<stream>   # the records of one stream
gxrepl digest /tmp/repl_demo/copy/db/by_time.btx --kind btree   # compare an index with the other node
vix_rebuild --digest /tmp/repl_demo/copy/db/vectors.vix         # a vector index's logical digest
```

What to look at:

- **status.json** (`status_file`, rewritten every few seconds): per set `ok`, `last_ok`, `last_error`, `warn`,
  bytes and records sent, and per stream `source`, `copy` and `lag` (records the copy is behind). With derived
  indexes there is a `derived` block: the mode, folders whose stamps are behind, an index step in progress, the
  breaker and the last ALERT.
- **The stall check.** `lag` should come back to 0 within seconds of a quiet writer. A lag that stays above 0
  while the writer is quiet, or a `last_ok` that stops moving while `ok` is false, means the set is stuck: read
  `last_error` and the server log. `gxrepl status` itself exits 1 when the status file is older than 30 seconds
  (gxrepl is not running).
- **Digests.** `gxrepl digest` compares what an index holds, not its bytes: `btree` hashes the keys in order,
  the same for any node layout; `objects` hashes a data file's live objects; `bytes` hashes the file.
  `vix_rebuild --digest` gives a vector index's logical digest (live nodes in id order). A primary's index and a
  copy's index made by incremental apply or by a rebuild agree on the logical digests. Two rebuilds of the same
  data also agree on the bytes.
- **Log lines.** `derived indexes rebuilt`, `rebuild needed: <why>` and `ALERT` are the ones to alert on.

## 4. Failover and failback

### 4.1 Before a node takes over

(test pending)

```
gxrepl promote --config /tmp/repl_demo/copy.conf            # dry run: what promote would do
gxrepl promote --config /tmp/repl_demo/copy.conf --apply    # finish stages, recover, then the torn check
```

`promote --apply` recovers the apply journal and finishes or discards staged unit copies. It then runs the same
check as `gxrepl torn`. Exit codes: 0 every copy is whole (go); 1 a copy is torn (do not take over); 3 promote
could not finish its own work, such as a swap while the writer runs or an I/O error (do not take over until it
has; the lines say why); 2 usage.

`gxrepl torn` lists each folder:

- `TORN`: not one whole commit (a copy stopped half way). Never promote it.
- `INDEXES`: the derived indexes are behind or missing ("rebuild"). That is a warning, not torn: the data is
  whole, and the new writer's open (or the copy's derived pass) rebuilds them. Missing `<index>.stamp` files on
  an incremental set read the same way.
- `STAGED`: a staged unit copy is waiting for its swap.
- `OWN`: files this node has that gxrepl never copies (its own data).
- `WARNING`: the check could not take the apply lock in time (`GXREPL_TORN_LOCK_WAIT`, 15 s) and read without it.

Then start the writer on the node taking over, holding its `writer_lock`, so gxrepl will not overwrite it.

### 4.2 Failback

When the old primary is back, copy the new primary's changes to it before it writes again. On the node that
serves, for that run only:

(test pending)

```
gxrepl serve --config FILE --accept-both                 # its copies may also be read as sources
gxrepl once  --config FILE --pull-all                    # on the client: pull every set once, no deletes
gxrepl torn  --config FILE --copied-only                 # only the folders gxrepl copied
```

Then stop the writer on the interim primary, swap the roles back, and start the normal configs.

## 5. Archived folders

A writer that archives a folder (a finished day moved away) deletes it on the primary. On the copy, gxrepl
removes the copied files, then the folder whole, with its stage, derived indexes, `derived.stamp`, stamps and its
seal, under the apply lock. A minute sweep (`GXREPL_SWEEP_SEC`) catches folders left by a stop part way.

A folder is never swept when it:
- is unsealed, or sealed "copying";
- has a commit block, a copied file, a sub-folder or a writer lock held;
- holds a file gxrepl neither copies nor owns;
- changed in the last 10 minutes (`GXREPL_DEAD_QUIET_SEC`).

It is also never swept on a node that is the set's source. Archive your folders on the primary only.

Readers of vector index sets (`.vixset`) skip a listed day that is missing and count it, so an archived day never
breaks a search.

## 6. Upgrades

- **Mixed versions are safe.** A copy on older code skips the 7.4 "objects" records and applies every commit's
  pages. A source on older code works with a new copy. Protocol 3 is unchanged.
- **With the notes off (the default), the ship log is byte for byte what 7.3.2 writes.** Turn the object notes and
  `derived_apply = incremental` on only once both nodes run 7.4 beta 2 or later.
- Upgrade the copy first, then the primary. Keep the old binary (for example `gxrepl.prev`) for a rollback, and
  never copy a new binary over a running one (build to `gxrepl.new`, then rename).
- After the upgrade, run `gxrepl once` (exit 0) and `gxrepl torn` (0) before calling it done.

## 7. Troubleshooting

| Symptom | Likely cause | What to do |
|---|---|---|
| `refused ... SSL exception` in the server log | a client without the right certificate, or a port scanner | check `allow_cn` / `allow_fingerprint` and the client's `cert`, `ca`, `server_name` |
| `lag` stays above 0, `last_error` "busy" | a reader or a derived rebuild holds the apply lock | wait; if it lasts, look for a stuck reader of the copy |
| `needs a verify (open, gap or old position)` | a writer restarted, a gap, or a copy behind its log | normal; the verify copies only the pieces that differ |
| `record N is bigger than max_message` | one commit larger than a message | raise `max_message` on both nodes (16 MB is common); the copy resyncs the file meanwhile |
| `torn` says INDEXES for a long time | `derived_rebuild` failing, or a busy folder never quiet | check the rebuild command by hand on a scratch copy; set `derived_busy_rebuild` |
| `rebuild needed:` lines on an incremental set | a gap, an extractor error, or the cap | expected after faults; frequent ones mean look at the extractor or raise `derived_apply_max_mb` |
| `ALERT ... derived_apply back on rebuild` | the breaker tripped | find the crash in the log, fix the extractor, then `gxrepl derived rebuild --set NAME` |
| `ALERT ... outside the staged scope` | an extractor wrote its index directly | the extractor must write only through DataReel in its own gxrepl build |
| a TORN folder after a cut link | a copy stopped half way | let the next pass finish it; `promote --apply` before any take over |

## 8. When a plain file copy is enough

You do not always need gxrepl. A plain copy of the database folder is enough when all of these hold:

- the database is written rarely (a few times a day, or by a nightly job), so a copy minutes old is fine;
- you can stop or quiesce the writer while the copy runs: no transaction open, the journal checkpointed
  (`gxdJournal::Checkpoint()`, or closing the writer), so the files hold every commit and nothing else;
- nothing reads the copy while it is being written, or readers can wait for it to finish.

Then copy the folder with your file sync tool while the writer is stopped, and verify after.

(test pending)

```
# on the primary, with the writer stopped (or between its jobs)
rsync -a --delete -n /tmp/repl_demo/primary/db/ copy-host:/tmp/repl_demo/copy/db/   # dry run first
rsync -a --delete    /tmp/repl_demo/primary/db/ copy-host:/tmp/repl_demo/copy/db/
# verify: the same digests on both sides
gxrepl digest /tmp/repl_demo/primary/db/data.gxd --kind objects
ssh copy-host gxrepl digest /tmp/repl_demo/copy/db/data.gxd --kind objects
```

Exclude the journal (`*.jnl`) and lock files if the writer may have left them, and copy the indexes with the data
or rebuild them on the copy. If the writer cannot stop, if the database changes all day, or if the copy must be
seconds behind and always one whole commit, use gxrepl.

## 9. Test status of this guide

The commands above were written against DataReel 7.4 beta 2 (gxrepl 4f8a1b7) and match its usage text and the
gxrepl test suite. They are still to be run, in order, on a throwaway local setup (two folders on one machine,
the loopback address), with each block marked tested and any correction made. Until then, every block marked
"(test pending)" is a description, not a recipe.
