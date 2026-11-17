// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: dr7inner.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/17/2026
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
#include <exception>
#include <stdint.h>
#include "gxvindex.h"
#include "gxvset.h"
#include "gxobjrepl.h"
// The 7.x multiplex session (gxdbmux.cpp) and so the object client are
// POSIX only in DataReel 7.4: on Windows the dr7c_ calls
// below return an error.
#if !defined(_WIN32)
#define DR7_OBJECT_CLIENT 1
#include "gxstream.h"
#include "gxdbmux.h"
#include "gxdbobject.h"
#endif
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

#ifdef DR7_OBJECT_CLIENT
struct dr7c_client_s {
  dr7c_client_s() : mux(0) { }
  dr::gxStream st;               // Must outlive mux (its transport)
  dr::gxdMuxSession *mux;
  std::string err;
  std::vector<std::string> names; // File paths of the last search
};
#else
struct dr7c_client_s {
  std::string err;
  std::vector<std::string> names;
};
static const char *dr7c_posix_only =
  "the DataReel 7.x object client is POSIX only (DataReel 7.4 builds its multiplex session on POSIX systems only)";
#endif

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

// The object server client
#ifdef DR7_OBJECT_CLIENT
dr7c_client *dr7c_new(void) { return new (std::nothrow) dr7c_client; }

void dr7c_free(dr7c_client *h)
{
  if(!h) return;
  dr7c_close(h);
  delete h;
}

int dr7c_connect(dr7c_client *h, const char *host, int port,
		 const char *server_name, const char *cert, const char *key,
		 const char *ca, const char *db_name, int timeout_secs)
{
  if(!h) return -1;
  dr7c_close(h);
  h->err.clear();
  if(!host || !server_name || !cert || !key || !ca || !db_name) {
    h->err = "host, server name, certificate, key, CA and database name are needed";
    return -1;
  }
  try {
    dr::gxsTLSConnection &c = h->st.Connection();
    c.SetCAFile(ca);
    c.SetClientCert(cert, key);
    c.SetVerifyPeer(true);
    dr::gxSocket sock;
    if(sock.Connect(host, port, timeout_secs > 0 ? timeout_secs : 10) != 0 ||
       c.AttachClient(std::move(sock), server_name) != 0) {
      h->err = "cannot connect: " + c.Error();
      return -1;
    }
    h->mux = new dr::gxdMuxSession(dr::gxdMuxStreamTransport(h->st));
    h->mux->Start();
    if(h->mux->Hello(h->err) != 0 ||
       h->mux->Open(1, "object", db_name, "read", 0, h->err) != 0) {
      dr7c_close(h);
      if(h->err.empty()) h->err = "the server refused the channel";
      return -1;
    }
    return 0;
  }
  catch(const std::exception &e) {
    h->err = e.what();
    dr7c_close(h);
    return -1;
  }
}

void dr7c_close(dr7c_client *h)
{
  if(!h || !h->mux) return;
  h->mux->Shutdown();
  delete h->mux;
  h->mux = 0;
}

int dr7c_is_connected(const dr7c_client *h) { return h && h->mux && h->mux->Alive(); }

// One request on channel 1: the op and body out, the result back
static int dr7c_call(dr7c_client *h, unsigned char op,
		     const std::vector<unsigned char> &body,
		     std::vector<unsigned char> &result)
{
  if(!h || !h->mux) { if(h) h->err = "not connected"; return -1; }
  std::vector<unsigned char> m(1, op);
  m.insert(m.end(), body.begin(), body.end());
  if(h->mux->Send(1, m, dr::gxdMUX_LAST, h->err) != 0) return -1;
  std::vector<unsigned char> back;
  uint64_t seq;
  if(h->mux->ReceiveMessage(1, back, seq, 30000, h->err) != 0) return -1;
  if(back.empty() || back[0] != 0) {
    // status 1: a str error follows
    h->err = "the server refused the request";
    if(back.size() >= 5) {
      uint32_t n; memcpy(&n, back.data() + 1, 4);
      if(n && 5 + (size_t)n <= back.size()) h->err.assign((const char *)back.data() + 5, n);
    }
    return -1;
  }
  result.assign(back.begin() + 1, back.end());
  return 0;
}

