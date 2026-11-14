// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxvindex.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
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

gxVectorIndex: a vector index stored in a gxDatabase file (".vix" by
convention). Each node is one fixed-size block holding the 64-bit id of
the object it belongs to (its FAU in the data file, or the
application's own id), up to four 32-bit tags, the vector and the
neighbor slots of the graph index. Because the file is an ordinary
gxDatabase file it is attached to the same gxdJournal as its data file
and B-tree indexes, so one commit carries an object, its keys and its
vector together, a crash leaves all or none of them, and gxrepl ships
the .vix like any other database file.

  gxdJournal j;
  j.Open("events.jnl");
  gxVectorIndex vix;
  vix.Open("events.vix");             // or Create(name, dim, gxvFLOAT32,
                                      //   gxvL2, 16, "anomaly/v1/<hash>")
  j.Attach(vix.Database());
  j.Begin();
  unsigned tags[4] = { session, kind, yyyymmdd, 0 };
  vix.Insert(root_fau, vec, tags);
  j.Commit();

  gxvFilter f;
  f.Tag(0, session);                  // tag 0 equal to session
  f.Range(2, 0, cutoff_yyyymmdd);     // tag 2 at most the cutoff day
  std::vector<gxvResult> hits;
  vix.Search(query, 20, hits, 64, &f); // id + distance + tags

The vectors (and ids and tags) of every node are kept in memory while
the index is open, so a search never reads the file. Search() takes a
shared lock and Insert() / Update() / Delete() an exclusive one, so
several threads can search while one thread writes. A waiting writer
goes ahead of new searches, so steady searching never stalls inserts.

Search() walks an HNSW graph (phase B): each node's level comes from a
hash of its id, so the same nodes in the same order always build the
same graph. Small indexes (up to gxvExactLimit nodes) and very
selective filters are searched exactly, so they never miss. Insert()
finds its neighbors under the shared lock (searches keep running) and
takes the exclusive lock only to link the node, a few microseconds.
Delete() keeps the node in the graph as a stepping stone; Update()
with a new vector deletes the node and inserts a new one. Repair()
rebuilds the graph from the live nodes; Merge() builds one index from
many (the day files) with every link written once.

Phase A files (no graph) open unchanged: the graph is built in memory
when the file is opened, and Repair() writes it to the file.

Spec: a 63-character text kept in the header (e.g. family, version and
a hash of the feature builder). Open(name, mode, spec) refuses a file
whose spec differs, so vectors from another builder never mix with
these. A new builder version means a new index file.

Metrics: gxvL2 = squared Euclidean distance; gxvCOSINE = 1 - cosine
similarity (vectors are stored and searched unit length); gxvIP = minus
the dot product. Smaller is closer in all three. Results are sorted by
distance, then by id, so equal distances always come back in the same
order.

Element types (phase C): gxvFLOAT32 keeps the vector as given;
gxvFLOAT16 rounds each value to IEEE binary16 (half the size; values
above 65504 in size are refused with gxv_OUT_OF_RANGE); gxvINT8 keeps
one signed byte per value and a scale per vector (a quarter of the
size), within half a step (max|v| / 254) of each value. gxvCOSINE
vectors are normalized first, then stored. Every distance, during
inserts as well as searches, is computed on the decoded vectors, so a
node's place in the graph does not depend on how it got in (Insert,
Repair or Merge); queries are not rounded. Get() returns the decoded
vector.

After gxdJournal::Rollback() call Reload(): the rollback restores the
file but not the copy in memory.

