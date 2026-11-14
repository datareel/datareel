// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxrepl.h
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/07/2026
// Date Last Modified: 11/07/2026
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

Database replication over gxStream (DataReel 7.3), the engine of the
gxrepl utility (utils/gxrepl). One node (the client) dials the other
(the server, gxStreamServer with mutual TLS, default port 2073,
gxsDATA_PORT) and drives everything; the server only answers. For
each replication set (a folder, see gxdrepl.h) the client is either
the source (push) or the copy (pull), so changes can flow both ways
over a connection only one side can open.

Each set is kept the same three ways:
- Commit feed: the ship logs of the journals that write the set are
  sent record by record and applied as whole transactions
  (gxdJournal::ApplyPages() through <root>/.gxrepl/apply.jnl).
- Scan: files no ship log covers are compared by size and modification
  time every scan_interval seconds; a changed file is compared in
  pieces by hash and only the pieces that differ are sent.
- Verify: every file, covered or not, is compared piece by piece on
  connect, after a writer opens its log or a gap, and every
  verify_interval seconds. A covered file is read under the writer's
  journal lock and sent with the log position it matches, so the copy
  never mixes two transactions.

Messages are gxStream blocks (gxSendBlock): uint8 op, uint8 flags
(bit 0 zlib), uint32 length before compression, then the body. All
numbers are native byte order; both nodes must be the same platform
and build (checked in the hello).

Changes:
==============================================================
11/07/2026: New in DataReel 7.3.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_REPLICATION_HPP__
#define __GX_REPLICATION_HPP__

#include "gxdlcode.h"

#include <stdint.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "gxstream.h"
#include "gxdrepl.h"
#include "gxdbserver.h" // 11/08/2026: gxReplServer is a service of the database object server
#include "gxdbmux.h"    // 11/08/2026: protocol 3, many sets over one connection
#include "gxdbobject.h" // 11/08/2026: database objects over the network
#include <set>
#include <thread>

