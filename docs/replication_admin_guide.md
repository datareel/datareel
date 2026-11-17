# DataReel Replication Administrator's Guide (gxObjRepl)

This guide is for the person who runs object replication on two nodes: setting it up, watching it, taking over
when the primary fails, and getting back. It covers gxObjRepl as shipped in DataReel 7.4 beta 5. The class
reference is `docs/classes/database/gxobjrepl.htm`. The tool is `utils/objrepl/objrepl.cpp`, and the release
notes are `RELEASE_NOTES_7.4.txt`.

> **7.4 beta 5:** gxObjRepl replaces gxrepl page shipping, which 7.4 no longer has. A gxrepl binary installed
> from a 7.4 beta keeps working as it is, but it is not part of 7.4. The gxrepl guide of the 7.4 betas 1 to 4 is
> kept with the gxrepl source in the DataReel repository's history.

Every example uses throwaway folders under `/tmp/objrepl_demo`, two node names (`n1` and `n2`) and the loopback
address. Never try the commands here for the first time on production nodes: run them on a local throwaway setup
first. **Test status:** each block marked **(tested)** was run as printed, in order, on a throwaway setup
(section 10 says how).

## 1. What gxObjRepl replicates

- **Objects, not file pages.** Each node is a folder with its own object databases (`<db>.gxd` and an id index).
  The primary sends the objects a commit added, changed or removed, and the copy applies them in one transaction.
  Each node keeps its own file addresses and builds its own B-tree and vector indexes, so nothing about one node's
  files has to match the other's.
- **Between Windows and Linux, in both directions.** Mixed Windows and Linux nodes replicate either way, with the
  primary on either side, and takeover and rejoin work across the platforms. Every platform sends gxStream's block
  header in one canonical 32-byte form. TLS uses each platform's own TLS library (OpenSSL on Linux, the Windows
  Schannel library on Windows), with certificates on both sides.
- **Asynchronous.** A commit is durable on the primary when `Commit()` returns, and on the copy once the copy has
  acknowledged it. If the primary is lost, the commits the copy did not have are lost with it. The primary's
  status shows them as lag. After a takeover, the old primary's rejoin counts them.
- **The application is the primary.** A program writes through the gxObjRepl API (`Begin()`, `Put()`, `Remove()`,
  `Commit()`) and sends with `Pump()` or `Drain()`. The copy runs `objrepl copy` or the program's own `Serve()`.
  `objrepl primary` is a test data generator: it writes test commits and sends them. This guide uses it as the
  demo's writer. The example program `examples/oodb/object_replication.cpp`
  ([its page](docs/examples/object_replication.htm)) shows the API side: a primary and a copy, commits, a planned switch.
- **32-bit and 64-bit.** With the 32-bit engine each database file is limited to 2 GB (FAU_t); 1M-object stores
  need the 64-bit engine. Mixed 32/64-bit nodes replicate. Not tested: a 32-bit Windows build.

## 2. Setting up a primary and a copy

### 2.1 Build

Build every binary from one 7.4 beta 5 tree: the library, the `objrepl` tool and your programs. A program built
from an older tree may send the old stream header, which is refused (section 6).

```
cmake -S datareel -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
build/utils/objrepl engine        # "64-bit" and exit 0 for the 64-bit engine (the default)
```

### 2.2 Certificates

`utils/objrepl/objrepl_certs.sh` makes a private CA and the two certificates: the copy's server certificate, and
the primary's client certificate, whose CN the copy allows. It is a dry run unless you add `--apply`, and it never
overwrites a file.

(tested)

```
mkdir -p /tmp/objrepl_demo
datareel/utils/objrepl/objrepl_certs.sh /tmp/objrepl_demo/certs --copy-host 127.0.0.1           # dry run
datareel/utils/objrepl/objrepl_certs.sh /tmp/objrepl_demo/certs --copy-host 127.0.0.1 --apply
ls /tmp/objrepl_demo/certs
```