File layout (native byte order, little endian on the supported nodes):
static area (256 bytes): magic "GXVIX74", version, dim, element type,
metric, M, M0 (2M), ef_construction, tag count, node size, top level,
entry node (its block address), node count, deleted count, spec, graph
state (offset 136: 0 = no graph saved, 1 = saved), and (phase D, all
zero in older files) features_required (140: a bit this library does
not know makes the file gxv_UNSUPPORTED), features_optional (144: bit 0
the tag ranges below are kept, bit 1 the index is derived from record
members), codec (148: 0 plain codes; 1 is kept for product
quantization), graph layout (152: 0 the in-memory HNSW), partition id
and count (156, 160: 0 and 0 when the index is not a partition of a
set), 4 zero bytes, tag_min[4] (168) and tag_max[4] (184): the smallest
and largest value of each tag over every node ever inserted (an empty
index: 0xFFFFFFFF and 0; Delete() never narrows them, Repair() and
Merge() work them out again), and 56 reserved bytes (200: written as
zero, ignored when read). Node block: id
(int64), flags (1 = deleted), level, tags[4], upper-level block
address (int64, 0 = none), level 0 neighbor count (and 4 zero bytes),
M0 neighbor block addresses (int64), vector. The vector is stored in
its element type: gxvFLOAT32 4 * dim bytes; gxvFLOAT16 2 * dim bytes
(IEEE binary16); gxvINT8 dim signed bytes then a float32 scale (dim + 4
bytes). Node size = 48 + 8 * M0 + those bytes. Upper-level block: owner
node address (int64), magic 0x55565847 ("GXVU"), level count, then per
level 1..L: neighbor count, 4 zero bytes, M neighbor addresses (int64).

Changes:
==============================================================
11/12/2026: New in DataReel 7.4 (vector extensions, phase A).
11/12/2026: Phase B: HNSW graph search, Merge(), Repair(), Update()
as delete + insert, writers find neighbors under the shared lock.
11/12/2026: Phase C: gxvFLOAT16 and gxvINT8 element types,
gxv_OUT_OF_RANGE. gxvFLOAT32 files are unchanged.
11/14/2026: Phase D: format fields for growth (features, codec, graph
layout, partition, tag ranges), gxv_INDEX_FULL. Files of phases A-C
open unchanged (no tag ranges).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_VECTOR_INDEX_HPP__
#define __GX_VECTOR_INDEX_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <memory>
#include "gxdbase.h"

namespace datareel {

enum gxvElementType {
  gxvFLOAT32 = 1,
  gxvFLOAT16 = 2, // IEEE binary16 (phase C)
  gxvINT8 = 3     // Signed bytes and a float scale per vector (phase C)
};

enum gxvMetric {
  gxvL2 = 1,
  gxvCOSINE = 2,
  gxvIP = 3
};

enum gxvError {
  gxv_NO_ERROR = 0,
  gxv_NOT_OPEN,
  gxv_DB_ERROR,       // The gxDatabase call failed, see Database()
  gxv_BAD_FILE,       // Not a vector index, or a damaged header or node
  gxv_BAD_VERSION,
  gxv_BAD_ARG,
  gxv_UNSUPPORTED,    // Element type this library does not know
  gxv_SPEC_MISMATCH,
  gxv_DUPLICATE_ID,
  gxv_ID_NOT_FOUND,
  gxv_NOT_FINITE,     // NaN or infinity in a vector
  gxv_READ_ONLY,
  gxv_OUT_OF_RANGE,   // A value too large for the element type (float16: 65504)
  gxv_INDEX_FULL      // 11/14/2026: gxvMaxNodes nodes (node numbers are 32-bit)
};

const unsigned gxvMaxDim = 4096;
const unsigned gxvTagCount = 4;
const unsigned gxvSpecSize = 64;    // Including the terminating zero
const unsigned gxvHeaderSize = 256; // The static area
const unsigned gxvVersion = 1;
const unsigned gxvMaxLevel = 31;
const unsigned gxvExactLimit = 4096; // Search() is exact up to this many nodes
const unsigned gxvMemberHead = 12;   // Bytes before the code in a vector member
// 11/14/2026 (phase D): format fields (see the file layout above)
const unsigned long long gxvMaxNodes = 4294967280ULL; // Nodes a file may hold (deleted ones too)
const unsigned gxvFeaturesKnown = 0;         // features_required bits this library reads
const unsigned gxvOptTagRanges = 1;          // features_optional: tag_min/tag_max are kept
const unsigned gxvOptFromMembers = 2;        // features_optional: derived from record members
const unsigned gxvCodecPlain = 0;
const unsigned gxvGraphHNSW = 0;

// Per-node arrays kept in fixed-size chunks, so growing an index never
// copies what is already there (no stall while searches wait).
template<class T> class gxvChunks {
public:
  enum { SHIFT = 14, CHUNK = 1 << SHIFT, MASK = CHUNK - 1 };
  gxvChunks() : w(1), n(0) { }
  void Init(unsigned width) { w = width ? width : 1; Clear(); }
  void Clear() { chunks.clear(); n = 0; }
  size_t size() const { return n; }
  T *at(size_t i) { return chunks[i >> SHIFT].get() + (i & MASK) * (size_t)w; }
  const T *at(size_t i) const { return chunks[i >> SHIFT].get() + (i & MASK) * (size_t)w; }
  // Make room for one more without making it visible
  void Reserve() {
    if((n >> SHIFT) >= chunks.size()) chunks.emplace_back(new T[(size_t)CHUNK * w]);
  }
  T *Push() { Reserve(); return at(n++); }
private:
  std::vector<std::unique_ptr<T[]> > chunks;
  unsigned w;
  size_t n;
};

struct gxvPlan; // An insert's links, worked out before they are made

struct gxvResult {
  long long id;
  float distance;
  unsigned tags[gxvTagCount];
};

// Which nodes a search may return. Tag i is tested when bit i of
// tag_mask is set: lo[i] <= tag <= hi[i]. accept, when set, is asked
// last and returns nonzero to keep the node.
struct GXDLCODE_API gxvFilter {
  gxvFilter();
  void Tag(unsigned i, unsigned value) { Range(i, value, value); }
  void Range(unsigned i, unsigned lo_value, unsigned hi_value);
  int Match(long long id, const unsigned *tags) const;