namespace datareel {

// 2 (11/08/2026): databases only (a node refuses flat files), status 3 on a read that changed
// 3 (11/08/2026): the client uses the database object server's multiplex: one channel per set
const int gxREPL_PROTOCOL = 3;
const int gxREPL_DEFAULT_PORT = gxsDATA_PORT; // 2073
const uint32_t gxREPL_PIECE = 64 * 1024;

// 1 if this library compresses replication messages (built with zlib)
GXDLCODE_API int gxReplHasZlib();

// ---------------- Configuration ----------------

struct GXDLCODE_API gxReplSetConfig {
  std::string name;
  std::string root;                // This node's folder for the set
  std::string direction = "push";  // Client: push (it is the source) or pull
  std::string accept = "target";   // Server: target, source or both
  int scan_interval = 10;          // Seconds
  int verify_interval = 3600;      // Files changed in verify_recent_hours
  int verify_recent_hours = 48;
  int full_verify_interval = 86400;// Every file
  std::vector<std::string> exclude;      // Patterns (fnmatch), name or path
  std::vector<std::string> writer_locks; // Relative paths: refuse to change
                                         // the copy while one is locked
  bool deletes = true;             // Remove files the source no longer has
  // 11/08/2026: gxrepl carries DataReel databases only (.gxd and .btx). Other
  // files are never offered or accepted, except the ones named here that
  // belong to a database folder (e.g. commit.bin, sources.json, export/*.gz).
  // Flat files go by DRCM's rsync.
  std::vector<std::string> include;
  // 11/10/2026: A live database folder whose writer keeps a seqlock commit
  // block: "commit_block = commit.bin@8" names the file and the byte offset
  // of its u64 generation (odd while a batch writes the files). A folder
  // holding that file is copied as one unit: a change is found by the
  // generation (an mmap'd commit block keeps its size and mtime), and a copy
  // is accepted only when the source generation was even and the same
  // before and after every file of the folder was read (else it is read
  // again), so the copy always matches one commit. Empty: off.
  std::string commit_block;
  uint32_t commit_gen_offset = 8;
  // 11/10/2026: The ship logs of this set (all streams under
  // <root>/.gxrepl/log) may use at most this many bytes; the oldest
  // segments go first (a copy that needed one is verified whole) and the
  // status warns. 0: no cap.
  unsigned long long ship_log_max_total = 4ULL * 1024 * 1024 * 1024;
  // 11/10/2026: Indexes the copy rebuilds itself (e.g. "by_*.btx"): never
  // sent or deleted by gxrepl, and their pages in log records are skipped.
  // The copy (a commit_block set) runs derived_rebuild ("{dir}" = the
  // folder) once a folder's generation is even and has not changed for
  // derived_delay seconds, under the apply lock; "gxrepl torn" refuses a
  // folder whose derived indexes are missing, stale or failed.
  std::vector<std::string> derived;
  std::string derived_rebuild;
  int derived_delay = 30;
  // 11/11/2026 (r4e): a folder whose indexes stay behind this long (a busy
  // writer's day, never quiet for derived_delay) is rebuilt anyway, under
  // the apply lock, at the generation it holds; 0 = only when quiet.
  // Missing or stale indexes are a warning in "gxrepl torn", not torn: a
  // take over's writer open rebuilds them; a failed rebuild stays torn.
  int derived_busy_rebuild = 900;
  // 11/12/2026 (F7, staged unit copies): the copy of a commit_block unit by its files goes into a copy-on-write
  // stage (<root>/.gxrepl/stage/<unit>/, reflinked from the unit) and is swapped in at its commit block, so the
  // live unit stays whole at its last sealed generation until the new one is whole: a link lost in the middle of
  // a unit copy leaves an older, whole, promotable copy instead of a torn one. A unit over stage_max_bytes, a disk
  // short of room, or a set fed by a ship log is copied in place as before (a WARNING says why).
  bool staged_fill = true;
  unsigned long long stage_max_bytes = 16ULL * 1024 * 1024 * 1024;
};

struct GXDLCODE_API gxReplConfig {
  std::string node;                // This node's name
  std::string role = "client";     // client or server
  // Client
  std::string connect_host;
  int connect_port = gxREPL_DEFAULT_PORT;
  std::string server_name;         // Name on the server certificate
  std::string ssh_tunnel;          // user@host: connect through ssh -L
  std::string ssh_command = "ssh";
  std::vector<std::string> ssh_options;
  int ssh_local_port = 12073;
  std::string ssh_remote = "127.0.0.1:2073";
  int poll_ms = 500;
  // Server
  std::string listen_address;      // Empty: every address
  int listen_port = gxREPL_DEFAULT_PORT;
  std::vector<std::string> allow_cn, allow_fingerprint;
  int max_clients = 4;
  int max_handshakes = 16;          // server: TLS handshakes running at once (unauthenticated)
  int handshake_timeout = 10;        // server: seconds a client gets to finish TLS
  // Both
  std::string cert, key, key_passphrase, ca;
  std::string status_file;
  int timeout = 120;               // Seconds for one message
  size_t max_message = 4 * 1024 * 1024;
  int compress = 1;                // zlib level 1 when built with zlib
  std::vector<gxReplSetConfig> sets;
  // 11/08/2026: The object service: [db NAME] sections (path, journal,
  // access read|write), and the database 6.x style clients get (legacy_db)
  std::vector<gxObjectDbConfig> object_dbs;
  std::string legacy_db;

