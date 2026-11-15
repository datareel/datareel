# DataReel 7.4 Benchmarks

What the DataReel 7.4 database and vector index cost on one Linux machine, measured by one program
(`utils/dr_bench`, built with the utilities) with fixed random seeds. Every number below comes from the run of
11/15/2026 described under "How it was measured"; run `dr_bench` on your own hardware for yours.

## How it was measured

- **Machine:** a 4-core, 8-thread Intel Xeon W-2223 (3.6 GHz) workstation with 64-bit Linux, an XFS file system
  on an encrypted (dm-crypt) volume. The machine was in its normal use, at a 1-minute load of 2.5 to 3.7.
- **Build:** a Release build (`-DCMAKE_BUILD_TYPE=Release -DDATAREEL_64BIT_DATABASE=ON`, the default native
  types) with GCC 11.5, inside a rootless container (Rocky Linux 9.8, no network, removed after the run). The work
  folder was a bind mount of the host's XFS disk, so a durable commit's sync reaches the disk.
- **Priority:** the whole run at the lowest CPU and I/O priority (`nice -n 19`, `ionice -c3`).
- **Threads:** one, except the four-thread vector set search.
- **Times:** wall clock; p50 and p99 are the median and the 99th percentile of the single operations.

```
dr_bench [WORK_FOLDER] [--quick]          # --quick: a tenth of the sizes, for a smoke test
```

## 1. Records (gxDatabase)

200,000 fixed 128-byte records, appended with `Alloc()` and `Write()`, then read back at 200,000 random addresses.

| Operation | Rate | Total |
|---|---|---|
| Append (Alloc + Write) | 289,000 records/s | 692 ms |
| Random read | 1,421,000 records/s | 141 ms |
| File size | 30.5 MB (160 bytes a record, with its block header) | |

The file is the records and their block headers, nothing else: a record's address (its FAU) is its key, so a read
at a known address is one positioned read.

## 2. Transactions (gxdJournal)

Commits of 10 records of 128 bytes each through a `gxdJournal`.

| Commit | Rate | p50 | p99 |
|---|---|---|---|
| `Commit(1)`, durable (journal and data synced before it returns) | 1,302 commits/s | 0.41 ms | 0.61 ms |
| `Commit(0)`, not durable (synced at the next checkpoint) | 25,100 commits/s | 0.01 ms | 0.02 ms |

A durable commit costs about one disk sync; the rest is under 0.02 ms. Batching more records into one commit is
what raises the durable record rate: the sync is paid once a commit, whatever its size.

## 3. Indexes (gxBtree)

200,000 random 64-bit keys in a B-tree of node order 32.

| Operation | Rate | Total |
|---|---|---|
| Insert (random order) | 209,000 keys/s | 959 ms |
| Find (random) | 234,000 keys/s | 854 ms |
| Walk in order | 2,678,000 keys/s | 75 ms |
| File size | 4.9 MB | |

## 4. Vector search (gxVectorIndex)

50,000 vectors of dimension 96 in 100 clusters, cosine metric, graph degree M 16, `ef_construction` 200. 500
queries; recall@10 is the share of the exact 10 nearest (from `Exact()`) that `Search()` returns.

| Element type | Build | File | Bytes a node | Exact() a query |
|---|---|---|---|---|
| float32 | 4,760 vectors/s | 34.9 MB | 688 | 1.93 ms |
| float16 | 3,430 vectors/s | 25.8 MB | 496 | 2.91 ms |
| int8 | 3,400 vectors/s | 21.4 MB | 404 | 4.14 ms |

| Element type | Search() ef 32: p50 / p99 / recall@10 | ef 64 | ef 128 |
|---|---|---|---|
| float32 | 67 / 109 µs / 0.976 | 100 / 141 µs / 0.998 | 131 / 170 µs / 1.000 |
| float16 | 82 / 123 µs / 0.976 | 115 / 166 µs / 0.998 | 145 / 206 µs / 1.000 |
| int8 | 92 / 125 µs / 0.977 | 123 / 173 µs / 0.998 | 164 / 224 µs / 1.000 |

What the numbers show:

- **The graph search is 15 to 45 times faster than the exact scan** at this size, at 0.976 to 1.000 recall. The
  gap grows with the index: `Exact()` reads every node, while `Search()` follows the graph through a small part of
  them.
- **The search width (ef) trades time for recall.** ef 64 is the default.
- **float16 and int8 trade speed for size.** They cut the file by 26% and 39% with the same recall here, and each
  distance decodes the stored code, so they are slower than float32 in this build. Choose them when the files'
  size or the memory they take matters more than the last tens of microseconds.

## 5. Day files searched as one (gxVectorSet)

Ten day files of 10,000 vectors each (dimension 96, cosine), listed in one `.vixset` manifest, 500 queries, k 10,
ef 64.

| Search | p50 | p99 | Files opened |
|---|---|---|---|
| Every day, 1 thread | 0.70 ms | 0.97 ms | 10 |
| Every day, 4 threads | 0.34 ms | 0.77 ms | 10 |
| One day (a day range) | 0.05 ms | 0.09 ms | 1 |

A day range is answered from the manifest first: each listed file keeps the range of its day tag, so the nine
other days are skipped without being opened. Searching the set with several threads splits the files between
them.

## The techniques behind the numbers

- **Fixed records at known addresses.** A gxDatabase record is found by its file address, so the common read is a
  single positioned read with no lookup structure in between; indexes (B-trees, vector indexes) store addresses.
- **One journal for every file of a transaction.** The data file, its B-trees and its vector index attach to the
  same `gxdJournal`, so a record and all of its index entries are written together or not at all, with one sync
  for the whole commit.
- **A graph index for vectors (HNSW).** Approximate nearest-neighbor search walks a layered proximity graph
  instead of scanning; exact search stays available and is used for small indexes and very selective filters.
- **Compact vector codes.** float16 (IEEE half precision) and int8 (signed bytes with one scale per vector) store
  vectors in half or a quarter of the space; members of records can carry these codes, so an index can be rebuilt
  from the records alone.
- **Partitioning by day.** Many small files and a manifest let a search skip whole days, archive a day by moving
  one file, and keep the live day's index open in its writer.

Compared with typical technologies for the same jobs, which often put records, indexes and vectors in separate
services, DataReel keeps them in ordinary files of one library in the application's own process: no network hop
and no second copy of the data for the vectors, at the price of running everything on one machine (or a
replicated copy, see the [replication administrator's guide](docs/replication_admin_guide.html)).

## Running it yourself

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target dr_bench
build/utils/dr_bench /path/on/the/disk/to/measure
```

Give it a folder on the disk you want measured: a durable commit's time is that disk's sync time. The work folder
is removed at the end.