static void p32(std::vector<unsigned char> &v, uint32_t x)
{
  unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4);
}

static int g32(const std::vector<unsigned char> &v, size_t &o, uint32_t &x)
{
  if(o + 4 > v.size()) return -1;
  memcpy(&x, v.data() + o, 4); o += 4;
  return 0;
}

int dr7c_vector_info(dr7c_client *h, dr7c_vinfo *info, char *spec,
		     unsigned spec_size)
{
  if(spec && spec_size) spec[0] = 0;
  if(!h || !info) return -1;
  std::vector<unsigned char> body, res;
  if(dr7c_call(h, dr::gxOBJ_VECTOR_INFO, body, res) != 0) return -1;
  size_t o = 0;
  uint32_t ver, dim, el, met, nf, sl;
  if(g32(res, o, ver) || g32(res, o, dim) || g32(res, o, el) || g32(res, o, met) ||
     g32(res, o, nf) || o + 16 > res.size()) { h->err = "short VECTOR_INFO reply"; return -1; }
  uint64_t nodes, del;
  memcpy(&nodes, res.data() + o, 8); memcpy(&del, res.data() + o + 8, 8); o += 16;
  if(g32(res, o, sl) || o + sl > res.size()) { h->err = "short VECTOR_INFO reply"; return -1; }
  info->version = ver; info->dim = dim; info->type = (int)el; info->metric = (int)met;
  info->files = nf; info->nodes = nodes; info->deleted = del;
  if(spec && spec_size) {
    size_t n = sl < spec_size - 1 ? sl : spec_size - 1;
    memcpy(spec, res.data() + o, n);
    spec[n] = 0;
  }
  return 0;
}

int dr7c_vector_search(dr7c_client *h, const float *query, unsigned dim,
		       unsigned k, unsigned ef, const dr7v_filter *filter,
		       int day_range, unsigned day_lo, unsigned day_hi,
		       int strict, dr7v_result *results, unsigned *count,
		       unsigned *skipped, unsigned *missing)
{
  if(count) *count = 0;
  if(skipped) *skipped = 0;
  if(missing) *missing = 0;
  if(!h) return -1;
  h->names.clear();
  if(!query || dim == 0 || (k > 0 && !results)) { h->err = "bad arguments"; return -1; }
  std::vector<unsigned char> body, res;
  p32(body, dr::gxObjVectorVersion); p32(body, k); p32(body, ef); p32(body, dim);
  p32(body, (day_range ? 1u : 0u) | (strict ? 2u : 0u));
  p32(body, filter ? filter->mask : 0);
  for(int i = 0; i < DR7_TAG_COUNT; i++) p32(body, filter ? filter->lo[i] : 0);
  for(int i = 0; i < DR7_TAG_COUNT; i++) p32(body, filter ? filter->hi[i] : 0);
  p32(body, day_lo); p32(body, day_hi);
  body.insert(body.end(), (const unsigned char *)query, (const unsigned char *)query + 4 * (size_t)dim);
  if(dr7c_call(h, dr::gxOBJ_VECTOR_SEARCH, body, res) != 0) return -1;
  size_t o = 0;
  uint32_t ver, n, sk, mi;
  if(g32(res, o, ver) || g32(res, o, n) || g32(res, o, sk) || g32(res, o, mi)) {
    h->err = "short VECTOR_SEARCH reply"; return -1;
  }
  if(n > k || o + (size_t)n * 32 > res.size()) { h->err = "bad VECTOR_SEARCH reply"; return -1; }
  for(uint32_t i = 0; i < n; i++) {
    memcpy(&results[i].id, res.data() + o, 8);
    memcpy(&results[i].distance, res.data() + o + 8, 4);
    memcpy(results[i].tags, res.data() + o + 12, 16);
    memcpy(&results[i].file, res.data() + o + 28, 4);
    o += 32;
  }
  uint32_t nn;
  if(g32(res, o, nn)) { h->err = "short VECTOR_SEARCH reply"; return -1; }
  for(uint32_t i = 0; i < nn; i++) {
    uint32_t f, sl;
    if(g32(res, o, f) || g32(res, o, sl) || o + sl > res.size()) { h->err = "bad file names"; return -1; }
    if(f < 100000) {
      if(h->names.size() <= f) h->names.resize(f + 1);
      h->names[f].assign((const char *)res.data() + o, sl);
    }
    o += sl;
  }
  if(count) *count = n;
  if(skipped) *skipped = sk;
  if(missing) *missing = mi;
  return 0;
}