  // Read a config file: "key = value" lines, "[set NAME]" sections,
  // # comments, ${VAR} replaced from the environment. Returns 0, or -1
  // with "err" set.
  int Load(const std::string &fname, std::string &err);
  const gxReplSetConfig *Set(const std::string &name) const;
  // 11/14/2026 (7.4): vector searches of the object service ([db] vectors =)
  unsigned vector_threads = 2;
  unsigned vector_max_concurrent = 4;
};

// ---------------- Wire types ----------------

struct gxReplFile {
  std::string path;          // Relative to the set root
  unsigned long long size = 0;
  long long mtime_ns = 0;
  uint32_t mode = 0;
  uint8_t covered = 0;       // Written by a ship log stream
};

struct gxReplStream {        // A ship log on the source
  std::string name, log_id, journal;
  unsigned long long first_lsn = 0, next_lsn = 1;
  std::vector<std::string> files;
};

struct gxReplStreamState {   // What the copy has applied
  std::string name, log_id;
  unsigned long long lsn = 0;
};

struct gxReplPiece {
  unsigned long long offset = 0;
  std::vector<unsigned char> data;
};

struct gxReplFileUpdate {
  std::string path;
  unsigned long long size = 0;
  long long mtime_ns = 0;
  uint32_t mode = 0;
  uint8_t last = 1;          // Last part of this file: set mtime and mode
  std::vector<gxReplPiece> pieces;
  // A covered file: the stream position the pieces match, and the log
  // records the copy needs first (from its lsn + 1 to lsn)
  std::string stream, log_id;
  unsigned long long lsn = 0;
  std::vector<unsigned char> records;
};

struct gxReplDiffRequest {
  std::string path;
  uint32_t piece = gxREPL_PIECE;
  std::vector<uint64_t> hashes;  // The copy's piece hashes
  unsigned long long start = 0;  // First offset to compare
  size_t max_bytes = 4 * 1024 * 1024;
  gxReplStreamState copy_state;  // The copy's state of the covering stream
};

struct gxReplDiffResult {
  int status = 0;            // 0 ok, 1 no such file, 2 apply the log first,
                             // 3 changed while read (11/08/2026: read again)
  gxReplFileUpdate update;
  unsigned long long next = 0;
  int more = 0;
  unsigned long long pieces = 0, sent = 0;
};

// ---------------- Operations ----------------

class GXDLCODE_API gxReplSource {
public:
  virtual ~gxReplSource() { }
  virtual int Streams(const std::string &set, std::vector<gxReplStream> &v,
		      std::string &err) = 0;
  // status: 0 ok, 1 "from" is gone (verify instead)
  virtual int ReadLog(const std::string &set, const std::string &stream,
		      unsigned long long from, size_t max_bytes,
		      std::vector<unsigned char> &out,
		      unsigned long long &next, int &status,
		      std::string &err) = 0;
  virtual int Manifest(const std::string &set, std::vector<gxReplFile> &v,
		       std::string &err) = 0;
  virtual int Diff(const std::string &set, const gxReplDiffRequest &rq,
		   gxReplDiffResult &res, std::string &err) = 0;
  // 11/10/2026: The u64 at "offset" of the file (a seqlock commit block's
  // generation; see gxReplSetConfig::commit_block). exists 0: no such file.
  virtual int CommitGen(const std::string &set, const std::string &path,
			uint32_t offset, uint64_t &gen, int &exists,
			std::string &err) {
    (void)set; (void)path; (void)offset; gen = 0; exists = 0;
    err = "commit generations are not supported"; return -1;
  }
};

class GXDLCODE_API gxReplTarget {
public:
  virtual ~gxReplTarget() { }
  virtual int State(const std::string &set,
		    std::vector<gxReplStreamState> &v, std::string &err) = 0;
  virtual int Manifest(const std::string &set, std::vector<gxReplFile> &v,
		       std::string &err) = 0;
  virtual int Hashes(const std::string &set, const std::string &path,
		     uint32_t piece, std::vector<uint64_t> &h,
		     int &exists, std::string &err) = 0;
  virtual int ApplyLog(const std::string &set, const std::string &stream,
		       const std::string &log_id,
		       const std::vector<unsigned char> &records,
		       unsigned long long &lsn, std::string &err) = 0;
  // results: one per update, 0 applied, 1 stale (the copy is not at the
  // update's stream position: apply the log and try again)
  virtual int ApplyFiles(const std::string &set,
			 const std::vector<gxReplFileUpdate> &v,
			 std::vector<int> &results, std::string &err) = 0;
  virtual int Delete(const std::string &set,
		     const std::vector<std::string> &paths,
		     std::string &err) = 0;
  virtual int SetState(const std::string &set, const gxReplStreamState &s,
		       std::string &err) = 0;
  // 11/10/2026: Mark a copy's commit block in progress: write "gen" (odd) at
  // "offset" of the copy's file, synced, before a unit's files are copied
  virtual int MarkGen(const std::string &set, const std::string &path,
		      uint32_t offset, uint64_t gen, std::string &err) {
    (void)set; (void)path; (void)offset; (void)gen;
    err = "marking a commit block is not supported"; return -1;
  }
  // 11/10/2026: Seal a unit copied whole at generation "gen": the copy hashes
  // the folder's database files and keeps them with "gen" (written atomically,
  // after the commit block), so "gxrepl torn" checks the content, not only the
  // generation. SealGen: the generation a folder was sealed at (0: never).
  virtual int SealUnit(const std::string &set, const std::string &folder,
		       uint64_t gen, std::string &err) {
    (void)set; (void)folder; (void)gen; err = "sealing is not supported"; return -1;
  }
  virtual int SealGen(const std::string &set, const std::string &folder,
		      uint64_t &gen, std::string &err) {
    (void)set; (void)folder; gen = 0; err = "sealing is not supported"; return -1;
  }
  virtual int CommitGen(const std::string &set, const std::string &path,
			uint32_t offset, uint64_t &gen, int &exists,
			std::string &err) {
    (void)set; (void)path; (void)offset; gen = 0; exists = 0;
    err = "commit generations are not supported"; return -1;
  }
};

// 11/10/2026: The folders of a commit_block set's copy that are not one
// whole commit: a commit block with an odd generation (a copy in progress,
// or one that stopped half way) or database files with no commit block.
// A fail over must not promote such a copy ("gxrepl torn", exit 1).
// Returns the count of folders checked; "torn" lists "folder: why".
GXDLCODE_API int gxReplTornCopies(const gxReplSetConfig &sc, std::vector<std::string> &torn);
// 11/11/2026 (r4e): and "warn": folders whose derived indexes are behind
// (missing or stale; the take over rebuilds them), not torn
GXDLCODE_API int gxReplTornCopies(const gxReplSetConfig &sc, std::vector<std::string> &torn,
				  std::vector<std::string> &warn);
// 11/12/2026 (F7): make a copy set's units promotable before a take over: a stage whose swap had begun is
// finished (it was whole); any other stage (a unit copy in progress or stopped by a lost link) is discarded,
// the unit staying whole at its sealed generation. "lines" says what was done and what was lost. apply false:
// only says what it would do. Returns 0, -1 on an error ("err"). It takes the set's apply lock (waits up to
// 60 s). It never touches a unit that has no stage.
// 11/13/2026: true when the library was built with -DGXREPL_TEST_HOOKS (the test builds only)
GXDLCODE_API bool gxReplTestHooks();
GXDLCODE_API int gxReplPromote(const gxReplSetConfig &sc, bool apply, std::vector<std::string> &lines,
			       std::string &err);
// 11/10/2026: Rebuild the derived indexes of a commit_block set's copy
// where its generation moved (see gxReplSetConfig::derived). "seen" keeps,
// per folder, the generation and when it was first seen (the quiet time).
// Returns the count of folders rebuilt; "log" gets one line each.
// 11/13/2026: removes the copy's dead folders of a commit_block set (their source folder is gone: no commit block, no
// file gxrepl copies, sealed whole once): the files gxrepl does not copy, the folder, its seal and derived state,
// under the set's apply lock and never while its writer runs. Returns how many; log gets a line for each.
GXDLCODE_API int gxReplSweepDeadCopies(const gxReplSetConfig &sc, std::vector<std::string> &log);

GXDLCODE_API int gxReplDerivedRefresh(const gxReplSetConfig &sc,
				      std::map<std::string, std::pair<uint64_t, long long> > &seen,
				      std::vector<std::string> &log);

// The sets of this node, as source and as copy
class GXDLCODE_API gxReplLocal : public gxReplSource, public gxReplTarget {
public:
  explicit gxReplLocal(const gxReplConfig &cfg);
  ~gxReplLocal();

