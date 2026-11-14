// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxvset.h
// Compiler Used: GCC, MSVC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
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
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Vector index sets (DataReel 7.4, phase D). A gxVectorSet searches many
vector index files (gxvindex.h) as one: a day file each, partitions of
a large index, or both. The files are listed in a manifest, a small
text file "<name>.vixset" next to them:

  # gxVectorSet manifest
  set <name>
  version 1
  spec <the files' spec: the rest of the line, may be empty>
  dim 96
  element f32|f16|i8
  metric l2|cosine|ip
  codec 0
  file <path> nodes=<n> deleted=<n> tag2=<min>..<max>|none crc=<8 hex>
  ...

A file's path is relative to the manifest's folder ('/' separated, no
"..", never absolute) and runs to " nodes=", so it may hold spaces.
nodes, deleted and tag2 (the range of tag 2, the day in yyyymmdd by
convention) are copied from the file's header by AddFile() and
Refresh(); a search with a day range skips a file whose tag2 range is
outside it without opening the file. crc is a CRC-32 of the fields of
the file's header that never change (version, dimension, element type,
metric, M, codec, graph layout, required features, spec): a file
replaced by one that does not match is refused. Lines starting with '#'
and empty lines are ignored; any other unknown line is an error, so a
manifest from a later version is never half read. The manifest is
written to "<name>.vixset.tmp", synced and renamed over the old one.

Files are opened read-only when a search first needs them and kept in a
cache of at most max_open open files (least recently used out first; a
search under way keeps its files). Search() runs over the files with
up to "threads" threads; each file returns its k nearest, and the merged
results, sorted by distance, then file number, then id, are the same
for any number of threads. Every result names its file.

A gxVectorSet is for searching: write to the files with gxVectorIndex
(one writer per file), then Refresh() the manifest. The live file (the
day being written) is searched as it is now: before a search uses an
open file, the file's size and modification time are checked and a
changed file is read again (Reload()). A writer process can instead put
its own open writer index in the set with AttachOpen(): it is searched
directly, never reopened, closed or evicted. A file the manifest lists
that is missing (an archived day already moved away) is skipped and
counted in Missing(), unless gxvSetSearch::strict is set: readers never
need the manifest to be current to the second.

Result order: by distance, then by file number (the file's line in the
manifest), then by id. It is the order of a per-day merge that walks the
days in manifest order and keeps the earlier day on a tie, so a caller
that merged day files by hand gets the same answers.

Changes:
==============================================================
11/14/2026: New in DataReel 7.4 (vector extensions, phase D).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_VECTOR_SET_HPP__
#define __GX_VECTOR_SET_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include "gxvindex.h"

namespace datareel {

const unsigned gxvSetVersion = 1;

// One file of a set, as the manifest lists it
struct gxvSetFile {
  std::string path;                 // Relative to the manifest's folder
  unsigned long long nodes = 0;
  unsigned long long deleted = 0;
  int has_tag2 = 0;                 // 0: unknown ("none"): never skipped
  unsigned tag2_min = 0, tag2_max = 0;
  unsigned crc = 0;
};

struct gxvSetResult {
  long long id;
  float distance;
  unsigned tags[gxvTagCount];
  unsigned file;                    // Index into Files()
};

struct gxvSetSearch {
  unsigned k = 10;
  unsigned ef = 64;
  const gxvFilter *filter = 0;      // Applied in every file
  int day_range = 0;                // 1: only nodes with day_lo <= tag 2 <= day_hi
  unsigned day_lo = 0, day_hi = 0;
  unsigned threads = 1;
  int strict = 0;                   // 1: a missing file is an error, not skipped
};

// What one Search() did (for callers searching from several threads at once,
// where Skipped()/Missing()/Error() belong to the last search of any thread)
struct gxvSetStats {
  unsigned long long skipped = 0, missing = 0;
  std::string error;
};

class GXDLCODE_API gxVectorSet
{
public:
  gxVectorSet();
  ~gxVectorSet();

private:
  gxVectorSet(const gxVectorSet &);
  void operator=(const gxVectorSet &);

public:
  // A new, empty set: the manifest "path" (ending in .vixset) is written
  gxvError Create(const char *path, const char *name, unsigned dim,
		  gxvElementType type = gxvFLOAT32, gxvMetric metric = gxvL2,
		  const char *spec = 0);
  // Read the manifest. max_open: files kept open at a time (at least 1).
  // spec, when given, must be the set's.
  gxvError Open(const char *path, const char *spec = 0, unsigned max_open = 16);
  // One .vix file as a set of one (no manifest: AddFile/RemoveFile/Refresh
  // are refused); its path in results is the file's name
  gxvError OpenFile(const char *vix_path, const char *spec = 0);
  void Close();

