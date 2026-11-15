// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: vector_records.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/15/2026
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
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Records that carry their own vectors (vector members, gxvindex.h and
gxvmember.h, DataReel 7.4):

  1. Fixed records in a gxDatabase file: a name, a day, a category and a
     vector member (MakeMember(): a 12-byte head and the stored code, here
     float16). Each record and its index node are written in the same
     gxdJournal transaction; the node id is the record's address.
  2. A search returns addresses: the program reads the records back and
     prints their names. MemberVector() decodes a record's member.
  3. Verify() with a resolver that reads each node's record and checks
     that its member still matches the node (MemberMatches()).
  4. The index made again from the records alone (gxvRebuildFromMembers,
     what vix_rebuild and a replicated copy do): the member and tag
     places are given as "member=offset:32 tags=offset:28,-,offset:24,-".
     The rebuilt index and the one written with the records hold the
     same nodes: the same logical digest.

  vector_records [WORK_FOLDER]
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include "gxdbase.h"
#include "gxdjrnl.h"
#include "gxvindex.h"
#include "gxvmember.h"

using namespace datareel;

static const unsigned DIM = 16;
static const unsigned MEMBER = gxvMemberHead + 2 * DIM;   // float16: 2 bytes a value
static int failures = 0;

static void check(bool ok, const char *what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
  if(!ok) failures++;
}

// The record: name at 0, day at 24, category at 28, the vector member at 32
struct Record {
  char name[24];
  uint32_t day;
  uint32_t category;
  unsigned char member[MEMBER];
};

struct Resolver { gxDatabase *db; const gxVectorIndex *vix; };

// Verify()'s resolver: the record at "id" exists and its member is the node's
static int still_there(long long id, void *data)
{
  Resolver *r = (Resolver *)data;
  Record rec;
  if(r->db->Read(&rec, sizeof(rec), (FAU_t)id) != gxDBASE_NO_ERROR) return 0;
  return r->vix->MemberMatches(id, rec.member, MEMBER);
}

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : "vector_records_work";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  const std::string data_path = dir + "/items.gxd", index_path = dir + "/items.vix";
  const char *categories[3] = { "tool", "part", "kit" };

  // 1. Records and their index in one transaction per batch -----------------
  printf("1. 600 records with float16 vector members\n");
  {
    gxdJournal j;
    gxDatabase db;
    gxVectorIndex vix;
    j.Open((dir + "/items.jnl").c_str());
    db.Create(data_path.c_str());
    vix.Create(index_path.c_str(), DIM, gxvFLOAT16, gxvCOSINE, 16, "example/items", 200);
    j.Attach(&db);
    j.Attach(vix.Database());
    std::mt19937 g(11);
    std::normal_distribution<float> n(0.0f, 1.0f);
    std::vector<float> v(DIM);
    std::vector<unsigned char> m;
    for(unsigned batch = 0; batch < 6; batch++) {
      j.Begin();
      for(unsigned k = 0; k < 100; k++) {
	unsigned i = batch * 100 + k;
	Record rec;
	memset(&rec, 0, sizeof(rec));
	snprintf(rec.name, sizeof(rec.name), "%s-%04u", categories[i % 3], i);
	rec.day = 20261110 + i % 5;
	rec.category = i % 3;
	for(unsigned d = 0; d < DIM; d++) v[d] = n(g) + (d == i % 3 ? 4.0f : 0.0f);   // each category leans one way
	vix.MakeMember(v.data(), m);
	memcpy(rec.member, m.data(), MEMBER);
	FAU_t a = db.Alloc(sizeof(rec));
	db.Write(&rec, sizeof(rec), a);
	unsigned tags[gxvTagCount] = { rec.category, 0, rec.day, 0 };
	vix.InsertMember((long long)a, rec.member, MEMBER, tags);   // the node id is the record's address
      }
      if(j.Commit() != gxDBASE_NO_ERROR) { printf("commit failed\n"); return 1; }
    }
    printf("  %llu records indexed, %u-byte member (head %u + code %u)\n", vix.Count(), vix.MemberSize(),
	   (unsigned)gxvMemberHead, vix.CodeSize());
    check(vix.Count() == 600, "every record has its node");
    j.Close();
    vix.Close();
    db.Close();
  }

  // 2. Search, then read the records -----------------------------------------
  printf("2. The 5 records nearest a \"part\"-like vector, read back by address\n");
  {
    gxDatabase db;
    gxVectorIndex vix;
    db.Open(data_path.c_str(), gxDBASE_READONLY);
    vix.Open(index_path.c_str(), gxDBASE_READONLY);
    std::vector<float> q(DIM, 0.0f);
    q[1] = 4.0f;
    std::vector<gxvResult> r;
    vix.Search(q.data(), 5, r, 64);
    bool parts = !r.empty();
    for(auto &x : r) {
      Record rec;
      db.Read(&rec, sizeof(rec), (FAU_t)x.id);
      std::vector<float> back(DIM);
      vix.MemberVector(rec.member, MEMBER, back.data());
      printf("  %-10s day %u  distance %.4f  member[1] %.2f\n", rec.name, rec.day, x.distance, back[1]);
      parts = parts && rec.category == 1;
    }
    check(parts, "the nearest records are all of category \"part\"");

  // 3. Verify with a resolver ------------------------------------------------
    printf("3. Verify() reading every node's record\n");
    Resolver res = { &db, &vix };
    std::vector<long long> bad;
    std::string report;
    unsigned long problems = vix.Verify(still_there, &res, &bad, &report);
    printf("  %lu problem(s)\n", problems);
    check(problems == 0, "every node's record exists and holds the node's code");
    vix.Close();
    db.Close();
  }

  // 4. The index made again from the records alone ---------------------------
  printf("4. The index rebuilt from the records (as vix_rebuild does)\n");
  {
    gxVectorIndex like;
    like.Open(index_path.c_str(), gxDBASE_READONLY);
    unsigned long long n_before = 0, d_before = like.LogicalDigest(&n_before);
    std::string rebuilt = dir + "/items_rebuilt.vix";
    std::filesystem::copy_file(index_path, rebuilt, std::filesystem::copy_options::overwrite_existing, ec);
    unsigned long long records = 0;
    std::string why;
    int rc = gxvRebuildFromMembers(data_path, rebuilt, like, "member=offset:32 tags=offset:28,-,offset:24,-",
				   records, why);
    like.Close();
    check(rc == 0, rc == 0 ? "rebuilt" : why.c_str());
    gxVectorIndex again;
    again.Open(rebuilt.c_str(), gxDBASE_READONLY);
    unsigned long long n_after = 0, d_after = again.LogicalDigest(&n_after);
    printf("  %llu records with a member; digest %016llx (%llu nodes) against %016llx (%llu nodes)\n", records,
	   d_after, n_after, d_before, n_before);
    check(d_after == d_before && n_after == n_before, "the rebuilt index holds the same nodes and tags");
    check((again.FeaturesOptional() & gxvOptFromMembers) != 0, "it is marked as derived from record members");
    again.Close();
  }

  printf(failures ? "%d check(s) failed\n" : "All checks pass\n", failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
