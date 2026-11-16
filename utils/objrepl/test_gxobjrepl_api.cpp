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
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "gxobjrepl.h"
#include "gxdbase.h"
#include "gxstream.h"
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
  int crc = c.Begin(); int cput = c.Put("ev", 999, "x", 1);
  check(crc == -1 && cput == -1 && !c.IsLive("ev", 999), "design 8: the copy refuses application writes [" + c.Error() + "]");
  p.Close();
  c.Stop();
  srv.join();
  c.Close();
  std::string a, b;
  gxObjRepl::Digest(dir + "/p", "", a); gxObjRepl::Digest(dir + "/c", "", b);
  check(a == b, "digest: primary = copy (" + a + ")");
  std::string rep;
  check(gxObjRepl::IndexCheck(dir + "/c", cc.index, rep) == 0, "the copy's indexes are consistent");
  {
    // design 2: after a takeover the new primary's ids are above every id of the old epoch
    std::string msg;
    int trc = gxObjRepl::Takeover(dir + "/c", msg);
    gxObjReplConfig tc = pc; tc.copy = "127.0.0.1:1";
    gxObjRepl t;
    int orc2 = t.Open(dir + "/c", tc, gxObjRepl::PRIMARY);
    uint64_t nid = t.NewId();
    check(trc == 0 && orc2 == 0 && t.Epoch() == 2 && nid > (2ULL << 40), "design 2: at epoch 2 NewId() = " + std::to_string(nid) +
	  " (2 << 40 = " + std::to_string(2ULL << 40) + ")");
    t.Close();
  }

  // ---------- 11/16/2026: the review's findings ----------
  {
    // a commit over the frame limit is refused at Commit() (it could never be delivered), an object over 32 MB at Put()
    gxObjReplConfig bc = pc; bc.copy = "127.0.0.1:1"; bc.index = "";
    gxObjRepl b;
    b.Open(dir + "/big", bc, gxObjRepl::PRIMARY); b.OpenDatabase("x");
    std::vector<unsigned char> mb(1024 * 1024, 7), huge(33u * 1024 * 1024, 1);
    b.Begin();
    int prc = b.Put("x", b.NewId(), huge.data(), (uint32_t)huge.size());
    check(prc == -1, "review 4: a 33 MB object is refused at Put() [" + b.Error() + "]");
    for(int i = 0; i < 70; i++) b.Put("x", b.NewId(), mb.data(), (uint32_t)mb.size());
    int rc = b.Commit();
    check(rc == -1 && b.Error().find("split it") != std::string::npos && b.LastSeq() == 0, "review 4: a 70 MB commit is refused at Commit(), nothing recorded [" + b.Error() + "]");
    b.Begin(); b.Put("x", b.NewId(), mb.data(), 1000);
    check(b.Commit() == 0 && b.LastSeq() == 1, "review 4: the next commit works");
    b.Close();

    // purge of an object a link still names is refused; once no link names it, it goes
    gxObjReplConfig lc = bc; lc.links = "a:0:b";
    gxObjRepl l;
    check(l.Open(dir + "/link", lc, gxObjRepl::PRIMARY) == 0, "open a primary with links " + l.Error());
    l.Begin();
    unsigned char tb[16] = {1}; l.Put("b", 1, tb, 16);
    uint64_t addr = (uint64_t)l.AddressOf("b", 1); unsigned char ta[16]; memset(ta, 0, 16); memcpy(ta, &addr, 8);
    l.Put("a", 2, ta, 16);
    l.Commit();
    l.Begin(); l.Remove("b", 1);
    rc = l.Purge("b", 1);
    check(rc == -1 && l.Error().find("still named by a link") != std::string::npos, "review 7: purge of a linked tombstone refused [" + l.Error() + "]");
    l.Commit();
    l.Begin(); l.Remove("a", 2);
    check(l.Purge("b", 1) == 0 && l.Commit() == 0, "review 7: purge once no live link names it");
    // NewId never hands out an id used before, even one purged since (and after a restart)
    l.Begin(); l.Put("b", 500, tb, 16); l.Commit();
    l.Begin(); l.Remove("b", 500); l.Purge("b", 500); l.Commit();
    l.Close();
    check(l.Open(dir + "/link", lc, gxObjRepl::PRIMARY) == 0, "reopen " + l.Error());
    uint64_t nid = l.NewId();
    check(nid > 500, "review 14: NewId() after the highest id was purged and the node reopened: " + std::to_string(nid));
    l.Close();
  }
  {
    // a refusal does not stick: after the copy is set right, Pump() says 0
    std::string port2 = std::to_string(atoi(port.c_str()) + 1);
    gxObjReplConfig c2 = cc; c2.listen_port = atoi(port2.c_str()); c2.peer_name = "Q"; c2.index = "";
    gxObjReplConfig p2 = pc; p2.copy = "127.0.0.1:" + port2; p2.index = "";
    gxObjRepl cp;
    cp.Open(dir + "/c2", c2, gxObjRepl::COPY);
    std::thread t1([&]() { cp.Serve(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    gxObjRepl pp;
    pp.Open(dir + "/p2", p2, gxObjRepl::PRIMARY); pp.OpenDatabase("x");
    pp.Begin(); pp.Put("x", pp.NewId(), "abcd", 4); pp.Commit();
    int r1 = pp.Pump();
    cp.Stop(); t1.join(); cp.Close();
    std::string msg; gxObjRepl::ClearAlarm(dir + "/c2", msg);
    c2.peer_name = "P";
    cp.Open(dir + "/c2", c2, gxObjRepl::COPY);
    std::thread t2([&]() { cp.Serve(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    int r2 = pp.Pump();
    check(r1 == 3 && r2 == 0 && pp.CopyApplied() == pp.LastSeq(), "review 10: refused (" + std::to_string(r1) + "), then accepted (" + std::to_string(r2) + ")");

    pp.Close();                                                // the copy serves one primary at a time
    // a client with the primary's certificate sends an 8-byte object with a 200 MB capacity: refused, nothing grows
    gxStream st; st.Connection().SetCAFile(pki + "/ca.pem"); st.SetClientCert(pki + "/primary.pem", pki + "/primary.key"); st.SetTimeOut(10);
    bool sent = false, acked = false;
    if(st.Connect("127.0.0.1", atoi(port2.c_str())) == 0) {
      std::vector<unsigned char> h;
      auto u8 = [&](uint8_t v) { h.push_back(v); };
      auto u16 = [&](uint16_t v) { u8((uint8_t)v); u8((uint8_t)(v >> 8)); };
      auto u32 = [&](uint32_t v) { for(int i = 0; i < 4; i++) u8((uint8_t)(v >> (8 * i))); };
      auto u64 = [&](uint64_t v) { for(int i = 0; i < 8; i++) u8((uint8_t)(v >> (8 * i))); };
      auto str = [&](const std::string &x) { u16((uint16_t)x.size()); h.insert(h.end(), x.begin(), x.end()); };
      u32(0x3450524fu); u64(1); str(""); str("P"); u8(1); u64(1000); u64(0);
      st.WriteBlock(h.data(), (int)h.size());
      gxBlockHeader gx; std::vector<unsigned char> rep;
      if(st.ReadHeader(gx) == 0 && st.ReadBlock(rep, gx) == 0 && !rep.empty() && rep[0] == 1) {
	h.clear();
	u8(1); u64(cp.AppliedSeq() + 1); u64(1); u32(1); u8(1); str("x"); u64(77); u32(200000000u); u8(0); u32(8); u64(0x4141414141414141ULL);
	sent = st.WriteBlock(h.data(), (int)h.size()) == 0;
	std::vector<unsigned char> ack;
	acked = st.ReadHeader(gx) == 0 && st.ReadBlock(ack, gx) == 0;
      }
      st.Close();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    struct stat sb; long long sz = stat((dir + "/c2/x.gxd").c_str(), &sb) == 0 ? (long long)sb.st_size : -1;
    check(sent && !acked && sz >= 0 && sz < 1000000 && !cp.IsLive("x", 77), "review 9: a 200 MB capacity from the wire refused (no ack, x.gxd " + std::to_string(sz) + " bytes)");
    cp.Stop(); t2.join(); cp.Close();
  }


  // ---------- 11/16/2026: the second review's findings ----------
  {
    int base = atoi(port.c_str()) + 10;
    auto pcfg = [&](int pt, bool trim) { gxObjReplConfig c = pc; c.copy = "127.0.0.1:" + std::to_string(pt); c.trim = trim; c.index = ""; return c; };
    struct Srv {
      gxObjRepl r; std::thread t;
      bool start(const std::string &d, const gxObjReplConfig &c) { if(r.Open(d, c, gxObjRepl::COPY) != 0) return false; t = std::thread([this]() { r.Serve(); }); std::this_thread::sleep_for(std::chrono::milliseconds(300)); return true; }
      void stop() { r.Stop(); t.join(); r.Close(); }
    };
    auto ccfg = [&](int pt, const std::string &name) { gxObjReplConfig c = cc; c.listen_port = pt; c.name = name; c.peer_name = ""; c.index = ""; return c; };
    auto commit1 = [&](gxObjRepl &x) { x.OpenDatabase("ev"); x.Begin(); x.Put("ev", x.NewId(), "abcdefgh", 8); return x.Commit(); };
    std::string msg;
    // 1 fence: a new-epoch primary with nothing to send must still fence the old primary out of the copy
    {
      int pc1 = base, pt1 = base + 1;
      Srv C; C.start(dir + "/f_c", ccfg(pc1, "C"));
      { gxObjRepl x; x.Open(dir + "/f_p1", pcfg(pc1, false), gxObjRepl::PRIMARY); for(int k = 0; k < 3; k++) commit1(x); x.Drain(20); x.Close(); }
      Srv T; T.start(dir + "/f_t", ccfg(pt1, "T"));
      { gxObjRepl x; x.Open(dir + "/f_p1", pcfg(pt1, false), gxObjRepl::PRIMARY); x.Drain(20); x.Close(); }
      T.stop();
      gxObjRepl::Takeover(dir + "/f_t", msg);
      { gxObjRepl t; t.Open(dir + "/f_t", pcfg(pc1, false), gxObjRepl::PRIMARY); t.Drain(20); t.Close(); }
      uint64_t ce = C.r.Epoch();
      int orc;
      { gxObjRepl x; x.Open(dir + "/f_p1", pcfg(pc1, false), gxObjRepl::PRIMARY); commit1(x); orc = x.Drain(10); x.Close(); }
      int trc;
      { gxObjRepl t; t.Open(dir + "/f_t", pcfg(pc1, false), gxObjRepl::PRIMARY); trc = t.Pump(); t.Close(); }
      check(ce == 2 && orc == 3 && C.r.AppliedSeq() == 3 && trc == 0, "review2 1: the copy took epoch 2 from a primary with nothing to send (" + std::to_string(ce) +
	    "); the old primary was refused (" + std::to_string(orc) + "), the copy stayed at 3, the new primary still streams (" + std::to_string(trc) + ")");
      C.stop();
    }
    // 2 digest log: a node that took over after a full send can check a copy at an older seq of the same history
    {
      int pc2 = base + 2, pt2 = base + 3;
      Srv C; C.start(dir + "/d_c", ccfg(pc2, "C2"));
      { gxObjRepl x; x.Open(dir + "/d_p", pcfg(pc2, true), gxObjRepl::PRIMARY); for(int k = 0; k < 3; k++) commit1(x); x.Drain(20); commit1(x); commit1(x); x.Close(); }
      C.stop();
      Srv T; T.start(dir + "/d_t", ccfg(pt2, "T2"));
      long snaps = 0;
      { gxObjRepl x; x.Open(dir + "/d_p", pcfg(pt2, true), gxObjRepl::PRIMARY); x.Drain(20); snaps = x.Snapshots(); x.Close(); }
      T.stop();
      gxObjRepl::Takeover(dir + "/d_t", msg);
      C.start(dir + "/d_c", ccfg(pc2, "C2"));
      int rc;
      { gxObjRepl t; t.Open(dir + "/d_t", pcfg(pc2, true), gxObjRepl::PRIMARY); rc = t.Drain(20); t.Close(); }
      check(snaps == 1 && rc == 0 && C.r.AppliedSeq() == 5, "review2 2: after a full send and a takeover, a copy at seq 3 of the same history is accepted and caught up (rc " +
	    std::to_string(rc) + ", at " + std::to_string(C.r.AppliedSeq()) + ")");
      C.stop();
    }
    // 3 an index that cannot be opened: its database is not kept open without it
    {
      std::string d = dir + "/ix"; mkdir(d.c_str(), 0700); mkdir((d + "/ev.k.ix.btx.building").c_str(), 0700);
      gxObjReplConfig c = pcfg(base + 4, true); c.index = "k:ev:B:0:8";
      gxObjRepl x; x.Open(d, c, gxObjRepl::PRIMARY);
      int r1 = x.OpenDatabase("ev"), r2 = x.OpenDatabase("ev");
      check(r1 == -1 && r2 == -1, "review2 3: the database stays closed while its index cannot open (" + std::to_string(r1) + ", " + std::to_string(r2) + ")");
      x.Close();
    }
    // 4 the lock is not inherited by a child process
    {
      std::string d = dir + "/lk";
      gxObjRepl x; x.Open(d, pcfg(base + 5, true), gxObjRepl::PRIMARY);
      pid_t kid = fork();
      if(kid == 0) { execlp("sleep", "sleep", "3", (char *)0); _exit(127); }
      x.Close();
      gxObjRepl y; int orc = y.Open(d, pcfg(base + 5, true), gxObjRepl::PRIMARY);
      check(orc == 0, "review2 4: the node opens again while a child of the first opener still runs [" + y.Error() + "]");
      y.Close(); kill(kid, SIGKILL); waitpid(kid, 0, 0);
    }
    // 5 a state file created but never written (a crash in the first open): the new node starts; a node with data
    // whose state file is gone is refused
    {
      std::string d = dir + "/st"; mkdir(d.c_str(), 0700);
      { gxDatabase sdb; sdb.Create((d + "/state.gxd").c_str(), (FAU_t)96); sdb.Close(); }
      gxObjRepl x; int r1 = x.Open(d, pcfg(base + 6, true), gxObjRepl::PRIMARY); commit1(x); x.Close();
      std::remove((d + "/state.gxd").c_str());
      gxObjRepl y; int r2 = y.Open(d, pcfg(base + 6, true), gxObjRepl::PRIMARY);
      check(r1 == 0 && r2 == -1 && y.Error().find("missing") != std::string::npos, "review2 5: an unwritten state file on a new node is replaced (" + std::to_string(r1) +
	    "); a missing one on a node with data is refused [" + y.Error() + "]");
    }
    // scale: a commit near the block limit (55 x 1 MB) is delivered, applied and read back on the copy
    {
      int pc4 = base + 8;
      Srv C; C.start(dir + "/big_c", ccfg(pc4, "C4"));
      gxObjRepl x; x.Open(dir + "/big_p", pcfg(pc4, true), gxObjRepl::PRIMARY); x.OpenDatabase("blob");
      std::vector<unsigned char> mb(1024 * 1024);
      x.Begin();
      for(int i = 0; i < 55; i++) { for(size_t k = 0; k < mb.size(); k += 4096) mb[k] = (unsigned char)(i + k); x.Put("blob", 1000 + i, mb.data(), (uint32_t)mb.size()); }
      int crc = x.Commit();
      auto t0 = std::chrono::steady_clock::now();
      int drc = x.Drain(120);
      double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      std::vector<unsigned char> got; bool same = true;
      for(int i = 0; i < 55 && same; i++) { std::vector<unsigned char> want; x.Get("blob", 1000 + i, want); same = C.r.Get("blob", 1000 + i, got) == 0 && got == want; }
      check(crc == 0 && drc == 0 && same, "scale: a 55 MB commit was delivered in " + std::to_string(secs).substr(0, 5) + " s and reads back equal on the copy");
      x.Close(); C.stop();
    }
    // ---------- the third review's findings ----------
    // r3-1: Close() from another thread stops Serve(); the folder is free; a primary then finds no copy
    {
      int pc5 = base + 9;
      Srv C; C.start(dir + "/cl_c", ccfg(pc5, "C5"));
      C.r.Close();
      C.t.join();                                              // Serve() left
      std::string m; int trc = gxObjRepl::Takeover(dir + "/cl_c", m);   // fails anyway (no copy role yet?) or works: the lock is free
      gxObjRepl x; x.Open(dir + "/cl_p", pcfg(pc5, true), gxObjRepl::PRIMARY); commit1(x);
      int prc = x.Pump(); x.Close();
      check(prc == 1 && m.find("in use") == std::string::npos, "r3-1: Close() stopped Serve() in the other thread; the folder is free (" + m +
	    ") and a primary finds no copy (Pump " + std::to_string(prc) + ")");
      (void)trc;
    }
    // r3-2: a database whose index failed to open is not opened again over the failed objects
    {
      std::string d = dir + "/nv"; mkdir(d.c_str(), 0700); mkdir((d + "/nv.m.ix.btx.building").c_str(), 0700);
      gxObjReplConfig c = pcfg(base + 4, true); c.index = "k:nv:B:0:8,m:nv:B:8:8";
      gxObjRepl x; x.Open(d, c, gxObjRepl::PRIMARY);
      int r1 = x.OpenDatabase("nv"), r2 = x.OpenDatabase("nv");
      std::string e2 = x.Error();
      x.Close();
      rmdir((d + "/nv.m.ix.btx.building").c_str());
      gxObjRepl y; y.Open(d, c, gxObjRepl::PRIMARY);
      int r3 = y.OpenDatabase("nv");
      y.Begin(); for(int i = 0; i < 50; i++) { char b8[16]; memset(b8, 0, 16); b8[0] = 'A' + i % 2; b8[8] = (char)i; y.Put("nv", y.NewId(), b8, 16); } y.Commit();
      std::vector<uint64_t> f; y.FindKey("k", "A", 1, f); y.Close();
      std::string rep2; int ixr = gxObjRepl::IndexCheck(d, c.index, rep2);
      check(r1 == -1 && r2 == -1 && e2.find("failed to open earlier") != std::string::npos && r3 == 0 && f.size() == 25 && ixr == 0,
	    "r3-2: a failed open is refused on retry [" + e2 + "]; after a reopen the indexes are whole (" + std::to_string(f.size()) + " keys, IndexCheck " + std::to_string(ixr) + ")");
    }
    // r3-3: an application id in the next epoch's range does not move NewId() there, also after a restart
    {
      std::string d = dir + "/nid";
      gxObjRepl x; x.Open(d, pcfg(base + 4, true), gxObjRepl::PRIMARY); x.OpenDatabase("ev");
      x.Begin(); x.Put("ev", (2ULL << 40) + 5, "x", 1); x.Commit(); x.Close();
      gxObjRepl y; y.Open(d, pcfg(base + 4, true), gxObjRepl::PRIMARY); uint64_t nid = y.NewId(); y.Close();
      check(nid < (2ULL << 40), "r3-3: after a restart NewId() = " + std::to_string(nid) + ", still in epoch 1's range");
    }
    // r3-4: a copy in the middle of a full send refuses reads
    {
      std::string m; gxObjRepl::Rejoin(dir + "/d_c", m);     // d_c: a copy from above, now waiting for a full send
      gxObjRepl c4; c4.Open(dir + "/d_c", ccfg(base + 2, "C2"), gxObjRepl::COPY);
      std::vector<unsigned char> b; int g = c4.Get("ev", 1, b); std::string e = c4.Error();
      check(c4.InSnapshot() && g == -1 && e.find("full send") != std::string::npos, "r3-4: reads on a copy inside a full send are refused [" + e + "]");
      c4.Close();
    }
    // r3-5: while Pump() waits on a copy that never answers TLS, a reader is not held up
    {
      int ls = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons(base + 11);
      a.sin_addr.s_addr = htonl(0x7f000001); int one = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
      bind(ls, (sockaddr *)&a, sizeof(a)); listen(ls, 4);     // accepts in the kernel, never speaks TLS
      gxObjRepl x; x.Open(dir + "/pb_p", pcfg(base + 11, true), gxObjRepl::PRIMARY); commit1(x);
      std::thread pt([&]() { x.Pump(); });
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      auto t0 = std::chrono::steady_clock::now();
      std::vector<unsigned char> b; x.Get("ev", 1, b);
      double w = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      pt.join(); x.Close(); close(ls);
      check(w < 1.0, "r3-5: a reader waited " + std::to_string(w).substr(0, 5) + " s while Pump() was stuck in a TLS handshake");
    }
    // r3-6: Drain() after the copy was replaced by an empty one does not report 0 from stale knowledge
    {
      int pc6 = base + 12;
      Srv C; C.start(dir + "/rp_c1", ccfg(pc6, "R1"));
      gxObjRepl x; x.Open(dir + "/rp_p", pcfg(pc6, true), gxObjRepl::PRIMARY);
      for(int k = 0; k < 3; k++) commit1(x); x.Drain(20);
      C.stop();
      Srv D; D.start(dir + "/rp_c2", ccfg(pc6, "R2"));
      long s0 = x.Snapshots(); int rc = x.Drain(20); long s1 = x.Snapshots();
      check(rc == 0 && s1 == s0 + 1 && D.r.AppliedSeq() == x.LastSeq(), "r3-6: Drain() found the new empty copy and gave it a full send (rc " + std::to_string(rc) +
	    ", snapshots " + std::to_string(s0) + " -> " + std::to_string(s1) + ")");
      x.Close(); D.stop();
    }
    // r3-8/9: a copy refuses OpenDatabase; a Stop() before Serve() does not stop the next Serve()
    {
      gxObjRepl c9; c9.Open(dir + "/st_c", ccfg(base + 13, "C9"), gxObjRepl::COPY);
      int od = c9.OpenDatabase("x");
      c9.Stop();
      std::thread t([&]() { c9.Serve(); });
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      gxObjRepl x; x.Open(dir + "/st_p", pcfg(base + 13, true), gxObjRepl::PRIMARY); commit1(x); int rc = x.Drain(20); x.Close();
      c9.Stop(); t.join();
      check(od == -1 && rc == 0 && c9.AppliedSeq() == 1, "r3-8/9: the copy refused OpenDatabase; Serve() after an earlier Stop() served (drain " + std::to_string(rc) + ")");
      c9.Close();
    }
    // 8 with trim off, the copy's acknowledgement still counts: a rejoin after a full drain drops nothing
    {
      int pc3 = base + 7;
      Srv C; C.start(dir + "/r_c", ccfg(pc3, "C3"));
      { gxObjRepl x; x.Open(dir + "/r_p", pcfg(pc3, false), gxObjRepl::PRIMARY); for(int k = 0; k < 3; k++) commit1(x); x.Drain(20); x.Close(); }
      C.stop();
      gxObjRepl::Rejoin(dir + "/r_p", msg);
      check(msg.find("0 commit(s) past seq 3") != std::string::npos, "review2 8: with trim off, rejoin counts from the acknowledged seq [" + msg + "]");
    }
  }

  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
