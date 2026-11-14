// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxvmember.h
// Compiler Used: GCC, MSVC
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

Vector indexes derived from record members (DataReel 7.4, phase D).
A data file whose records keep their vectors as vector members
(gxVectorIndex::MakeMember(): "GXVM", element type, dimension, spec CRC
and the stored code) can have its .vix rebuilt from the records alone,
so a replicated copy never needs the .vix bytes: the set names the .vix
"derived" and the copy rebuilds it (gxrepl derived_rebuild running
vix_rebuild, or object by object with the "gxvmember" extractor of
gxrextract.h).

Where the member is (the extractor's config, space separated):
  member=infohog:N   InfoHog member N (from 0) of the record
  member=offset:N    a fixed field at byte N of the record (POD records)
  tags=A,B,C,D       where tags 0-3 come from: infohog:N or offset:N (a
                     4-byte unsigned value), or - (0)
The node id is the record's address in the data file, so the index of a
copy names the same records as the primary's. A record without a valid
member of the index's type, dimension and spec is not in the index (the
same on every node). Rebuilt in ascending address order, the index is
the same file, byte for byte, on every node.

Changes:
==============================================================
11/14/2026: New in DataReel 7.4.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_VECTOR_MEMBER_EXTRACT_HPP__
#define __GX_VECTOR_MEMBER_EXTRACT_HPP__

#include "gxdlcode.h"

#include <string>
#include "gxvindex.h"
#include "gxrextract.h"

namespace datareel {

struct GXDLCODE_API gxvMemberSpec {
  // where: 0 none, 1 an InfoHog member, 2 a byte offset
  int where = 0;
  unsigned at = 0;
  int tag_where[gxvTagCount] = { 0, 0, 0, 0 };
  unsigned tag_at[gxvTagCount] = { 0, 0, 0, 0 };
  // Parse the config text. 0, or -1 with why.
  int Parse(const std::string &config, std::string &why);
  // The member and the tags of a record ("len" bytes). 1 found, 0 the
  // record has no such member (or it is too short for it).
  int Extract(const unsigned char *rec, unsigned len, const unsigned char *&member, unsigned &mlen,
	      unsigned tags[gxvTagCount]) const;
};

// The "gxvmember" extractor (gxrextract.h)
GXDLCODE_API const gxrExtractorOps *gxvMemberExtractor();

// Rebuild "index_path" from every record of "data_path" in ascending
// address order (into index_path + ".rebuild", then renamed over it). The
// header comes from "like" (an index whose dimension, element type,
// metric, M, ef_construction and spec are used: the old index itself when
// it exists). "records" gets the number of records with a member. 0 ok,
// -1 with why.
GXDLCODE_API int gxvRebuildFromMembers(const std::string &data_path, const std::string &index_path,
				       const gxVectorIndex &like, const std::string &config,
				       unsigned long long &records, std::string &why);

} // namespace datareel

#endif // __GX_VECTOR_MEMBER_EXTRACT_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
