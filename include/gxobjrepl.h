// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxobjrepl.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

gxObjRepl: object replication between two independent object
databases. Only objects travel over the wire (gxStream blocks over TLS
with certificates on both sides), never file pages. The primary and its
copy each keep their own file addresses, their own free space and their
own indexes; the copy is a database in its own right, not an image of
the primary's files.

A node is a folder ROOT:
  ROOT/<db>.gxd, <db>.id.btx   each database: its objects by logical id
  ROOT/state.gxd               role, epoch, sequence numbers, digests
  ROOT/outbox.gxd              primary: one entry per commit, kept until
                               the copy has it
  ROOT/node.jnl                one gxdJournal over every file: a commit
                               (objects, indexes, outbox entry, state)
                               is durable as a whole or not at all
  ROOT/objrepl.status          a running node's status ("key value")
  ROOT/objrepl.alarm           reasons the stream was refused

Objects are identified by a 64-bit id that both nodes share. A remove
leaves a tombstone where the object was, so its id and address are kept
and links to it still resolve; a tombstone goes only with Purge(). An
object may hold links (file addresses of other objects, 8 bytes at a
fixed offset, named in the link map): they travel as ids and each node
stores its own addresses.

  gxObjReplConfig cfg;
  cfg.Load("primary/objrepl.conf");       // or set the members
  gxObjRepl p;
  if(p.Open("primary", cfg, gxObjRepl::PRIMARY) != 0) ... p.Error()
  p.OpenDatabase("events");               // before Begin()
  p.Begin();
  uint64_t id = p.NewId();
  p.Put("events", id, &rec, sizeof(rec));
  p.Commit();                             // durable, in the outbox
  p.Pump();                               // to the copy (or later)

  gxObjRepl c;                            // the copy, another process
  c.Open("copy", cfg_copy, gxObjRepl::COPY);
  c.Serve();                              // until Stop()

Replication is asynchronous: Commit() is durable on the primary, and
the copy has it once Pump() returns 0. If the primary is lost, commits
the copy does not have are lost with it (the primary's status shows them
as lag_seq). After a Takeover() the old primary is fenced off (the copy
and any other copy refuse its older epoch); it comes back with Rejoin(),
which replaces its databases with the new primary's and records how
many of its own commits were dropped (dropped_at_rejoin in its status).
The copy refuses application writes (Begin() and Put() on a copy fail).
Threads: every call takes one lock in the object, so readers (Get,
FindKey, Nearest ...) may run in other threads while Serve() applies
frames (it holds the lock only while it applies one) or while the
application writes; a reader sees uncommitted changes of a transaction
in progress. Pump() holds the lock while it exchanges frames with the
copy (not while it connects). Close() stops a Serve() running in another
thread and waits for it. Stop() may be called from a signal handler.
While a copy receives a full send, reads fail with "a full send is under
way" (InSnapshot() says so).

After a connection drops, the primary's next Pump() connects again and
both sides check names, roles, epoch, sequence and a running digest of
every commit; a change of roles or of history refuses the stream and
writes an alarm. A copy that is behind gets what it misses from the
outbox, or the whole database when the outbox no longer has it.
Takeover() makes a copy the primary at the next epoch.

Indexes are each node's own (gxObjReplConfig::index): B-tree indexes on
up to 32 bytes at an offset and vector indexes (gxVectorIndex) on
floats at an offset, kept in the same transaction as the objects.

Changes:
==============================================================
11/16/2026: New in DataReel 7.4, from the objrepl prototype (the
utils/objrepl tool and its tests run on this class).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_OBJREPL_HPP__
#define __GX_OBJREPL_HPP__

#include "gxdlcode.h"

#include <stdint.h>
#include <string>
#include <vector>
#include <functional>
#include "gxheader.h"

#ifdef __BCC32__
#pragma warn -8022
#pragma warn -8084
#pragma warn -8085
#endif