  int Streams(const std::string &set, std::vector<gxReplStream> &v,
	      std::string &err) override;
  int ReadLog(const std::string &set, const std::string &stream,
	      unsigned long long from, size_t max_bytes,
	      std::vector<unsigned char> &out, unsigned long long &next,
	      int &status, std::string &err) override;
  int Manifest(const std::string &set, std::vector<gxReplFile> &v,
	       std::string &err) override;
  int Diff(const std::string &set, const gxReplDiffRequest &rq,
	   gxReplDiffResult &res, std::string &err) override;

  int State(const std::string &set, std::vector<gxReplStreamState> &v,
	    std::string &err) override;
  int Hashes(const std::string &set, const std::string &path, uint32_t piece,
	     std::vector<uint64_t> &h, int &exists, std::string &err) override;
  int CommitGen(const std::string &set, const std::string &path, uint32_t offset,
		uint64_t &gen, int &exists, std::string &err) override;
  int MarkGen(const std::string &set, const std::string &path, uint32_t offset,
	      uint64_t gen, std::string &err) override;
  int SealUnit(const std::string &set, const std::string &folder, uint64_t gen, std::string &err) override;
  int SealGen(const std::string &set, const std::string &folder, uint64_t &gen, std::string &err) override;
  int ApplyLog(const std::string &set, const std::string &stream,
	       const std::string &log_id,
	       const std::vector<unsigned char> &records,
	       unsigned long long &lsn, std::string &err) override;
  int ApplyFiles(const std::string &set,
		 const std::vector<gxReplFileUpdate> &v,
		 std::vector<int> &results, std::string &err) override;
  int Delete(const std::string &set, const std::vector<std::string> &paths,
	     std::string &err) override;
  int SetState(const std::string &set, const gxReplStreamState &s,
	       std::string &err) override;

