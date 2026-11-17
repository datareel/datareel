// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: csv_merge.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
// Date Last Modified: 11/17/2026
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

csv_merge: merges two spreadsheets exported as CSV on a key column
(gxCSVReader and gxCSVWriter, gxcsv.h). From dougg's merge_ss (2009).

  csv_merge [--inner] [--delimiter C] KEY A.csv B.csv [OUT.csv]

Both files start with a header row; KEY names the column to merge on.
The output has A's columns, then B's other columns (a B column with
the name of an A column is called "b.NAME"). Each row of A is written
with the B row of the same key (B's columns empty if there is none);
then the rows of B whose key is not in A, unless --inner (only keys in
both). A key twice in B: the first row is used and a warning given.
OUT.csv is written with CR LF line ends; without it the rows go to
standard output. A summary goes to standard error. Exit 0, 1 on a
usage or read error.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "gxcsv.h"

using namespace datareel;

static int usage()
{
  fprintf(stderr, "usage: csv_merge [--inner] [--delimiter C] KEY A.csv B.csv [OUT.csv]\n");
  return 1;
}

static bool read_all(const std::string &fn, const gxCSVOptions &o, std::vector<std::string> &header,
		     std::vector<std::vector<std::string>> &rows)
{
  gxCSVReader r(o);
  if(!r.Open(fn)) { fprintf(stderr, "csv_merge: %s\n", r.ErrorMessage().c_str()); return false; }
  if(!r.ReadHeader()) { fprintf(stderr, "csv_merge: %s has no header row%s%s\n", fn.c_str(), r.Error() ? ": " : "", r.ErrorMessage().c_str()); return false; }
  header = r.Header();
  std::vector<std::string> row;
  while(r.ReadRow(row)) rows.push_back(row);
  if(r.Error()) { fprintf(stderr, "csv_merge: %s: %s\n", fn.c_str(), r.ErrorMessage().c_str()); return false; }
  return true;
}

int main(int argc, char **argv)
{
  gxCSVOptions o;
  o.skip_empty_rows = true;
  bool inner = false;
  std::vector<std::string> args;
  for(int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--inner") inner = true;
    else if(a == "--delimiter" && i + 1 < argc) { std::string d = argv[++i]; o.delimiter = d == "\\t" ? '\t' : d.empty() ? ',' : d[0]; }
    else if(a.size() > 1 && a[0] == '-') return usage();
    else args.push_back(a);
  }
  if(args.size() < 3 || args.size() > 4) return usage();
  const std::string key = args[0];
  std::vector<std::string> ha, hb;
  std::vector<std::vector<std::string>> ra, rb;
  if(!read_all(args[1], o, ha, ra) || !read_all(args[2], o, hb, rb)) return 1;
  int ka = -1, kb = -1;
  for(size_t i = 0; i < ha.size(); i++) if(ha[i] == key) ka = (int)i;
  for(size_t i = 0; i < hb.size(); i++) if(hb[i] == key) kb = (int)i;
  if(ka < 0 || kb < 0) { fprintf(stderr, "csv_merge: no column %s in %s\n", key.c_str(), ka < 0 ? args[1].c_str() : args[2].c_str()); return 1; }

  // the output header: A's columns, then B's other columns
  std::set<std::string> anames(ha.begin(), ha.end());
  std::vector<std::string> head = ha;
  std::vector<int> bcols;
  for(size_t i = 0; i < hb.size(); i++) {
    if((int)i == kb) continue;
    bcols.push_back((int)i);
    head.push_back(anames.count(hb[i]) ? "b." + hb[i] : hb[i]);
  }
  // B's rows by key (the first of a key)
  std::map<std::string, size_t> bkey;
  long dup = 0;
  for(size_t i = 0; i < rb.size(); i++) {
    std::string k = (size_t)kb < rb[i].size() ? rb[i][kb] : std::string();
    if(!bkey.emplace(k, i).second) { if(!dup) fprintf(stderr, "csv_merge: warning: key %s is in %s more than once (the first row is used)\n", k.c_str(), args[2].c_str()); dup++; }
  }

  gxCSVWriter w(o);
  std::string text;
  if(args.size() == 4) { if(!w.Open(args[3])) { fprintf(stderr, "csv_merge: %s\n", w.ErrorMessage().c_str()); return 1; } }
  else w.SetOutput(&text);
  w.WriteRow(head);
  long both = 0, only_a = 0, only_b = 0;
  std::set<std::string> akeys;
  auto cell = [](const std::vector<std::string> &r, size_t i) { return i < r.size() ? r[i] : std::string(); };
  for(auto &r : ra) {
    std::vector<std::string> out(ha.size());
    for(size_t i = 0; i < ha.size(); i++) out[i] = cell(r, i);
    std::string k = cell(r, ka);
    akeys.insert(k);
    auto it = bkey.find(k);
    if(it == bkey.end()) { if(inner) continue; only_a++; for(size_t j = 0; j < bcols.size(); j++) out.push_back(""); }
    else { both++; for(int c : bcols) out.push_back(cell(rb[it->second], c)); }
    w.WriteRow(out);
  }
  if(!inner) {
    for(size_t i = 0; i < rb.size(); i++) {
      std::string k = cell(rb[i], kb);
      if(akeys.count(k) || bkey[k] != i) continue;            // in A, or a later duplicate
      std::vector<std::string> out(ha.size());
      out[ka] = k;
      for(int c : bcols) out.push_back(cell(rb[i], c));
      only_b++;
      w.WriteRow(out);
    }
  }
  bool ok = w.Close();
  if(args.size() == 3) fwrite(text.data(), 1, text.size(), stdout);
  fprintf(stderr, "csv_merge: %ld row(s) in both, %ld only in %s, %ld only in %s%s%s\n", both, only_a, args[1].c_str(),
	  only_b, args[2].c_str(), inner ? " (--inner: rows in both only)" : "", dup ? ", duplicate keys in B" : "");
  if(!ok) { fprintf(stderr, "csv_merge: %s\n", w.ErrorMessage().c_str()); return 1; }
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
