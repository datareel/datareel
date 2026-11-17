// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxddigest.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

What a database file holds, as one 64-bit number, whatever its layout
(DataReel 7.4). Two copies of a database are the same when their
digests are:

  objects  the live objects of a gxDatabase file, in any order
  btree    the keys of a gxBtree index file, in the tree's order (the
           same for any node layout, and for the old 4-byte layout)
  bytes    the file's bytes

Changes:
==============================================================
11/16/2026: Moved here from the gxrepl replication code (gxReplDigest,
gxdReplHash), which 7.4 no longer has: the same digests and the same
hash, so digests printed by 7.4 betas compare equal.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_DIGEST_HPP__
#define __GX_DATABASE_DIGEST_HPP__

#include "gxdlcode.h"

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace datareel {

// A fast 64-bit hash of n bytes (not cryptographic), chained with seed
GXDLCODE_API uint64_t gxDatabaseHash(const void *p, size_t n, uint64_t seed = 0);

// The digest of the file at path: kind "objects", "btree" or "bytes"
// (empty: "btree" for a .btx file, else "bytes"). count is the number
// of objects, keys or bytes. Returns 0, or -1 with the reason in err.
GXDLCODE_API int gxDatabaseDigest(const std::string &path, const std::string &kind, uint64_t &hash,
				  uint64_t &count, std::string &err);

} // namespace datareel

#endif // __GX_DATABASE_DIGEST_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
