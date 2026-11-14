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
// member its length and the bytes. The count and lengths are gxUINT32 values, sizeof(gxUINT32) bytes wide
// (8 on a 64-bit native build, where gxUINT32 is unsigned long), read whole and from zero.
static const size_t ih_word = sizeof(gxUINT32);
static int infohog_member(const unsigned char *rec, unsigned len, unsigned n, const unsigned char *&p, unsigned &plen)
{
  size_t off = sizeof(gxObjectHeader);
  if(len < off + ih_word) return 0;
  gxUINT32 num = 0;
  memcpy(&num, rec + off, ih_word);
  off += ih_word;
  if((unsigned long long)n >= (unsigned long long)num) return 0;
  for(unsigned i = 0; i <= n; i++) {
    if(len - off < ih_word) return 0;
    gxUINT32 lw = 0;
    memcpy(&lw, rec + off, ih_word);
    if((unsigned long long)lw > 0xFFFFFFFFULL) return 0;
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
    if(!infohog_member(rec, len, at, member, mlen)) return 0;
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
    if(tag_where[i] == 1) { if(!infohog_member(rec, len, tag_at[i], t, tl) || tl < 4) return 0; }
    else if(tag_where[i] == 2) { if(tag_at[i] > len || len - tag_at[i] < 4) return 0; t = rec + tag_at[i]; }
    else continue;
    memcpy(&tags[i], t, 4);
  }
  return 1;
}

// ---------------- The "gxvmember" extractor ----------------

namespace {

struct MemberH {
  gxVectorIndex vix;
  gxvMemberSpec spec;
};

void say(char *err, size_t n, const std::string &m) { if(err && n) snprintf(err, n, "%s", m.c_str()); }

const char *m_version() { return "gxvmember 1 (DataReel 7.4 vector members)"; }

void *m_open(const char *index_path, const char *, const char *config, char *err, size_t n)
{
  MemberH *h = new MemberH;
  std::string why;
  if(h->spec.Parse(config ? config : "", why) != 0) { say(err, n, why); delete h; return 0; }
  gxvError e = h->vix.Open(index_path);
  if(e != gxv_NO_ERROR) { say(err, n, std::string(index_path) + ": " + gxVectorIndex::ErrorMessage(e)); delete h; return 0; }
  return h;
}

// The member of a record as the index takes it: exactly MemberSize() bytes for a fixed field
static int member_of(MemberH *h, const unsigned char *b, uint32_t len, const unsigned char *&m, unsigned &ml, unsigned t[4])
{
  if(!h->spec.Extract(b, len, m, ml, t)) return 0;
  if(h->spec.where == 2) { if(ml < h->vix.MemberSize()) return 0; ml = h->vix.MemberSize(); }
  return 1;
}

int m_remove(void *hv, uint64_t addr, const unsigned char *, uint32_t, char *err, size_t n)
{
  MemberH *h = (MemberH *)hv;
  gxvError e = h->vix.Delete((long long)addr);
  if(e == gxv_NO_ERROR || e == gxv_ID_NOT_FOUND) return 0;   // a record without a member was never in it
  say(err, n, gxVectorIndex::ErrorMessage(e));
  return -1;
}

int m_insert(void *hv, uint64_t addr, const unsigned char *b, uint32_t len, char *err, size_t n)
{
  MemberH *h = (MemberH *)hv;
  const unsigned char *m;
  unsigned ml, t[gxvTagCount];
  if(!member_of(h, b, len, m, ml, t)) return 0;              // no vector: not in the index (on every node)
  gxvError e = h->vix.InsertMember((long long)addr, m, ml, t);
  if(e == gxv_NO_ERROR || e == gxv_BAD_ARG || e == gxv_SPEC_MISMATCH || e == gxv_NOT_FINITE) return 0;
  say(err, n, gxVectorIndex::ErrorMessage(e));
  return -1;
}

int m_flush(void *hv, uint64_t, char *err, size_t n)
{
  MemberH *h = (MemberH *)hv;
  gxvError e = h->vix.Flush();
  if(e == gxv_NO_ERROR) return 0;
  say(err, n, gxVectorIndex::ErrorMessage(e));
  return -1;
}

uint64_t m_count(void *hv) { return ((MemberH *)hv)->vix.Count(); }

void m_close(void *hv)
{
  MemberH *h = (MemberH *)hv;
  h->vix.Close();
  delete h;
}

const gxrExtractorOps member_ops = { GXR_EXTRACTOR_ABI, m_version, m_open, m_remove, m_insert, m_flush, m_count, m_close };

} // namespace

const gxrExtractorOps *gxvMemberExtractor() { return &member_ops; }

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