namespace datareel {

struct GXDLCODE_API gxObjReplConfig {
  std::string name = "node";        // this node, as the peer sees it
  std::string peer_name;            // the expected peer (empty: any)
  // TLS with certificates on both sides: the CA that signs both nodes'
  // certificates, this node's certificate and key, and (copy) the CN
  // of the primary's client certificate
  std::string ca, cert, key, allow_cn;
  std::string copy;                 // primary: the copy, HOST:PORT
  std::string listen_addr = "127.0.0.1"; // copy
  int listen_port = 0;              // copy
  // "DB:OFFSET:TARGET_DB,...": link fields (8-byte addresses). Only the
  // primary's map matters; it is sent to the copy when it connects
  std::string links;
  // "NAME:DB:B:OFFSET:LEN" (B-tree on LEN <= 32 bytes) or
  // "NAME:DB:V:OFFSET:DIM" (vector of DIM floats), comma separated:
  // this node's indexes. Index fields may not overlap link fields
  std::string index;
  bool trim = true;                 // primary: drop outbox entries the copy has
  // seconds without a frame after which the copy drops a connection
  // (a primary that lost power leaves it half open); the primary
  // reconnects before using a connection idle half as long
  int idle_timeout = 120;
  unsigned long long max_lag = 100; // Check(): commits behind
  unsigned long long max_behind = 60; // Check(): seconds
  long min_cert_days = 30;          // Check(): certificate days left
  // Messages (placeholders, index builds, alarms). Default: stderr
  std::function<void(const std::string &)> log;

  // One "key value" setting, key as in the config file (name,
  // peer-name, copy, listen ([ADDR:]PORT), ca, cert, key, allow-cn,
  // links, index, max-lag, max-behind, min-cert-days, trim (0/1),
  // idle-timeout).
  // Returns false (and why) for an unknown key or a bad value
  bool Set(const std::string &key, const std::string &value,
	   std::string *why = 0);
  // A config file: "key value" lines, # comments. Returns false (and
  // why, with the line) when it cannot be read or a line is bad
  bool Load(const std::string &fname, std::string *why = 0);
};

struct GXDLCODE_API gxObjReplHit {
  uint64_t id;
  float distance;
};

class GXDLCODE_API gxObjRepl
{
public:
  enum Role { PRIMARY = 1, COPY = 2 };
  enum Which { LIVE = 1, TOMBSTONES = 2 };

