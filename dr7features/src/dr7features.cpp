// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: dr7features.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/15/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
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

DataReel 6.4: the 6.x classes for the DataReel 7.x vector database
features (see dr7features.h). Built against the DataReel 6.x headers
and dr7inner.h only: never include a DataReel 7.x header here.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <string.h>
#include "dr7features.h"
#include "dr7inner.h"

#define IDX ((dr7v_index *)h)
#define SET ((dr7s_set *)h)

static void to_inner(const gxv7Filter *f, dr7v_filter &out)
{
  memset(&out, 0, sizeof(out));
  if(!f) return;
  out.mask = f->mask;
  for(unsigned i = 0; i < gxv7TagCount; i++) {
    out.lo[i] = f->lo[i];
    out.hi[i] = f->hi[i];
  }
}

static void from_inner(const dr7v_result *r, unsigned n, gxv7Result *out)
{
  for(unsigned i = 0; i < n; i++) {
    out[i].id = r[i].id;
    out[i].distance = r[i].distance;
    for(unsigned t = 0; t < gxv7TagCount; t++) out[i].tags[t] = r[i].tags[t];
    out[i].file = r[i].file;
  }
}

gxv7Result::gxv7Result()
{
  id = -1;
  distance = 0;
  for(unsigned t = 0; t < gxv7TagCount; t++) tags[t] = 0;
  file = 0;
}

void gxv7Filter::Clear()
{
  mask = 0;
  for(unsigned i = 0; i < gxv7TagCount; i++) lo[i] = hi[i] = 0;
}

void gxv7Filter::Range(unsigned i, unsigned lo_value, unsigned hi_value)
{
  if(i >= gxv7TagCount) return;
  mask |= (1u << i);
  lo[i] = lo_value;
  hi[i] = hi_value;
}

// gxv7VectorIndex
gxv7VectorIndex::gxv7VectorIndex() { h = dr7v_new(); }
gxv7VectorIndex::~gxv7VectorIndex() { dr7v_free(IDX); }

int gxv7VectorIndex::Create(const char *fname, unsigned dim,
			    gxv7ElementType type, gxv7Metric metric,
			    unsigned M, const char *spec,
			    unsigned ef_construction)
{
  return dr7v_create(IDX, fname, dim, (int)type, (int)metric, M, spec,
		     ef_construction);
}

int gxv7VectorIndex::Open(const char *fname, int read_only, const char *spec)
{
  return dr7v_open(IDX, fname, read_only, spec);
}

int gxv7VectorIndex::Close() { return dr7v_close(IDX); }
int gxv7VectorIndex::Flush() { return dr7v_flush(IDX); }
int gxv7VectorIndex::IsOpen() const { return dr7v_is_open(IDX); }

int gxv7VectorIndex::Insert(long long id, const float *vec,
			    const unsigned *tags)
{
  return dr7v_insert(IDX, id, vec, tags);
}

int gxv7VectorIndex::Insert(long long id, const MemoryBuffer &vec,
			    const unsigned *tags)
{
  // The buffer holds Dim() floats
  if(vec.length() != Dim() * sizeof(float)) return gxv7_BAD_ARG;
  return dr7v_insert(IDX, id, (const float *)vec.m_buf(), tags);
}

int gxv7VectorIndex::Update(long long id, const float *vec,
			    const unsigned *tags)
{
  return dr7v_update(IDX, id, vec, tags);
}

int gxv7VectorIndex::Delete(long long id) { return dr7v_delete(IDX, id); }
int gxv7VectorIndex::Contains(long long id) const { return dr7v_contains(IDX, id); }

int gxv7VectorIndex::Get(long long id, float *vec, unsigned *tags) const
{
  return dr7v_get(IDX, id, vec, tags);
}

int gxv7VectorIndex::Search(const float *query, unsigned k,
			    gxv7Result *results, unsigned &count,
			    unsigned ef, const gxv7Filter *filter) const
{
  count = 0;
  if(k == 0) return gxv7_NO_ERROR;
  if(!results) return gxv7_BAD_ARG;
  dr7v_result *r = new dr7v_result[k];
  dr7v_filter f;
  to_inner(filter, f);
  int rv = dr7v_search(IDX, query, k, ef, 0, filter ? &f : 0, r, &count);
  if(rv == DR7_OK) from_inner(r, count, results);
  delete[] r;
  return rv;
}

int gxv7VectorIndex::Exact(const float *query, unsigned k,
			   gxv7Result *results, unsigned &count,
			   const gxv7Filter *filter) const
{
  count = 0;
  if(k == 0) return gxv7_NO_ERROR;
  if(!results) return gxv7_BAD_ARG;
  dr7v_result *r = new dr7v_result[k];
  dr7v_filter f;
  to_inner(filter, f);
  int rv = dr7v_search(IDX, query, k, 0, 1, filter ? &f : 0, r, &count);
  if(rv == DR7_OK) from_inner(r, count, results);
  delete[] r;
  return rv;
}

int gxv7VectorIndex::Repair() { return dr7v_repair(IDX); }

unsigned long gxv7VectorIndex::Verify(gxString &report)
{
  const unsigned size = 64 * 1024;
  char *buf = new char[size];
  unsigned long problems = dr7v_verify(IDX, buf, size);
  report = buf;
  delete[] buf;
  return problems;
}

unsigned gxv7VectorIndex::Dim() const
{
  dr7v_info info;
  return dr7v_info_get(IDX, &info) == DR7_OK ? info.dim : 0;
}

gxv7ElementType gxv7VectorIndex::ElementType() const
{
  dr7v_info info;
  return dr7v_info_get(IDX, &info) == DR7_OK ? (gxv7ElementType)info.type :
    gxv7FLOAT32;
}