Give each node its own certificate and key, and the CA certificate (`ca.pem`). Keep `ca.key` off the nodes. The
keys are written mode 600. `objrepl` refuses a key that others can read, and it also checks the certificates'
dates, CA and key when it starts. A Windows (Schannel) build makes those checks at the TLS handshake instead.

### 2.3 Configuration

Each node reads `ROOT/objrepl.conf`, one `key value` line per setting (`#` starts a comment). The same keys work
as command line options with `--` in front, and the command line wins.

| key | node | meaning |
|---|---|---|
| `name`, `peer-name` | both | this node and the peer it expects (empty: any) |
| `ca`, `cert`, `key` | both | the CA, this node's certificate and key |
| `copy` | primary | the copy, `HOST:PORT` |
| `listen` | copy | `[ADDR:]PORT` (default address 127.0.0.1) |
| `allow-cn` | copy | the CN of the primary's client certificate |
| `links` | primary | `DB:OFFSET:TARGET_DB,...`: link fields (8-byte addresses) sent as ids |
| `index` | both | `NAME:DB:B:OFFSET:LEN` (B-tree) or `NAME:DB:V:OFFSET:DIM` (vector): this node's own indexes |
| `max-lag`, `max-behind`, `min-cert-days` | both | `check` thresholds: commits, seconds, certificate days (100, 60, 30) |
| `idle-timeout` | both | seconds without a frame before a connection is dropped (120) |
| `trim` | primary | 1: drop outbox entries the copy has (default) |

(tested)

```
D=/tmp/objrepl_demo; C=$D/certs
mkdir -p $D/n1 $D/n2
cat > $D/n2/objrepl.conf <<EOF
name n2
peer-name n1
ca $C/ca.pem
cert $C/copy.pem
key $C/copy.key
listen 127.0.0.1:24700
allow-cn objrepl-primary
EOF
cat > $D/n1/objrepl.conf <<EOF
name n1
peer-name n2
ca $C/ca.pem
cert $C/primary.pem
key $C/primary.key
copy 127.0.0.1:24700
EOF
```

### 2.4 Start the copy, then the primary

(tested)

```
B=build/utils/objrepl; D=/tmp/objrepl_demo
$B copy $D/n2 > $D/n2.log 2>&1 &  echo $! > $D/n2.pid
$B primary $D/n1 --commits 200               # the demo's writer: 200 test commits, then waits for the copy
echo "exit $?"                                # 0: the copy has every commit
```

The copy runs until it gets TERM or INT on Linux, or Ctrl+C or Ctrl+Break on Windows. On either platform it also
stops when `ROOT/objrepl.stop` appears. Stop it by its process id, never by a name pattern.

## 3. Watching it

- `objrepl status ROOT`: the node's state from `ROOT/objrepl.status`: role, epoch, sequence numbers, lag, and
  whether it is running.
- `objrepl check ROOT`: one line, OK, WARN or FAIL, with exit 0, 1 or 2. WARN covers a node that is not running,
  a copy that is away, lag over `max-lag` or `max-behind`, and a certificate within `min-cert-days` of expiring.
  FAIL means an alarm, such as a refused peer. Run it from cron and send the line to your monitoring.
- `objrepl clear-alarm ROOT`: acknowledges an alarm once you have dealt with its cause.
- `objrepl digest ROOT`: every database's ids and contents, with links as ids. Two nodes that hold the same
  objects print the same line.
- `objrepl ixcheck ROOT --index ...`: each index checked against entries rebuilt from the objects.

(tested)

```
B=build/utils/objrepl; D=/tmp/objrepl_demo
$B status $D/n1; $B status $D/n2
$B check $D/n2; echo "exit $?"
$B digest $D/n1 > $D/n1.digest; $B digest $D/n2 > $D/n2.digest
cmp $D/n1.digest $D/n2.digest && echo "the same objects"
```

## 4. Failover and failback

- **Takeover.** `objrepl takeover ROOT` makes a stopped copy the primary at the next epoch. New ids carry the
  epoch, so the new primary never hands out an id the old one may have used.
