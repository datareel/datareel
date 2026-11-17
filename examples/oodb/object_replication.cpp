// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: object_replication.cpp
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

Object replication with gxObjRepl (gxobjrepl.h, DataReel 7.4): a
primary and a copy, a few commits, then a planned switch.

Two nodes, folders WORK/a and WORK/b, run in this one process (the copy
serves in a thread), on Linux and Windows alike:

  1. a is the primary and b the copy. a commits three transactions of
     notes and drains them to b, which reads them back.
  2. Both stop. The two folders hold the same objects (Digest()).
  3. A planned switch: Takeover() makes b the primary at epoch 2, and
     Rejoin() makes a its copy (a's copy had every commit, so no full
     send is needed).
  4. b commits two more transactions and drains them to a. The digests
     agree again.

Usage: object_replication PKI WORK [PORT]

  PKI   the certificates of utils/objrepl/objrepl_certs.sh PKI
        --copy-host 127.0.0.1 --apply (ca.pem, copy.pem/key, the
        server side; primary.pem/key, the client side)
  WORK  an empty folder for the two nodes
  PORT  the first of two ports on 127.0.0.1 (default 24750)

Exit code 0 when every step worked and the digests agree.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#include "gxobjrepl.h"

using namespace datareel;

static int fail(const char *what, const std::string &why)
{
  printf("FAILED: %s: %s\n", what, why.c_str());
  return 1;
}

// A node's config: the copy listens with the server certificate, the
// primary dials with the client certificate
static gxObjReplConfig node_config(const std::string &pki, const std::string &name,
				   const std::string &peer, bool copy, int port)
{
  gxObjReplConfig cfg;
  cfg.name = name;
  cfg.peer_name = peer;
  cfg.ca = pki + "/ca.pem";
  if(copy) {
    cfg.cert = pki + "/copy.pem";
    cfg.key = pki + "/copy.key";
    cfg.listen_port = port;                    // on 127.0.0.1
    cfg.allow_cn = "objrepl-primary";          // the primary's certificate CN
  }
  else {
    cfg.cert = pki + "/primary.pem";
    cfg.key = pki + "/primary.key";
    cfg.copy = "127.0.0.1:" + std::to_string(port);
  }
  cfg.log = [](const std::string &) { };       // quiet: the example prints its own lines
  return cfg;
}

// Commits n transactions of two notes each on a primary
static int write_notes(gxObjRepl &p, int first, int n, std::vector<uint64_t> &ids)
{
  for(int t = first; t < first + n; t++) {
    if(p.Begin() != 0) return -1;
    for(int k = 0; k < 2; k++) {
      std::string note = "note " + std::to_string(t) + "." + std::to_string(k);
      uint64_t id = p.NewId();
      if(p.Put("notes", id, note.data(), (uint32_t)note.size()) != 0) { p.Rollback(); return -1; }
      ids.push_back(id);
    }
    if(p.Commit() != 0) return -1;
  }
  return 0;
}

static std::string digest(const std::string &root)
{
  std::string line;
  if(gxObjRepl::Digest(root, "", line) != 0) return "(no digest: " + line + ")";
  return line;
}

// One phase: the copy serves in a thread while the primary writes and
// drains, then both close
static int phase(gxObjRepl &prim, gxObjRepl &copy, int first, int n,
		 std::vector<uint64_t> &ids)
{
  std::thread server([&copy]() { copy.Serve(); });
  int rc = 0;
  if(prim.OpenDatabase("notes") != 0) rc = fail("OpenDatabase", prim.Error());
  else if(write_notes(prim, first, n, ids) != 0) rc = fail("commit", prim.Error());
  else if(prim.Drain(30) != 0) rc = fail("drain", prim.Error());
  else {
    printf("  primary: last commit %llu, the copy has %llu\n",
	   (unsigned long long)prim.LastSeq(), (unsigned long long)prim.CopyApplied());
    std::vector<unsigned char> b;
    if(copy.Get("notes", ids.back(), b) != 0) rc = fail("read on the copy", copy.Error());
    else printf("  the copy reads its newest note: \"%s\"\n", std::string(b.begin(), b.end()).c_str());
  }
  prim.Close();
  copy.Stop();
  server.join();
  copy.Close();
  return rc;
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    printf("Usage: %s PKI WORK [PORT]\n", argv[0]);
    return 2;
  }
  std::string pki = argv[1], work = argv[2];
  int port = argc > 3 ? atoi(argv[3]) : 24750;
  std::string ra = work + "/a", rb = work + "/b";
  std::vector<uint64_t> ids;
  std::string msg;

  printf("1. a is the primary, b the copy: three commits\n");
  {
    gxObjRepl a, b;
    if(b.Open(rb, node_config(pki, "b", "a", true, port), gxObjRepl::COPY) != 0) return fail("open b", b.Error());
    if(a.Open(ra, node_config(pki, "a", "b", false, port), gxObjRepl::PRIMARY) != 0) return fail("open a", a.Error());
    if(phase(a, b, 1, 3, ids) != 0) return 1;
  }

  printf("2. both stopped: the same objects?\n");
  std::string da = digest(ra), db = digest(rb);
  printf("  a: %s\n  b: %s\n", da.c_str(), db.c_str());
  if(da != db) return fail("digest", "a and b differ");

  printf("3. a planned switch: b takes over, a rejoins as its copy\n");
  if(gxObjRepl::Takeover(rb, msg) != 0) return fail("takeover", msg);
  printf("  %s\n", msg.c_str());
  if(gxObjRepl::Rejoin(ra, msg) != 0) return fail("rejoin", msg);
  printf("  %s\n", msg.c_str());

  printf("4. b is the primary, a the copy: two more commits\n");
  {
    gxObjRepl a, b;
    if(a.Open(ra, node_config(pki, "a", "b", true, port + 1), gxObjRepl::COPY) != 0) return fail("open a", a.Error());
    if(b.Open(rb, node_config(pki, "b", "a", false, port + 1), gxObjRepl::PRIMARY) != 0) return fail("open b", b.Error());
    printf("  b is the primary at epoch %llu\n", (unsigned long long)b.Epoch());
    if(phase(b, a, 4, 2, ids) != 0) return 1;
  }
  da = digest(ra); db = digest(rb);
  printf("  a: %s\n  b: %s\n", da.c_str(), db.c_str());
  if(da != db) return fail("digest", "a and b differ");
  printf("OK: %d notes replicated, then a switch, the same objects on both nodes\n", (int)ids.size());
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