#else // Windows: no 7.x object client
dr7c_client *dr7c_new(void) { return new (std::nothrow) dr7c_client; }
void dr7c_free(dr7c_client *h) { delete h; }
int dr7c_connect(dr7c_client *h, const char *, int, const char *, const char *,
		 const char *, const char *, const char *, int)
{
  if(h) h->err = dr7c_posix_only;
  return -1;
}
void dr7c_close(dr7c_client *) { }
int dr7c_is_connected(const dr7c_client *) { return 0; }
int dr7c_vector_info(dr7c_client *h, dr7c_vinfo *, char *spec, unsigned spec_size)
{
  if(spec && spec_size) spec[0] = 0;
  if(h) h->err = dr7c_posix_only;
  return -1;
}
int dr7c_vector_search(dr7c_client *h, const float *, unsigned, unsigned, unsigned,
		       const dr7v_filter *, int, unsigned, unsigned, int,
		       dr7v_result *, unsigned *count, unsigned *skipped, unsigned *missing)
{
  if(count) *count = 0;
  if(skipped) *skipped = 0;
  if(missing) *missing = 0;
  if(h) h->err = dr7c_posix_only;
  return -1;
}
#endif // DR7_OBJECT_CLIENT

unsigned dr7c_file_name(const dr7c_client *h, unsigned file, char *buf,
			unsigned buf_size)
{
  if(buf && buf_size) buf[0] = 0;
  if(!h || file >= h->names.size()) return 0;
  const std::string &p = h->names[file];
  if(buf && buf_size) {
    size_t n = p.size() < buf_size - 1 ? p.size() : buf_size - 1;
    memcpy(buf, p.data(), n);
    buf[n] = 0;
  }
  return (unsigned)p.size();
}

const char *dr7c_error(const dr7c_client *h) { return h ? h->err.c_str() : ""; }

// 11/17/2026: object replication (gxObjRepl)
struct dr7r_node_s {
  dr::gxObjReplConfig cfg;
  dr::gxObjRepl node;
  std::string err;
};

static int dr7r_fail(dr7r_node *h, const std::string &why)
{
  if(h) h->err = why;
  return -1;
}

// The node's error after a call that failed
static int dr7r_rc(dr7r_node *h, int rc)
{
  if(rc != 0) h->err = h->node.Error();
  return rc == 0 ? 0 : -1;
}

static unsigned copy_out(const std::string &s, char *buf, unsigned buf_size)
{
  if(buf && buf_size) {
    size_t n = s.size() < buf_size - 1 ? s.size() : buf_size - 1;
    memcpy(buf, s.data(), n);
    buf[n] = 0;
  }
  return (unsigned)s.size();
}

#define DR7R_TRY(h, expr) \
  if(!h) return -1; \
  h->err.clear(); \
  try { expr } \
  catch(const std::exception &e) { return dr7r_fail(h, e.what()); }

dr7r_node *dr7r_new(void) { return new (std::nothrow) dr7r_node; }

void dr7r_free(dr7r_node *h)
{
  if(!h) return;
  try { h->node.Close(); } catch(...) { }
  delete h;
}

int dr7r_config_set(dr7r_node *h, const char *key, const char *value)
{
  DR7R_TRY(h, {
    std::string why;
    if(!key || !value) return dr7r_fail(h, "a key and a value are needed");
    if(!h->cfg.Set(key, value, &why)) return dr7r_fail(h, why);
    return 0;
  })
}

int dr7r_config_load(dr7r_node *h, const char *fname)
{
  DR7R_TRY(h, {
    std::string why;
    if(!fname) return dr7r_fail(h, "no file name");
    if(!h->cfg.Load(fname, &why)) return dr7r_fail(h, why);
    return 0;
  })
}

void dr7r_config_clear(dr7r_node *h) { if(h) h->cfg = dr::gxObjReplConfig(); }

