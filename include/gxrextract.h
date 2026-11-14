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

For each batch of records, under the apply lock, gxrepl reads the old
bytes of every noted object from the copy's data file and the new bytes
from the same file with the batch's pages laid over it (nothing is
written yet), then, inside a staged scope (gxdstage.h: no write of the
extractor can reach a file), calls Open(), Remove(old bytes) for every
removed and updated object, Insert(new bytes) for every added and
updated object (each in ascending address), Flush() with the batch's
last lsn and Count(). The index pages the extractor changed, its
<index>.stamp, the data pages and the copy's position are then written
as one atomic batch, and Close() is called. Any error, an incomplete or
missing objects record, or more staged pages than derived_apply_max_mb
puts the index back on a full rebuild ("rebuild needed").

Rules for an extractor:
  - deterministic: the same objects in the same order give the same
    index bytes (or, for a B-tree, the same keys);
  - no I/O but the index (and its own open), no threads, no globals;
  - keep the index open from Open() to Close() and write every header
    (gxBtree::WriteBtreeHeader(), the file header) in Flush();
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
  /* Open the index "index_path" for the data file "data_path" (read
     the data file only if need be). "config" is the set's
     derived_extractor_config (may be empty). Returns a handle, or 0
     with a message in err. */
  void *(*Open)(const char *index_path, const char *data_path,
		const char *config, char *err, size_t err_len);
  int (*Remove)(void *h, uint64_t object_address,
		const unsigned char *old_bytes, uint32_t len,
		char *err, size_t err_len);
  int (*Insert)(void *h, uint64_t object_address,
		const unsigned char *new_bytes, uint32_t len,
		char *err, size_t err_len);
  /* End of one batch: "lsn" is its last ship log lsn */
  int (*Flush)(void *h, uint64_t lsn, char *err, size_t err_len);
  /* Entries in the index now (gxrepl checks it against the data's
     object count in derived.stamp) */
  uint64_t (*Count)(void *h);
  void (*Close)(void *h);
} gxrExtractorOps;

typedef const gxrExtractorOps *(*gxrExtractorEntry)(void);

/* During an extractor call: read "len" bytes at "offset" of a file of
   the set (its full path) as it will be once the batch is applied (the
   batch's pages over the file; zeros past its end). Returns the bytes
   read, or -1 outside an extractor call. gxrepl provides it. */
int gxrReadAfterBatch(const char *path, uint64_t offset, void *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __GX_REPL_EXTRACTOR_H__ */
