// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdrepl.h
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

Ship log for database replication (DataReel 7.3). A gxdJournal with
EnableShipLog() appends every commit to a ship log; gxrepl (gxrepl.h)
reads it and sends the commits to another node over gxStream, where
gxdJournal::ApplyPages() writes them.

A replication set is one folder (the set root). Each journal that
writes files in the set has its own ship log, a "stream":

  <set root>/.gxrepl/log/<stream>/
    log.id                  16 hex digits, new when the folder is made
    writer.lock             held (flock) by the one writer
    journal                 the journal file's path, relative to the root
    files                   the journal's files, relative to the root
    <first lsn, 16 hex>.gxl segments

Segment: a 32 byte header ("GXRSEG01", the log id as 16 hex digits,
uint64 first lsn) and records. Record (native byte order, both nodes
must be the same platform):

  uint32  magic 0x52525847 ("GXRR")
  uint32  type: 1 commit, 2 open (a writer opened the log), 3 gap (an
          append failed, the files may hold changes the log lacks)
  uint64  lsn (1, 2, 3, ... for the life of the log id)
  uint64  time (microseconds since 1970)
  uint32  payload length
  uint32  checksum of the 24 bytes after the magic and the payload
  payload

Commit payload: uint32 number of files, then per file: uint32 name
length, the name (relative to the root, '/' separated), uint64 file
size after the commit, uint32 number of pages, and per page a uint64
page number and gxdPageSize bytes.

A gxdShipLog on its own is not synced (SetSync(0)). 11/08/2026: a
gxdJournal with the ship log enabled syncs each record (fdatasync)
unless its sync level is gxdJOURNAL_SYNC_OFF, and the record is
appended before the commit's pages reach the files, so after a power
loss the log does not lack a commit that the files hold. Every Open()
also writes an open record, and gxrepl answers an open or gap record
by comparing the stream's files with the copy.

Changes:
==============================================================
11/07/2026: New in DataReel 7.3.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_REPLICATION_LOG_HPP__
#define __GX_DATABASE_REPLICATION_LOG_HPP__

#include "gxdlcode.h"

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

namespace datareel {

struct gxdPage;

const uint32_t gxdShipCommit = 1;
const uint32_t gxdShipOpen   = 2;
const uint32_t gxdShipGap    = 3;
// 11/10/2026: a commit record whose payload is compressed: u32 raw length,
// then a zlib stream of the gxdShipCommit payload. The writer uses it when
// the library has zlib and it is smaller (GXDSHIP_COMPRESS=0 turns it off);
// readers take both types, so older logs still read.
const uint32_t gxdShipCommitZ = 4;
const size_t gxdShipRecordHeaderSize = 32;
const size_t gxdShipSegmentHeaderSize = 32;
const unsigned long long gxdShipDefaultSegmentSize = 64ULL * 1024 * 1024;
const unsigned long long gxdShipDefaultMaxBytes = 2048ULL * 1024 * 1024;
const int gxdShipDefaultKeepDays = 7;

// 64-bit hash used by the ship log and gxrepl's file comparisons
GXDLCODE_API uint64_t gxdReplHash(const void *p, size_t n, uint64_t seed = 0);

// One file of a commit, given to gxdShipLog::AppendCommit()
struct gxdShipFile {
  std::string path; // Full path
  unsigned long long size = 0;
  const std::vector<gxdPage *> *pages = 0;
};

// Writer side, owned by a gxdJournal (EnableShipLog())
class GXDLCODE_API gxdShipLog
{
public:
  gxdShipLog();
  ~gxdShipLog();

private:
  gxdShipLog(const gxdShipLog &);
  void operator=(const gxdShipLog &);

public:
  // Open (or make) the stream's log and write an open record. "jname"
  // is the full path of the journal. Returns 0 if successful.
  int Open(const char *set_root, const char *stream, const char *jname);
  void Close();
  int IsOpen() const { return fd >= 0; }
  int AppendCommit(const std::vector<gxdShipFile> &files);

