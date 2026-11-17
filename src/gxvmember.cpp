// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxvmember.cpp
// Compiler Used: GCC, MSVC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
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

Vector indexes derived from record members (gxvmember.h).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sstream>
#include <vector>
#include "gxvmember.h"
#include "gpersist.h"

namespace datareel {

static int parse_where(const std::string &s, int &where, unsigned &at)
{
  if(s == "-") { where = 0; at = 0; return 0; }
  size_t c = s.find(':');
  if(c == std::string::npos) return -1;
  std::string kind = s.substr(0, c), num = s.substr(c + 1);
  if(num.empty() || num.find_first_not_of("0123456789") != std::string::npos || num.size() > 9) return -1;
  at = (unsigned)strtoul(num.c_str(), 0, 10);
  if(kind == "infohog") where = 1;
  else if(kind == "offset") where = 2;
  else return -1;
  return 0;
}

int gxvMemberSpec::Parse(const std::string &config, std::string &why)
{
  *this = gxvMemberSpec();
  std::istringstream in(config);
  std::string w;
  while(in >> w) {
    size_t eq = w.find('=');
    std::string k = eq == std::string::npos ? w : w.substr(0, eq), v = eq == std::string::npos ? "" : w.substr(eq + 1);
    if(k == "member") {
      if(parse_where(v, where, at) != 0 || where == 0) { why = "bad member=" + v + " (infohog:N or offset:N)"; return -1; }
    }
    else if(k == "tags") {
      unsigned i = 0;
      size_t a = 0;
      while(a <= v.size()) {
	size_t b = v.find(',', a);
	if(b == std::string::npos) b = v.size();
	if(i >= gxvTagCount || parse_where(v.substr(a, b - a), tag_where[i], tag_at[i]) != 0) {
	  why = "bad tags=" + v + " (up to 4 of infohog:N, offset:N or -)"; return -1;
	}
	i++;
	a = b + 1;
      }
    }
    else { why = "unknown setting " + k; return -1; }
  }
  if(where == 0) { why = "no member="; return -1; }
  return 0;
}

// An InfoHog record as InfoHog<TYPE>::Write() lays it out: its object header, the member count, then per
// member its length and the bytes. The count and lengths are longs of the data file's layout (11/15/2026:
// gxvMemberSpec::SetLayout(); 8 bytes canonical, 4 in an old 4-byte layout file), little endian.
static unsigned long long ih_get(const unsigned char *p, size_t w)
{
  unsigned long long v = 0;
  for(size_t i = w; i-- > 0; ) v = (v << 8) | p[i];
  return v;
}
static int infohog_member(const unsigned char *rec, unsigned len, unsigned n, const unsigned char *&p, unsigned &plen,
			  size_t ih_word, size_t ih_ohdr)
{
  size_t off = ih_ohdr;
  if(len < off + ih_word) return 0;
  unsigned long long num = ih_get(rec + off, ih_word);
  off += ih_word;
  if((unsigned long long)n >= (unsigned long long)num) return 0;
  for(unsigned i = 0; i <= n; i++) {
    if(len - off < ih_word) return 0;
    unsigned long long lw = ih_get(rec + off, ih_word);
    if(lw > 0xFFFFFFFFULL) return 0;
    unsigned l = (unsigned)lw;
    off += ih_word;
    if(l > len - off) return 0;
    if(i == n) { p = rec + off; plen = l; return 1; }
    off += l;
  }
  return 0;
}

int gxvMemberSpec::Extract(const unsigned char *rec, unsigned len, const unsigned char *&member, unsigned &mlen,
			   unsigned tags[gxvTagCount]) const
{
  member = 0; mlen = 0;
  for(unsigned i = 0; i < gxvTagCount; i++) tags[i] = 0;
  if(!rec) return 0;
  if(where == 1) {
    if(!infohog_member(rec, len, at, member, mlen, ih_word, ih_ohdr)) return 0;
  }
  else if(where == 2) {
    if(at >= len) return 0;
    member = rec + at;
    mlen = len - at;        // the caller takes MemberSize() bytes of it
  }
  else return 0;
  for(unsigned i = 0; i < gxvTagCount; i++) {
    const unsigned char *t = 0;
    unsigned tl = 0;
    if(tag_where[i] == 1) { if(!infohog_member(rec, len, tag_at[i], t, tl, ih_word, ih_ohdr) || tl < 4) return 0; }
    else if(tag_where[i] == 2) { if(tag_at[i] > len || len - tag_at[i] < 4) return 0; t = rec + tag_at[i]; }
    else continue;
    memcpy(&tags[i], t, 4);
  }
  return 1;
}

// 11/16/2026: the "gxvmember" extractor for gxrepl (gxrextract.h) went with gxrepl (7.4 replication is gxObjRepl)

// ---------------- Rebuild ----------------

int gxvRebuildFromMembers(const std::string &data_path, const std::string &index_path, const gxVectorIndex &like,
			  const std::string &config, unsigned long long &records, std::string &why)
{
  records = 0;
  if(!like.IsOpen()) { why = "no index to take the header from"; return -1; }
  gxvMemberSpec spec;
  if(spec.Parse(config, why) != 0) return -1;
  gxDatabase db;
  if(db.Open(data_path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { why = "cannot open " + data_path; return -1; }
  spec.SetLayout(db);                       // 11/15/2026: the data file's layout
  std::string tmp = index_path + ".rebuild";
  remove(tmp.c_str());
  {
    gxVectorIndex out;
    gxvError e = out.Create(tmp.c_str(), like.Dim(), like.ElementType(), like.Metric(), like.M(), like.Spec(),
			    like.EfConstruction());
    if(e != gxv_NO_ERROR) { why = tmp + ": " + gxVectorIndex::ErrorMessage(e); return -1; }
    if(like.PartitionCount()) out.SetPartition(like.PartitionId(), like.PartitionCount());
    out.SetFromMembers(1);
    std::vector<unsigned char> b;
    // ascending address: the same order, so the same file, on every node
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ n = db.ObjectLength(a);
      if(n == 0) continue;
      b.resize(n);
      if(db.Read(b.data(), n, a) != gxDBASE_NO_ERROR) { why = "cannot read the record at " + std::to_string((long long)a); return -1; }
      const unsigned char *m;
      unsigned ml, t[gxvTagCount];
      if(!spec.Extract(b.data(), (unsigned)n, m, ml, t)) continue;
      if(spec.where == 2) { if(ml < out.MemberSize()) continue; ml = out.MemberSize(); }
      e = out.InsertMember((long long)a, m, ml, t);
      if(e == gxv_NO_ERROR) records++;
      else if(e != gxv_BAD_ARG && e != gxv_SPEC_MISMATCH && e != gxv_NOT_FINITE) {
	why = "insert: " + std::string(gxVectorIndex::ErrorMessage(e)); return -1;
      }
    }
    if(out.Flush() != gxv_NO_ERROR || out.Close() != gxv_NO_ERROR) { why = "cannot write " + tmp; return -1; }
  }
  db.Close();
  {
    gxDatabase s;           // synced before it replaces the old index
    if(s.Open(tmp.c_str()) == gxDBASE_NO_ERROR) { s.Sync(1); s.Close(); }
  }
#if defined (__WIN32__) || defined (_WIN32)
  remove(index_path.c_str());
#endif
  if(rename(tmp.c_str(), index_path.c_str()) != 0) { remove(tmp.c_str()); why = "cannot rename " + tmp; return -1; }
  return 0;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