int dr7r_open(dr7r_node *h, const char *root, int role)
{
  DR7R_TRY(h, {
    if(!root) return dr7r_fail(h, "no node folder");
    if(role != DR7R_PRIMARY && role != DR7R_COPY) return dr7r_fail(h, "the role is DR7R_PRIMARY or DR7R_COPY");
    return dr7r_rc(h, h->node.Open(root, h->cfg, role == DR7R_PRIMARY ? dr::gxObjRepl::PRIMARY : dr::gxObjRepl::COPY));
  })
}

void dr7r_close(dr7r_node *h) { if(h) { try { h->node.Close(); } catch(...) { } } }
int dr7r_is_open(const dr7r_node *h) { return h && h->node.IsOpen(); }
const char *dr7r_error(const dr7r_node *h) { return h ? h->err.c_str() : "no node (null handle)"; }

int dr7r_open_database(dr7r_node *h, const char *db)
{
  DR7R_TRY(h, { return dr7r_rc(h, h->node.OpenDatabase(db ? db : "")); })
}

int dr7r_begin(dr7r_node *h) { DR7R_TRY(h, { return dr7r_rc(h, h->node.Begin()); }) }

unsigned long long dr7r_new_id(dr7r_node *h)
{
  if(!h) return 0;
  h->err.clear();
  try {
    uint64_t id = h->node.NewId();
    if(id == 0) h->err = h->node.Error();
    return id;
  }
  catch(const std::exception &e) { h->err = e.what(); return 0; }
}

int dr7r_put(dr7r_node *h, const char *db, unsigned long long id,
	     const void *bytes, unsigned n)
{
  DR7R_TRY(h, {
    if(!bytes && n) return dr7r_fail(h, "no object bytes");
    return dr7r_rc(h, h->node.Put(db ? db : "", id, bytes, n));
  })
}

int dr7r_remove(dr7r_node *h, const char *db, unsigned long long id)
{
  DR7R_TRY(h, { return dr7r_rc(h, h->node.Remove(db ? db : "", id)); })
}

int dr7r_purge(dr7r_node *h, const char *db, unsigned long long id)
{
  DR7R_TRY(h, { return dr7r_rc(h, h->node.Purge(db ? db : "", id)); })
}

int dr7r_commit(dr7r_node *h) { DR7R_TRY(h, { return dr7r_rc(h, h->node.Commit()); }) }
int dr7r_rollback(dr7r_node *h) { DR7R_TRY(h, { return dr7r_rc(h, h->node.Rollback()); }) }
int dr7r_pump(dr7r_node *h) { DR7R_TRY(h, { return dr7r_rc(h, h->node.Pump()); }) }
int dr7r_drain(dr7r_node *h, int seconds) { DR7R_TRY(h, { return dr7r_rc(h, h->node.Drain(seconds)); }) }
int dr7r_serve(dr7r_node *h) { DR7R_TRY(h, { return dr7r_rc(h, h->node.Serve()); }) }
void dr7r_stop(dr7r_node *h) { if(h) h->node.Stop(); }

int dr7r_get(dr7r_node *h, const char *db, unsigned long long id,
	     void *buf, unsigned buf_size, unsigned *n)
{
  if(n) *n = 0;
  DR7R_TRY(h, {
    std::vector<unsigned char> b;
    if(dr7r_rc(h, h->node.Get(db ? db : "", id, b)) != 0) return -1;
    if(n) *n = (unsigned)b.size();
    if(b.size() > buf_size || (!buf && !b.empty())) return dr7r_fail(h, "the buffer is too small for the object");
    if(!b.empty()) memcpy(buf, b.data(), b.size());
    return 0;
  })
}

int dr7r_is_live(dr7r_node *h, const char *db, unsigned long long id)
{
  if(!h) return 0;
  try { return h->node.IsLive(db ? db : "", id); } catch(...) { return 0; }
}

unsigned long dr7r_ids(dr7r_node *h, const char *db, int tombstones,
		       unsigned long long *ids, unsigned long max)
{
  if(!h) return 0;
  h->err.clear();
  try {
    std::vector<uint64_t> v = h->node.Ids(db ? db : "", tombstones ? dr::gxObjRepl::TOMBSTONES : dr::gxObjRepl::LIVE);
    if(ids) for(unsigned long i = 0; i < v.size() && i < max; i++) ids[i] = v[i];
    return (unsigned long)v.size();
  }
  catch(const std::exception &e) { h->err = e.what(); return 0; }
}