  void SetSync(int on) { sync = on; } // fdatasync each record
  // 11/10/2026: zlib level for commit records (0 off; default 1, or the
  // environment's GXDSHIP_COMPRESS). Bytes before / after, for measuring.
  void SetCompress(int level) { compress = level; }
  // 11/10/2026: files (name or path patterns, fnmatch) whose pages are left
  // out of the records: indexes the copy rebuilds itself (gxrepl "derived").
  // Use it only with "derived" on both sides of the set.
  void SetExclude(const std::vector<std::string> &patterns) { exclude = patterns; }
  // 11/10/2026: the writer's commit block (a memory-mapped seqlock block
  // outside the journal, such as OSA's commit.bin) as it will be once the
  // next commit lands: "full_path" (inside the set root) and its bytes (at
  // most one page; the file is "n" bytes). The next commit record carries
  // it as its last file, so the copy applies the data, the commit block and
  // its generation together. Call it right before each journal Commit().
  void SetCommitBlock(const std::string &full_path, const void *bytes, size_t n);
  unsigned long long RawBytes() const { return raw_bytes; }
  unsigned long long StoredBytes() const { return stored_bytes; }
  void SetSegmentSize(unsigned long long bytes) { seg_size = bytes; }
  void SetRetention(int days, unsigned long long max_bytes) {
    keep_days = days; max_bytes_kept = max_bytes;
  }
  unsigned long long NextLSN() const { return next_lsn; }
  const std::string &LogId() const { return log_id; }
  const std::string &Stream() const { return stream_name; }
  const std::string &Dir() const { return dir; }
  const std::string &Error() const { return error; }
  unsigned long long Appends() const { return appends; }
  unsigned long long Failures() const { return failures; }

