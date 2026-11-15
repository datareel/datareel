// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: dr7inner.h
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

#ifdef __cplusplus
}
#endif

#endif // __DR7_INNER_H__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
