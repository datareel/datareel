// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdbobject.h
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Date Last Modified: 11/08/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Database objects over the network: the object service of the DataReel
database object server (gxdbserver.h), the database port's original
and primary use (docs/DATABASE_OBJECT_SERVER_DESIGN.md section 7).

The service serves the databases its config names ([db NAME] sections:
path, access read|write, optional journal). A client opens a multiplex
channel (gxdbmux.h) with service "object", the database name and mode
read or write, then sends one request per message:

  op 1 ADD      bytes data           -> u64 address
  op 2 REQUEST  u64 address          -> bytes data
  op 3 CHANGE   u64 address, bytes   -> (the same length as the object)
  op 4 DELETE   u64 address          -> nothing
  op 5 FIRST                         -> u64 address (0: none)
  op 6 NEXT     u64 address          -> u64 next address (0: none)
  op 7 VECTOR_SEARCH (7.4)           -> nearest neighbors (below)
  op 8 VECTOR_INFO   (7.4)           -> the vector index (below)
  reply: u8 status (0 ok, 1 error), then the result or str error
  (str: u32 length and the bytes; all numbers native little endian)

Vectors (DataReel 7.4, gxvset.h): a [db NAME] with "vectors = <file>"
(a .vixset manifest or one .vix file) also answers vector searches,
read-only, on any channel of that database:
  op 7 VECTOR_SEARCH  u32 version (1), u32 k (1..1000), u32 ef (0: 64,
       at most 4096), u32 dim (the index's), u32 flags (bit 0 day range,
       bit 1 strict: a listed file missing is an error), u32 tag_mask,
       u32 lo[4], u32 hi[4] (tag i must be in lo[i]..hi[i] when bit i of
       tag_mask is set), u32 day_lo, u32 day_hi (tag 2), f32 query[dim];
       the body is exactly that long, the query finite
    -> u32 version (1), u32 count, u32 files skipped (by their day range),
       u32 files missing, count x { i64 id, f32 distance, u32 tags[4],
       u32 file }, u32 names, names x { u32 file, str path } (the files
       the results name; paths relative to the manifest's folder).
       Results are sorted by distance, then file, then id.
  op 8 VECTOR_INFO  (empty)
    -> u32 version (1), u32 dim, u32 element type, u32 metric, u32 files,
       u64 nodes, u64 deleted (the manifest's counts), str spec
At most SetVectorLimits(threads, max_concurrent) searches run at once
(default 4); one past it waits up to 5 s, then is answered "busy".

A database opened for writing changes through its gxdJournal (one
transaction per request; with a ship log, a remote change replicates
like a local one). A database inside a replication set this server
receives (a copy) is read-only, and each read holds gxdJournalReadLock
on the set's apply journal, so it never sees a half-applied commit.

Legacy clients (the DataReel 6.x gxStream style: an 'A' block adds an
object and gets a result block with its address, an 'R' block with an
address gets the object back) work without a channel when the config
names a legacy database (legacy_db); without one they are refused.

Changes:
==============================================================
11/08/2026: New in DataReel 7.3.1 (phase 2 step 4).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_OBJECT_SERVICE_HPP__
#define __GX_DATABASE_OBJECT_SERVICE_HPP__

#include "gxdlcode.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include "gxdbserver.h"
#include "gxdbmux.h"

namespace datareel {

enum gxObjectOp {
  gxOBJ_ADD = 1, gxOBJ_REQUEST = 2, gxOBJ_CHANGE = 3, gxOBJ_DELETE = 4,
  gxOBJ_FIRST = 5, gxOBJ_NEXT = 6,
  gxOBJ_VECTOR_SEARCH = 7, gxOBJ_VECTOR_INFO = 8   // 11/14/2026 (7.4)
};
const uint32_t gxObjVectorVersion = 1;
const uint32_t gxObjVectorMaxK = 1000;
const uint32_t gxObjVectorMaxEf = 4096;

struct GXDLCODE_API gxObjectDbConfig {
  std::string name;
  std::string path;              // The .gxd file
  std::string journal;           // Default: path + ".jnl"
  std::string access = "read";   // read or write
  std::string vectors;           // 11/14/2026 (7.4): a .vixset manifest or a .vix file (vector searches)
};

class GXDLCODE_API gxObjectService : public gxDatabaseService {
public:
  gxObjectService(const std::vector<gxObjectDbConfig> &dbs,
		  const std::string &legacy_db = std::string());
  ~gxObjectService();

  // Databases under this folder are replicated copies: read-only, read
  // under gxdJournalReadLock on apply_journal
  void ProtectRoot(const std::string &root, const std::string &apply_journal);

  // gxDatabaseService: legacy sessions ('A'/'R'/'L' first block)
  const char *ServiceName() const override { return "object"; }
  bool Claims(const gxBlockHeader &gx, const std::vector<unsigned char> &body) override;
  void Serve(gxDatabaseSession &s) override;

  // Multiplex channels: may this channel open, then serve it until it
  // closes or "stop"
  bool OpenAllowed(const std::string &name, const std::string &mode, std::string &why);
  void ServeChannel(gxdMuxSession &mux, uint32_t ch, const std::string &name,
		    const std::string &mode, std::atomic<bool> &stop);

  // One request (op and body), its reply body. 0, or -1 with err.
  int Request(const std::string &name, bool write, uint8_t op,
	      const std::vector<unsigned char> &body,
	      std::vector<unsigned char> &reply, std::string &err);
  // 11/14/2026 (7.4): threads per vector search, searches at once
  void SetVectorLimits(unsigned threads, unsigned max_concurrent);

private:
  struct Db;
  Db *find(const std::string &name);

private:
  int vector_request(Db *d, uint8_t op, const std::vector<unsigned char> &body,
		     std::vector<unsigned char> &reply, std::string &err);

private:
  std::map<std::string, std::unique_ptr<Db> > dbs;
  std::string legacy;
  // 11/14/2026 (7.4): vector searches (last, so the members above keep their places)
  unsigned vec_threads = 2, vec_max = 4, vec_running = 0;
  std::mutex vec_m;
  std::condition_variable vec_cv;
};

} // namespace datareel

#endif // __GX_DATABASE_OBJECT_SERVICE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