- **Rejoin.** `objrepl rejoin ROOT` makes the former primary a copy again. If its copy had acknowledged every
  commit (a planned switch), it follows on from there with no full send. Otherwise, the commits the new primary
  never had are counted and dropped, and the node takes a full send from the new primary.

Either node can be Windows or Linux. A Windows copy can take over from a Linux primary, and the Linux node then
rejoins as its copy, or the other way round.

### 4.1 A planned switch (no commit lost)

Drain first: the primary's last run exited 0, and `objrepl primary ROOT --drain` sends what is left without new
commits. Then stop both nodes, and switch.

(tested)

```
B=build/utils/objrepl; D=/tmp/objrepl_demo
kill $(cat $D/n2.pid); wait $(cat $D/n2.pid) 2>/dev/null
$B takeover $D/n2                             # n2: primary, epoch 2
$B rejoin $D/n1                               # n1: a copy again
# n1 now listens and n2 sends: swap the settings
sed -i 's/^copy .*/listen 127.0.0.1:24701/; s/primary\./copy./g' $D/n1/objrepl.conf && echo "allow-cn objrepl-primary" >> $D/n1/objrepl.conf
sed -i 's/^listen .*/copy 127.0.0.1:24701/; /^allow-cn/d; s/copy\.\(pem\|key\)/primary.\1/g' $D/n2/objrepl.conf
$B copy $D/n1 > $D/n1.log 2>&1 &  echo $! > $D/n1.pid
$B primary $D/n2 --commits 50; echo "exit $?"
$B digest $D/n1 > $D/n1.digest; $B digest $D/n2 > $D/n2.digest
cmp $D/n1.digest $D/n2.digest && echo "the same objects"
kill $(cat $D/n1.pid)
```

The demo swaps the certificates as well, because its copy certificate is the server certificate. With one
certificate per host that can serve both roles, only `copy`, `listen` and `allow-cn` change.

### 4.2 After a crash

If the primary is gone, take over on the copy at once (`objrepl takeover`), and point your writers at it. When
the old primary comes back, do not start it as a primary. Run `objrepl rejoin` on it first: the status line says
how many of its commits the copy never had (`dropped_at_rejoin`). Then start it as a copy.

## 5. Upgrades

- Every node must run a 7.4 beta 5 build. A peer that sends the old header with 4-byte words (Windows and 32-bit
  builds from before beta 5) is refused with an alarm. Older 64-bit Linux builds always sent the canonical
  header, but upgrade them anyway.
- From DataReel 6.4, programs built with the matching stream header can talk to 7.4 replication.
- Build every binary from one tree (section 2.1).
- Never set `GXD_CREATE_LAYOUT` in production. It creates files in the old 4-byte layout, for readers older than
  beta 5 only.
- Upgrade the copy first, then switch (section 4.1), then upgrade the old primary.

## 6. Troubleshooting

- **`check` says FAIL with "refused".** The peer's name, role, epoch or history does not match, or it sent the old
  stream header. Read the alarm in the status, fix the cause (a wrong node, an old build), then
  `objrepl clear-alarm`.
- **A copy far behind.** A copy further behind than the digest log's 1,000,000 commits, or one whose history the
  primary cannot check, is refused. `objrepl rejoin` on it makes it take a full send.
- **Readers wait.** The node's lock is held while a frame is applied, while an index is built at startup, and
  while a full send sets the old data aside. A test build (`-DGXREPL_TEST_HOOKS=ON`) with `OBJREPL_TEST_HOLD_MS=N`
  set prints every hold of N ms or more on stderr, by phase. Release builds print nothing.
- **A commit failed on a journal sync error.** It may still survive recovery. Read the object back before
  counting on either outcome.

## 7. Known limits

- `Open()` of a node with many databases opens them, and builds any missing index, under the lock: readers wait
  for it once, at startup.
- A copy's SNAP_BEGIN (the start of a full send) still holds the lock while it sets the old data aside, but
  readers fail at once ("a full send is under way") rather than wait.
