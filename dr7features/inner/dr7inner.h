// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: dr7inner.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/17/2026
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

DataReel 6.4: the plain interface between a DataReel 6.x program and
the DataReel 7.x vector index and vector set classes (gxVectorIndex,
gxVectorSet, built from a DataReel 7.4 or later source tree).

This header uses no DataReel types and includes no DataReel header,
so it can be used from code built against either library. The 6.x and
7.x headers cannot be included in the same source file (they share
include guards), so dr7inner.cpp includes only 7.x headers and the
6.x facade (dr7features.h) includes only 6.x headers and this file.
Programs should use the facade classes in dr7features.h.

Return values: 0 (DR7_OK) if no errors occur, otherwise a gxvError
code of the 7.x library (see dr7v_error_message()).
*/
// ----------------------------------------------------------- //
#ifndef __DR7_INNER_H__
#define __DR7_INNER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DR7_OK 0
#define DR7_TAG_COUNT 4      // gxvTagCount
#define DR7_MAX_DIM 4096     // gxvMaxDim

// Element types and metrics (the 7.x gxvElementType and gxvMetric values)
#define DR7_FLOAT32 1
#define DR7_FLOAT16 2
#define DR7_INT8 3
#define DR7_L2 1
#define DR7_COSINE 2
#define DR7_IP 3

// Error codes this layer adds (the 7.x codes are below 100)
#define DR7_NO_MEMORY 100
#define DR7_BAD_HANDLE 101

typedef struct dr7v_result_s {
  long long id;
  float distance;
  unsigned tags[DR7_TAG_COUNT];
  unsigned file; // Vector sets: the file's index (dr7s_file_path())
} dr7v_result;

// Tag filter: bit i of mask set means lo[i] <= tag i <= hi[i]
typedef struct dr7v_filter_s {
  unsigned mask;
  unsigned lo[DR7_TAG_COUNT];
  unsigned hi[DR7_TAG_COUNT];
} dr7v_filter;

typedef struct dr7v_info_s {
  unsigned dim;
  int type;
  int metric;
  unsigned long long count;    // Live nodes
  unsigned long long deleted;
  int read_only;
} dr7v_info;

// The 7.x library: version string ("7.4 beta3") and major/minor numbers
const char *dr7_version_string(void);
int dr7_version_major(void);
int dr7_version_minor(void);
const char *dr7v_error_message(int err);

// Vector index (.vix)
typedef struct dr7v_index_s dr7v_index;
dr7v_index *dr7v_new(void);
void dr7v_free(dr7v_index *h);
int dr7v_create(dr7v_index *h, const char *fname, unsigned dim, int type,
		int metric, unsigned M, const char *spec,
		unsigned ef_construction);
int dr7v_open(dr7v_index *h, const char *fname, int read_only,
	      const char *spec);
int dr7v_close(dr7v_index *h);
int dr7v_flush(dr7v_index *h);
int dr7v_is_open(const dr7v_index *h);
int dr7v_info_get(const dr7v_index *h, dr7v_info *info);
int dr7v_insert(dr7v_index *h, long long id, const float *vec,
		const unsigned *tags);
int dr7v_update(dr7v_index *h, long long id, const float *vec,
		const unsigned *tags);
int dr7v_delete(dr7v_index *h, long long id);
int dr7v_contains(const dr7v_index *h, long long id);
int dr7v_get(const dr7v_index *h, long long id, float *vec,
	     unsigned *tags);
// exact = 1: Exact(), else Search() with ef. results holds k entries;
// *count is set to the number found.
int dr7v_search(const dr7v_index *h, const float *query, unsigned k,
		unsigned ef, int exact, const dr7v_filter *filter,
		dr7v_result *results, unsigned *count);
int dr7v_repair(dr7v_index *h);
// Number of problems; the report (one line a problem) is copied to
// report (report_size bytes, always null terminated) when not null.
unsigned long dr7v_verify(dr7v_index *h, char *report,
			  unsigned report_size);