  gxObjRepl();
  ~gxObjRepl();

private:
  gxObjRepl(const gxObjRepl &);
  void operator=(const gxObjRepl &);

public: // Open and close
  // Opens ROOT (created if new) as ROLE. Checks the config (link map,
  // indexes, overlaps) and the certificates (readable; the key not
  // readable by others; within their dates; signed by the CA; key and
  // certificate match). Returns 0; -1 with Error(); 4 when ROOT has the
  // other role (a copy that took over will not run as a copy, and a
  // copy is not written to as a primary until Takeover())
  int Open(const std::string &root, const gxObjReplConfig &cfg, Role role);
  void Close();
  int IsOpen() const;
  std::string Error() const;

public: // Primary: changes, committed together
  // Opens (or creates) a database before Begin(): the journal attaches
  // files only between transactions. Put() and Remove() open the
  // databases of the link map and those already in ROOT themselves
  int OpenDatabase(const std::string &db);
  int Begin();
  // An id no database here has used: the epoch in the high bits
  // (epoch << 40, plus a counter), so after a takeover the new primary
  // never reissues an id the old one may have handed out in commits
  // that never reached it
  uint64_t NewId();
  // An add, or an update of a live object or a tombstone. Link fields
  // hold this node's addresses (AddressOf()); 0 = no link. An object
  // is at most 32 MB, and a commit's changes at most 60 MB in all
  int Put(const std::string &db, uint64_t id, const void *bytes, uint32_t n);
  // A remove: the object becomes a tombstone (an unknown id gets one)
  int Remove(const std::string &db, uint64_t id);
  // A tombstone freed for good (with its forwarding stubs). Refused for
  // a live object, and while a link in a live object still names it
  // (its address would be reused under that link)
  int Purge(const std::string &db, uint64_t id);
  int Commit();                     // durable with its outbox entry
  int Rollback();

public: // Replication
  // Primary: connect (with the reconnect check) if not connected, send
  // what the copy does not have (or a full send), trim the outbox.
  // Returns 0 when the copy has every commit, 1 when it is away or
  // behind, 3 when the stream was refused (an alarm was written)
  int Pump();
  // Primary: Pump() until the copy has every commit or SECONDS pass.
  // Returns 0, 2 (not drained) or 3 (refused)
  int Drain(int seconds);
  // Copy: listen and apply what primaries send until Stop(). Returns 0,
  // or -1 with Error()
  int Serve();
  // Copy: Serve() returns within half a second (async-signal-safe)
  void Stop();

public: // Reads (either role)
  // The bytes of a live object. Returns 0, or -1 (no such live object)
  int Get(const std::string &db, uint64_t id, std::vector<unsigned char> &bytes);
  int IsLive(const std::string &db, uint64_t id);
  FAU_t AddressOf(const std::string &db, uint64_t id); // 0 if none
  uint64_t IdAt(const std::string &db, FAU_t address); // 0 if none
  std::vector<uint64_t> Ids(const std::string &db, Which which = LIVE);
  std::vector<std::string> Databases();
  // Index lookups (this node's indexes): ids whose key starts with KEY
  // (N bytes; the rest of the key zero), or the K nearest vectors
  int FindKey(const std::string &index, const void *key, unsigned n,
	      std::vector<uint64_t> &ids);
  int Nearest(const std::string &index, const float *query, unsigned k,
	      std::vector<gxObjReplHit> &hits);
  int InSnapshot();                // a copy in the middle of a full send

public: // State
  uint64_t Epoch() const;
  uint64_t LastSeq() const;        // primary: the last commit
  uint64_t AppliedSeq() const;     // copy: the last commit applied
  uint64_t CopyApplied() const;    // primary: the copy, as last heard
  long Snapshots() const;          // primary: full sends made
  // Primary (test): hold the add of (DB, ID) back to the next commit,
  // so the copy meets a link to an id it does not have yet
  void TestHoldAdd(const std::string &db, uint64_t id);

public: // Folders, without an open node
  // "objects N digest H": every database's ids and contents (links as
  // ids), the same on two nodes with the same objects. DETAIL:
  // tombstones, stubs and links that name nothing
  static int Digest(const std::string &root, const std::string &links,
		    std::string &line, std::string *detail = 0);
  // Each index against entries rebuilt from the objects, and a digest
  // of its contents. Returns 0 when all are consistent, 5 when not
  static int IndexCheck(const std::string &root, const std::string &index,
			std::string &report);
  // OK / WARN / FAIL in one line from ROOT/objrepl.status and the
  // alarm file: returns 0, 1 or 2
  static int Check(const std::string &root, const gxObjReplConfig &cfg,
		   std::string &line);
  static int ClearAlarm(const std::string &root, std::string &msg);
  static int Status(const std::string &root, std::string &line);
  // A copy becomes primary at the next epoch. Refused for anything but
  // a copy, and while the node is open. Returns 0, or 1 (and msg)
  static int Takeover(const std::string &root, std::string &msg);
  // A node becomes a copy waiting for a full send, which replaces its
  // databases: a former primary after another node took over, or a
  // copy whose history a primary cannot check. For a former primary,
  // msg gives the commits past the last one its copy acknowledged
  // (some may have reached the new primary; the rest are lost).
  // Returns 0, or 1 (and msg). The node must not be open
  static int Rejoin(const std::string &root, std::string &msg);

public:
  struct Impl;

private:
  Impl *p;
};

} // namespace datareel

#ifdef __BCC32__
#pragma warn .8022
#pragma warn .8084
#pragma warn .8085
#endif

#endif // __GX_OBJREPL_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