gxv7Metric gxv7VectorIndex::Metric() const
{
  dr7v_info info;
  return dr7v_info_get(IDX, &info) == DR7_OK ? (gxv7Metric)info.metric : gxv7L2;
}

unsigned long long gxv7VectorIndex::Count() const
{
  dr7v_info info;
  return dr7v_info_get(IDX, &info) == DR7_OK ? info.count : 0;
}

unsigned long long gxv7VectorIndex::Deleted() const
{
  dr7v_info info;
  return dr7v_info_get(IDX, &info) == DR7_OK ? info.deleted : 0;
}

const char *gxv7VectorIndex::ErrorMessage(int err)
{
  return dr7v_error_message(err);
}

const char *gxv7VectorIndex::LibraryVersion() { return dr7_version_string(); }

// gxv7VectorSet
gxv7VectorSet::gxv7VectorSet() { h = dr7s_new(); }
gxv7VectorSet::~gxv7VectorSet() { dr7s_free(SET); }

int gxv7VectorSet::Create(const char *path, const char *name, unsigned dim,
			  gxv7ElementType type, gxv7Metric metric,
			  const char *spec)
{
  return dr7s_create(SET, path, name, dim, (int)type, (int)metric, spec);
}

int gxv7VectorSet::Open(const char *path, const char *spec, unsigned max_open)
{
  return dr7s_open(SET, path, spec, max_open);
}

void gxv7VectorSet::Close() { dr7s_close(SET); }
int gxv7VectorSet::AddFile(const char *rel_path) { return dr7s_add_file(SET, rel_path); }
int gxv7VectorSet::RemoveFile(const char *rel_path) { return dr7s_remove_file(SET, rel_path); }
int gxv7VectorSet::Refresh() { return dr7s_refresh(SET); }

int gxv7VectorSet::Search(const float *query, unsigned k, gxv7Result *results,
			  unsigned &count, unsigned ef, const gxv7Filter *filter,
			  int day_range, unsigned day_lo, unsigned day_hi,
			  unsigned threads) const
{
  count = 0;
  if(k == 0) return gxv7_NO_ERROR;
  if(!results) return gxv7_BAD_ARG;
  dr7v_result *r = new dr7v_result[k];
  dr7v_filter f;
  to_inner(filter, f);
  int rv = dr7s_search(SET, query, k, ef, filter ? &f : 0, day_range,
		       day_lo, day_hi, threads, 0, r, &count);
  if(rv == DR7_OK) from_inner(r, count, results);
  delete[] r;
  return rv;
}

unsigned gxv7VectorSet::FileCount() const { return dr7s_file_count(SET); }

gxString gxv7VectorSet::FilePath(unsigned file) const
{
  unsigned n = dr7s_file_path(SET, file, 0, 0);
  char *buf = new char[n + 1];
  dr7s_file_path(SET, file, buf, n + 1);
  gxString s(buf);
  delete[] buf;
  return s;
}

unsigned gxv7VectorSet::Dim() const { return dr7s_dim(SET); }
gxString gxv7VectorSet::Error() const { return gxString(dr7s_error(SET)); }

// gxv7ObjectClient
#define CLI ((dr7c_client *)h)

gxv7VectorInfo::gxv7VectorInfo()
{
  version = dim = files = 0;
  type = gxv7FLOAT32;
  metric = gxv7L2;
  nodes = deleted = 0;
}

gxv7ObjectClient::gxv7ObjectClient() { h = dr7c_new(); }
gxv7ObjectClient::~gxv7ObjectClient() { dr7c_free(CLI); }

int gxv7ObjectClient::Connect(const char *host, int port, const char *server_name,
			      const char *cert, const char *key, const char *ca,
			      const char *db_name, int timeout_secs)
{
  return dr7c_connect(CLI, host, port, server_name, cert, key, ca, db_name,
		      timeout_secs);
}

void gxv7ObjectClient::Close() { dr7c_close(CLI); }
int gxv7ObjectClient::IsConnected() const { return dr7c_is_connected(CLI); }

int gxv7ObjectClient::VectorInfo(gxv7VectorInfo &info)
{
  dr7c_vinfo vi;
  char spec[256];
  if(dr7c_vector_info(CLI, &vi, spec, sizeof(spec)) != 0) return -1;
  info.version = vi.version;
  info.dim = vi.dim;
  info.type = (gxv7ElementType)vi.type;
  info.metric = (gxv7Metric)vi.metric;
  info.files = vi.files;
  info.nodes = vi.nodes;
  info.deleted = vi.deleted;
  info.spec = spec;
  return 0;
}

int gxv7ObjectClient::VectorSearch(const float *query, unsigned dim, unsigned k,
				   gxv7Result *results, unsigned &count,
				   unsigned ef, const gxv7Filter *filter,
				   int day_range, unsigned day_lo, unsigned day_hi,
				   unsigned *skipped, unsigned *missing)
{
  count = 0;
  if(k == 0) return 0;
  if(!results) return -1;
  dr7v_result *r = new dr7v_result[k];
  dr7v_filter f;
  to_inner(filter, f);
  int rv = dr7c_vector_search(CLI, query, dim, k, ef, filter ? &f : 0,
			      day_range, day_lo, day_hi, 0, r, &count,
			      skipped, missing);
  if(rv == 0) from_inner(r, count, results);
  delete[] r;
  return rv;
}

gxString gxv7ObjectClient::FileName(unsigned file) const
{
  unsigned n = dr7c_file_name(CLI, file, 0, 0);
  char *buf = new char[n + 1];
  dr7c_file_name(CLI, file, buf, n + 1);
  gxString s(buf);
  delete[] buf;
  return s;
}

gxString gxv7ObjectClient::Error() const { return gxString(dr7c_error(CLI)); }
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
