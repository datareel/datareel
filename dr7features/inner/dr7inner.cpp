// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: dr7inner.cpp
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

DataReel 6.4: the plain interface to the DataReel 7.x vector classes
(see dr7inner.h). Built by dr7features/inner/CMakeLists.txt against
the DataReel 7.x headers ONLY: never include a DataReel 6.x header in
this file. The 7.x names are used qualified (DATAREEL_NO_GLOBAL_NAMES).
*/
// ----------------------------------------------------------- //
#define DATAREEL_NO_GLOBAL_NAMES
#include "gxdlcode.h"

#include <string.h>
#include <new>
#include <string>
#include <vector>
#include "gxvindex.h"
#include "gxvset.h"
#include "dr7inner.h"

namespace dr = datareel;

struct dr7v_index_s {
  dr::gxVectorIndex vix;
  int read_only;
};

struct dr7s_set_s {
  dr::gxVectorSet set;
  std::string last_error;
};

static void make_filter(const dr7v_filter *in, dr::gxvFilter &out)
{
  if(!in) return;
  for(unsigned i = 0; i < DR7_TAG_COUNT; i++) {
    if(in->mask & (1u << i)) out.Range(i, in->lo[i], in->hi[i]);
  }
}

extern "C" {

const char *dr7_version_string(void) { return DATAREEL_VERSION_STRING; }
int dr7_version_major(void) { return DATAREEL_VERSION_MAJOR; }
int dr7_version_minor(void) { return DATAREEL_VERSION_MINOR; }

const char *dr7v_error_message(int err)
{
  if(err == DR7_NO_MEMORY) return "Out of memory";
  if(err == DR7_BAD_HANDLE) return "No vector index or set (null handle)";
  return dr::gxVectorIndex::ErrorMessage((dr::gxvError)err);
}

dr7v_index *dr7v_new(void)
{
  dr7v_index *h = new (std::nothrow) dr7v_index;
  if(h) h->read_only = 0;
  return h;
}

void dr7v_free(dr7v_index *h) { delete h; }

int dr7v_create(dr7v_index *h, const char *fname, unsigned dim, int type,
		int metric, unsigned M, const char *spec,
		unsigned ef_construction)
{
  if(!h) return DR7_BAD_HANDLE;
  h->read_only = 0;
  return h->vix.Create(fname, dim, (dr::gxvElementType)type,
		       (dr::gxvMetric)metric, M, spec, ef_construction);
}

int dr7v_open(dr7v_index *h, const char *fname, int read_only,
	      const char *spec)
{
  if(!h) return DR7_BAD_HANDLE;
  h->read_only = read_only ? 1 : 0;
  return h->vix.Open(fname, read_only ? dr::gxDBASE_READONLY :
		     dr::gxDBASE_READWRITE, spec);
}

int dr7v_close(dr7v_index *h) { return h ? h->vix.Close() : DR7_BAD_HANDLE; }
int dr7v_flush(dr7v_index *h) { return h ? h->vix.Flush() : DR7_BAD_HANDLE; }
int dr7v_is_open(const dr7v_index *h) { return h ? h->vix.IsOpen() : 0; }

int dr7v_info_get(const dr7v_index *h, dr7v_info *info)
{
  if(!h || !info) return DR7_BAD_HANDLE;
  info->dim = h->vix.Dim();
  info->type = (int)h->vix.ElementType();
  info->metric = (int)h->vix.Metric();
  info->count = h->vix.Count();
  info->deleted = h->vix.Deleted();
  info->read_only = h->read_only;
  return DR7_OK;
}

int dr7v_insert(dr7v_index *h, long long id, const float *vec,
		const unsigned *tags)
{
  return h ? h->vix.Insert(id, vec, tags) : DR7_BAD_HANDLE;
}

int dr7v_update(dr7v_index *h, long long id, const float *vec,
		const unsigned *tags)
{
  return h ? h->vix.Update(id, vec, tags) : DR7_BAD_HANDLE;
}

int dr7v_delete(dr7v_index *h, long long id)
{
  return h ? h->vix.Delete(id) : DR7_BAD_HANDLE;
}

int dr7v_contains(const dr7v_index *h, long long id)
{
  return h ? h->vix.Contains(id) : 0;
}

int dr7v_get(const dr7v_index *h, long long id, float *vec, unsigned *tags)
{
  return h ? h->vix.Get(id, vec, tags) : DR7_BAD_HANDLE;
}

int dr7v_search(const dr7v_index *h, const float *query, unsigned k,
		unsigned ef, int exact, const dr7v_filter *filter,
		dr7v_result *results, unsigned *count)
{
  if(count) *count = 0;
  if(!h) return DR7_BAD_HANDLE;
  if(k > 0 && !results) return dr::gxv_BAD_ARG;
  try {
    dr::gxvFilter f;
    make_filter(filter, f);
    std::vector<dr::gxvResult> r;
    dr::gxvError err = exact ?
      h->vix.Exact(query, k, r, filter ? &f : 0) :
      h->vix.Search(query, k, r, ef, filter ? &f : 0);
    if(err != dr::gxv_NO_ERROR) return err;
    unsigned n = (unsigned)r.size();
    if(n > k) n = k;
    for(unsigned i = 0; i < n; i++) {
      results[i].id = r[i].id;
      results[i].distance = r[i].distance;
      memcpy(results[i].tags, r[i].tags, sizeof(results[i].tags));
      results[i].file = 0;
    }
    if(count) *count = n;
    return DR7_OK;
  }
  catch(const std::bad_alloc &) {
    return DR7_NO_MEMORY;
  }
}

int dr7v_repair(dr7v_index *h) { return h ? h->vix.Repair() : DR7_BAD_HANDLE; }

unsigned long dr7v_verify(dr7v_index *h, char *report, unsigned report_size)
{
  if(report && report_size) report[0] = 0;
  if(!h) return 1;
  std::string rep;
  unsigned long problems = h->vix.Verify(0, 0, 0, &rep);
  if(report && report_size) {
    size_t n = rep.size() < report_size - 1 ? rep.size() : report_size - 1;
    memcpy(report, rep.data(), n);
    report[n] = 0;
  }
  return problems;
}

dr7s_set *dr7s_new(void) { return new (std::nothrow) dr7s_set; }
void dr7s_free(dr7s_set *h) { delete h; }

int dr7s_create(dr7s_set *h, const char *path, const char *name,
		unsigned dim, int type, int metric, const char *spec)
{
  if(!h) return DR7_BAD_HANDLE;
  return h->set.Create(path, name, dim, (dr::gxvElementType)type,
		       (dr::gxvMetric)metric, spec);
}

int dr7s_open(dr7s_set *h, const char *path, const char *spec,
	      unsigned max_open)
{
  if(!h) return DR7_BAD_HANDLE;
  return h->set.Open(path, spec, max_open ? max_open : 16);
}

void dr7s_close(dr7s_set *h) { if(h) h->set.Close(); }

int dr7s_add_file(dr7s_set *h, const char *rel_path)
{
  return h ? h->set.AddFile(rel_path) : DR7_BAD_HANDLE;
}

int dr7s_remove_file(dr7s_set *h, const char *rel_path)
{
  return h ? h->set.RemoveFile(rel_path) : DR7_BAD_HANDLE;
}

int dr7s_refresh(dr7s_set *h) { return h ? h->set.Refresh() : DR7_BAD_HANDLE; }

unsigned dr7s_file_count(const dr7s_set *h)
{
  return h ? (unsigned)h->set.Files().size() : 0;
}

unsigned dr7s_file_path(const dr7s_set *h, unsigned file, char *buf,
			unsigned buf_size)
{
  if(buf && buf_size) buf[0] = 0;
  if(!h || file >= h->set.Files().size()) return 0;
  const std::string &p = h->set.Files()[file].path;
  if(buf && buf_size) {
    size_t n = p.size() < buf_size - 1 ? p.size() : buf_size - 1;
    memcpy(buf, p.data(), n);
    buf[n] = 0;
  }
  return (unsigned)p.size();
}

unsigned dr7s_dim(const dr7s_set *h) { return h ? h->set.Dim() : 0; }

int dr7s_search(const dr7s_set *h, const float *query, unsigned k,
		unsigned ef, const dr7v_filter *filter, int day_range,
		unsigned day_lo, unsigned day_hi, unsigned threads,
		int strict, dr7v_result *results, unsigned *count)
{
  if(count) *count = 0;
  if(!h) return DR7_BAD_HANDLE;
  if(k > 0 && !results) return dr::gxv_BAD_ARG;
  try {
    dr::gxvFilter f;
    make_filter(filter, f);
    dr::gxvSetSearch opt;
    opt.k = k;
    opt.ef = ef ? ef : 64;
    opt.filter = filter ? &f : 0;
    opt.day_range = day_range ? 1 : 0;
    opt.day_lo = day_lo;
    opt.day_hi = day_hi;
    opt.threads = threads ? threads : 1;
    opt.strict = strict ? 1 : 0;
    std::vector<dr::gxvSetResult> r;
    dr::gxvSetStats stats;
    dr::gxvError err = h->set.Search(query, opt, r, &stats);
    const_cast<dr7s_set *>(h)->last_error = stats.error;
    if(err != dr::gxv_NO_ERROR) return err;
    unsigned n = (unsigned)r.size();
    if(n > k) n = k;
    for(unsigned i = 0; i < n; i++) {
      results[i].id = r[i].id;
      results[i].distance = r[i].distance;
      memcpy(results[i].tags, r[i].tags, sizeof(results[i].tags));
      results[i].file = r[i].file;
    }
    if(count) *count = n;
    return DR7_OK;
  }
  catch(const std::bad_alloc &) {
    return DR7_NO_MEMORY;
  }
}

const char *dr7s_error(const dr7s_set *h)
{
  if(!h) return "";
  if(!h->last_error.empty()) return h->last_error.c_str();
  return h->set.Error().c_str();
}

} // extern "C"
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
