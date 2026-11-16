// test_gxobjrepl_api.cpp - 11/16/2026: the gxObjRepl calls the objrepl tool does not make, in one process: a copy
// serving in a thread, Put/Get/Remove/Purge, FindKey and Nearest on both nodes, Rollback, NewId, and the errors.
//   test_gxobjrepl_api DIR PKI_DIR PORT       (PKI_DIR: ca.pem, copy.pem/key, primary.pem/key; CN objrepl-primary)
#include "gxdlcode.h"
#include <stdio.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include "gxobjrepl.h"
using namespace datareel;

static int pass = 0, fail = 0;
static void check(bool ok, const std::string &what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); (ok ? pass : fail)++; }

struct Rec { char key[8]; float v[4]; char text[20]; };   // B-tree key at 0, vector at 8

int main(int argc, char **argv)
{
  if(argc < 4) { fprintf(stderr, "usage: test_gxobjrepl_api DIR PKI_DIR PORT\n"); return 64; }
  std::string dir = argv[1], pki = argv[2], port = argv[3];
  gxObjReplConfig pc, cc;
  std::string why;
  bool ok = pc.Set("ca", pki + "/ca.pem") && pc.Set("cert", pki + "/primary.pem") && pc.Set("key", pki + "/primary.key") &&
	    pc.Set("copy", "127.0.0.1:" + port) && pc.Set("index", "k:ev:B:0:8,v:ev:V:8:4") && pc.Set("name", "P");
  ok = ok && cc.Set("ca", pki + "/ca.pem") && cc.Set("cert", pki + "/copy.pem") && cc.Set("key", pki + "/copy.key") &&
       cc.Set("listen", "127.0.0.1:" + port) && cc.Set("allow-cn", "objrepl-primary") && cc.Set("index", "k:ev:B:0:8,v:ev:V:8:4") &&
       cc.Set("name", "C") && cc.Set("peer-name", "P");
  check(ok, "config: Set() takes every key");
  check(!pc.Set("listen", "x:0", &why) && !pc.Set("nope", "1", &why) && why == "unknown key nope", "config: a bad value and an unknown key are refused");
  pc.log = cc.log = [](const std::string &) {};               // quiet

  // config errors at Open
  {
    gxObjReplConfig bad = pc; bad.links = "ev:8:ev";            // a link field inside the vector field
    gxObjRepl r;
    int rc = r.Open(dir + "/bad", bad, gxObjRepl::PRIMARY);
    check(rc == -1 && r.Error().find("overlaps") != std::string::npos, "open: an index over a link field is refused [" + r.Error() + "]");
  }

  gxObjRepl c;
  int orc = c.Open(dir + "/c", cc, gxObjRepl::COPY);
  check(orc == 0, "open copy " + c.Error());
  std::thread srv([&]() { c.Serve(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  gxObjRepl p;
  orc = p.Open(dir + "/p", pc, gxObjRepl::PRIMARY);
  check(orc == 0, "open primary " + p.Error());
  check(p.Put("ev", 1, "x", 1) == -1, "Put() outside a transaction is refused");
  check(p.OpenDatabase("ev") == 0, "OpenDatabase");

  // 30 objects: keys 0..2 (10 each), vectors (i, 0, 0, 0)
  std::vector<uint64_t> ids;
  p.Begin();
  for(int i = 0; i < 30; i++) {
    Rec r; memset(&r, 0, sizeof(r)); r.key[0] = (char)('a' + i % 3); r.v[0] = (float)i; snprintf(r.text, sizeof(r.text), "object %d", i);
    uint64_t id = p.NewId(); ids.push_back(id);
    if(p.Put("ev", id, &r, sizeof(r)) != 0) check(false, "put " + p.Error());
  }
  check(p.Commit() == 0, "commit 30 objects");
  check(p.Drain(30) == 0, "drain: the copy has commit " + std::to_string(p.LastSeq()));

  // a rollback leaves nothing behind, and the node still works
  p.Begin();
  Rec t; memset(&t, 0, sizeof(t)); t.key[0] = 'z';
  uint64_t rid = p.NewId();
  p.Put("ev", rid, &t, sizeof(t));
  check(p.Rollback() == 0 && !p.IsLive("ev", rid), "rollback: the object is gone");
  std::vector<uint64_t> f;
  check(p.FindKey("k", "z", 1, f) == 0 && f.empty(), "rollback: its key is gone from the index");

  // remove 5 of key 'a', update one, purge one of the removed
  p.Begin();
  int removed = 0;
  for(int i = 0; i < 30 && removed < 5; i += 3) { p.Remove("ev", ids[i]); removed++; }
  Rec u; memset(&u, 0, sizeof(u)); u.key[0] = 'b'; u.v[0] = 100; strcpy(u.text, "updated");
  p.Put("ev", ids[2], &u, sizeof(u));                          // key c -> b
  check(p.Purge("ev", ids[1]) == -1, "Purge() of a live object is refused");
  p.Purge("ev", ids[0]);
  check(p.Commit() == 0, "commit: 5 removes, an update, a purge");
  check(p.Drain(30) == 0, "drain");
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // both nodes: the same answers from their own indexes
  for(gxObjRepl *n : {&p, &c}) {
    std::string who = n == &p ? "primary" : "copy";
    std::vector<uint64_t> a, b, cc2;
    n->FindKey("k", "a", 1, a); n->FindKey("k", "b", 1, b); n->FindKey("k", "c", 1, cc2);
    check(a.size() == 5 && b.size() == 11 && cc2.size() == 9, who + ": FindKey a/b/c = " + std::to_string(a.size()) + "/" +
	  std::to_string(b.size()) + "/" + std::to_string(cc2.size()) + " (want 5/11/9)");
    float q[4] = {7.2f, 0, 0, 0};
    std::vector<gxObjReplHit> h;
    n->Nearest("v", q, 3, h);
    bool near = h.size() == 3 && h[0].id == ids[7] && (h[1].id == ids[8] || h[1].id == ids[6]);
    check(near, who + ": Nearest(7.2) = ids of objects 7, 6/8");
    std::vector<unsigned char> bytes;
    check(n->Get("ev", ids[2], bytes) == 0 && bytes.size() == sizeof(Rec) && strcmp(((Rec *)bytes.data())->text, "updated") == 0,
	  who + ": Get() returns the update");
    check(n->Get("ev", ids[3], bytes) == -1 && !n->IsLive("ev", ids[3]), who + ": a removed object is not live");
    std::vector<uint64_t> tombs = n->Ids("ev", gxObjRepl::TOMBSTONES);
    check(tombs.size() == 4 && std::find(tombs.begin(), tombs.end(), ids[0]) == tombs.end(),
	  who + ": 4 tombstones (5 removed, 1 purged)");
    check(n->Ids("ev").size() == 25, who + ": 25 live objects");
  }
  check(c.AppliedSeq() == p.LastSeq() && p.CopyApplied() == p.LastSeq(), "seq: copy applied " + std::to_string(c.AppliedSeq()) +
	" = primary " + std::to_string(p.LastSeq()));
  p.Close();
  c.Stop();
  srv.join();
  c.Close();
  std::string a, b;
  gxObjRepl::Digest(dir + "/p", "", a); gxObjRepl::Digest(dir + "/c", "", b);
  check(a == b, "digest: primary = copy (" + a + ")");
  std::string rep;
  check(gxObjRepl::IndexCheck(dir + "/c", cc.index, rep) == 0, "the copy's indexes are consistent");
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
