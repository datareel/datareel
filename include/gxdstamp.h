// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdstamp.h
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
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
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Derived index stamps (DataReel 7.4, per-object derived apply). A copy
that updates a derived index object by object keeps, next to the index,
"<index>.stamp" (for example by_symbol.btx.stamp): a small gxDatabase
file whose static area holds the ship log id and the last lsn applied
to the index. The copy changes it in the same journal transaction as
the index, so the two never disagree after a crash. A full rebuild
writes a fresh stamp (WriteFresh(): a new file renamed into place).

Stamps exist only on copies. They match the set's derived patterns
(gxrepl treats "<derived>.stamp" as derived: never sent, never a data
file), so a primary's stamp is never shipped over a copy's. A copy
without a stamp (older code, or a set in rebuild mode) is in rebuild
mode: at most a warning.

Static area, 64 bytes (native byte order):
   0  "GXDSTAMP"
   8  uint32 format (1)
  12  uint32 digest kind (1 B-tree logical, 2 bit-for-bit)
  16  16 hex digits: the ship log id
  32  uint64 applied lsn
  40  uint64 records (objects in the index, as the extractor counts)
  48  uint64 time (microseconds since 1970)
  56  uint32 flags (0)
  60  uint32 CRC-32 of bytes 0-59

Changes:
==============================================================
11/14/2026: New in DataReel 7.4.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_DERIVED_STAMP_HPP__
#define __GX_DATABASE_DERIVED_STAMP_HPP__

#include "gxdlcode.h"
#include "gxdbase.h"

#include <stdint.h>
#include <string>

namespace datareel {

const uint32_t gxdStampFormat = 1;
const uint32_t gxdStampDigestBtree = 1; // Compare keys, not bytes
const uint32_t gxdStampDigestBytes = 2; // Bit-for-bit (vector indexes)
const size_t gxdStampSize = 64;

struct gxdStampData {
  std::string log_id;               // 16 hex digits
  unsigned long long lsn = 0;       // Last ship log record applied
  unsigned long long records = 0;
  unsigned long long time_us = 0;
  uint32_t digest_kind = 0;
  uint32_t flags = 0;
};

class GXDLCODE_API gxdDerivedStamp
{
public:
  gxdDerivedStamp() { }
  ~gxdDerivedStamp() { Close(); }

private:
  gxdDerivedStamp(const gxdDerivedStamp &);
  void operator=(const gxdDerivedStamp &);

public:
  static std::string PathFor(const std::string &index_path) {
    return index_path + ".stamp";
  }
  // 1 if "name" (a file name or path) is a stamp
  static int IsStamp(const std::string &name);

  // Open (with "create", make) the stamp. Attach File() to the index's
  // journal and Write() inside its transactions.
  gxDatabaseError Open(const char *path, int create = 1);
  void Close();
  gxDatabase *File() { return &f; }
  // 0, or -1 if the stamp is not open or damaged (a damaged stamp means
  // "rebuild needed")
  int Read(gxdStampData &d);
  gxDatabaseError Write(const gxdStampData &d);

  // Read a stamp file without keeping it open: 0, 1 none, -1 damaged
  static int ReadFile(const std::string &path, gxdStampData &d);
  // A fresh stamp after a rebuild: written to "<path>.tmp", synced and
  // renamed over "path". 0, or -1.
  static int WriteFresh(const std::string &path, const gxdStampData &d);

private:
  gxDatabase f;
};

} // namespace datareel

#endif // __GX_DATABASE_DERIVED_STAMP_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
