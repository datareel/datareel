// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: dr7features.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/15/2026
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

DataReel 6.4: the DataReel 7.x vector database features for DataReel
6.x programs. DataReel 6.4 has no vector database code of its own:
the classes below call the DataReel 7.x gxVectorIndex (.vix files:
exact and HNSW search, float32/float16/int8 vectors, L2, cosine and
inner product metrics, four tags per vector) and gxVectorSet (.vixset
manifests that search many .vix files as one), built from a DataReel
7.4 or later source tree:

  cmake -S datareel-6.4 -B build -DDATAREEL7_FEATURES_DIR=/path/to/datareel-7.4

The build makes the 6.x library (datareel), the 7.x library and the
dr7features library; link the program with DataReel::dr7features.
A program that uses these classes needs a C++17 compiler and the same
compiler for both libraries. Programs that do not use them are not
affected, and the 6.x library and its files are the same as 6.3.1.

The .vix and .vixset files are the 7.x files: a 7.x program can open
what a 6.x program writes and the other way round. Do not open the
same .vix file in a 6.x program and a 7.x program at the same time
for writing (each has its own copy of the index in memory). As with
every DataReel database file, files made on Linux and on Windows are
laid out differently (C unsigned long header fields): use them on the
system that made them.

Return values: gxv7_NO_ERROR (0) if no errors occur, otherwise an
error code; ErrorMessage() gives its text.
*/
// ----------------------------------------------------------- //
#ifndef __DR7_FEATURES_H__
#define __DR7_FEATURES_H__

#include "gxdlcode.h"
#include "gxstring.h"
#include "membuf.h"

// Element types, metrics and error codes (the same values as the 7.x
// gxvElementType, gxvMetric and gxvError)
enum gxv7ElementType { gxv7FLOAT32 = 1, gxv7FLOAT16 = 2, gxv7INT8 = 3 };
enum gxv7Metric { gxv7L2 = 1, gxv7COSINE = 2, gxv7IP = 3 };
enum gxv7Error {
  gxv7_NO_ERROR = 0,
  gxv7_NOT_OPEN, gxv7_DB_ERROR, gxv7_BAD_FILE, gxv7_BAD_VERSION,
  gxv7_BAD_ARG, gxv7_UNSUPPORTED, gxv7_SPEC_MISMATCH, gxv7_DUPLICATE_ID,
  gxv7_ID_NOT_FOUND, gxv7_NOT_FINITE, gxv7_READ_ONLY, gxv7_OUT_OF_RANGE,
  gxv7_INDEX_FULL,
  gxv7_NO_MEMORY = 100,
  gxv7_BAD_HANDLE = 101
};

const unsigned gxv7TagCount = 4;   // Tags kept with each vector
const unsigned gxv7MaxDim = 4096;  // Largest dimension

// One search result
struct GXDLCODE_API gxv7Result {
  gxv7Result();
  long long id;
  float distance;
  unsigned tags[gxv7TagCount];
  unsigned file; // gxv7VectorSet: the file the vector is in (FilePath())
};

// Tag filter: only vectors whose tags are in the given ranges
struct GXDLCODE_API gxv7Filter {
  gxv7Filter() { Clear(); }
  void Clear();
  void Tag(unsigned i, unsigned value) { Range(i, value, value); }
  void Range(unsigned i, unsigned lo_value, unsigned hi_value);
  unsigned mask;
  unsigned lo[gxv7TagCount];
  unsigned hi[gxv7TagCount];
};

class GXDLCODE_API gxv7VectorIndex
{
public:
  gxv7VectorIndex();
  ~gxv7VectorIndex();

private: // Disallow copying and assignment
  gxv7VectorIndex(const gxv7VectorIndex &) { }
  void operator=(const gxv7VectorIndex &) { }

public:
  // Make a new index file of vectors with dim elements. M is the HNSW
  // graph width. spec is an optional text (up to 63 characters) naming
  // what the vectors are; Open() can require it.
  int Create(const char *fname, unsigned dim, gxv7ElementType type = gxv7FLOAT32,
	     gxv7Metric metric = gxv7L2, unsigned M = 16, const char *spec = 0,
	     unsigned ef_construction = 200);
  int Open(const char *fname, int read_only = 0, const char *spec = 0);
  int Close();
  int Flush();
  int IsOpen() const;

  // Vectors are Dim() floats. id is any number unique in the index (for
  // example a database block address or a record number).
  int Insert(long long id, const float *vec, const unsigned *tags = 0);
  int Insert(long long id, const MemoryBuffer &vec, const unsigned *tags = 0);
  int Update(long long id, const float *vec, const unsigned *tags = 0);
  int Delete(long long id);
  int Contains(long long id) const;
  int Get(long long id, float *vec, unsigned *tags = 0) const;