- An object is at most 32 MB, and a commit's changes at most 60 MB.
- Index fields may not overlap link fields.
- With the 32-bit engine each database file is limited to 2 GB (FAU_t); 1M-object stores need the 64-bit engine.
  Mixed 32/64-bit nodes replicate. Not tested: a 32-bit Windows build.
- The property test (`prop_objrepl`) and the wire fuzzer (`fuzz_objrepl`) are POSIX only; the API test, the
  `objrepl` tool and its test scripts run on Windows and Linux.

## 8. The journal and the lock

Each node writes through one gxdJournal (`ROOT/node.jnl`) in write-back mode: a commit's pages reach the files
only after its batch is durable in the journal, as with write-through. This lets the journal sync run without
the node's lock, so readers keep going during it. Checkpoints sync the files the same way. `Begin()`, `Close()`
and a soft checkpoint wait until no commit's sync is running. `Pump()` gives the lock up for its network I/O.
Do not copy a running node's folder: use `objrepl digest` to compare, and rejoin to rebuild a copy.

## 9. When a plain file copy is enough

You do not always need replication. A plain copy of the database folder is enough when all of these hold:

- the database is written rarely (a few times a day, or by a nightly job), so a copy minutes old is fine;
- you can stop or quiesce the writer while the copy runs: no transaction open, the journal checkpointed
  (`gxdJournal::Checkpoint()`, or closing the writer), so the files hold every commit and nothing else;
- nothing reads the copy while it is being written, or readers can wait for it to finish.

Then copy the folder with your file sync tool while the writer is stopped, and verify after with `dr_digest`,
which prints the same digest for the same contents whatever the file's layout:

```
# on the primary, with the writer stopped (or between its jobs)
rsync -a --delete -n --exclude '*.jnl' /data/db/ copy-host:/data/db/   # dry run first
rsync -a --delete    --exclude '*.jnl' /data/db/ copy-host:/data/db/
# verify: the same digests on both sides
dr_digest /data/db/data.gxd --kind objects
ssh copy-host dr_digest /data/db/data.gxd --kind objects
```

The exclude leaves out the journal (`*.jnl`); leave out lock files the same way. Copy the indexes with the data
or rebuild them on the copy.

**On Windows**, the same with robocopy, and the digests of the example program `replication_copy`. Your
application needs its own digest, the way `replication_copy.cpp` makes one: the records in address order, the
B-tree's keys in order, and `gxVectorIndex::LogicalDigest()`.

(tested on Windows 11 with DataReel 7.4 beta 3)

```
rem with the writer stopped
robocopy C:\repl_demo\primary C:\repl_demo\copy /MIR /XF *.jnl /R:0 /L    &rem dry run first
robocopy C:\repl_demo\primary C:\repl_demo\copy /MIR /XF *.jnl /R:0
rem robocopy exit codes 0 to 7 are success, 8 and up a failure
replication_copy digest C:\repl_demo\primary > primary.digest
replication_copy digest C:\repl_demo\copy > copy.digest
fc /b primary.digest copy.digest
vix_rebuild --digest C:\repl_demo\copy\vectors.vix
```

robocopy cannot copy a file another program holds open, which is one more reason to stop the writer first. A
copy that lost its indexes rebuilds them from its own data (`replication_copy rebuild`); the digests then agree
again.

If the writer cannot stop, if the database changes all day, or if the copy must be seconds behind and always one
whole commit, use gxObjRepl.

## 10. Test status of this guide

The blocks marked (tested) in sections 2 to 4 were run as printed, in order, with DataReel 7.4 beta 5
(header-fixed-width after the gxObjRepl merge) on one Linux machine: two folders under `/tmp/objrepl_demo` and the
loopback address, with `objrepl primary` as the writer. The robocopy block in section 9 was run on Windows 11
with 7.4 beta 3. The cross-platform cases (Linux to Windows and back, takeover and rejoin, the 2,500-commit stress
run each way, old builds refused) come from the replication tests of 11/16/2026.