unsigned dr7r_databases(dr7r_node *h, char *buf, unsigned buf_size)
{
  if(buf && buf_size) buf[0] = 0;
  if(!h) return 0;
  try {
    std::string all;
    for(const std::string &d : h->node.Databases()) { all += d; all += '\n'; }
    return copy_out(all, buf, buf_size);
  }
  catch(const std::exception &e) { h->err = e.what(); return 0; }
}

int dr7r_find_key(dr7r_node *h, const char *index, const void *key,
		  unsigned n, unsigned long long *ids, unsigned max,
		  unsigned *count)
{
  if(count) *count = 0;
  DR7R_TRY(h, {
    std::vector<uint64_t> v;
    if(dr7r_rc(h, h->node.FindKey(index ? index : "", key, n, v)) != 0) return -1;
    if(count) *count = (unsigned)v.size();
    if(ids) for(unsigned i = 0; i < v.size() && i < max; i++) ids[i] = v[i];
    return 0;
  })
}

int dr7r_nearest(dr7r_node *h, const char *index, const float *query,
		 unsigned k, dr7r_hit *hits, unsigned *count)
{
  if(count) *count = 0;
  DR7R_TRY(h, {
    std::vector<dr::gxObjReplHit> v;
    if(!query) return dr7r_fail(h, "no query vector");
    if(dr7r_rc(h, h->node.Nearest(index ? index : "", query, k, v)) != 0) return -1;
    unsigned c = 0;
    for(; c < v.size() && c < k; c++) if(hits) { hits[c].id = v[c].id; hits[c].distance = v[c].distance; }
    if(count) *count = c;
    return 0;
  })
}

int dr7r_in_snapshot(dr7r_node *h) { return h ? h->node.InSnapshot() : 0; }
unsigned long long dr7r_epoch(const dr7r_node *h) { return h ? h->node.Epoch() : 0; }
unsigned long long dr7r_last_seq(const dr7r_node *h) { return h ? h->node.LastSeq() : 0; }
unsigned long long dr7r_applied_seq(const dr7r_node *h) { return h ? h->node.AppliedSeq() : 0; }
unsigned long long dr7r_copy_applied(const dr7r_node *h) { return h ? h->node.CopyApplied() : 0; }
long dr7r_snapshots(const dr7r_node *h) { return h ? h->node.Snapshots() : 0; }

// A static gxObjRepl call on a folder: its line or message to buf
#define DR7R_FOLDER(call) \
  std::string out; \
  int rc; \
  try { rc = call; } \
  catch(const std::exception &e) { out = e.what(); rc = -1; } \
  copy_out(out, buf, buf_size); \
  return rc;

int dr7r_digest(const char *root, const char *links, char *buf, unsigned buf_size)
{
  DR7R_FOLDER(dr::gxObjRepl::Digest(root ? root : "", links ? links : "", out))
}
int dr7r_status(const char *root, char *buf, unsigned buf_size)
{
  DR7R_FOLDER(dr::gxObjRepl::Status(root ? root : "", out))
}
int dr7r_check(const char *root, dr7r_node *cfg, char *buf, unsigned buf_size)
{
  dr::gxObjReplConfig none;
  DR7R_FOLDER(dr::gxObjRepl::Check(root ? root : "", cfg ? cfg->cfg : none, out))
}
int dr7r_takeover(const char *root, char *buf, unsigned buf_size)
{
  DR7R_FOLDER(dr::gxObjRepl::Takeover(root ? root : "", out))
}
int dr7r_rejoin(const char *root, char *buf, unsigned buf_size)
{
  DR7R_FOLDER(dr::gxObjRepl::Rejoin(root ? root : "", out))
}
int dr7r_clear_alarm(const char *root, char *buf, unsigned buf_size)
{
  DR7R_FOLDER(dr::gxObjRepl::ClearAlarm(root ? root : "", out))
}

int dr7_engine_64(void) { return dr::gxSignatureSize == 10 ? 1 : 0; }

} // extern "C"
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
