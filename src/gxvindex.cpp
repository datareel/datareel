// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxvindex.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
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

The vector index, see gxvindex.h.
*/
// ----------------------------------------------------------- //
#include "gxvindex.h"
#include "gxvkern.h"
#include "gxcrc32.h"

#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <mutex>
#include <queue>
#include <functional>
#include <math.h>

namespace datareel {

static const char gxv_magic[8] = { 'G', 'X', 'V', 'I', 'X', '7', '4', 0 };

// Header offsets in the static area
enum {
  hMAGIC = 0, hVERSION = 8, hDIM = 12, hELEM = 16, hMETRIC = 20, hM = 24,
  hM0 = 28, hEFC = 32, hNTAGS = 36, hNODE_SIZE = 40, hTOP_LEVEL = 44,
  hENTRY = 48, hNODES = 56, hDELETED = 64, hSPEC = 72, hGRAPH = 136,
  // 11/14/2026 (phase D)
  hFEAT_REQ = 140, hFEAT_OPT = 144, hCODEC = 148, hLAYOUT = 152, hPART_ID = 156, hPART_N = 160,
  hTAG_MIN = 168, hTAG_MAX = 184, hRESERVED = 200
};

// Node offsets
enum {
  nID = 0, nFLAGS = 8, nLEVEL = 12, nTAGS = 16, nUPPER = 32, nN0 = 40,
  nNEIGHBORS = 48
};

const unsigned gxvNodeDeleted = 1;
const unsigned gxvUpperMagic = 0x55565847u; // "GXVU"
const unsigned gxvUpperHead = 16;           // owner, magic, level count

typedef std::pair<float, unsigned> gxvPair; // distance, node index

// Links worked out for one insert before any of them is made
struct gxvPlan {
  struct Change {
    size_t node;
    unsigned level;
    std::vector<unsigned> list;
  };
  unsigned level;
  std::vector<std::vector<unsigned> > own; // The new node's list per level
  std::vector<Change> changes;             // Neighbors' new lists
  int new_entry;
};

// Writer-preferring locks. A plain shared_mutex lets a steady stream of
// searches starve a writer (glibc read-write locks prefer readers).
// A writer holds the turnstile while it waits for and holds the
// exclusive lock, so new searches queue behind it; searches only pass
// through the turnstile, so they still run side by side.
namespace {
struct gxvReadLock {
  gxvReadLock(std::mutex &turn, std::shared_mutex &mu) : lock(mu, std::defer_lock) {
    { std::lock_guard<std::mutex> t(turn); }
    lock.lock();
  }
  std::shared_lock<std::shared_mutex> lock;
};
struct gxvWriteLock {
  gxvWriteLock(std::mutex &turn, std::shared_mutex &mu) : t(turn), lock(mu) { }
  std::lock_guard<std::mutex> t;
  std::unique_lock<std::shared_mutex> lock;
};

// Nodes seen by one search, per thread: a mark per node, cleared by
// moving to the next epoch
struct gxvVisited {
  gxvVisited() : epoch(0) { }
  void Start(size_t n) {
    if(mark.size() < n) {
      mark.assign(n + n / 2 + 1024, 0);
      epoch = 0;
    }
    if(++epoch == 0) {
      std::fill(mark.begin(), mark.end(), 0);
      epoch = 1;
    }
  }
  bool Seen(unsigned i) {
    if(mark[i] == epoch) return true;
    mark[i] = epoch;
    return false;
  }
  std::vector<unsigned> mark;
  unsigned epoch;
};
thread_local gxvVisited gxv_visited;

// Decoded node vectors (float16 and int8 indexes): one for the node a
// query is measured against, two for node-to-node distances
thread_local float gxv_buf_q[gxvMaxDim];
thread_local float gxv_buf_a[gxvMaxDim];
thread_local float gxv_buf_b[gxvMaxDim];

// A search result kept in a max heap on (distance, id)
struct gxvCand {
  float d;
  long long id;
  size_t n;
};
struct gxvWorse {
  bool operator()(const gxvCand &a, const gxvCand &b) const {
    if(a.d != b.d) return a.d < b.d;
    return a.id < b.id;
  }
};
}

static inline void gxv_prefetch(const void *p)
{
#if defined(__GNUC__)
  __builtin_prefetch(p);
#else
  (void)p;
#endif
}

template<class T> static inline void put(unsigned char *b, size_t at, T v)
{
  memcpy(b + at, &v, sizeof(T));
}

template<class T> static inline T get(const unsigned char *b, size_t at)
{
  T v;
  memcpy(&v, b + at, sizeof(T));
  return v;
}

static inline unsigned gxv_upper_size(unsigned levels, unsigned m)
{
  return gxvUpperHead + levels * (8 + 8 * m);
}

gxvFilter::gxvFilter() : tag_mask(0), accept(0), accept_data(0)
{
  for(unsigned i = 0; i < gxvTagCount; i++) {
    lo[i] = 0;
    hi[i] = 0xFFFFFFFFu;
  }
}

void gxvFilter::Range(unsigned i, unsigned lo_value, unsigned hi_value)
{
  if(i >= gxvTagCount) return;
  tag_mask |= (1u << i);
  lo[i] = lo_value;
  hi[i] = hi_value;
}

int gxvFilter::Match(long long id, const unsigned *t) const
{
  if(tag_mask) {
    for(unsigned i = 0; i < gxvTagCount; i++) {
      if((tag_mask & (1u << i)) && (t[i] < lo[i] || t[i] > hi[i])) return 0;
    }
  }
  if(accept) return accept(id, t, accept_data) != 0;
  return 1;
}

gxVectorIndex::gxVectorIndex()
{
  is_open = 0;
  read_only = 0;
  Clear();
}

gxVectorIndex::~gxVectorIndex()
{
  Close();
}

void gxVectorIndex::Clear()
{
  static_area = 0;
  dim = 0;
  elem_type = gxvFLOAT32;
  metric = gxvL2;
  m = m0 = ef_construction = node_size = vbytes = top_level = 0;
  entry = 0;
  memset(spec, 0, sizeof(spec));
  node_count = live_count = deleted_count = change_count = 0;
  graph_saved = 0;
  entry_node = -1;
  level_mult = 0;
  features_required = features_optional = codec = graph_layout = 0;   // 11/14/2026 (phase D)
  partition_id = partition_count = 0;
  ResetRanges();
  ranges_dirty = 0;
  InitArrays();
}

// 11/14/2026 (phase D): the tag ranges from the live nodes (Repair(), Merge())
void gxVectorIndex::RecomputeRanges()
{
  features_optional |= gxvOptTagRanges;
  ResetRanges();
  for(size_t n = 0; n < ids.size(); n++)
    if(*live.at(n)) WidenRanges(tags.at(n));
  ranges_dirty = 1;
}

void gxVectorIndex::InitArrays()
{
  vectors.Init((vbytes + 3) / 4);
  ids.Init(1);
  tags.Init(gxvTagCount);
  live.Init(1);
  addresses.Init(1);
  levels.Init(1);
  cnt0.Init(1);
  nbr0.Init(m0);
  upper.Init(1);
  upper_addr.Init(1);
  by_id.clear();
}

const char *gxVectorIndex::ErrorMessage(gxvError err)
{
  switch(err) {
    case gxv_NO_ERROR: return "No error";
    case gxv_NOT_OPEN: return "The vector index is not open";
    case gxv_DB_ERROR: return "Database file error";
    case gxv_BAD_FILE: return "Not a vector index file, or a damaged node";
    case gxv_BAD_VERSION: return "Vector index file version not supported";
    case gxv_BAD_ARG: return "Bad argument";
    case gxv_UNSUPPORTED: return "Element type not supported by this library";
    case gxv_SPEC_MISMATCH: return "The vector spec differs from the file's";
    case gxv_DUPLICATE_ID: return "The id is already in the index";
    case gxv_ID_NOT_FOUND: return "The id is not in the index";
    case gxv_NOT_FINITE: return "The vector has a NaN or infinite value";
    case gxv_READ_ONLY: return "The vector index is open read only";
    case gxv_OUT_OF_RANGE: return "A vector value is too large for the element type";
    case gxv_INDEX_FULL: return "The vector index holds the most nodes a file can (4,294,967,280)";
  }
  return "Unknown vector index error";
}

// P(level >= L) = M^-L, from a 64-bit hash of the id with integer
// thresholds only, so every node and compiler gives the same level.
unsigned gxVectorIndex::LevelFor(long long id, unsigned M)
{
  if(M < 2) M = 2;
  unsigned long long z = (unsigned long long)id + 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  z = z ^ (z >> 31);
  unsigned level = 0;
  unsigned long long t = 0xFFFFFFFFFFFFFFFFULL / M;
  while(level < gxvMaxLevel && z < t) {
    level++;
    t /= M;
  }
  return level;
}

gxvError gxVectorIndex::Create(const char *fname, unsigned d,
			       gxvElementType type, gxvMetric met,
			       unsigned M, const char *s, unsigned efc)
{
  std::lock_guard<std::mutex> w(wmu);
  gxvWriteLock lock(turn, mu);
  if(is_open) {
    db.Close();
    is_open = 0;
  }
  Clear();
  if(!fname || d == 0 || d > gxvMaxDim || M < 2 || M > 128) return gxv_BAD_ARG;
  if(met != gxvL2 && met != gxvCOSINE && met != gxvIP) return gxv_BAD_ARG;
  if(type != gxvFLOAT32 && type != gxvFLOAT16 && type != gxvINT8) return gxv_BAD_ARG;
  if(s && strlen(s) >= gxvSpecSize) return gxv_BAD_ARG;
  if(efc == 0) efc = 200;
  if(efc > 100000) return gxv_BAD_ARG;

  if(db.Create(fname, (FAU_t)gxvHeaderSize) != gxDBASE_NO_ERROR) {
    return gxv_DB_ERROR;
  }
  // The static area starts right after the file header (StaticArea()
  // returns its size)
  static_area = (FAU_t)db.FileHeaderSize();
  dim = d;
  elem_type = type;
  metric = met;
  m = M;
  m0 = 2 * M;
  ef_construction = efc;
  vbytes = CodeBytes(type, dim);
  node_size = nNEIGHBORS + 8 * m0 + vbytes;
  if(s) strcpy(spec, s);
  InitArrays();
  features_optional = gxvOptTagRanges;   // 11/14/2026 (phase D): kept from the start (empty: min > max)
  ResetRanges();
  graph_saved = 1; // An empty graph
  read_only = 0;
  is_open = 1;
  gxvError err = WriteHeader();
  if(err != gxv_NO_ERROR) {
    db.Close();
    is_open = 0;
    return err;
  }
  db.Flush();
  change_count = 1;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::Open(const char *fname, gxDatabaseAccessMode mode,
			     const char *s)
{
  std::lock_guard<std::mutex> w(wmu);
  gxvWriteLock lock(turn, mu);
  if(is_open) {
    db.Close();
    is_open = 0;
  }
  Clear();
  if(!fname) return gxv_BAD_ARG;
  if(db.Open(fname, mode) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  read_only = (mode == gxDBASE_READONLY);
  is_open = 1;
  gxvError err = ReadHeader();
  if(err == gxv_NO_ERROR && s && strcmp(s, spec) != 0) err = gxv_SPEC_MISMATCH;
  if(err == gxv_NO_ERROR) err = LoadNodes();
  if(err != gxv_NO_ERROR) {
    db.Close();
    is_open = 0;
    Clear();
    return err;
  }
  change_count = 1;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::Close()
{
  std::lock_guard<std::mutex> w(wmu);
  gxvWriteLock lock(turn, mu);
  if(!is_open) return gxv_NO_ERROR;
  gxDatabaseError e = db.Close();
  is_open = 0;
  Clear();
  return e == gxDBASE_NO_ERROR ? gxv_NO_ERROR : gxv_DB_ERROR;
}

gxvError gxVectorIndex::Flush()
{
  std::lock_guard<std::mutex> w(wmu);
  if(!is_open) return gxv_NOT_OPEN;
  return db.Flush() == gxDBASE_NO_ERROR ? gxv_NO_ERROR : gxv_DB_ERROR;
}

gxvError gxVectorIndex::Reload()
{
  std::lock_guard<std::mutex> w(wmu);
  gxvWriteLock lock(turn, mu);
  if(!is_open) return gxv_NOT_OPEN;
  unsigned long long changes = change_count;
  // 11/14/2026 (phase D): a reader of a file another process writes: the database's own header first
  db.NoteFileChanged();
  if(db.ReadFileHdr() != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  gxvError err = ReadHeader();
  if(err == gxv_NO_ERROR) err = LoadNodes();
  change_count = changes + 1;
  return err;
}

gxvError gxVectorIndex::WriteHeader()
{
  unsigned char h[gxvHeaderSize];
  memset(h, 0, sizeof(h));
  memcpy(h + hMAGIC, gxv_magic, sizeof(gxv_magic));
  put<unsigned>(h, hVERSION, gxvVersion);
  put<unsigned>(h, hDIM, dim);
  put<unsigned>(h, hELEM, (unsigned)elem_type);
  put<unsigned>(h, hMETRIC, (unsigned)metric);
  put<unsigned>(h, hM, m);
  put<unsigned>(h, hM0, m0);
  put<unsigned>(h, hEFC, ef_construction);
  put<unsigned>(h, hNTAGS, gxvTagCount);
  put<unsigned>(h, hNODE_SIZE, node_size);
  put<unsigned>(h, hTOP_LEVEL, top_level);
  put<long long>(h, hENTRY, entry_node >= 0 ? (long long)*addresses.at((size_t)entry_node) : 0);
  put<unsigned long long>(h, hNODES, node_count);
  put<unsigned long long>(h, hDELETED, deleted_count);
  memcpy(h + hSPEC, spec, gxvSpecSize);
  put<unsigned>(h, hGRAPH, graph_saved ? 1u : 0u);
  put<unsigned>(h, hFEAT_REQ, features_required);
  put<unsigned>(h, hFEAT_OPT, features_optional);
  put<unsigned>(h, hCODEC, codec);
  put<unsigned>(h, hLAYOUT, graph_layout);
  put<unsigned>(h, hPART_ID, partition_id);
  put<unsigned>(h, hPART_N, partition_count);
  for(unsigned i = 0; i < gxvTagCount; i++) {
    put<unsigned>(h, hTAG_MIN + 4 * i, tag_min[i]);
    put<unsigned>(h, hTAG_MAX + 4 * i, tag_max[i]);
  }
  if(db.Write(h, sizeof(h), static_area) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  ranges_dirty = 0;
  return gxv_NO_ERROR;
}

// 11/14/2026 (phase D): tag ranges
void gxVectorIndex::ResetRanges()
{
  for(unsigned i = 0; i < gxvTagCount; i++) { tag_min[i] = 0xFFFFFFFFu; tag_max[i] = 0; }
}

void gxVectorIndex::WidenRanges(const unsigned *t)
{
  if(!(features_optional & gxvOptTagRanges)) return;
  for(unsigned i = 0; i < gxvTagCount; i++) {
    unsigned v = t ? t[i] : 0;
    if(v < tag_min[i]) { tag_min[i] = v; ranges_dirty = 1; }
    if(v > tag_max[i]) { tag_max[i] = v; ranges_dirty = 1; }
  }
}

gxvError gxVectorIndex::WriteRanges()
{
  if(!ranges_dirty) return gxv_NO_ERROR;
  unsigned char r[8 * gxvTagCount];
  for(unsigned i = 0; i < gxvTagCount; i++) {
    put<unsigned>(r, 4 * i, tag_min[i]);
    put<unsigned>(r, 16 + 4 * i, tag_max[i]);
  }
  if(db.Write(r, sizeof(r), static_area + hTAG_MIN) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  ranges_dirty = 0;
  return gxv_NO_ERROR;
}

int gxVectorIndex::TagRange(unsigned i, unsigned &lo, unsigned &hi) const
{
  lo = 0; hi = 0xFFFFFFFFu;
  if(i >= gxvTagCount || !(features_optional & gxvOptTagRanges)) return 0;
  if(tag_min[i] > tag_max[i]) return 0;          // never held a node
  lo = tag_min[i]; hi = tag_max[i];
  return 1;
}

int gxVectorIndex::TagRangeExcludes(unsigned i, unsigned lo, unsigned hi) const
{
  if(i >= gxvTagCount || !(features_optional & gxvOptTagRanges)) return 0;
  if(tag_min[i] > tag_max[i]) return 1;          // empty: nothing can match
  return hi < tag_min[i] || lo > tag_max[i];
}

gxvError gxVectorIndex::SetPartition(unsigned id, unsigned count)
{
  std::lock_guard<std::mutex> w(wmu);
  if(!is_open) return gxv_NOT_OPEN;
  if(read_only) return gxv_READ_ONLY;
  if((count == 0 && id != 0) || (count && id >= count)) return gxv_BAD_ARG;
  partition_id = id; partition_count = count;
  unsigned char p[8];
  put<unsigned>(p, 0, id);
  put<unsigned>(p, 4, count);
  if(db.Write(p, sizeof(p), static_area + hPART_ID) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  change_count++;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::SetFromMembers(int on)
{
  std::lock_guard<std::mutex> w(wmu);
  if(!is_open) return gxv_NOT_OPEN;
  if(read_only) return gxv_READ_ONLY;
  if(on) features_optional |= gxvOptFromMembers; else features_optional &= ~gxvOptFromMembers;
  unsigned char p[4];
  put<unsigned>(p, 0, features_optional);
  if(db.Write(p, sizeof(p), static_area + hFEAT_OPT) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  change_count++;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::WriteCounts()
{
  unsigned char c[16];
  put<unsigned long long>(c, 0, node_count);
  put<unsigned long long>(c, 8, deleted_count);
  if(db.Write(c, sizeof(c), static_area + hNODES) != gxDBASE_NO_ERROR) {
    return gxv_DB_ERROR;
  }
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::WriteEntry()
{
  unsigned char c[12];
  put<unsigned>(c, 0, top_level);
  put<long long>(c, 4, entry_node >= 0 ? (long long)*addresses.at((size_t)entry_node) : 0);
  if(db.Write(c, sizeof(c), static_area + hTOP_LEVEL) != gxDBASE_NO_ERROR) {
    return gxv_DB_ERROR;
  }
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::ReadHeader()
{
  static_area = (FAU_t)db.FileHeaderSize();
  if(db.StaticArea() < (FAU_t)gxvHeaderSize) return gxv_BAD_FILE;
  unsigned char h[gxvHeaderSize];
  if(db.Read(h, sizeof(h), static_area) != gxDBASE_NO_ERROR) return gxv_BAD_FILE;
  if(memcmp(h + hMAGIC, gxv_magic, sizeof(gxv_magic)) != 0) return gxv_BAD_FILE;
  if(get<unsigned>(h, hVERSION) != gxvVersion) return gxv_BAD_VERSION;
  dim = get<unsigned>(h, hDIM);
  elem_type = (gxvElementType)get<unsigned>(h, hELEM);
  metric = (gxvMetric)get<unsigned>(h, hMETRIC);
  m = get<unsigned>(h, hM);
  m0 = get<unsigned>(h, hM0);
  ef_construction = get<unsigned>(h, hEFC);
  node_size = get<unsigned>(h, hNODE_SIZE);
  top_level = get<unsigned>(h, hTOP_LEVEL);
  entry = get<long long>(h, hENTRY);
  node_count = get<unsigned long long>(h, hNODES);
  deleted_count = get<unsigned long long>(h, hDELETED);
  memcpy(spec, h + hSPEC, gxvSpecSize);
  spec[gxvSpecSize - 1] = 0;
  graph_saved = get<unsigned>(h, hGRAPH) == 1;
  // 11/14/2026 (phase D): zero in files of phases A-C (the defaults: no tag ranges)
  features_required = get<unsigned>(h, hFEAT_REQ);
  features_optional = get<unsigned>(h, hFEAT_OPT);
  codec = get<unsigned>(h, hCODEC);
  graph_layout = get<unsigned>(h, hLAYOUT);
  partition_id = get<unsigned>(h, hPART_ID);
  partition_count = get<unsigned>(h, hPART_N);
  for(unsigned i = 0; i < gxvTagCount; i++) {
    tag_min[i] = get<unsigned>(h, hTAG_MIN + 4 * i);
    tag_max[i] = get<unsigned>(h, hTAG_MAX + 4 * i);
  }
  if(!(features_optional & gxvOptTagRanges)) ResetRanges();
  ranges_dirty = 0;
  if(features_required & ~gxvFeaturesKnown) return gxv_UNSUPPORTED;   // a later version's file
  if(codec != gxvCodecPlain || graph_layout != gxvGraphHNSW) return gxv_UNSUPPORTED;
  if(partition_count && partition_id >= partition_count) return gxv_BAD_FILE;
  if(dim == 0 || dim > gxvMaxDim || get<unsigned>(h, hNTAGS) != gxvTagCount ||
     m < 2 || m > 128 || m0 != 2 * m) {
    return gxv_BAD_FILE;
  }
  // A type from a later version: its node layout is not known here
  if(elem_type != gxvFLOAT32 && elem_type != gxvFLOAT16 && elem_type != gxvINT8) {
    return gxv_UNSUPPORTED;
  }
  vbytes = CodeBytes(elem_type, dim);
  if(node_size != nNEIGHBORS + 8 * m0 + vbytes) return gxv_BAD_FILE;
  if(metric != gxvL2 && metric != gxvCOSINE && metric != gxvIP) return gxv_BAD_FILE;
  if(ef_construction == 0 || ef_construction > 100000) ef_construction = 200;
  if(node_count > gxvMaxNodes) return gxv_BAD_FILE;
  InitArrays();
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::LoadNodes()
{
  InitArrays();
  live_count = 0;
  entry_node = -1;
  unsigned long long deleted = 0;
  by_id.reserve((size_t)node_count + 1024);

  // The level 0 lists as block addresses, turned into node indexes once
  // every node's address is known
  std::vector<long long> raw0;
  std::vector<size_t> raw0_at;
  int graph_ok = graph_saved;
  if(graph_ok) {
    raw0.reserve((size_t)node_count * m);
    raw0_at.reserve((size_t)node_count + 1);
  }
  std::vector<unsigned char> b(node_size);
  FAU_t addr = db.FindFirstObject();
  while(addr) {
    __ULWORD__ len = db.ObjectLength(addr);
    if(len >= gxvUpperHead) {
      if(db.Read(b.data(), gxvUpperHead, addr) != gxDBASE_NO_ERROR) return gxv_BAD_FILE;
      if(get<unsigned>(b.data(), 8) == gxvUpperMagic) { // An upper-level block
	addr = db.FindNextObject(addr);
	continue;
      }
    }
    if(len < node_size) return gxv_BAD_FILE;
    if(db.Read(b.data(), node_size, addr) != gxDBASE_NO_ERROR) return gxv_BAD_FILE;
    if(ids.size() >= gxvMaxNodes) return gxv_BAD_FILE;
    long long id = get<long long>(b.data(), nID);
    unsigned flags = get<unsigned>(b.data(), nFLAGS);
    if(flags & ~gxvNodeDeleted) return gxv_BAD_FILE;
    int alive = (flags & gxvNodeDeleted) == 0;
    if(alive && by_id.count(id)) return gxv_BAD_FILE; // Two live nodes
    size_t n = ids.size();
    memcpy(vectors.Push(), b.data() + nNEIGHBORS + 8 * m0, vbytes);
    unsigned *t = tags.Push();
    for(unsigned i = 0; i < gxvTagCount; i++) t[i] = get<unsigned>(b.data(), nTAGS + 4 * i);
    *ids.Push() = id;
    *live.Push() = (unsigned char)alive;
    *addresses.Push() = addr;
    unsigned level = get<unsigned>(b.data(), nLEVEL);
    long long ua = get<long long>(b.data(), nUPPER);
    unsigned c0 = get<unsigned>(b.data(), nN0);
    if(level > gxvMaxLevel || c0 > m0 || (level > 0) != (ua != 0)) {
      graph_ok = 0;
      level = 0;
      ua = 0;
    }
    *levels.Push() = (unsigned char)level;
    *upper_addr.Push() = (FAU_t)ua;
    *cnt0.Push() = 0;
    nbr0.Push();
    upper.Push()->assign((size_t)level * (m + 1), 0);
    if(graph_ok) {
      raw0_at.push_back(raw0.size());
      *cnt0.at(n) = c0;
      for(unsigned i = 0; i < c0; i++) raw0.push_back(get<long long>(b.data(), nNEIGHBORS + 8 * i));
    }
    if(alive) {
      by_id[id] = n;
      live_count++;
    }
    else {
      deleted++;
    }
    addr = db.FindNextObject(addr);
  }
  // The counts in the header are written in the same transaction as the
  // nodes; a difference means the file was changed without the journal.
  if(ids.size() != node_count || deleted != deleted_count) return gxv_BAD_FILE;
  if(graph_ok) {
    raw0_at.push_back(raw0.size());
    if(LoadGraph(raw0, raw0_at) != gxv_NO_ERROR) graph_ok = 0;
  }
  if(!graph_ok) BuildGraph(); // Phase A file, or a damaged graph
  graph_saved = graph_ok;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::LoadGraph(const std::vector<long long> &raw0,
				  const std::vector<size_t> &raw0_at)
{
  const size_t count = ids.size();
  std::vector<std::pair<long long, unsigned> > where(count);
  for(size_t n = 0; n < count; n++) where[n] = std::make_pair((long long)*addresses.at(n), (unsigned)n);
  std::sort(where.begin(), where.end());
  auto find = [&](long long a, unsigned &n) -> bool {
    auto it = std::lower_bound(where.begin(), where.end(), std::make_pair(a, 0u));
    if(it == where.end() || it->first != a) return false;
    n = it->second;
    return true;
  };
  for(size_t n = 0; n < count; n++) {
    unsigned c0 = *cnt0.at(n);
    if(raw0_at[n + 1] - raw0_at[n] != c0) return gxv_BAD_FILE;
    unsigned *dst = nbr0.at(n);
    for(unsigned i = 0; i < c0; i++) {
      if(!find(raw0[raw0_at[n] + i], dst[i]) || dst[i] == n) return gxv_BAD_FILE;
    }
  }
  std::vector<unsigned char> b;
  for(size_t n = 0; n < count; n++) {
    unsigned level = *levels.at(n);
    if(level == 0) continue;
    if(level > top_level) return gxv_BAD_FILE;
    FAU_t ua = *upper_addr.at(n);
    unsigned size = gxv_upper_size(level, m);
    if(db.ObjectLength(ua) < size) return gxv_BAD_FILE;
    b.resize(size);
    if(db.Read(b.data(), size, ua) != gxDBASE_NO_ERROR) return gxv_BAD_FILE;
    if(get<long long>(b.data(), 0) != (long long)*addresses.at(n) ||
       get<unsigned>(b.data(), 8) != gxvUpperMagic || get<unsigned>(b.data(), 12) != level) {
      return gxv_BAD_FILE;
    }
    std::vector<unsigned> &u = *upper.at(n);
    for(unsigned l = 1; l <= level; l++) {
      size_t at = gxvUpperHead + (size_t)(l - 1) * (8 + 8 * m);
      unsigned c = get<unsigned>(b.data(), at);
      if(c > m) return gxv_BAD_FILE;
      size_t base = (size_t)(l - 1) * (m + 1);
      u[base] = c;
      for(unsigned i = 0; i < c; i++) {
	unsigned x;
	if(!find(get<long long>(b.data(), at + 8 + 8 * i), x) || x == n || *levels.at(x) < l) {
	  return gxv_BAD_FILE;
	}
	u[base + 1 + i] = x;
      }
    }
  }
  entry_node = -1;
  if(!entry) {
    // No entry: only right when no node is linked (no live nodes)
    if(top_level != 0 || live_count != 0) return gxv_BAD_FILE;
    for(size_t n = 0; n < count; n++) {
      if(*cnt0.at(n) || *levels.at(n)) return gxv_BAD_FILE;
    }
    return gxv_NO_ERROR;
  }
  unsigned e;
  if(!find(entry, e) || *levels.at(e) != top_level) return gxv_BAD_FILE;
  entry_node = e;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::Check(const float *vec) const
{
  if(!is_open) return gxv_NOT_OPEN;
  if(read_only) return gxv_READ_ONLY;
  if(!vec) return gxv_BAD_ARG;
  if(!gxv_finite(vec, dim)) return gxv_NOT_FINITE;
  return gxv_NO_ERROR;
}

void gxVectorIndex::Prepare(const float *vec, float *out) const
{
  memcpy(out, vec, 4 * (size_t)dim);
  if(metric == gxvCOSINE) gxv_normalize(out, dim);
}

unsigned gxVectorIndex::CodeBytes(gxvElementType type, unsigned d)
{
  switch(type) {
    case gxvFLOAT16: return 2 * d;
    case gxvINT8: return d + 4; // The bytes, then the float scale
    default: return 4 * d;
  }
}

gxvError gxVectorIndex::Encode(const float *p, unsigned char *code) const
{
  switch(elem_type) {
    case gxvFLOAT16:
      for(unsigned i = 0; i < dim; i++) {
	if(!(fabsf(p[i]) <= 65504.0f)) return gxv_OUT_OF_RANGE;
      }
      for(unsigned i = 0; i < dim; i++) {
	unsigned short h = gxv_f16_from_float(p[i]);
	memcpy(code + 2 * (size_t)i, &h, 2);
      }
      return gxv_NO_ERROR;
    case gxvINT8: {
      float s = gxv_i8_encode(p, (signed char *)code, dim);
      memcpy(code + dim, &s, 4);
      return gxv_NO_ERROR;
    }
    default:
      memcpy(code, p, 4 * (size_t)dim);
      return gxv_NO_ERROR;
  }
}

void gxVectorIndex::Decode(const unsigned char *code, float *out) const
{
  switch(elem_type) {
    case gxvFLOAT16:
      gxv_f16_decode((const unsigned short *)code, out, dim); // Reads with memcpy: any alignment
      break;
    case gxvINT8: {
      float s;
      memcpy(&s, code + dim, 4);
      gxv_i8_decode((const signed char *)code, s, out, dim);
      // Cosine: back to unit length. Rounding each value to a step leaves
      // the decoded vector a little off unit length, which skews 1 - dot
      // by the length error. Part of the decode, so every distance,
      // insert and rebuild uses the same vector.
      if(metric == gxvCOSINE) gxv_normalize(out, dim);
      break;
    }
    default:
      memcpy(out, code, 4 * (size_t)dim);
      break;
  }
}

float gxVectorIndex::DistTo(const float *q, size_t n) const
{
  if(elem_type == gxvFLOAT16) {
    // Decoded on the fly: the same bits as Distance(q, decoded vector)
    const unsigned short *h = (const unsigned short *)Code(n);
    switch(metric) {
      case gxvCOSINE: return 1.0f - gxv_dot_f16(q, h, dim);
      case gxvIP: return -gxv_dot_f16(q, h, dim);
      default: return gxv_l2sq_f16(q, h, dim);
    }
  }
  return Distance(q, Vec(n, gxv_buf_q));
}

float gxVectorIndex::Distance(const float *a, const float *b) const
{
  switch(metric) {
    case gxvCOSINE: return 1.0f - gxv_dot(a, b, dim);
    case gxvIP: return -gxv_dot(a, b, dim);
    default: return gxv_l2sq(a, b, dim);
  }
}

// ----------------------------------------------------------- //
// Graph
// ----------------------------------------------------------- //

unsigned gxVectorIndex::ListOf(size_t n, unsigned level, const unsigned *&list) const
{
  if(level == 0) {
    list = nbr0.at(n);
    return *cnt0.at(n);
  }
  const std::vector<unsigned> &u = *upper.at(n);
  size_t base = (size_t)(level - 1) * (m + 1);
  if(base + m + 1 > u.size()) {
    list = 0;
    return 0;
  }
  list = &u[base + 1];
  return u[base];
}

void gxVectorIndex::SetList(size_t n, unsigned level, const unsigned *list, unsigned count)
{
  if(level == 0) {
    *cnt0.at(n) = count;
    if(count) memcpy(nbr0.at(n), list, 4 * (size_t)count);
    return;
  }
  std::vector<unsigned> &u = *upper.at(n);
  size_t base = (size_t)(level - 1) * (m + 1);
  u[base] = count;
  if(count) memcpy(&u[base + 1], list, 4 * (size_t)count);
}

// The ef nodes closest to q at one level, starting from eps; eps is
// replaced by the result, closest first. Deleted nodes take part: they
// are still stepping stones.
void gxVectorIndex::SearchLayer(const float *q, std::vector<gxvPair> &eps,
				unsigned ef, unsigned level) const
{
  gxvVisited &vis = gxv_visited;
  vis.Start(ids.size() + 1);
  std::priority_queue<gxvPair, std::vector<gxvPair>, std::greater<gxvPair> > cand;
  std::priority_queue<gxvPair> top;
  for(const gxvPair &e : eps) {
    if(vis.Seen(e.second)) continue;
    cand.push(e);
    top.push(e);
    if(top.size() > ef) top.pop();
  }
  while(!cand.empty()) {
    gxvPair c = cand.top();
    if(top.size() >= ef && c > top.top()) break;
    cand.pop();
    const unsigned *list;
    unsigned cnt = ListOf(c.second, level, list);
    for(unsigned i = 0; i < cnt; i++) gxv_prefetch(Code(list[i]));
    for(unsigned i = 0; i < cnt; i++) {
      unsigned x = list[i];
      if(vis.Seen(x)) continue;
      gxvPair p(DistTo(q, x), x);
      if(top.size() < ef || p < top.top()) {
	cand.push(p);
	top.push(p);
	if(top.size() > ef) top.pop();
      }
    }
  }
  eps.resize(top.size());
  for(size_t i = top.size(); i > 0; i--) {
    eps[i - 1] = top.top();
    top.pop();
  }
}

// Keep at most "keep" candidates (distances to one point), each closer
// to that point than to any candidate already kept, so the links spread
// out instead of all going one way. newn / q stand for the node being
// inserted when it is not in the arrays yet.
void gxVectorIndex::Heuristic(std::vector<gxvPair> &cands, unsigned keep,
			      size_t newn, const float *q) const
{
  std::sort(cands.begin(), cands.end());
  if(cands.size() <= keep) return;
  std::vector<gxvPair> kept;
  kept.reserve(keep);
  for(const gxvPair &c : cands) {
    if(kept.size() >= keep) break;
    const float *cv = c.second == newn ? q : Vec(c.second, gxv_buf_a);
    int good = 1;
    for(const gxvPair &r : kept) {
      const float *rv = r.second == newn ? q : Vec(r.second, gxv_buf_b);
      if(Distance(cv, rv) < c.first) {
	good = 0;
	break;
      }
    }
    if(good) kept.push_back(c);
  }
  cands.swap(kept);
}

void gxVectorIndex::PlanInsert(size_t newn, const float *q, unsigned level,
			       gxvPlan &plan) const
{
  plan.level = level;
  plan.own.assign(level + 1, std::vector<unsigned>());
  plan.changes.clear();
  plan.new_entry = 0;
  if(entry_node < 0) {
    plan.new_entry = 1;
    return;
  }
  unsigned cur = (unsigned)entry_node;
  float curd = DistTo(q, cur);
  for(unsigned l = top_level; l > level; l--) {
    int changed = 1;
    while(changed) {
      changed = 0;
      const unsigned *list;
      unsigned cnt = ListOf(cur, l, list);
      for(unsigned i = 0; i < cnt; i++) {
	float d = DistTo(q, list[i]);
	if(d < curd || (d == curd && list[i] < cur)) {
	  curd = d;
	  cur = list[i];
	  changed = 1;
	}
      }
    }
  }
  std::vector<gxvPair> eps(1, gxvPair(curd, cur));
  unsigned efc = ef_construction > m ? ef_construction : m;
  for(int l = (int)(level < top_level ? level : top_level); l >= 0; l--) {
    SearchLayer(q, eps, efc, (unsigned)l);
    std::vector<gxvPair> sel = eps;
    Heuristic(sel, m, newn, q);
    std::vector<unsigned> &own = plan.own[l];
    for(const gxvPair &s : sel) own.push_back(s.second);
    unsigned mmax = l == 0 ? m0 : m;
    for(const gxvPair &s : sel) {
      gxvPlan::Change ch;
      ch.node = s.second;
      ch.level = (unsigned)l;
      const unsigned *list;
      unsigned cnt = ListOf(s.second, (unsigned)l, list);
      if(cnt < mmax) {
	ch.list.assign(list, list + cnt);
	ch.list.push_back((unsigned)newn);
      }
      else {
	// ev in buffer a, each neighbor in buffer b; Heuristic() below
	// reuses both, after ev is no longer needed
	const float *ev = Vec(s.second, gxv_buf_a);
	std::vector<gxvPair> c;
	c.reserve(cnt + 1);
	for(unsigned i = 0; i < cnt; i++) c.push_back(gxvPair(Distance(ev, Vec(list[i], gxv_buf_b)), list[i]));
	c.push_back(gxvPair(s.first, (unsigned)newn));
	Heuristic(c, mmax, newn, q);
	for(const gxvPair &x : c) ch.list.push_back(x.second);
      }
      plan.changes.push_back(ch);
    }
  }
  if(level > top_level) plan.new_entry = 1;
}

void gxVectorIndex::ApplyPlan(size_t newn, const gxvPlan &plan)
{
  for(unsigned l = 0; l < plan.own.size(); l++) {
    SetList(newn, l, plan.own[l].data(), (unsigned)plan.own[l].size());
  }
  for(const gxvPlan::Change &ch : plan.changes) {
    SetList(ch.node, ch.level, ch.list.data(), (unsigned)ch.list.size());
  }
  if(plan.new_entry) {
    entry_node = (long long)newn;
    top_level = plan.level;
  }
}

void gxVectorIndex::BuildGraph()
{
  const size_t count = ids.size();
  entry_node = -1;
  top_level = 0;
  for(size_t n = 0; n < count; n++) {
    unsigned level = *live.at(n) ? LevelFor(*ids.at(n), m) : 0;
    *levels.at(n) = (unsigned char)level;
    *cnt0.at(n) = 0;
    upper.at(n)->assign((size_t)level * (m + 1), 0);
  }
  gxvPlan plan;
  std::vector<float> qb(dim);
  for(size_t n = 0; n < count; n++) {
    if(!*live.at(n)) continue;
    // The decoded node, as Insert() measured it
    PlanInsert(n, Vec(n, qb.data()), *levels.at(n), plan);
    ApplyPlan(n, plan);
  }
}

// The whole node block, and its upper-level block, from memory
gxvError gxVectorIndex::WriteNode(size_t n)
{
  std::vector<unsigned char> b(node_size, 0);
  unsigned level = *levels.at(n);
  put<long long>(b.data(), nID, *ids.at(n));
  put<unsigned>(b.data(), nFLAGS, *live.at(n) ? 0 : gxvNodeDeleted);
  put<unsigned>(b.data(), nLEVEL, level);
  const unsigned *t = tags.at(n);
  for(unsigned i = 0; i < gxvTagCount; i++) put<unsigned>(b.data(), nTAGS + 4 * i, t[i]);
  put<long long>(b.data(), nUPPER, level ? (long long)*upper_addr.at(n) : 0);
  const unsigned *list;
  unsigned c = ListOf(n, 0, list);
  put<unsigned>(b.data(), nN0, c);
  for(unsigned i = 0; i < c; i++) put<long long>(b.data(), nNEIGHBORS + 8 * i, (long long)*addresses.at(list[i]));
  memcpy(b.data() + nNEIGHBORS + 8 * m0, Code(n), vbytes);
  if(db.Write(b.data(), node_size, *addresses.at(n)) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  if(level == 0) return gxv_NO_ERROR;
  std::vector<unsigned char> u(gxv_upper_size(level, m), 0);
  put<long long>(u.data(), 0, (long long)*addresses.at(n));
  put<unsigned>(u.data(), 8, gxvUpperMagic);
  put<unsigned>(u.data(), 12, level);
  for(unsigned l = 1; l <= level; l++) {
    size_t at = gxvUpperHead + (size_t)(l - 1) * (8 + 8 * m);
    c = ListOf(n, l, list);
    put<unsigned>(u.data(), at, c);
    for(unsigned i = 0; i < c; i++) put<long long>(u.data(), at + 8 + 8 * i, (long long)*addresses.at(list[i]));
  }
  if(db.Write(u.data(), u.size(), *upper_addr.at(n)) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  return gxv_NO_ERROR;
}

// ----------------------------------------------------------- //
// Writes
// ----------------------------------------------------------- //

gxvError gxVectorIndex::InsertWith(long long id, const unsigned char *code,
				   const float *q, const unsigned *t, long long replace)
{
  // 1. Work out the links under the shared lock: searches keep running,
  // and no other writer can change the graph (wmu is held).
  gxvPlan plan;
  size_t newn;
  {
    gxvReadLock r(turn, mu);
    newn = ids.size();
    if(newn >= gxvMaxNodes) return gxv_INDEX_FULL;   // 11/14/2026
    PlanInsert(newn, q, LevelFor(id, m), plan);
  }
  const unsigned level = plan.level;

  // 2. Write the file (searches never read it)
  FAU_t addr = db.Alloc((__ULWORD__)node_size);
  if(!addr) return gxv_DB_ERROR;
  FAU_t uaddr = 0;
  if(level > 0) {
    uaddr = db.Alloc((__ULWORD__)gxv_upper_size(level, m));
    if(!uaddr) return gxv_DB_ERROR;
  }
  auto fau = [&](unsigned x) -> long long {
    return x == newn ? (long long)addr : (long long)*addresses.at(x);
  };
  std::vector<unsigned char> b(node_size, 0);
  put<long long>(b.data(), nID, id);
  put<unsigned>(b.data(), nFLAGS, 0);
  put<unsigned>(b.data(), nLEVEL, level);
  for(unsigned i = 0; i < gxvTagCount; i++) put<unsigned>(b.data(), nTAGS + 4 * i, t ? t[i] : 0);
  put<long long>(b.data(), nUPPER, (long long)uaddr);
  put<unsigned>(b.data(), nN0, (unsigned)plan.own[0].size());
  for(size_t i = 0; i < plan.own[0].size(); i++) put<long long>(b.data(), nNEIGHBORS + 8 * i, fau(plan.own[0][i]));
  memcpy(b.data() + nNEIGHBORS + 8 * m0, code, vbytes);
  if(db.Write(b.data(), node_size, addr) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  if(level > 0) {
    std::vector<unsigned char> u(gxv_upper_size(level, m), 0);
    put<long long>(u.data(), 0, (long long)addr);
    put<unsigned>(u.data(), 8, gxvUpperMagic);
    put<unsigned>(u.data(), 12, level);
    for(unsigned l = 1; l <= level; l++) {
      size_t at = gxvUpperHead + (size_t)(l - 1) * (8 + 8 * m);
      const std::vector<unsigned> &own = plan.own[l];
      put<unsigned>(u.data(), at, (unsigned)own.size());
      for(size_t i = 0; i < own.size(); i++) put<long long>(u.data(), at + 8 + 8 * i, fau(own[i]));
    }
    if(db.Write(u.data(), u.size(), uaddr) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  }
  // The neighbors' new lists. A graph built at Open() (not in the file)
  // is built again at the next Open(), so its lists are not written.
  if(graph_saved) {
    std::vector<unsigned char> lb(8 + 8 * (size_t)m0);
    for(const gxvPlan::Change &ch : plan.changes) {
      unsigned slots = ch.level == 0 ? m0 : m;
      size_t len = 8 + 8 * (size_t)slots;
      memset(lb.data(), 0, len);
      put<unsigned>(lb.data(), 0, (unsigned)ch.list.size());
      for(size_t i = 0; i < ch.list.size(); i++) put<long long>(lb.data(), 8 + 8 * i, fau(ch.list[i]));
      FAU_t at = ch.level == 0 ? *addresses.at(ch.node) + nN0 :
	*upper_addr.at(ch.node) + gxvUpperHead + (FAU_t)(ch.level - 1) * (8 + 8 * m);
      if(db.Write(lb.data(), len, at) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
    }
  }
  if(replace >= 0) {
    unsigned char f[4];
    put<unsigned>(f, 0, gxvNodeDeleted);
    if(db.Write(f, sizeof(f), *addresses.at((size_t)replace) + nFLAGS) != gxDBASE_NO_ERROR) {
      return gxv_DB_ERROR;
    }
  }
  unsigned char c[16];
  put<unsigned long long>(c, 0, node_count + 1);
  put<unsigned long long>(c, 8, deleted_count + (replace >= 0 ? 1 : 0));
  if(db.Write(c, sizeof(c), static_area + hNODES) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  // 11/14/2026 (phase D): the tag ranges with the counts, in the caller's transaction
  WidenRanges(t);
  if(WriteRanges() != gxv_NO_ERROR) return gxv_DB_ERROR;
  if(plan.new_entry) {
    unsigned char e[12];
    put<unsigned>(e, 0, level);
    put<long long>(e, 4, (long long)addr);
    if(db.Write(e, sizeof(e), static_area + hTOP_LEVEL) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  }

  // 3. Link it: the only part that holds searches back
  gxvWriteLock x(turn, mu);
  memcpy(vectors.Push(), code, vbytes);
  *ids.Push() = id;
  unsigned *nt = tags.Push();
  for(unsigned i = 0; i < gxvTagCount; i++) nt[i] = t ? t[i] : 0;
  *live.Push() = 1;
  *addresses.Push() = addr;
  *levels.Push() = (unsigned char)level;
  *cnt0.Push() = 0;
  nbr0.Push();
  upper.Push()->assign((size_t)level * (m + 1), 0);
  *upper_addr.Push() = uaddr;
  ApplyPlan(newn, plan);
  if(replace >= 0) {
    *live.at((size_t)replace) = 0;
    deleted_count++;
    live_count--;
  }
  by_id[id] = newn;
  node_count++;
  live_count++;
  change_count++;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::Insert(long long id, const float *vec,
			       const unsigned *t)
{
  std::lock_guard<std::mutex> w(wmu);
  gxvError err = Check(vec);
  if(err != gxv_NO_ERROR) return err;
  if(by_id.count(id)) return gxv_DUPLICATE_ID;
  // Normalize (cosine), store in the element type, and link the node by
  // its decoded vector, the one every later distance uses
  std::vector<float> prepared(dim), q(dim);
  std::vector<unsigned char> code(vbytes);
  Prepare(vec, prepared.data());
  err = Encode(prepared.data(), code.data());
  if(err != gxv_NO_ERROR) return err;
  Decode(code.data(), q.data());
  return InsertWith(id, code.data(), q.data(), t, -1);
}

gxvError gxVectorIndex::Update(long long id, const float *vec,
			       const unsigned *t)
{
  std::lock_guard<std::mutex> w(wmu);
  gxvError err = Check(vec);
  if(err != gxv_NO_ERROR) return err;
  auto it = by_id.find(id);
  if(it == by_id.end()) return gxv_ID_NOT_FOUND;
  size_t n = it->second;
  std::vector<float> prepared(dim), q(dim);
  std::vector<unsigned char> code(vbytes);
  Prepare(vec, prepared.data());
  err = Encode(prepared.data(), code.data());
  if(err != gxv_NO_ERROR) return err;
  if(memcmp(code.data(), Code(n), vbytes) != 0) { // The stored form changes
    // A new vector moves the node in the graph: a new node replaces it
    unsigned keep[gxvTagCount];
    if(!t) {
      memcpy(keep, tags.at(n), sizeof(keep));
      t = keep;
    }
    Decode(code.data(), q.data());
    return InsertWith(id, code.data(), q.data(), t, (long long)n);
  }
  if(!t) return gxv_NO_ERROR;
  unsigned char tb[4 * gxvTagCount];
  for(unsigned i = 0; i < gxvTagCount; i++) put<unsigned>(tb, 4 * i, t[i]);
  if(db.Write(tb, sizeof(tb), *addresses.at(n) + nTAGS) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  WidenRanges(t);                                  // 11/14/2026 (phase D)
  if(WriteRanges() != gxv_NO_ERROR) return gxv_DB_ERROR;
  gxvWriteLock x(turn, mu);
  memcpy(tags.at(n), t, sizeof(tb));
  change_count++;
  return gxv_NO_ERROR;
}

// ----------------------------------------------------------- //
// Vector members
// ----------------------------------------------------------- //

static const char gxv_member_magic[4] = { 'G', 'X', 'V', 'M' };

unsigned gxVectorIndex::SpecCRC() const
{
  return (unsigned)calcCRC32(spec, (unsigned)strlen(spec));
}

gxvError gxVectorIndex::MakeMember(const float *vec, std::vector<unsigned char> &member) const
{
  if(!is_open) return gxv_NOT_OPEN;
  if(!vec) return gxv_BAD_ARG;
  if(!gxv_finite(vec, dim)) return gxv_NOT_FINITE;
  std::vector<float> prepared(dim);
  Prepare(vec, prepared.data());
  member.assign(MemberSize(), 0);
  memcpy(member.data(), gxv_member_magic, 4);
  member[4] = (unsigned char)elem_type;
  put<unsigned short>(member.data(), 6, (unsigned short)dim);
  put<unsigned>(member.data(), 8, SpecCRC());
  return Encode(prepared.data(), member.data() + gxvMemberHead);
}

gxvError gxVectorIndex::CheckMember(const unsigned char *m, unsigned len) const
{
  if(!is_open) return gxv_NOT_OPEN;
  if(!m || len != MemberSize() || memcmp(m, gxv_member_magic, 4) != 0) return gxv_BAD_ARG;
  if(m[4] != (unsigned char)elem_type || get<unsigned short>(m, 6) != dim) return gxv_BAD_ARG;
  if(get<unsigned>(m, 8) != SpecCRC()) return gxv_SPEC_MISMATCH;
  std::vector<float> v(dim); // A damaged record must not put NaN in the index
  Decode(m + gxvMemberHead, v.data());
  if(!gxv_finite(v.data(), dim)) return gxv_NOT_FINITE;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::MemberVector(const unsigned char *m, unsigned len, float *vec) const
{
  gxvError err = CheckMember(m, len);
  if(err != gxv_NO_ERROR) return err;
  if(vec) Decode(m + gxvMemberHead, vec);
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::PutCode(long long id, const unsigned char *code, const unsigned *t,
				int update)
{
  if(read_only) return gxv_READ_ONLY;
  std::vector<float> q(dim);
  Decode(code, q.data());
  auto it = by_id.find(id);
  if(!update) {
    if(it != by_id.end()) return gxv_DUPLICATE_ID;
    return InsertWith(id, code, q.data(), t, -1);
  }
  if(it == by_id.end()) return gxv_ID_NOT_FOUND;
  size_t n = it->second;
  if(memcmp(code, Code(n), vbytes) != 0) {
    unsigned keep[gxvTagCount];
    if(!t) {
      memcpy(keep, tags.at(n), sizeof(keep));
      t = keep;
    }
    return InsertWith(id, code, q.data(), t, (long long)n);
  }
  if(!t) return gxv_NO_ERROR;
  unsigned char tb[4 * gxvTagCount];
  for(unsigned i = 0; i < gxvTagCount; i++) put<unsigned>(tb, 4 * i, t[i]);
  if(db.Write(tb, sizeof(tb), *addresses.at(n) + nTAGS) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  WidenRanges(t);                                  // 11/14/2026 (phase D)
  if(WriteRanges() != gxv_NO_ERROR) return gxv_DB_ERROR;
  gxvWriteLock x(turn, mu);
  memcpy(tags.at(n), t, sizeof(tb));
  change_count++;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::InsertMember(long long id, const unsigned char *m, unsigned len,
				     const unsigned *t)
{
  std::lock_guard<std::mutex> w(wmu);
  gxvError err = CheckMember(m, len);
  if(err != gxv_NO_ERROR) return err;
  return PutCode(id, m + gxvMemberHead, t, 0);
}

gxvError gxVectorIndex::UpdateMember(long long id, const unsigned char *m, unsigned len,
				     const unsigned *t)
{
  std::lock_guard<std::mutex> w(wmu);
  gxvError err = CheckMember(m, len);
  if(err != gxv_NO_ERROR) return err;
  return PutCode(id, m + gxvMemberHead, t, 1);
}

int gxVectorIndex::MemberMatches(long long id, const unsigned char *m, unsigned len) const
{
  if(CheckMember(m, len) != gxv_NO_ERROR) return 0;
  gxvReadLock lock(turn, mu);
  auto it = by_id.find(id);
  if(it == by_id.end()) return 0;
  return memcmp(m + gxvMemberHead, Code(it->second), vbytes) == 0;
}

gxvError gxVectorIndex::Rename(long long old_id, long long new_id)
{
  std::lock_guard<std::mutex> w(wmu);
  if(!is_open) return gxv_NOT_OPEN;
  if(read_only) return gxv_READ_ONLY;
  auto it = by_id.find(old_id);
  if(it == by_id.end()) return gxv_ID_NOT_FOUND;
  if(old_id == new_id) return gxv_NO_ERROR;
  if(by_id.count(new_id)) return gxv_DUPLICATE_ID;
  size_t n = it->second;
  std::vector<unsigned char> code(Code(n), Code(n) + vbytes);
  std::vector<float> q(dim);
  Decode(code.data(), q.data());
  unsigned t[gxvTagCount];
  memcpy(t, tags.at(n), sizeof(t));
  gxvError err = InsertWith(new_id, code.data(), q.data(), t, (long long)n);
  if(err == gxv_NO_ERROR) {
    gxvWriteLock x(turn, mu);
    by_id.erase(old_id);
  }
  return err;
}

gxvError gxVectorIndex::Delete(long long id)
{
  std::lock_guard<std::mutex> w(wmu);
  if(!is_open) return gxv_NOT_OPEN;
  if(read_only) return gxv_READ_ONLY;
  auto it = by_id.find(id);
  if(it == by_id.end()) return gxv_ID_NOT_FOUND;
  size_t n = it->second;
  unsigned char f[4];
  put<unsigned>(f, 0, gxvNodeDeleted);
  if(db.Write(f, sizeof(f), *addresses.at(n) + nFLAGS) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  unsigned char c[16];
  put<unsigned long long>(c, 0, node_count);
  put<unsigned long long>(c, 8, deleted_count + 1);
  if(db.Write(c, sizeof(c), static_area + hNODES) != gxDBASE_NO_ERROR) return gxv_DB_ERROR;
  gxvWriteLock x(turn, mu);
  *live.at(n) = 0;
  by_id.erase(it);
  deleted_count++;
  live_count--;
  change_count++;
  return gxv_NO_ERROR;
}

int gxVectorIndex::Contains(long long id) const
{
  gxvReadLock lock(turn, mu);
  return by_id.count(id) != 0;
}

gxvError gxVectorIndex::Get(long long id, float *vec, unsigned *t) const
{
  gxvReadLock lock(turn, mu);
  if(!is_open) return gxv_NOT_OPEN;
  auto it = by_id.find(id);
  if(it == by_id.end()) return gxv_ID_NOT_FOUND;
  size_t n = it->second;
  if(vec) Decode(Code(n), vec);
  if(t) memcpy(t, tags.at(n), 4 * gxvTagCount);
  return gxv_NO_ERROR;
}

// ----------------------------------------------------------- //
// Searches
// ----------------------------------------------------------- //

void gxVectorIndex::ExactLocked(const float *q, unsigned k,
				std::vector<gxvResult> &results,
				const gxvFilter *filter) const
{
  std::priority_queue<gxvCand, std::vector<gxvCand>, gxvWorse> heap;
  gxvWorse worse;
  const size_t count = ids.size();
  for(size_t n = 0; n < count; n++) {
    if(!*live.at(n)) continue;
    long long id = *ids.at(n);
    if(filter && !filter->Match(id, tags.at(n))) continue;
    gxvCand c;
    c.d = DistTo(q, n);
    c.id = id;
    c.n = n;
    if(heap.size() < k) {
      heap.push(c);
    }
    else if(worse(c, heap.top())) {
      heap.pop();
      heap.push(c);
    }
  }
  results.resize(heap.size());
  for(size_t i = heap.size(); i > 0; i--) {
    const gxvCand &c = heap.top();
    gxvResult &r = results[i - 1];
    r.id = c.id;
    r.distance = c.d;
    memcpy(r.tags, tags.at(c.n), sizeof(r.tags));
    heap.pop();
  }
}

gxvError gxVectorIndex::Exact(const float *query, unsigned k,
			      std::vector<gxvResult> &results,
			      const gxvFilter *filter) const
{
  results.clear();
  gxvReadLock lock(turn, mu);
  if(!is_open) return gxv_NOT_OPEN;
  if(!query || k == 0) return gxv_BAD_ARG;
  if(!gxv_finite(query, dim)) return gxv_NOT_FINITE;
  std::vector<float> q(dim);
  Prepare(query, q.data());
  ExactLocked(q.data(), k, results, filter);
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::Search(const float *query, unsigned k,
			       std::vector<gxvResult> &results, unsigned ef,
			       const gxvFilter *filter) const
{
  results.clear();
  gxvReadLock lock(turn, mu);
  if(!is_open) return gxv_NOT_OPEN;
  if(!query || k == 0) return gxv_BAD_ARG;
  if(!gxv_finite(query, dim)) return gxv_NOT_FINITE;
  std::vector<float> q(dim);
  Prepare(query, q.data());
  const size_t count = ids.size();
  if(count <= gxvExactLimit || entry_node < 0) {
    ExactLocked(q.data(), k, results, filter);
    return gxv_NO_ERROR;
  }
  // A filter (or tombstones) that leaves few nodes: the graph would have
  // to be walked almost whole to find k of them, so scan instead.
  if(filter || deleted_count * 2 > node_count) {
    const size_t samples = 512, step = count / samples;
    size_t pass = 0;
    for(size_t i = 0; i < samples; i++) {
      size_t n = i * step + (i * 7919) % step;
      if(*live.at(n) && (!filter || filter->Match(*ids.at(n), tags.at(n)))) pass++;
    }
    if(pass * 20 < samples) {
      ExactLocked(q.data(), k, results, filter);
      return gxv_NO_ERROR;
    }
  }
  if(ef < k) ef = k;

  // Down the upper levels, one closest node at a time
  unsigned cur = (unsigned)entry_node;
  float curd = DistTo(q.data(), cur);
  for(unsigned l = top_level; l > 0; l--) {
    int changed = 1;
    while(changed) {
      changed = 0;
      const unsigned *list;
      unsigned cnt = ListOf(cur, l, list);
      for(unsigned i = 0; i < cnt; i++) {
	float d = DistTo(q.data(), list[i]);
	if(d < curd || (d == curd && list[i] < cur)) {
	  curd = d;
	  cur = list[i];
	  changed = 1;
	}
      }
    }
  }

  // Level 0: every node is a step; only live nodes that pass the filter
  // are results
  gxvVisited &vis = gxv_visited;
  vis.Start(count);
  std::priority_queue<gxvPair, std::vector<gxvPair>, std::greater<gxvPair> > cand;
  std::priority_queue<gxvCand, std::vector<gxvCand>, gxvWorse> top;
  auto accept = [&](unsigned n) -> bool {
    return *live.at(n) && (!filter || filter->Match(*ids.at(n), tags.at(n)));
  };
  auto take = [&](float d, unsigned n) {
    gxvCand c;
    c.d = d;
    c.id = *ids.at(n);
    c.n = n;
    top.push(c);
    if(top.size() > ef) top.pop();
  };
  const float inf = HUGE_VALF;
  vis.Seen(cur);
  cand.push(gxvPair(curd, cur));
  if(accept(cur)) take(curd, cur);
  float lower = top.size() >= ef ? top.top().d : inf;
  while(!cand.empty()) {
    gxvPair c = cand.top();
    if(c.first > lower) break;
    cand.pop();
    const unsigned *list;
    unsigned cnt = ListOf(c.second, 0, list);
    for(unsigned i = 0; i < cnt; i++) gxv_prefetch(Code(list[i]));
    for(unsigned i = 0; i < cnt; i++) {
      unsigned x = list[i];
      if(vis.Seen(x)) continue;
      float d = DistTo(q.data(), x);
      if(top.size() < ef || d < lower) {
	cand.push(gxvPair(d, x));
	if(accept(x)) take(d, x);
	if(top.size() >= ef) lower = top.top().d;
      }
    }
  }
  while(top.size() > k) top.pop();
  results.resize(top.size());
  for(size_t i = top.size(); i > 0; i--) {
    const gxvCand &c = top.top();
    gxvResult &r = results[i - 1];
    r.id = c.id;
    r.distance = c.d;
    memcpy(r.tags, tags.at(c.n), sizeof(r.tags));
    top.pop();
  }
  return gxv_NO_ERROR;
}

// ----------------------------------------------------------- //
// Maintenance
// ----------------------------------------------------------- //

gxvError gxVectorIndex::Repair()
{
  std::lock_guard<std::mutex> w(wmu);
  gxvWriteLock x(turn, mu);
  if(!is_open) return gxv_NOT_OPEN;
  if(read_only) return gxv_READ_ONLY;
  BuildGraph();
  const size_t count = ids.size();
  // New upper-level blocks sized for the new levels
  for(size_t n = 0; n < count; n++) {
    FAU_t old = *upper_addr.at(n);
    if(old) {
      unsigned char hb[gxvUpperHead];
      if(db.ObjectLength(old) >= gxvUpperHead &&
	 db.Read(hb, sizeof(hb), old) == gxDBASE_NO_ERROR &&
	 get<unsigned>(hb, 8) == gxvUpperMagic &&
	 get<long long>(hb, 0) == (long long)*addresses.at(n)) {
	db.Delete(old);
      }
      *upper_addr.at(n) = 0;
    }
  }
  for(size_t n = 0; n < count; n++) {
    unsigned level = *levels.at(n);
    if(level == 0) continue;
    FAU_t ua = db.Alloc((__ULWORD__)gxv_upper_size(level, m));
    if(!ua) return gxv_DB_ERROR;
    *upper_addr.at(n) = ua;
  }
  for(size_t n = 0; n < count; n++) {
    if(WriteNode(n) != gxv_NO_ERROR) return gxv_DB_ERROR;
  }
  graph_saved = 1;
  RecomputeRanges();                       // 11/14/2026 (phase D)
  if(WriteHeader() != gxv_NO_ERROR) return gxv_DB_ERROR;
  change_count++;
  return gxv_NO_ERROR;
}

gxvError gxVectorIndex::Merge(const char *out_fname,
			      const std::vector<std::string> &inputs,
			      const char *spec_wanted, unsigned M,
			      unsigned efc, std::string *report)
{
  char line[512];
  if(!out_fname || inputs.empty()) return gxv_BAD_ARG;
  gxVectorIndex out;
  gxvError err = gxv_NO_ERROR;
  for(size_t i = 0; i < inputs.size(); i++) {
    gxVectorIndex in;
    err = in.Open(inputs[i].c_str(), gxDBASE_READONLY, spec_wanted);
    if(err != gxv_NO_ERROR) {
      if(report) {
	snprintf(line, sizeof(line), "%s: %s\n", inputs[i].c_str(), ErrorMessage(err));
	*report += line;
      }
      return err;
    }
    if(i == 0) {
      err = out.Create(out_fname, in.dim, in.elem_type, in.metric, M ? M : in.m,
		       in.spec, efc);
      if(err != gxv_NO_ERROR) return err;
      spec_wanted = out.spec[0] ? out.spec : 0;
    }
    else if(in.dim != out.dim || in.metric != out.metric || in.elem_type != out.elem_type ||
	    strcmp(in.spec, out.spec) != 0) {
      if(report) {
	snprintf(line, sizeof(line), "%s: dimension, metric or spec differs from %s\n",
		 inputs[i].c_str(), inputs[0].c_str());
	*report += line;
      }
      gxvError why = strcmp(in.spec, out.spec) != 0 ? gxv_SPEC_MISMATCH : gxv_BAD_ARG;
      out.Close();
      return why;
    }
    std::lock_guard<std::mutex> w(out.wmu);
    gxvWriteLock x(out.turn, out.mu);
    unsigned long long taken = 0;
    for(size_t n = 0; n < in.ids.size(); n++) {
      if(!*in.live.at(n)) continue;
      long long id = *in.ids.at(n);
      if(out.by_id.count(id)) {
	if(report) {
	  snprintf(line, sizeof(line), "%s: id %lld is also in an earlier input\n",
		   inputs[i].c_str(), id);
	  *report += line;
	}
	return gxv_DUPLICATE_ID;
      }
      size_t nn = out.ids.size();
      memcpy(out.vectors.Push(), in.Code(n), out.vbytes); // Codes as stored
      *out.ids.Push() = id;
      memcpy(out.tags.Push(), in.tags.at(n), 4 * gxvTagCount);
      *out.live.Push() = 1;
      *out.addresses.Push() = 0;
      *out.levels.Push() = 0;
      *out.cnt0.Push() = 0;
      out.nbr0.Push();
      out.upper.Push()->clear();
      *out.upper_addr.Push() = 0;
      out.by_id[id] = nn;
      taken++;
    }
    if(report) {
      snprintf(line, sizeof(line), "%s: %llu live nodes\n", inputs[i].c_str(), taken);
      *report += line;
    }
  }
  {
    std::lock_guard<std::mutex> w(out.wmu);
    gxvWriteLock x(out.turn, out.mu);
    out.BuildGraph();
    const size_t count = out.ids.size();
    for(size_t n = 0; n < count; n++) {
      FAU_t a = out.db.Alloc((__ULWORD__)out.node_size);
      if(!a) return gxv_DB_ERROR;
      *out.addresses.at(n) = a;
      unsigned level = *out.levels.at(n);
      if(level) {
	FAU_t ua = out.db.Alloc((__ULWORD__)gxv_upper_size(level, out.m));
	if(!ua) return gxv_DB_ERROR;
	*out.upper_addr.at(n) = ua;
      }
    }
    for(size_t n = 0; n < count; n++) {
      if(out.WriteNode(n) != gxv_NO_ERROR) return gxv_DB_ERROR;
    }
    out.node_count = count;
    out.live_count = count;
    out.deleted_count = 0;
    out.graph_saved = 1;
    out.RecomputeRanges();                 // 11/14/2026 (phase D)
    if(out.WriteHeader() != gxv_NO_ERROR) return gxv_DB_ERROR;
    if(report) {
      snprintf(line, sizeof(line), "%s: %llu nodes, top level %u\n", out_fname,
	       (unsigned long long)count, out.top_level);
      *report += line;
    }
  }
  return out.Close();
}

unsigned long gxVectorIndex::Verify(int (*resolve)(long long, void *),
				    void *data, std::vector<long long> *bad,
				    std::string *report)
{
  std::lock_guard<std::mutex> w(wmu);
  gxvReadLock lock(turn, mu);
  unsigned long problems = 0;
  char line[256];
  auto note = [&](long long id, const char *what) {
    problems++;
    if(bad) bad->push_back(id);
    if(report) {
      snprintf(line, sizeof(line), "node %lld: %s\n", id, what);
      *report += line;
    }
  };
  if(!is_open) {
    if(report) *report += "not open\n";
    return 1;
  }
  std::vector<unsigned char> b(node_size), u;
  std::vector<float> dec(dim);
  const size_t count = ids.size();
  for(size_t n = 0; n < count; n++) {
    long long id = *ids.at(n);
    if(db.Read(b.data(), node_size, *addresses.at(n)) != gxDBASE_NO_ERROR) {
      note(id, "cannot read the node");
      continue;
    }
    if(get<long long>(b.data(), nID) != id) note(id, "id differs from the file");
    int alive = (get<unsigned>(b.data(), nFLAGS) & gxvNodeDeleted) == 0;
    if(alive != (int)*live.at(n)) note(id, "deleted flag differs from the file");
    if(memcmp(b.data() + nNEIGHBORS + 8 * m0, Code(n), vbytes) != 0) {
      note(id, "vector differs from the file");
    }
    Decode(Code(n), dec.data());
    if(!gxv_finite(dec.data(), dim)) note(id, "vector not finite");
    if(*live.at(n) && resolve && !resolve(id, data)) note(id, "object not found");
    // The graph: every link names a node, and the file holds the same
    const unsigned *list;
    unsigned level = *levels.at(n);
    for(unsigned l = 0; l <= level; l++) {
      unsigned c = ListOf(n, l, list);
      for(unsigned i = 0; i < c; i++) {
	if(list[i] >= count || list[i] == n || *levels.at(list[i]) < l) {
	  note(id, "graph link to a node that is not there");
	  break;
	}
      }
    }
    if(!graph_saved) continue;
    if(get<unsigned>(b.data(), nLEVEL) != level) {
      note(id, "level differs from the file");
      continue;
    }
    unsigned c = ListOf(n, 0, list);
    int same = get<unsigned>(b.data(), nN0) == c;
    for(unsigned i = 0; same && i < c; i++) {
      same = get<long long>(b.data(), nNEIGHBORS + 8 * i) == (long long)*addresses.at(list[i]);
    }
    if(!same) note(id, "level 0 links differ from the file");
    if(level == 0) continue;
    u.resize(gxv_upper_size(level, m));
    if(get<long long>(b.data(), nUPPER) != (long long)*upper_addr.at(n) ||
       db.Read(u.data(), u.size(), *upper_addr.at(n)) != gxDBASE_NO_ERROR ||
       get<unsigned>(u.data(), 8) != gxvUpperMagic || get<unsigned>(u.data(), 12) != level) {
      note(id, "upper-level block missing or damaged");
      continue;
    }
    for(unsigned l = 1; l <= level; l++) {
      size_t at = gxvUpperHead + (size_t)(l - 1) * (8 + 8 * m);
      c = ListOf(n, l, list);
      same = get<unsigned>(u.data(), at) == c;
      for(unsigned i = 0; same && i < c; i++) {
	same = get<long long>(u.data(), at + 8 + 8 * i) == (long long)*addresses.at(list[i]);
      }
      if(!same) {
	note(id, "upper-level links differ from the file");
	break;
      }
    }
  }
  return problems;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
