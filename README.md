# DataReel

DataReel is a high-performance C++ object-oriented database (OODB) with native vector
search, database sockets and multi-node replication. It is released under the GNU Lesser
General Public License, version 3 or any later version (see [LICENSE](LICENSE)).

There are two release tracks:

| Track | For | API |
|---|---|---|
| **DataReel 7.x** (current: 7.4 beta) | New projects, modern C++ pipelines, AI-assisted development | Database, vector index, sockets, replication, on standard C++ (`std::`) |
| **DataReel 6.4** (legacy bridge, planned) | Existing 6.x systems where moving to 7.x is too large a rewrite | The full 6.x API plus the vector index and replication, backported |

## DataReel 7.x

### What it is

DataReel 7.x is no longer a general-purpose C++ utility toolkit. It is an object database
and networking engine:

- **Object database:** gxDatabase file storage with block reuse, B-tree indexes, typed
  persistent objects, and gxdJournal transactions (begin, commit, rollback, crash recovery).
- **Vector index (7.4):** gxVectorIndex `.vix` files bound to database objects (a vector's
  id is the object's address), so a similarity lookup returns the records themselves. Sets of
  day or partition files are searched as one (gxVectorSet), the object server answers vector
  searches, and a replicated copy rebuilds its vector indexes from the records.
- **Database sockets:** gxStream client/server sockets with TLS, including mutual
  (client certificate) TLS since 7.3.1, and an object server.
- **Windows and Linux object replication (7.4):** gxObjRepl replicates objects between
  Windows and Linux nodes in both directions, with the primary on either side, and takeover
  and rejoin work across the platforms. It replaces gxrepl page shipping (7.3.1 to the 7.4
  betas), removed in 7.4 beta 5. The
  [replication administrator's guide](docs/replication_admin_guide.md) covers setup, watching,
  takeover and rejoin, upgrades, and when a plain file copy is enough.

### Legacy utility classes removed

The custom containers, string classes, file helpers and thread wrappers that standard C++
now provides have been removed in favor of `std::`. The library is smaller and easier to
read, and code that uses it looks like ordinary modern C++.

This also matters for AI-assisted development (for example Claude Code). A smaller code
base fits better in a model's context window, costs fewer tokens to read, and leaves no
room to confuse a DataReel wrapper with the `std::` type it duplicated. AGENTS.md,
`llms.txt` and `llms-full.txt` describe the library for coding assistants.

### Legacy (6.0-6.3) compared with 7.x

| | Legacy (6.0-6.3) | DataReel 7.x |
|---|---|---|
| **Scope** | One large toolkit: containers, string utilities, thread wrappers and database parts | Database, vector, socket and replication APIs only |
| **Standard library** | Its own wrappers for work modern `std::` now does | Uses standard C++ directly |
| **Data and indexing** | Object persistence with B-tree indexes | B-tree indexes plus vector similarity (ANN) search |
| **Networking** | File storage and basic socket wrappers | TLS database sockets, an object server and built-in replication |
| **AI-assisted development** | Large surface of wrapper boilerplate | Small surface, easy to keep in context |

### The vector index (7.4)

`gxvindex.h` adds an approximate nearest neighbor index stored in an ordinary DataReel
database file:

- HNSW graph search, with exact search for small indexes and for very selective filters,
  so those never miss.
- L2, cosine and inner-product distances; results sorted by distance, then id.
- Four integer tags per vector (for example session, kind, day, definition) with range
  filters, plus an optional callback filter.
- Writes go through gxdJournal, so a crash never leaves a half-written vector or link.
- Searches keep running while one writer inserts. Measured on 1,000,000 clustered vectors
  (dimension 96) on a 4-core VM: recall 0.987 at 0.17 ms median / 0.34 ms p99 per search,
  and p99 0.61 ms while 1,000+ vectors a second were being inserted.
- A spec string in the header (feature family, version and builder hash): an index refuses
  to open against a different spec, so vectors from two feature builders never mix.
- Distances are summed in a fixed order, so results are bit-identical on every build and
  node.
- Merge() builds one index from many (for example the day files of a month); Repair()
  rebuilds the graph; Verify() checks every vector against its object.
- float16 and int8 storage (about half and a quarter of the vector memory), and vector
  members: a POD or InfoHog record can carry its vector, kept in step with the index in
  the same journal transaction.

See [RELEASE_NOTES_7.4.txt](RELEASE_NOTES_7.4.txt) and
[docs/ai/object-database.md](docs/ai/object-database.md).

### Building

DataReel 7.x builds with CMake and a C++17 compiler. TLS needs OpenSSL (Schannel on
Windows; see [README_windows_setup.txt](README_windows_setup.txt)).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
tests/run_tests.sh
```

The examples in `examples/` show typed objects, journal crash recovery, a day-organized
event database, replication, trading sessions and the string utilities; each has a page
under `docs/examples`. The full documentation starts at [docs/index.html](docs/index.html),
and [README.txt](README.txt) has the complete release notes index.

### Coming next

- **7.5 alpha:** a Python module, published as the `datareel` pip package, built from this
  repository.

## DataReel 6.4: the legacy bridge release

### Why

DataReel 7.x is the way forward for new projects, but several mature production systems
depend on the 6.x utility classes, containers and threading model. Rewriting them to
remove those dependencies would be a large job with real regression risk.

### What it will deliver

DataReel 6.4 keeps the full 6.x API and adds the main 7.x data features:

- **The vector index**, built on the 6.x classes, so existing applications can add
  similarity search without changing their data structures.
- **The replication engine**, with the 7.x database socket fixes.
- **No rewrite needed:** a 6.x application compiles against 6.4 unchanged and can then use
  the new features.

### Systems it is for

- **DRCM (DataReel Cluster Manager):** keeps its cluster orchestration and 6.x threading
  and socket code, and gains replication.
- **File encryption (`fcrypt`):** keeps its cryptographic file handling on the 6.x buffer
  and I/O classes.
- **Encrypted password database:** keeps its proven storage code, with vector search or
  replication available as options.

## Which release to use

| Release | Use it for | API | Main features |
|---|---|---|---|
| **DataReel 6.4** | Existing 6.x systems (DRCM, file encryption, password database) | Full 6.x API plus the backported features | Backward compatible; vector index and replication |
| **DataReel 7.x** | New builds, modern C++ pipelines, AI-assisted development (for example market anomaly databases) | Database, vector, sockets and replication | Lean modern C++, OODB plus vector search, database sockets, replication |