  // The k nearest vectors, closest first: Search() is approximate (HNSW,
  // ef = search width; exact for small indexes), Exact() is exact.
  // results must hold k entries; count is set to the number found.
  int Search(const float *query, unsigned k, gxv7Result *results,
	     unsigned &count, unsigned ef = 64, const gxv7Filter *filter = 0) const;
  int Exact(const float *query, unsigned k, gxv7Result *results,
	    unsigned &count, const gxv7Filter *filter = 0) const;

  // Rebuild the graph from the live vectors, and check the file.
  int Repair();
  unsigned long Verify(gxString &report);

  unsigned Dim() const;
  gxv7ElementType ElementType() const;
  gxv7Metric Metric() const;
  unsigned long long Count() const;   // Live vectors
  unsigned long long Deleted() const;

  static const char *ErrorMessage(int err);
  // The DataReel 7.x library the features were built with ("7.4 beta3")
  static const char *LibraryVersion();

private:
  void *h; // The 7.x index (dr7inner.h)
};

class GXDLCODE_API gxv7VectorSet
{
public:
  gxv7VectorSet();
  ~gxv7VectorSet();

private: // Disallow copying and assignment
  gxv7VectorSet(const gxv7VectorSet &) { }
  void operator=(const gxv7VectorSet &) { }

public:
  // path is the manifest (.vixset); name is a text kept in it. The
  // .vix files of a set have the same dimension, element type, metric
  // and spec, and are listed by their path relative to the manifest.
  int Create(const char *path, const char *name, unsigned dim,
	     gxv7ElementType type = gxv7FLOAT32, gxv7Metric metric = gxv7L2,
	     const char *spec = 0);
  int Open(const char *path, const char *spec = 0, unsigned max_open = 16);
  void Close();
  int AddFile(const char *rel_path);
  int RemoveFile(const char *rel_path);
  int Refresh();

  // Search every file. day_range = 1 searches only vectors whose tag 2
  // is between day_lo and day_hi (files outside the range are skipped).
  int Search(const float *query, unsigned k, gxv7Result *results,
	     unsigned &count, unsigned ef = 64, const gxv7Filter *filter = 0,
	     int day_range = 0, unsigned day_lo = 0, unsigned day_hi = 0,
	     unsigned threads = 1) const;

  unsigned FileCount() const;
  gxString FilePath(unsigned file) const;
  unsigned Dim() const;
  gxString Error() const;

private:
  void *h; // The 7.x set (dr7inner.h)
};

// What a vector database on a DataReel 7.x object server holds
struct GXDLCODE_API gxv7VectorInfo {
  gxv7VectorInfo();
  unsigned version;
  unsigned dim;
  gxv7ElementType type;
  gxv7Metric metric;
  unsigned files;              // .vix files in the server's set
  unsigned long long nodes;    // Live vectors
  unsigned long long deleted;
  gxString spec;
};

// Client of the DataReel 7.x database object server: op 7
// VECTOR_SEARCH and op 8 VECTOR_INFO on a database the server's config
// gives "vectors = <.vixset or .vix>" (for example "gxrepl serve" with
// a [db NAME] section). The connection is mutual TLS: the client sends
// its certificate (cert, key) and checks the server's against the CA
// and server_name. The searches are read only. Return values: 0 if no
// errors occur, -1 otherwise (Error() says why). POSIX only, as the
// 7.4 object client: on Windows Connect() returns -1.
class GXDLCODE_API gxv7ObjectClient
{
public:
  gxv7ObjectClient();
  ~gxv7ObjectClient();

private: // Disallow copying and assignment
  gxv7ObjectClient(const gxv7ObjectClient &) { }
  void operator=(const gxv7ObjectClient &) { }

public:
  int Connect(const char *host, int port, const char *server_name,
	      const char *cert, const char *key, const char *ca,
	      const char *db_name, int timeout_secs = 10);
  void Close();
  int IsConnected() const;
  int VectorInfo(gxv7VectorInfo &info);
  // The k nearest vectors (k at most 1000), closest first. results
  // must hold k entries. day_range = 1: only vectors with tag 2 between
  // day_lo and day_hi; skipped/missing count the server's files skipped
  // by their day range and listed but missing.
  int VectorSearch(const float *query, unsigned dim, unsigned k,
		   gxv7Result *results, unsigned &count, unsigned ef = 64,
		   const gxv7Filter *filter = 0, int day_range = 0,
		   unsigned day_lo = 0, unsigned day_hi = 0,
		   unsigned *skipped = 0, unsigned *missing = 0);
  // The path (relative to the server's manifest) of results[i].file
  gxString FileName(unsigned file) const;
  gxString Error() const;

private:
  void *h; // The 7.x client (dr7inner.h)
};

#endif // __DR7_FEATURES_H__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
