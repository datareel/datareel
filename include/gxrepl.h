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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

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
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "gxstream.h"
#include "gxdrepl.h"

namespace datareel {

// 2 (11/08/2026): databases only (a node refuses flat files), status 3 on a read that changed
const int gxREPL_PROTOCOL = 2;
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

  // Read a config file: "key = value" lines, "[set NAME]" sections,
  // # comments, ${VAR} replaced from the environment. Returns 0, or -1
  // with "err" set.
  int Load(const std::string &fname, std::string &err);
  const gxReplSetConfig *Set(const std::string &name) const;
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
};

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

  unsigned long long applied_records = 0, applied_pages = 0;

private:
  struct SetState_;
  SetState_ *set_state(const std::string &name, std::string &err);
  int writer_active(const gxReplSetConfig &sc, std::string &err);
  int read_states(SetState_ *s, std::map<std::string, gxReplStreamState> &m,
		  std::string &err);
  int write_state(SetState_ *s, const gxReplStreamState &st,
		  std::string &err);
  int apply_batch(SetState_ *s,
		  std::map<std::string, std::pair<unsigned long long,
		  std::map<unsigned long long, const unsigned char *> > > &pages,
		  std::string &err);

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
  const std::string &PeerNode() const { return peer_node; }
  // The server's sets and what it accepts (target, source or both)
  const std::map<std::string, std::string> &PeerSets() const {
    return peer_sets;
  }
  unsigned long long bytes_out = 0, bytes_in = 0, messages = 0;

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

private:
  gxReplConfig cfg;
  std::unique_ptr<gxStream> stream;
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

class GXDLCODE_API gxReplServer {
public:
  explicit gxReplServer(const gxReplConfig &cfg);
  ~gxReplServer();
  int Run(std::string &err); // Listen and serve until Stop()
  void Stop() { stop = true; }

private:
  void handshake(int slot, std::shared_ptr<gxSocket> c);
  void serve(int slot, std::shared_ptr<gxStream> s);
  void write_status();

private:
  gxReplConfig cfg;
  gxReplLocal local;
  gxStreamServer server;
  std::atomic<bool> stop{false};
  std::atomic<int> clients{0};
  std::atomic<int> handshaking{0};
  std::mutex status_lock;
  std::map<std::string, std::string> set_last; // set -> last change note
  std::map<int, std::string> conn_info;
};

// Counters and state of one set, kept by the client and written to the
// status file
struct gxReplSetStatus {
  std::string direction;
  bool ok = false;
  std::string last_error;
  long long last_ok = 0, last_scan = 0, last_verify = 0, last_full_verify = 0;
  unsigned long long records = 0, files_sent = 0, pieces_sent = 0,
    bytes_sent = 0, deletes = 0, verifies = 0;
  std::map<std::string, std::pair<unsigned long long, unsigned long long> >
    streams; // name -> (source next lsn - 1, copy lsn)
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
  void RequestVerify() { verify_now = true; }

private:
  int pass(gxReplRemote &remote, bool once, const std::string &only_set,
	   std::string &err);
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
  std::atomic<bool> verify_now{false};
  std::atomic<bool> connect_verify{false}; // 11/08/2026: verify the ship-log streams after a connect
  bool pass_connected = false;              // 11/08/2026: for the status written during a long pass
  long long last_status_write = 0;          // 11/08/2026
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