  // Add a file (its path relative to the manifest's folder): it is opened,
  // checked against the set and listed. Writes the manifest.
  gxvError AddFile(const char *rel_path);
  gxvError RemoveFile(const char *rel_path);
  // Copy every file's counts and tag 2 range from its header again (after
  // writes); a file that no longer matches its crc is an error. Writes the
  // manifest.
  gxvError Refresh();

  gxvError Search(const float *query, const gxvSetSearch &opt,
		  std::vector<gxvSetResult> &results, gxvSetStats *stats = 0) const;
  // A writer's own open index for a listed file (the live day): searched
  // as it is, never reopened, closed or evicted. The caller keeps it open
  // while the set uses it; 0 detaches it.
  gxvError AttachOpen(const char *rel_path, gxVectorIndex *vix);

  const std::vector<gxvSetFile> &Files() const { return files; }
  std::string FullPath(unsigned file) const;
  const std::string &Name() const { return name; }
  const char *Spec() const { return spec.c_str(); }
  unsigned Dim() const { return dim; }
  gxvElementType ElementType() const { return elem_type; }
  gxvMetric Metric() const { return metric; }
  // Files the last Search() skipped by their tag 2 range without opening
  // them, and files it opened; a line per problem in Error()
  unsigned long long Skipped() const { return skipped; }
  unsigned long long Opened() const { return opened; }
  unsigned long long Missing() const { return missing; }     // listed, not on disk
  unsigned long long Reloaded() const { return reloaded; }   // changed since opened
  const std::string &Error() const { return error; }

  // The header crc a file of a set must have (see above)
  static unsigned HeaderCrc(const gxVectorIndex &vix);
  // 1 if "rel" is a path a manifest may hold (relative, no "..", no empty
  // or "." parts, no backslash or control characters)
  static int SafeRelPath(const std::string &rel);

private:
  gxvError Write();
  gxvError Parse(const std::string &text);
  gxvError Check(const gxVectorIndex &vix, const std::string &rel) const;
  std::shared_ptr<gxVectorIndex> Get(unsigned file, gxvError &err, int &is_missing, std::string &why) const;

private:
  std::string mpath, dirpath, name, spec;
  unsigned dim = 0;
  gxvElementType elem_type = gxvFLOAT32;
  gxvMetric metric = gxvL2;
  unsigned codec = 0;
  unsigned max_open = 16;
  int is_open = 0;
  std::vector<gxvSetFile> files;
  // The open files: least recently used at the back
  mutable std::mutex cache_mu;
  mutable std::list<unsigned> lru;
  struct Entry {
    std::shared_ptr<gxVectorIndex> vix;
    std::list<unsigned>::iterator pos;
    std::string id;                 // size and mtime when it was read
    std::shared_ptr<std::mutex> reload_mu;
  };
  mutable std::map<unsigned, Entry> cache;
  std::map<unsigned, gxVectorIndex *> attached;   // AttachOpen()
  mutable unsigned long long skipped = 0, opened = 0, missing = 0, reloaded = 0;
  int single = 0;                   // OpenFile(): no manifest
  mutable std::string error;
};

} // namespace datareel

#endif // __GX_VECTOR_SET_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