  // A relative path is safe for the set: normalized, not in .gxrepl,
  // not excluded, no "databento" folder. Sets "full" on success.
  int SafePath(const gxReplSetConfig &sc, const std::string &rel,
	       std::string &full, std::string &err) const;
  // Finish a batch a crash left in a set's apply journal
  int RecoverAll(std::string &err);
  // 11/08/2026: The retention floor. As the source of a set, note the last
  // lsn a copy applied durably (<root>/.gxrepl/log/<stream>/acked/<copy>),
  // so the writer's ship log keeps what that copy still needs (up to its
  // size cap).
  int WriteAckFloor(const std::string &set, const std::string &stream,
		    const std::string &copy, unsigned long long lsn, std::string &err);

  std::atomic<unsigned long long> applied_records{0}, applied_pages{0}; // 11/08/2026: read by the status

private:
  struct SetState_;
  SetState_ *set_state(const std::string &name, std::string &err);
  int writer_active(const gxReplSetConfig &sc, std::string &err);
  int read_states(SetState_ *s, std::map<std::string, gxReplStreamState> &m,
		  std::string &err);
  int write_state(SetState_ *s, const gxReplStreamState &st,
		  std::string &err);
  // in_lock (11/12/2026): run after the pages are applied, before the apply lock is let go (ApplyLog writes its
  // stream's applied position there, so the seal and the position change together for anyone holding the lock)
  int apply_batch(SetState_ *s,
		  std::map<std::string, std::pair<unsigned long long,
		  std::map<unsigned long long, const unsigned char *> > > &pages,
		  std::string &err, const std::function<int(std::string &)> &in_lock = nullptr,
		  const std::function<int(std::string &)> &pre_lock = nullptr);   // pre_lock (F7 review): once the
		  // apply lock is held, before any page is written (a non-zero return drops the batch)

private:
  gxReplConfig cfg;
  std::map<std::string, SetState_ *> sets;
  std::mutex lock;
};

// A connection to the other node: a gxReplSource, and through
// gxReplRemoteTarget a gxReplTarget
class GXDLCODE_API gxReplRemote : public gxReplSource {
public:
  explicit gxReplRemote(const gxReplConfig &cfg);
  ~gxReplRemote();