// Vector set (.vixset)
typedef struct dr7s_set_s dr7s_set;
dr7s_set *dr7s_new(void);
void dr7s_free(dr7s_set *h);
int dr7s_create(dr7s_set *h, const char *path, const char *name,
		unsigned dim, int type, int metric, const char *spec);
int dr7s_open(dr7s_set *h, const char *path, const char *spec,
	      unsigned max_open);
void dr7s_close(dr7s_set *h);
int dr7s_add_file(dr7s_set *h, const char *rel_path);
int dr7s_remove_file(dr7s_set *h, const char *rel_path);
int dr7s_refresh(dr7s_set *h);
unsigned dr7s_file_count(const dr7s_set *h);
// The file's path relative to the manifest (buf_size bytes, null
// terminated). Returns the full length.
unsigned dr7s_file_path(const dr7s_set *h, unsigned file, char *buf,
			unsigned buf_size);
unsigned dr7s_dim(const dr7s_set *h);
// day_range = 1: only nodes with day_lo <= tag 2 <= day_hi
int dr7s_search(const dr7s_set *h, const float *query, unsigned k,
		unsigned ef, const dr7v_filter *filter, int day_range,
		unsigned day_lo, unsigned day_hi, unsigned threads,
		int strict, dr7v_result *results, unsigned *count);
const char *dr7s_error(const dr7s_set *h);

// Client of the DataReel 7.x database object server (gxdbobject.h,
// a database with "vectors = ...", e.g. the 6.4 vector_server tool):
// mutual TLS (the 7.x gxStream), one multiplex channel to database
// db_name, read mode, op 7 VECTOR_SEARCH and op 8 VECTOR_INFO.
typedef struct dr7c_client_s dr7c_client;
typedef struct dr7c_vinfo_s {
  unsigned version;
  unsigned dim;
  int type;
  int metric;
  unsigned files;
  unsigned long long nodes;
  unsigned long long deleted;
} dr7c_vinfo;
dr7c_client *dr7c_new(void);
void dr7c_free(dr7c_client *h);
// 0 if connected and the channel opened; else -1 (dr7c_error())
int dr7c_connect(dr7c_client *h, const char *host, int port,
		 const char *server_name, const char *cert, const char *key,
		 const char *ca, const char *db_name, int timeout_secs);
void dr7c_close(dr7c_client *h);
int dr7c_is_connected(const dr7c_client *h);
// spec: the index's spec text (spec_size bytes, null terminated)
int dr7c_vector_info(dr7c_client *h, dr7c_vinfo *info, char *spec,
		     unsigned spec_size);
// results holds k entries. 0 if no errors occur, else -1.
int dr7c_vector_search(dr7c_client *h, const float *query, unsigned dim,
		       unsigned k, unsigned ef, const dr7v_filter *filter,
		       int day_range, unsigned day_lo, unsigned day_hi,
		       int strict, dr7v_result *results, unsigned *count,
		       unsigned *skipped, unsigned *missing);
// The path (relative to the server's manifest) of a file the last
// search's results name, or "" (buf_size bytes, null terminated)
unsigned dr7c_file_name(const dr7c_client *h, unsigned file, char *buf,
			unsigned buf_size);
const char *dr7c_error(const dr7c_client *h);

// 11/17/2026: object replication, the DataReel 7.x gxObjRepl
// (gxobjrepl.h, DataReel 7.4 beta 5 or later): objects travel between
// two independent object databases over gxStream with mutual TLS, on
// Linux and Windows. Ids are 64-bit and shared by both nodes. Returns 0
// if no errors occur, else -1 (dr7r_error()), unless noted.
#define DR7R_PRIMARY 1
#define DR7R_COPY 2
typedef struct dr7r_node_s dr7r_node;
typedef struct dr7r_hit_s {
  unsigned long long id;
  float distance;
} dr7r_hit;
dr7r_node *dr7r_new(void);
void dr7r_free(dr7r_node *h);
// The configuration (gxObjReplConfig) used by the next dr7r_open():
// one "key value" setting (the objrepl.conf keys: name, peer-name,
// copy, listen, ca, cert, key, allow-cn, links, index, max-lag,
// max-behind, min-cert-days, idle-timeout, trim), or a whole file.
int dr7r_config_set(dr7r_node *h, const char *key, const char *value);
int dr7r_config_load(dr7r_node *h, const char *fname);
void dr7r_config_clear(dr7r_node *h);
int dr7r_open(dr7r_node *h, const char *root, int role);
void dr7r_close(dr7r_node *h);
int dr7r_is_open(const dr7r_node *h);
const char *dr7r_error(const dr7r_node *h);
// Primary: changes, committed together
int dr7r_open_database(dr7r_node *h, const char *db);
int dr7r_begin(dr7r_node *h);
unsigned long long dr7r_new_id(dr7r_node *h); // 0 on an error
int dr7r_put(dr7r_node *h, const char *db, unsigned long long id,
	     const void *bytes, unsigned n);
