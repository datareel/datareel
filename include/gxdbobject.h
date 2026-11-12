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
  reply: u8 status (0 ok, 1 error), then the result or str error

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
#include <string>
#include <vector>
#include "gxdbserver.h"
#include "gxdbmux.h"

namespace datareel {

enum gxObjectOp {
  gxOBJ_ADD = 1, gxOBJ_REQUEST = 2, gxOBJ_CHANGE = 3, gxOBJ_DELETE = 4,
  gxOBJ_FIRST = 5, gxOBJ_NEXT = 6
};

struct GXDLCODE_API gxObjectDbConfig {
  std::string name;
  std::string path;              // The .gxd file
  std::string journal;           // Default: path + ".jnl"
  std::string access = "read";   // read or write
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

private:
  struct Db;
  Db *find(const std::string &name);

private:
  std::map<std::string, std::unique_ptr<Db> > dbs;
  std::string legacy;
};

} // namespace datareel

#endif // __GX_DATABASE_OBJECT_SERVICE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