  int Connect(std::string &err); // TLS, then the hello
  void Close();
  bool IsOpen() const;
  // 11/08/2026: Protocol 3. ConnectMux(): TLS, the multiplex HELLO, then
  // the gxrepl HELLO on a control channel. OpenSetChannel(): a channel
  // for one set (mode push or pull; the server checks it). UseChannel():
  // this object makes its calls on a channel of another, connected one
  // (one object per set, each used by its own thread).
  int ConnectMux(std::string &err);
  int OpenSetChannel(uint32_t ch, const std::string &set, const std::string &mode,
		     std::string &err);
  void UseChannel(gxReplRemote &connected, uint32_t ch);
  gxdMuxSession *Mux() { return mux; }
  uint32_t Channel() const { return mux_ch; }
  const std::string &PeerNode() const { return peer_node; }
  // The server's sets and what it accepts (target, source or both)
  const std::map<std::string, std::string> &PeerSets() const {
    return peer_sets;
  }
  unsigned long long bytes_out = 0, bytes_in = 0, messages = 0;
  // 11/12/2026 (F4e): a multiplex channel whose request had no reply in time (or whose reply did not parse) is out
  // of step: its late reply would be read as the next request's ("bad reply" forever, the 11/12 stall). It is
  // closed and marked; the set's thread opens a fresh channel (gxReplClient::run_sets).
  bool ChannelBroken() const { return chan_broken; }
  bool chan_broken = false;