  unsigned tag_mask;
  unsigned lo[gxvTagCount];
  unsigned hi[gxvTagCount];
  int (*accept)(long long id, const unsigned *tags, void *data);
  void *accept_data;
};

class GXDLCODE_API gxVectorIndex
{
public:
  gxVectorIndex();
  ~gxVectorIndex();

private:
  gxVectorIndex(const gxVectorIndex &) { }
  void operator=(const gxVectorIndex &) { }

public:
  gxvError Create(const char *fname, unsigned dim,
		  gxvElementType type = gxvFLOAT32, gxvMetric metric = gxvL2,
		  unsigned M = 16, const char *spec = 0,
		  unsigned ef_construction = 200);
  // spec, when given, must equal the file's spec
  gxvError Open(const char *fname,
		gxDatabaseAccessMode mode = gxDBASE_READWRITE,
		const char *spec = 0);
  gxvError Close();
  gxvError Flush();
  gxvError Reload(); // Read the nodes from the file again

  // The file, for gxdJournal::Attach(), Sync() and the like. Write to
  // it only through this class.
  gxDatabase *Database() { return &db; }

  gxvError Insert(long long id, const float *vec, const unsigned *tags = 0);
  // Same vector: the tags are rewritten in place. A new vector: the old
  // node is deleted and a new one inserted (Deleted() counts it).
  gxvError Update(long long id, const float *vec, const unsigned *tags = 0);
  gxvError Delete(long long id); // A tombstone: the node is kept, not found
  int Contains(long long id) const;

  // Vector members (phase C): a record (an InfoHog member, or a fixed
  // char field of MemberSize() bytes in a POD record) keeps its vector in
  // the index's stored form. The member is a 12-byte head ("GXVM",
  // element type, 0, dim as 16 bits, CRC-32 of the spec) and the code
  // bytes. InsertMember() and UpdateMember() put the member's code in the
  // index unchanged, so record and node always hold the same bytes; a
  // member of another type, dimension or spec is refused (gxv_BAD_ARG,
  // gxv_SPEC_MISMATCH). Write the record and the index in the same
  // gxdJournal transaction (attach the data file, its index files and
  // Database()); the id is the record's address (its root FAU).
  unsigned CodeSize() const { return vbytes; }
  unsigned MemberSize() const { return gxvMemberHead + vbytes; }
  unsigned SpecCRC() const;
  gxvError MakeMember(const float *vec, std::vector<unsigned char> &member) const;
  gxvError MemberVector(const unsigned char *member, unsigned len, float *vec) const;
  gxvError InsertMember(long long id, const unsigned char *member, unsigned len,
			const unsigned *tags = 0);
  gxvError UpdateMember(long long id, const unsigned char *member, unsigned len,
			const unsigned *tags = 0);
  // 1 when node id is live and holds exactly this member's code (for a
  // Verify() resolver that reads the record)
  int MemberMatches(long long id, const unsigned char *member, unsigned len) const;
  // The record moved to a new address (an InfoHog record that grew): the
  // node gets the new id, with its stored code and tags (a new node
  // replaces the old one, as in Update)
  gxvError Rename(long long old_id, long long new_id);
  // The stored vector (unit length for gxvCOSINE) and tags
  gxvError Get(long long id, float *vec, unsigned *tags = 0) const;