int dr7r_remove(dr7r_node *h, const char *db, unsigned long long id);
int dr7r_purge(dr7r_node *h, const char *db, unsigned long long id);
int dr7r_commit(dr7r_node *h);
int dr7r_rollback(dr7r_node *h);
// Replication: dr7r_pump() 0 when the copy has every commit;
// dr7r_serve() (copy) until dr7r_stop() (any thread, or a signal)
int dr7r_pump(dr7r_node *h);
int dr7r_drain(dr7r_node *h, int seconds);
int dr7r_serve(dr7r_node *h);
void dr7r_stop(dr7r_node *h);
// Reads (either role). dr7r_get(): *n is the object's size; the bytes
// are copied when it fits in buf_size (-1 and *n set when it does not).
int dr7r_get(dr7r_node *h, const char *db, unsigned long long id,
	     void *buf, unsigned buf_size, unsigned *n);
int dr7r_is_live(dr7r_node *h, const char *db, unsigned long long id);
// Live ids (tombstones = 1: removed ids): the count; up to max are
// copied to ids when not null
unsigned long dr7r_ids(dr7r_node *h, const char *db, int tombstones,
		       unsigned long long *ids, unsigned long max);
// Databases, one name a line (buf_size bytes, null terminated); the
// full length is returned
unsigned dr7r_databases(dr7r_node *h, char *buf, unsigned buf_size);
// Index lookups (gxObjReplConfig index): *count ids/hits found, at
// most max copied
int dr7r_find_key(dr7r_node *h, const char *index, const void *key,
		  unsigned n, unsigned long long *ids, unsigned max,
		  unsigned *count);
int dr7r_nearest(dr7r_node *h, const char *index, const float *query,
		 unsigned k, dr7r_hit *hits, unsigned *count);
int dr7r_in_snapshot(dr7r_node *h);
unsigned long long dr7r_epoch(const dr7r_node *h);
unsigned long long dr7r_last_seq(const dr7r_node *h);
unsigned long long dr7r_applied_seq(const dr7r_node *h);
unsigned long long dr7r_copy_applied(const dr7r_node *h);
long dr7r_snapshots(const dr7r_node *h);
// Folders, without an open node: the result line or message is copied
// to buf (buf_size bytes, null terminated). dr7r_digest(), dr7r_status(),
// dr7r_takeover(), dr7r_rejoin(), dr7r_clear_alarm(): 0 or -1;
// dr7r_check(): 0 OK, 1 WARN, 2 FAIL (gxObjRepl::Check()).
int dr7r_digest(const char *root, const char *links, char *buf,
		unsigned buf_size);
int dr7r_status(const char *root, char *buf, unsigned buf_size);
int dr7r_check(const char *root, dr7r_node *cfg, char *buf,
	       unsigned buf_size);
int dr7r_takeover(const char *root, char *buf, unsigned buf_size);
int dr7r_rejoin(const char *root, char *buf, unsigned buf_size);
int dr7r_clear_alarm(const char *root, char *buf, unsigned buf_size);
// 1 if the 7.x library uses the 64-bit database engine, else 0
int dr7_engine_64(void);

#ifdef __cplusplus
}
#endif

#endif // __DR7_INNER_H__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
