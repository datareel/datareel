# gxrepl: DataReel database replication over gxStream

gxrepl keeps DataReel object databases the same on two nodes, over a gxStream TLS connection on
TCP port **2073** (`gxsDATA_PORT`). A committed database transaction reaches the other node within
about half a second, and a cut link catches up by itself.

**Databases only (11/08/2026).** gxrepl carries DataReel database files (`.gxd`, `.btx`) and the
few files a set names with `include` (a database's own `commit.bin`, `sources.json`, its exports).
The sender never offers any other file, the receiver refuses one, and deletes touch only database
files. Hidden names (publish leftovers such as `.patterns.tmp-old-...`) are never copied. Flat files
(CSV, JSON, logs, reports) go by DRCM's two-way rsync over ssh, which excludes the database folders,
so the two never overlap.

**Port 2073 carries DataReel database traffic only:** replication, and remote access to database
objects over gxStream. Flat files never use it. The same mutual TLS (private CA, client CN and
fingerprint allow list), handshake limits and firewall rate limit cover any remote object access
on 2073.

One node runs `gxrepl serve` and listens. The other runs `gxrepl run`, dials it and drives every
replication set in both directions over that one connection. The node that cannot take inbound
connections (the workstation behind its firewall) is always the client. When port 2073 is not
open, the client opens an SSH tunnel to the server's loopback and still speaks TLS inside it.

## How it works

**Protocol 3 (11/08/2026): one connection, one channel per set.** The server is the DataReel
database object server (`gxdbserver.h`) and replication is one of its services. The client opens a
multiplex session (`gxdbmux.h`), says HELLO on a control channel, then opens one channel per set
(mode push or pull; the server refuses a set it lacks or a wrong role, and only that channel ends).
Each set runs on its own thread on the client, and on its own applier thread on the server, so
sets replicate at once and a large or slow set never holds up a small one. Each channel has its own
bounded queue and credit, and a full queue holds back only that set's sender. After a durable log
apply the server sends an ACK with the applied lsn, and the client writes it as the **retention
floor** (`<root>/.gxrepl/log/<stream>/acked/<copy>`): the writer's ship log removes an old segment
only when every copy has all of it, up to the size cap (2 GB; a copy that needs a removed record is
verified instead). The server status lists each channel and its queue.
Failback after a fail over is manual for now: `gxrepl once` with a pull config on the workstation.

**Database objects over the network (the object service, `gxdbobject.h`).** The same server also
serves database objects, the port's original and primary use. `[db NAME]` sections in the server
config name the databases (`path`, `access = read|write`, optional `journal`, default
`<path>.jnl`). A client opens a multiplex channel with service `object`, the database name and mode
`read` or `write`, and sends requests: ADD, REQUEST, CHANGE (same length), DELETE, FIRST, NEXT (see
`gxdbobject.h`). Writes go through the database's journal, one transaction per request. A database
inside a set this server receives is a replicated copy: read-only for object clients, and read under
the set's apply journal lock (the replicator holds it exclusively while it applies), so a reader
never sees a half-applied commit. `legacy_db = NAME` lets 6.x style clients (an `A` block adds, an
`R` block requests) use one database without a channel; without it they are refused. Examples:
`utils/dbserver/object_server.cpp` and `object_client.cpp`.

    [db orders]
    path = /data/orders.gxd
    access = write

- **Replication set:** a folder of databases (`root`), for example one published database folder
  or a folder of day databases. `direction = push` copies the client's
  folder to the server; `pull` copies the server's to the client. The server lists each set it
  takes part in, with `accept = target | source | both`.
- **Commit feed (the ship log):** a program that writes through a `gxdJournal` calls
  `EnableShipLog(set_root)` once. Each commit then also appends the committed pages to
  `<root>/.gxrepl/log/<stream>/` (segment files, retention 7 days or 2 GB). The append runs under
  the journal's write lock and before the pages reach the files, so the log never misses a commit
  that the files already hold. The client sends new records as they appear. The receiver applies
  them through its own journal (`<root>/.gxrepl/apply.jnl`, `gxdJournal::ApplyPages` and
  `Recover`), so a copy is always a committed state, even after a crash halfway through an apply.
  Object addresses do not change, and an object's address is its identity, so references between
  objects stay valid on the copy.
- **Scan:** every `scan_interval` seconds, the client compares the database files that no ship log
  covers (databases written without the log, published databases) by size and modification time.
  Changed files go over in 64 KB pieces, and only the pieces whose hash differs are sent.
- **Safe reads without a ship log:** a database in a folder with a journal (`*.jnl`) has a live
  writer: each piece read holds `gxdJournalReadLock` on that journal, so no commit is writing pages
  meanwhile. A folder without a journal is a published database (written once, swapped in by a
  rename): each read is checked against a stat before and after (same inode, size and mtime), and a
  file that changed is read again, never sent torn. For a live database the ship log is the right
  path (commit by commit, always a whole commit); the locked read is the fallback and the repair.
- **Crash and power-off safety:**
  - Sender: with the ship log on, each record is fdatasynced before the commit's pages reach the
    database files (unless the journal's sync level is OFF), and a new segment's folder is synced.
    A torn record at the end of the log (a power loss mid-append) is found by its checksum and cut
    back to the last whole record when the writer opens the log again.
  - Receiver: pages are applied through `.gxrepl/apply.jnl` (`ApplyPages`: the journal is synced
    before its header, `Recover` syncs the files before it clears the journal). The applied
    position of each stream is written with a temporary file, fdatasync, a rename and a folder sync,
    after the pages are durable, and only then is the request answered. The folders of files a batch
    creates are synced. `Recover` runs on every start.
  - The client keeps nothing in memory that it trusts after a restart: each pass reads the copy's
    applied position and sends from there.
- **Verify:** the client compares hashes of the covered files at connect, when a stream opens or
  has a gap (writer restart, crash, failed append), hourly for files changed in the last 48 hours,
  and fully once a day. It reads them under `gxdJournalReadLock`, so the pieces match an exact
  log position and the receiver goes on from there. If the log no longer holds what a copy needs
  (a long outage), a verify copies the files whole.
- **Safety:**
  - Paths are relative and checked on both sides. `..`, absolute paths, symlinks out of the
    root, and any path with `databento` in it are refused. Databento research data never leaves
    the node, and this cannot be configured off.
  - `*.jnl`, `*.lock`, `*.gxrtmp`, `*.tmp`, `*~` and `.nfs*` are never copied.
  - `exclude` patterns skip files and whole folders.
  - `writer_lock` names lock files (flock) that a node's own writer holds while it is the active
    node. While one is held, gxrepl refuses to change that node's copy. This prevents split
    brain after a failover.
  - Deletes are copied only with `deletes = yes`.
- **Open port:** the accept loop only takes the TCP connection. The TLS handshake runs in its own
  thread and has `handshake_timeout` seconds, at most `max_handshakes` at once, so silent
  connections (scanners) cannot keep the real client out (test_idle_connections_do_not_block).
- **TLS:** both sides authenticate with certificates (mutual TLS). The server accepts only the
  client CNs in `allow_cn` or the SHA-256 fingerprints in `allow_fingerprint`. The client checks
  that the server's certificate names `server_name`.
- **Wire:** each message is one gxStream block: op, flags (bit 0 = zlib), length, body. The hello
  checks the protocol, the address size (32- or 64-bit engine), the page size and the byte order.
  Two nodes that disagree refuse each other.

## Commands

    gxrepl serve  --config FILE [--key-fd N]     the server (listens)
    gxrepl run    --config FILE [--key-fd N]     the client (dials, replicates until stopped)
    gxrepl once   --config FILE [--set NAME] [--key-fd N]
                                                 one pass with a full verify; exit 0 = every set the same
    gxrepl status --config FILE                  the status file (JSON); exit 1 if older than 30 s
    gxrepl check  --config FILE                  certificates present, sets and folders
    gxrepl fingerprint CERT.pem                  SHA-256 fingerprint and CN (for allow_fingerprint)
    gxrepl shiplog DIR [--from LSN]              list a ship log's records
    gxrepl version

Signals: `TERM` / `INT` stop after the current message, and `HUP` on the client forces a full
verify at the next pass. Exit codes: 0 ok, 1 a set is not in step (`once`), 2 error, 64 usage.

The private key is `key = fd:N` (read from a descriptor, such as a pipe), `env:VAR` (read, then
removed from the environment) or a file path. `--key-fd N` overrides the config. A key read from
a descriptor or from the environment is kept only in an anonymous memory file (memfd), never in a
file on disk.

## Config

    node = drworkstation2          # this node's name (in the hello and the status)
    role = client                  # client or server
    connect = aws-osa:2073         # client: the server
    server_name = aws-osa          # client: the name the server's certificate must carry
    ssh_tunnel = user@host         # client: dial through ssh -L instead (connect is then not needed)
    ssh_option = IdentityFile=...  # client: more -o options for ssh (repeatable)
    ssh_local_port = 12073         # client: the tunnel's local end
    ssh_remote = 127.0.0.1:2073    # client: where the tunnel goes on the server
    ssh_command = ssh
    poll_ms = 500                  # client: pause between passes
    listen = 0.0.0.0:2073          # server
    allow_cn = drworkstation2      # server: client certificate CNs let in (list)
    allow_fingerprint = AB:CD:...  # server: or pinned SHA-256 fingerprints (list)
    max_clients = 4                # server: authenticated clients at once
    max_handshakes = 16            # server: TLS handshakes at once (each in its own thread)
    handshake_timeout = 10         # server: seconds a new connection gets to finish TLS
    cert = .../node.pem            # this node's certificate
    key = fd:3                     # this node's key (or --key-fd)
    key_passphrase =               # if the key is encrypted
    ca = .../ca.pem                # the CA that signed the other node's certificate
    status_file = .../status.json  # JSON status, written every few seconds
    timeout = 120                  # seconds without an answer before the connection is dropped
    max_message = 4194304          # largest message (64 KB - 64 MB)
    compress = yes                 # zlib (yes / no / level 1-9), if both sides have it

    [set NAME]
    root = /path/to/folder
    direction = push               # client: push or pull
    accept = target                # server: target, source or both
    scan_interval = 10             # seconds between scans of uncovered files
    verify_interval = 3600         # hash check of recently changed covered files
    verify_recent_hours = 48
    full_verify_interval = 86400   # hash check of everything
    exclude = /var/, *.pid, logs/  # list, repeatable: "/x/" = only x under the root (rsync style),
                                   # "name/" = a folder named name anywhere, else the file's path or name
    writer_lock = var/x/manager.lock   # see Safety
    deletes = yes                  # copy deletes of database files (default yes)
    include = commit.bin, sources.json, export/*.gz   # files carried with the databases (list)

`${VAR}` in a value is replaced from the environment. `examples/ws.conf` and `examples/aws.conf`
are the OSA workstation and AWS configs.

## Writing databases with the ship log

    gxdJournal j;
    j.Open("/data/set/db/writer.jnl");
    j.Attach(&db); j.Attach(&btree);
    j.EnableShipLog("/data/set");        // the replication set's root; the stream is named after the journal
    ... BeginTxn / Commit as usual ...

`EnableShipLog` needs write-through mode (the default) and no open transaction. A failed append
never fails the commit. It marks a gap, and the next verify repairs the copy. Programs that do
not call it still replicate, by scan.

## OSA

- Build: `cmake -S cpp -B cpp/build && cmake --build cpp/build -j` writes `cpp/bin/gxrepl`.
- Run: `automate/gxrepl.sh start|stop|restart|ensure|verify|once|status|check|fg`. It loads this
  node's key from the encrypted EPDB (`drsecrets.sh gxrepl-ws` on the workstation, `gxrepl-aws`
  on AWS) and hands it to gxrepl on descriptor 3, so the key never lands in a file, an argument
  or gxrepl's environment. With `ssh_tunnel`, it also loads the upload ssh key into the agent
  (`drsecrets.sh upload`). Only one gxrepl runs per config (a lock in `$OSAvar/gxrepl`).
- Certificates: `automate/gxrepl_certs.sh make --server-san DNS:aws-osa,IP:<AWS address>` makes a
  private CA and both certificates. The keys go into the EPDB records `gxrepl/ca`, `gxrepl/aws`
  and `gxrepl/workstation`, and the certificates into `~/.config/gxrepl/pki`.
- Ship log: `osa_ingest --ship-log $OSAvar/osa_monitor` and
  `osa_learn_ingest --ship-log $OSAvar/osa_learn` (both off by default).

## Tests

    python3 utils/gxrepl/test_gxrepl.py --bin BUILD_DIR [-k NAME] [--keep]

The tests need `gxrepl` and `gxrepl_testwriter` in BUILD_DIR, and the openssl command line tool.
They cover:

- the first copy
- live commits while files change
- a writer killed with `kill -9` mid-commit
- a cut link and its catch-up
- a lost log after a long outage
- the pull direction
- the split-brain guard
- a server that refuses a source set
- TLS refusals: an intruder, a foreign CA, a wrong name and plain TCP
- the SSH tunnel
- a key read from a pipe
- unsafe records

POSIX only (Linux; the ship log uses flock and the key path uses memfd).