  // The k nearest live nodes that pass the filter, closest first
  gxvError Exact(const float *query, unsigned k,
		 std::vector<gxvResult> &results,
		 const gxvFilter *filter = 0) const;
  // Approximate search over the graph; ef is the search width (larger
  // = closer to Exact(), slower). Exact for small indexes and for
  // filters that pass fewer than about 5% of the nodes.
  gxvError Search(const float *query, unsigned k,
		  std::vector<gxvResult> &results, unsigned ef = 64,
		  const gxvFilter *filter = 0) const;

  // Rebuild the graph from the live nodes and write it to the file
  // (attach the journal first to make it all or nothing). Also writes
  // the graph of a phase A file.
  gxvError Repair();

  // Build out_fname from the live nodes of the inputs (for example the
  // day files of a month), every link written once. The inputs must
  // have the same dimension, metric and spec (spec, when given, must
  // equal theirs) and no id in two of them (gxv_DUPLICATE_ID). M = 0
  // takes the first input's. Open the result read only to search it.
  static gxvError Merge(const char *out_fname,
			const std::vector<std::string> &inputs,
			const char *spec = 0, unsigned M = 0,
			unsigned ef_construction = 200,
			std::string *report = 0);

  // Check the file against the copy in memory, and optionally ask
  // resolve(id, data) whether each live node's object still exists
  // (nonzero = it does). Ids of nodes that fail go in "bad"; a line per
  // problem goes in "report". Returns the number of problems.
  unsigned long Verify(int (*resolve)(long long id, void *data) = 0,
		       void *data = 0, std::vector<long long> *bad = 0,
		       std::string *report = 0);

  int IsOpen() const { return is_open; }
  unsigned Dim() const { return dim; }
  gxvMetric Metric() const { return metric; }
  gxvElementType ElementType() const { return elem_type; }
  unsigned NeighborSlots() const { return m0; }
  unsigned M() const { return m; }
  unsigned EfConstruction() const { return ef_construction; }
  unsigned TopLevel() const { return top_level; }
  // 1 when the graph is saved in the file, 0 when it was built in
  // memory at Open() (a phase A file, or a damaged graph: Repair() it)
  int GraphSaved() const { return graph_saved; }
  unsigned NodeSize() const { return node_size; }
  const char *Spec() const { return spec; }
  unsigned long long Count() const { return live_count; }   // Live nodes
  unsigned long long Deleted() const { return deleted_count; }
  // 1 + the number of writes since Open(): lets a reader see a change
  unsigned long long ChangeCount() const { return change_count; }
  // 11/14/2026 (phase D): format fields
  unsigned FeaturesRequired() const { return features_required; }
  unsigned FeaturesOptional() const { return features_optional; }
  unsigned Codec() const { return codec; }
  unsigned GraphLayout() const { return graph_layout; }
  unsigned PartitionId() const { return partition_id; }
  unsigned PartitionCount() const { return partition_count; }
  // The smallest and largest value tag "i" ever had (inserts, updates):
  // 1, or 0 when the file does not keep them (phases A-C, until Repair())
  // or the index never held a node
  int TagRange(unsigned i, unsigned &lo, unsigned &hi) const;
  // 1 when no node can have tag "i" in [lo, hi] (a set can skip the file)
  int TagRangeExcludes(unsigned i, unsigned lo, unsigned hi) const;
  // A partition of a set (gxVectorSet): id < count; 0, 0 = none
  gxvError SetPartition(unsigned id, unsigned count);
  // Marks the index derived from record members (features_optional bit 1)
  gxvError SetFromMembers(int on);