  int Streams(const std::string &set, std::vector<gxReplStream> &v,
	      std::string &err) override;
  int ReadLog(const std::string &set, const std::string &stream,
	      unsigned long long from, size_t max_bytes,
	      std::vector<unsigned char> &out, unsigned long long &next,
	      int &status, std::string &err) override;
  int Manifest(const std::string &set, std::vector<gxReplFile> &v,
	       std::string &err) override;
  int Diff(const std::string &set, const gxReplDiffRequest &rq,
	   gxReplDiffResult &res, std::string &err) override;
  int State(const std::string &set, std::vector<gxReplStreamState> &v,
	    std::string &err);
  int TargetManifest(const std::string &set, std::vector<gxReplFile> &v,
		     std::string &err);
  int Hashes(const std::string &set, const std::string &path, uint32_t piece,
	     std::vector<uint64_t> &h, int &exists, std::string &err);
  int CommitGen(const std::string &set, const std::string &path, uint32_t offset,
		uint64_t &gen, int &exists, std::string &err) override;
  int MarkGen(const std::string &set, const std::string &path, uint32_t offset,
	      uint64_t gen, std::string &err);
  int SealUnit(const std::string &set, const std::string &folder, uint64_t gen, std::string &err);
  int SealGen(const std::string &set, const std::string &folder, uint64_t &gen, std::string &err);
  int ApplyLog(const std::string &set, const std::string &stream,
	       const std::string &log_id,
	       const std::vector<unsigned char> &records,
	       unsigned long long &lsn, std::string &err);
  int ApplyFiles(const std::string &set,
		 const std::vector<gxReplFileUpdate> &v,
		 std::vector<int> &results, std::string &err);
  int Delete(const std::string &set, const std::vector<std::string> &paths,
	     std::string &err);
  int SetState(const std::string &set, const gxReplStreamState &s,
	       std::string &err);

private:
  int call(const std::vector<unsigned char> &req,
	   std::vector<unsigned char> &reply, std::string &err);
  int start_tunnel(std::string &err);
  void stop_tunnel();
  int connect_tls(std::string &err);
  int hello(std::string &err);

private:
  gxReplConfig cfg;
  std::unique_ptr<gxStream> stream;
  std::unique_ptr<gxdMuxSession> own_mux; // 11/08/2026: protocol 3
  gxdMuxSession *mux = 0;
  uint32_t mux_ch = 0;
  std::string peer_node;
  std::map<std::string, std::string> peer_sets;
  int tunnel_pid = -1;
  int peer_zlib = 0;
};

// Wraps gxReplRemote so the target-side Manifest() goes to the target
// operation (a node can be both source and copy of different sets).
class GXDLCODE_API gxReplRemoteTarget : public gxReplTarget {
public:
  explicit gxReplRemoteTarget(gxReplRemote &r) : remote(r) { }
  int State(const std::string &set, std::vector<gxReplStreamState> &v,
	    std::string &err) override { return remote.State(set, v, err); }
  int Manifest(const std::string &set, std::vector<gxReplFile> &v,
	       std::string &err) override {
    return remote.TargetManifest(set, v, err);
  }
  int Hashes(const std::string &set, const std::string &path, uint32_t piece,
	     std::vector<uint64_t> &h, int &exists, std::string &err) override {
    return remote.Hashes(set, path, piece, h, exists, err);
  }
  int CommitGen(const std::string &set, const std::string &path, uint32_t offset,
		uint64_t &gen, int &exists, std::string &err) override {
    return remote.CommitGen(set, path, offset, gen, exists, err);
  }
  int MarkGen(const std::string &set, const std::string &path, uint32_t offset,
	      uint64_t gen, std::string &err) override {
    return remote.MarkGen(set, path, offset, gen, err);
  }
  int SealUnit(const std::string &set, const std::string &folder, uint64_t gen, std::string &err) override {
    return remote.SealUnit(set, folder, gen, err);
  }
  int SealGen(const std::string &set, const std::string &folder, uint64_t &gen, std::string &err) override {
    return remote.SealGen(set, folder, gen, err);
  }
  int ApplyLog(const std::string &set, const std::string &stream,
	       const std::string &log_id,
	       const std::vector<unsigned char> &records,
	       unsigned long long &lsn, std::string &err) override {
    return remote.ApplyLog(set, stream, log_id, records, lsn, err);
  }
  int ApplyFiles(const std::string &set,
		 const std::vector<gxReplFileUpdate> &v,
		 std::vector<int> &results, std::string &err) override {
    return remote.ApplyFiles(set, v, results, err);
  }
  int Delete(const std::string &set, const std::vector<std::string> &paths,
	     std::string &err) override {
    return remote.Delete(set, paths, err);
  }
  int SetState(const std::string &set, const gxReplStreamState &s,
	       std::string &err) override {
    return remote.SetState(set, s, err);
  }
private:
  gxReplRemote &remote;
};

// ---------------- Server and client ----------------

// 11/08/2026: The replication service of the DataReel database object
// server (gxdbserver.h). Run() starts a gxDatabaseServer on the config's
// listen address with this service; another program can instead add the
// service to its own gxDatabaseServer (AddService) beside other services.
class GXDLCODE_API gxReplServer : public gxDatabaseService {
public:
  explicit gxReplServer(const gxReplConfig &cfg);
  ~gxReplServer();
  int Run(std::string &err); // Listen and serve until Stop()
  void Stop();

  // gxDatabaseService: a session whose first block is a gxrepl HELLO
  const char *ServiceName() const override { return "replication"; }
  bool Claims(const gxBlockHeader &gx, const std::vector<unsigned char> &body) override;
  void Serve(gxDatabaseSession &s) override;
  // Recover every set's apply journal: call once before serving
  int Prepare(std::string &err) { return local.RecoverAll(err); }
  void WriteStatus() { write_status(); }