  // The stream name gxdJournal::EnableShipLog() uses for a journal path
  // relative to the set root: '/' and other odd characters become '_'.
  static std::string StreamName(const std::string &rel_path);
  // Path of "full" relative to "root" (both real paths), or empty if it
  // is not inside root.
  static std::string RelativePath(const std::string &root,
				  const std::string &full);

private:
  int append(uint32_t type, const unsigned char *payload, size_t len);
  int open_segment();
  void purge();
  int fail(const std::string &msg);
  int write_files_list(const std::vector<std::string> &names);

private:
  int fd;      // Current segment
  int lock_fd; // writer.lock
  std::string root, dir, stream_name, log_id, error;
  unsigned long long next_lsn;
  unsigned long long seg_first; // First lsn of the current segment
  unsigned long long seg_end;   // Bytes in the current segment
  unsigned long long seg_size, max_bytes_kept;
  int keep_days;
  int sync;
  int pending_gap;
  int compress = -1;            // -1: the default (GXDSHIP_COMPRESS, else 1)
  std::vector<std::string> exclude;
  std::string cb_path;          // 11/10/2026: SetCommitBlock()
  std::vector<unsigned char> cb_page;
  unsigned long long cb_size = 0;
  int cb_pending = 0;
  unsigned long long raw_bytes = 0, stored_bytes = 0;
  std::vector<unsigned char> zbuf;
  unsigned long long appends, failures;
  std::vector<std::string> files_list;
  std::vector<unsigned char> buf;
};

// A record inside a buffer of whole records (see gxdShipLogReader)
struct gxdShipRecordView {
  uint32_t type = 0;
  unsigned long long lsn = 0;
  unsigned long long time_us = 0;
  const unsigned char *payload = 0;
  uint32_t len = 0;
};

struct gxdShipCommitFile {
  std::string name;
  unsigned long long size = 0;
  std::vector< std::pair<unsigned long long, const unsigned char *> > pages;
};

// Check and split a buffer of whole records. Returns 0, or -1 if a
// record is damaged (nothing after it is returned).
GXDLCODE_API int gxdParseShipRecords(const unsigned char *p, size_t n,
				     std::vector<gxdShipRecordView> &recs);
// 11/10/2026: Split a commit record (gxdShipCommit or gxdShipCommitZ):
// a compressed payload is inflated into "storage", which the page pointers
// in "files" point into, so keep it while they are used. Returns 0, -1 if
// it is damaged (or compressed and the library has no zlib).
GXDLCODE_API int gxdParseShipCommitRecord(const gxdShipRecordView &r,
					  std::vector<unsigned char> &storage,
					  std::vector<gxdShipCommitFile> &files);
// Split a commit record's payload. Returns 0, or -1 if it is damaged.
GXDLCODE_API int gxdParseShipCommit(const unsigned char *p, size_t n,
				    std::vector<gxdShipCommitFile> &files);

// 11/10/2026: Remove a stream's log (<set_root>/.gxrepl/log/<stream>) once
// nothing needs it: its writer is gone (writer.lock free) and every copy
// that ACKed it (acked/<copy>) has applied its last record. A stream no
// copy has ACKed is kept. Returns 0 removed, 1 kept ("why" says why), -1
// on an error. Call it when the stream's database has moved away (osa_ingest
// does after it archives a day).
GXDLCODE_API int gxdShipLogPurgeStream(const std::string &set_root,
				       const std::string &stream, std::string &why);
// Every stream of set_root whose journal no longer exists there goes through
// gxdShipLogPurgeStream(). Returns the count removed; "removed" and "held"
// (with the reason) list them. gxdShipLog::Open() runs it as a backstop.
GXDLCODE_API int gxdShipLogPurgeOrphans(const std::string &set_root,
					std::vector<std::string> *removed = 0,
					std::vector<std::string> *held = 0);

// 11/10/2026: Keep the ship logs of set_root (every stream) at most
// max_total bytes, removing the oldest segments first (never a stream's
// newest). 0 only measures. Returns the bytes left; "before" the bytes
// found, "removed" the count of segments removed.
GXDLCODE_API unsigned long long gxdShipLogEnforceTotal(const std::string &set_root,
						       unsigned long long max_total,
						       unsigned long long *before = 0,
						       int *removed = 0);

// Reader side (gxrepl): reads a stream's log while the writer appends.
class GXDLCODE_API gxdShipLogReader
{
public:
  struct Info {
    std::string log_id;
    unsigned long long first_lsn = 0; // 0: no records
    unsigned long long next_lsn = 1;  // The lsn the next record gets
    std::string journal;              // Relative to the set root
    std::vector<std::string> files;   // Relative to the set root
  };

public:
  int Open(const std::string &stream_dir);
  int GetInfo(Info &info);
  // Copy whole records with lsn >= "from" into "out", stopping after
  // "max_bytes" (always at least one record when there is one). Sets
  // "next" to the lsn after the last record copied and "count".
  // Returns 0, 1 if "from" is older than the oldest record kept (or
  // the log id changed), 2 if the record at "from" is bigger than
  // "max_bytes" (11/10/2026: not read; the copy resyncs the files instead),
  // -1 on an error.
  int Read(unsigned long long from, size_t max_bytes,
	   std::vector<unsigned char> &out, unsigned long long &next,
	   unsigned long &count);
  const std::string &Error() const { return error; }

private:
  struct Seg { unsigned long long first; std::string path; };
  int list_segments(std::vector<Seg> &segs);
  int scan_end(const std::vector<Seg> &segs, unsigned long long &next);

private:
  std::string dir, log_id, error;
  // 11/10/2026: Two cursors, so finding the log's end (GetInfo) never moves
  // the reading place and a Read() never rescans a segment from its start:
  // where the end scan stopped, and where the last Read() stopped
  std::string end_path, rd_path;
  unsigned long long end_off = 0, end_lsn = 0, rd_off = 0, rd_lsn = 0;
  // A sparse lsn -> offset index of one segment (every 16th record), filled
  // by both walks: a Read() of an older lsn starts at the nearest one
  std::string idx_path;
  std::map<unsigned long long, unsigned long long> idx;
  void index_add(const std::string &path, unsigned long long lsn, unsigned long long off);
};

} // namespace datareel

#endif // __GX_DATABASE_REPLICATION_LOG_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
