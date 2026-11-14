/* ------------------------------- */
/* -------- Start of File -------- */
/* ------------------------------- */
/* ----------------------------------------------------------- */
/* C/C++ Header File Name: gxrextract.h                         */
/* Produced By: DataReel Software Development Team              */
/* File Creation Date: 11/14/2026                               */
/* Date Last Modified: 11/14/2026                               */
/* Copyright (c) 2001-2026 DataReel Software Development        */
/* ----------------------------------------------------------- */
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

Derived index extractors (DataReel 7.4, per-object derived apply).

A replication set with "derived_apply = incremental" updates each of
its derived indexes on the copy one object at a time, from the objects
record (gxdShipObjects, gxdrepl.h) that comes with each commit, instead
of running derived_rebuild. An extractor knows how to turn one object
of a data file into the index's entries. It is a shared library named
by the set's "derived_extractor" (a path), or the built-in "gxvmember"
(vector members of InfoHog records, gxvindex.h), and gives one entry
point:

  extern "C" const gxrExtractorOps *gxr_extractor(void);

For each commit, under the apply lock and inside the copy's staged
transaction, gxrepl reads the old bytes of every noted object, applies
the commit's pages, reads the new bytes, and then for each object in
ascending address calls Remove(old bytes) (update and remove) and then
Insert(new bytes) (add and update), and finally Flush() with the
commit's lsn. Any error, or an incomplete or missing objects record,
puts the index back on a full rebuild ("rebuild needed").

Rules for an extractor:
  - deterministic: the same objects in the same order give the same
    index bytes (or, for a B-tree, the same keys);
  - no I/O but the index (and its own open), no threads, no globals;
  - Remove() of an entry that is not there is not an error (0);
  - return 0 for success, non-zero for an error, with a message of at
    most "err_len" bytes in "err".

Changes:
==============================================================
11/14/2026: New in DataReel 7.4.
==============================================================
*/
#ifndef __GX_REPL_EXTRACTOR_H__
#define __GX_REPL_EXTRACTOR_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GXR_EXTRACTOR_ABI 1

typedef struct gxrExtractorOps {
  uint32_t abi_version;                 /* GXR_EXTRACTOR_ABI */
  const char *(*Version)(void);         /* Name and version, for logs */
  /* Open the index "index_path" (attached by the caller to the copy's
     journal through the returned handle's database, if any) for the
     data file "data_path". "config" is the set's derived_extractor_config
     (may be empty). Returns a handle, or 0 with a message in err. */
  void *(*Open)(const char *index_path, const char *data_path,
		const char *config, char *err, size_t err_len);
  int (*Remove)(void *h, uint64_t object_address,
		const unsigned char *old_bytes, uint32_t len,
		char *err, size_t err_len);
  int (*Insert)(void *h, uint64_t object_address,
		const unsigned char *new_bytes, uint32_t len,
		char *err, size_t err_len);
  /* End of one commit: "lsn" is the commit's ship log lsn */
  int (*Flush)(void *h, uint64_t lsn, char *err, size_t err_len);
  void (*Close)(void *h);
} gxrExtractorOps;

typedef const gxrExtractorOps *(*gxrExtractorEntry)(void);

#ifdef __cplusplus
}
#endif

#endif /* __GX_REPL_EXTRACTOR_H__ */