  // 11/08/2026: The state of one session (or one multiplex channel)
  struct ReqState {
    bool hello = false, peer_zlib = false;
    std::string who, peer_cn, peer_host;
    gxDatabaseSession *ses = 0;
    std::string only_set;           // A channel serves only its own set
  };

private:
  void write_status();
  int dispatch(uint8_t op, const std::vector<unsigned char> &body,
	       std::vector<unsigned char> &reply, std::string &e, ReqState &st);
  void serve_mux(gxDatabaseSession &ses);

private:
  gxReplConfig cfg;
  gxReplLocal local;
  std::atomic<gxDatabaseServer *> dbs{nullptr}; // The server Run() made (not with AddService)
  std::atomic<bool> stop{false};
  std::mutex status_lock;
  std::map<std::string, std::string> set_last; // set -> last change note
  std::set<gxdMuxSession *> muxes;              // 11/08/2026: for the status
  std::unique_ptr<gxObjectService> objsvc;      // 11/08/2026: [db NAME] sections
};

// Counters and state of one set, kept by the client and written to the
// status file
// 11/08/2026: Each set's thread updates its entry while the client writes
// the status file: the numbers are atomic, and last_error and streams are
// changed and read under m.
struct gxReplSetStatus {
  std::string direction;
  std::atomic<bool> ok{false};
  std::string last_error;
  std::atomic<long long> last_ok{0}, last_scan{0}, last_verify{0}, last_full_verify{0};
  std::atomic<unsigned long long> records{0}, files_sent{0}, pieces_sent{0},
    bytes_sent{0}, deletes{0}, verifies{0};
  std::map<std::string, std::pair<unsigned long long, unsigned long long> >
    streams; // name -> (source next lsn - 1, copy lsn)
  // 11/10/2026: the source's <root>/.gxrepl and its ship logs (bytes), and a
  // warning (the ship logs reached ship_log_max_total)
  std::atomic<unsigned long long> gxrepl_bytes{0}, ship_log_bytes{0};
  std::string warn;                       // kept an hour after the cap was last reached
  long long warn_time = 0;
  std::mutex m;
};

class GXDLCODE_API gxReplClient {
public:
  explicit gxReplClient(const gxReplConfig &cfg);
  ~gxReplClient();
  // Replicate until Stop(). once: one pass with a full verify of every
  // set (or only "only_set"), then return 0 if every set is the same.
  int Run(bool once, const std::string &only_set, std::string &err);
  void Stop() { stop = true; }
  // Force a full verify at the next pass (SIGHUP). A connect verifies only
  // the ship-log streams; plain files get the size/mtime scan unless a full
  // verify is due (full_verify_interval) or asked for (this, or once).
  void RequestVerify() { verify_gen++; }

private:
  int pass(gxReplRemote &remote, bool once, const std::string &only_set,
	   std::string &err);
  int run_sets(gxReplRemote &remote, bool once, const std::string &only_set,
	       std::string &err);
  int sync_unit(gxReplSource &src, gxReplTarget &tgt, const gxReplSetConfig &sc,
		const std::vector<const gxReplFile *> &files, const std::string &cb_path,
		gxReplSetStatus &st, std::string &err);
  int sync_set(gxReplSource &src, gxReplTarget &tgt,
	       const gxReplSetConfig &sc, bool once, std::string &err);
  int sync_file(gxReplSource &src, gxReplTarget &tgt, const std::string &set,
		const std::string &path, const gxReplStream *cover,
		gxReplSetStatus &st, std::string &err);
  int drain(gxReplSource &src, gxReplTarget &tgt, const std::string &set,
	    const gxReplStream &s, gxReplStreamState &ts,
	    gxReplSetStatus &st, bool &verify, std::string &err);
  void write_status(bool connected, const std::string &note);
  unsigned long long work_count() const;

private:
  gxReplConfig cfg;
  gxReplLocal local;
  std::atomic<bool> stop{false};
  // 11/08/2026: Each set runs on its own thread (protocol 3), so the verify
  // requests are per set: a SIGHUP bumps verify_gen, and each set does a
  // full verify when it has not seen the current generation; a connect
  // puts every set in cverify_sets. Both under flag_lock.
  std::atomic<unsigned long> verify_gen{0};
  std::mutex flag_lock;
  std::map<std::string, unsigned long> verify_seen;
  std::set<std::string> cverify_sets;
  std::atomic<bool> pass_connected{false};  // 11/08/2026: for the status written during a long pass
  std::atomic<long long> last_status_write{0}; // 11/08/2026
  std::mutex status_write_lock;             // 11/08/2026: sets write the status from their threads
  std::map<std::string, gxReplSetStatus> status;
  std::map<std::string, std::vector<std::string> > verify_streams;
  long long connected_since = 0;
  std::string peer;
};

} // namespace datareel

#endif // __GX_REPLICATION_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