  static const char *ErrorMessage(gxvError err);
  // The level a node with this id gets (a hash of the id)
  static unsigned LevelFor(long long id, unsigned M);

private:
  gxvError ReadHeader();
  gxvError WriteHeader();
  gxvError WriteCounts();
  gxvError WriteEntry();
  gxvError LoadNodes();
  gxvError LoadGraph(const std::vector<long long> &raw0,
		     const std::vector<size_t> &raw0_at);
  gxvError Check(const float *vec) const;
  void Prepare(const float *vec, float *out) const;
  float Distance(const float *a, const float *b) const;
  // The stored form of a prepared vector, and back. Every distance uses
  // decoded vectors, so a node's distances never depend on how it got in.
  gxvError Encode(const float *prepared, unsigned char *code) const;
  void Decode(const unsigned char *code, float *out) const;
  static unsigned CodeBytes(gxvElementType type, unsigned dim);
  void Clear();
  void InitArrays();
  // With wmu held: insert a prepared vector; replace >= 0 also deletes
  // that node in the same step (Update)
  gxvError InsertWith(long long id, const unsigned char *code, const float *q,
		      const unsigned *t, long long replace);
  gxvError CheckMember(const unsigned char *m, unsigned len) const;
  // With wmu held: store code for id (insert, or update an existing node)
  gxvError PutCode(long long id, const unsigned char *code, const unsigned *t, int update);
  void ExactLocked(const float *q, unsigned k, std::vector<gxvResult> &results,
		   const gxvFilter *filter) const;

  // Graph
  const unsigned char *Code(size_t n) const { return (const unsigned char *)vectors.at(n); }
  // Node n's vector as floats: the stored floats for gxvFLOAT32, else
  // decoded into buf (dim floats)
  const float *Vec(size_t n, float *buf) const {
    if(elem_type == gxvFLOAT32) return vectors.at(n);
    Decode(Code(n), buf);
    return buf;
  }
  float DistTo(const float *q, size_t n) const; // q against node n
  unsigned ListOf(size_t n, unsigned level, const unsigned *&list) const;
  void SetList(size_t n, unsigned level, const unsigned *list, unsigned count);
  void SearchLayer(const float *q, std::vector<std::pair<float, unsigned> > &eps,
		   unsigned ef, unsigned level) const;
  void Heuristic(std::vector<std::pair<float, unsigned> > &cands, unsigned keep,
		 size_t newn, const float *q) const;
  void PlanInsert(size_t newn, const float *q, unsigned level, gxvPlan &plan) const;
  void ApplyPlan(size_t newn, const gxvPlan &plan);
  void BuildGraph(); // Link every live node, in file order
  gxvError WriteNode(size_t n);

private:
  gxDatabase db;
  mutable std::shared_mutex mu;
  mutable std::mutex turn; // Lets a waiting writer in ahead of new searches
  int is_open;
  int read_only;
  FAU_t static_area;
  unsigned dim;
  gxvElementType elem_type;
  gxvMetric metric;
  unsigned m;
  unsigned m0;
  unsigned ef_construction;
  unsigned node_size;
  unsigned vbytes;         // Bytes of one stored vector (CodeBytes)
  unsigned top_level;
  long long entry;
  char spec[gxvSpecSize];
  unsigned long long node_count;
  unsigned long long live_count;
  unsigned long long deleted_count;
  unsigned long long change_count;

  mutable std::mutex wmu;  // One writer at a time
  int graph_saved;
  long long entry_node;    // Index of the entry node, -1 = none
  double level_mult;

  // One entry per node, in file order
  // The stored codes, vbytes each, kept in float-sized words (vbytes
  // rounded up to 4) so a gxvFLOAT32 vector is read as floats directly
  gxvChunks<float> vectors;
  gxvChunks<long long> ids;
  gxvChunks<unsigned> tags;   // gxvTagCount each
  gxvChunks<unsigned char> live;
  gxvChunks<FAU_t> addresses;
  gxvChunks<unsigned char> levels;
  gxvChunks<unsigned> cnt0;   // Level 0 neighbor count
  gxvChunks<unsigned> nbr0;   // M0 each: node indexes
  gxvChunks<std::vector<unsigned> > upper; // Levels 1..L: count + M each
  gxvChunks<FAU_t> upper_addr;
  std::unordered_map<long long, size_t> by_id; // Live nodes only

  // 11/14/2026 (phase D): format fields (last, so the members above keep their places)
  unsigned features_required = 0, features_optional = 0, codec = 0, graph_layout = 0;
  unsigned partition_id = 0, partition_count = 0;
  unsigned tag_min[gxvTagCount], tag_max[gxvTagCount];
  int ranges_dirty = 0;
  void ResetRanges();
  void WidenRanges(const unsigned *t);
  void RecomputeRanges();
  gxvError WriteRanges();
};

} // namespace datareel

#endif // __GX_VECTOR_INDEX_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
