// vector_search.cpp - a sample for the DataReel 7 AI guides (docs/ai/object-database.md):
// notes with a vector each, in a data file and a vector index written in one journal
// transaction; the 3 nearest notes to a query, read back by address; and a day range.
// Build: examples/CMakeLists.txt builds it as ai_vector_search. Run it in an empty folder.
#include <cstdio>
#include <cstring>
#include <iostream>
#include "gxdbase.h"
#include "gxdjrnl.h"
#include "gxvindex.h"

using namespace datareel;

struct Note {              // fixed size: no std::string or pointers in a record
  char text[40];
  unsigned day;            // yyyymmdd
};

int main()
{
  const unsigned DIM = 4;
  gxdJournal j;
  gxDatabase db;
  gxVectorIndex vix;
  j.Open("notes.jnl");                                   // the journal first: it recovers
  if(db.Open("notes.gxd") != gxDBASE_NO_ERROR) db.Create("notes.gxd");
  if(vix.Open("notes.vix", gxDBASE_READWRITE, "notes/v1") != gxv_NO_ERROR)
    vix.Create("notes.vix", DIM, gxvFLOAT32, gxvCOSINE, 16, "notes/v1");
  j.Attach(&db);
  j.Attach(vix.Database());

  const char *texts[5] = { "flush to the open", "reclaim of vwap", "halt resumed", "gap down fill", "vwap hold" };
  const float vecs[5][4] = { { 1, 0, 0, 0 }, { 0, 1, 0.2f, 0 }, { 0, 0, 1, 0 }, { 0.9f, 0.1f, 0, 0 }, { 0, 1, 0, 0.1f } };
  if(vix.Count() == 0) {
    j.Begin();                                           // the note and its node together, or neither
    for(int i = 0; i < 5; i++) {
      Note n;
      memset(&n, 0, sizeof(n));
      strncpy(n.text, texts[i], sizeof(n.text) - 1);
      n.day = 20261111 + i % 3;
      FAU_t a = db.Alloc(sizeof(n));
      db.Write(&n, sizeof(n), a);
      unsigned tags[gxvTagCount] = { 0, 0, n.day, 0 };   // tag 2: the day
      vix.Insert((long long)a, vecs[i], tags);           // id = the note's address
    }
    if(j.Commit() != gxDBASE_NO_ERROR) { std::cout << "commit failed\n"; return 1; }
  }

  const float query[4] = { 0, 1, 0, 0 };
  std::vector<gxvResult> hits;
  vix.Search(query, 3, hits);                            // closest first
  std::cout << "nearest to the query:\n";
  for(auto &h : hits) {
    Note n;
    db.Read(&n, sizeof(n), (FAU_t)h.id);
    std::printf("  %-20s day %u  distance %.3f\n", n.text, n.day, h.distance);
  }

  gxvFilter f;
  f.Range(2, 20261111, 20261111);                        // only 2026-11-11
  vix.Search(query, 3, hits, 64, &f);
  std::cout << "on 2026-11-11 only: " << hits.size() << " note(s)\n";

  j.Close();                                             // checkpoint, then the files
  vix.Close();
  db.Close();
  return 0;
}
