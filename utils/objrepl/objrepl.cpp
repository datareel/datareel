// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: objrepl.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

objrepl: the command line tool for gxObjRepl (gxobjrepl.h), object
replication between two independent object databases, and its test
data generator.

Usage:
  objrepl primary ROOT --copy HOST[:PORT] --ca CA --cert CERT --key KEY
                  [--commits N] [--per N] [--dbs N] [--sleep MS]
                  [--seed N] [--drain] [--no-trim]
                  [--drop-linked N] [--unknown-removes N] [--purge]
                  [--defer-target]
          writes test commits (adds, updates, removes, linked objects)
          to db0..dbN-1 (and the link map's databases), sends them to
          the copy, then waits up to 120 s for the copy to have them all.
          Exit 0 (drained), 2 (not drained), 3 (refused), 4 (ROOT is a
          copy), 64 (config or certificates)
  objrepl copy ROOT [--listen [ADDR:]PORT|ADDR] --ca CA --cert CERT --key KEY
                  --allow-cn CN          until TERM or INT
          (7.4.1: the copy listens on, and the primary connects to,
          port 2073, the registered gxStream port, unless a port is
          given; --listen ADDR sets the address only)
  objrepl digest ROOT       every database: ids and content hashes
  objrepl ixcheck ROOT --index ...   each index against the objects
  objrepl check ROOT        OK / WARN / FAIL (exit 0 / 1 / 2)
  objrepl clear-alarm ROOT
  objrepl status ROOT
  objrepl engine            the database engine (exit 0 for 64-bit, 1 for 32-bit)
  objrepl takeover ROOT     a copy becomes primary: epoch + 1
  objrepl rejoin ROOT [--full]  a former primary becomes a copy (7.5 beta 3:
          an incremental rejoin when the new primary can match the two
          histories and the schemas agree, else a full send; --full: a
          full send in any case)
  objrepl swap OLD NEW [--index SPEC]  7.5 beta 3: after a rebuild into
          NEW (both stopped), OLD is kept as OLD.before-rebuild-DATE and
          NEW becomes OLD (NEW's indexes checked first with --index)
  Common: --name N --peer-name N --links SPEC --index SPEC --max-lag N
  --max-behind S --min-cert-days N --idle-timeout S --ack-timeout S (7.4.1, primary)
  --schema TEXT --rejoin-max-share PERCENT (7.5 beta 3)
  --config FILE. Any of them may come
  from ROOT/objrepl.conf (or --config FILE), one "key value" line each
  (the option without "--"); the command line wins.
  Certificates: utils/objrepl/objrepl_certs.sh.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <thread>
#include <chrono>
#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif
#include <map>
#include <atomic>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include "gxobjrepl.h"

using namespace datareel;

static gxObjRepl *serving = 0;
static void on_term(int) { if(serving) serving->Stop(); }
#if defined(_WIN32)
// Windows has no TERM: Ctrl+C, Ctrl+Break and closing the console stop the copy, and so does ROOT/objrepl.stop
static BOOL WINAPI on_ctrl(DWORD) { if(serving) serving->Stop(); return TRUE; }
#endif

// ---------- the test data generator ----------
static unsigned rng = 2026;
static unsigned rnd() { rng = rng * 1103515245u + 12345u; return rng >> 8; }

struct LinkDef { std::string db; uint32_t off; std::string target; };
struct IxDef { std::string db; char kind; uint32_t off, len; };
static std::vector<std::string> split(const std::string &s, char c)
{
  std::vector<std::string> v; size_t i = 0;
  while(i <= s.size() && !s.empty()) { size_t j = s.find(c, i); if(j == std::string::npos) j = s.size(); v.push_back(s.substr(i, j - i)); i = j + 1; }
  return v;
}
static std::vector<LinkDef> links_of(const std::string &spec)
{
  std::vector<LinkDef> v;
  for(auto &e : split(spec, ',')) { auto f = split(e, ':'); if(f.size() == 3) v.push_back({f[0], (uint32_t)atol(f[1].c_str()), f[2]}); }
  return v;
}
static std::vector<IxDef> ix_of(const std::string &spec)
{
  std::vector<IxDef> v;
  for(auto &e : split(spec, ',')) { auto f = split(e, ':'); if(f.size() == 5) v.push_back({f[1], f[2][0], (uint32_t)atol(f[3].c_str()), (uint32_t)atol(f[4].c_str())}); }
  return v;
}

// index fields that mean something: a B-tree key from 64 values (so ids share keys), a vector of floats in [-1, 1]. A
// field the object is too short for is left as random bytes (that object is not indexed)
static void ix_fields(const std::vector<IxDef> &ixdefs, const std::string &db, std::vector<unsigned char> &b)
{
  for(auto &x : ixdefs) {
    if(x.db != db) continue;
    if(x.kind == 'B' && (size_t)x.off + x.len <= b.size()) { memset(b.data() + x.off, 0, x.len); b[x.off] = (unsigned char)(rnd() % 64); }
    if(x.kind == 'V' && (size_t)x.off + 4 * (size_t)x.len <= b.size())
      for(uint32_t i = 0; i < x.len; i++) { float f = (float)((int)(rnd() % 2001) - 1000) / 1000.0f; memcpy(b.data() + x.off + 4 * i, &f, 4); }
  }
}

struct GenOpts { long commits = 100, per = 10, ndbs = 3, sleep_ms = 0; bool drain = false; long drop_linked = 0, unknown_removes = 0;
		 bool purge = false, defer_target = false; long drain_secs = 120; };

static int run_primary(const std::string &root, const gxObjReplConfig &cfg, const GenOpts &o)
{
  gxObjRepl r;
  int rc = r.Open(root, cfg, gxObjRepl::PRIMARY);
  if(rc == 4) return 4;
  if(rc != 0) { fprintf(stderr, "objrepl primary: %s\n", r.Error().c_str()); return 64; }
  std::vector<LinkDef> links = links_of(cfg.links);
  std::vector<IxDef> ixdefs = ix_of(cfg.index);
  for(long i = 0; i < o.ndbs; i++) if(r.OpenDatabase("db" + std::to_string(i)) != 0) { fprintf(stderr, "objrepl primary: %s\n", r.Error().c_str()); return 1; }
  auto die = [&](const char *what) { fprintf(stderr, "objrepl primary: %s: %s\n", what, r.Error().c_str()); return 1; };
  bool held = false;
  for(long c = 0; !o.drain && c < o.commits; c++) {
    // the objects change first; the commit's record is made from their final bytes at Commit(), so a link to an
    // object added later in the same commit (or a cycle of links) is turned into ids once every target exists
    std::string lk = links.empty() ? std::string() : links[0].db;
    if(r.Begin() != 0) return die("begin");
    bool held_one = false;
    for(long k = 0; k < o.per; k++) {
      unsigned x = rnd() % 10;
      if(!lk.empty() && x == 0) {                              // link objects (32 bytes; the link fields from the map)
	const LinkDef *cyc = 0, *fw = 0;                       // cyc: a field into lk itself; fw: into another database
	for(auto &l : links) if(l.db == lk) { if(l.target == lk) cyc = &l; else fw = &l; }
	uint64_t a = r.NewId(), b = r.NewId();
	unsigned char z[32]; memset(z, 0, sizeof(z));
	if(r.Put(lk, a, z, 32) != 0) return die("put");
	if(cyc && (!fw || rnd() % 2)) {                        // a cycle in one commit, a -> b and b -> a
	  if(r.Put(lk, b, z, 32) != 0) return die("put");
	  uint64_t la = (uint64_t)r.AddressOf(lk, b), lb = (uint64_t)r.AddressOf(lk, a);
	  unsigned char p[32]; memset(p, 0, 32); memcpy(p + cyc->off, &la, 8); p[24] = 1;
	  unsigned char q[32]; memset(q, 0, 32); memcpy(q + cyc->off, &lb, 8); q[24] = 2;
	  if(r.Put(lk, a, p, 32) != 0 || r.Put(lk, b, q, 32) != 0) return die("put");
	} else if(fw) {                                        // a -> c, c added after a in the same commit
	  uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> cb(n); for(auto &q : cb) q = (unsigned char)rnd();
	  if(r.Put(fw->target, b, cb.data(), n) != 0) return die("put");
	  uint64_t lc = (uint64_t)r.AddressOf(fw->target, b);
	  unsigned char p[32]; memset(p, 0, 32); memcpy(p + fw->off, &lc, 8); p[24] = 3;
	  if(r.Put(lk, a, p, 32) != 0) return die("put");
	  // defer_target: c's add goes in the NEXT commit, so the copy meets a link to an id it does not hold
	  if(o.defer_target && !held_one) { r.TestHoldAdd(fw->target, b); held_one = held = true; }
	}
	continue;
      }
      std::string d = "db" + std::to_string(rnd() % o.ndbs);
      std::vector<uint64_t> lv = r.Ids(d);
      if(x < 6 || lv.empty()) {                                // add
	uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> b(n);
	for(auto &y : b) y = (unsigned char)rnd();
	ix_fields(ixdefs, d, b);
	if(r.Put(d, r.NewId(), b.data(), n) != 0) return die("add");
      } else {
	uint64_t id = lv[rnd() % lv.size()];
	if(x < 9) {                                            // update: new bytes, a new size (in place when it fits)
	  uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> b(n);
	  for(auto &y : b) y = (unsigned char)rnd();
	  ix_fields(ixdefs, d, b);
	  if(r.Put(d, id, b.data(), n) != 0) return die("update");
	} else if(r.Remove(d, id) != 0) return die("remove"); // a tombstone at the object's address
      }
    }
    if(r.Commit() != 0) return die("commit");
    if(r.Pump() == 3) return 3;                               // best effort: a copy that is away catches up later
    if(o.sleep_ms) std::this_thread::sleep_for(std::chrono::milliseconds(o.sleep_ms));
  }
  if(held) { if(r.Begin() != 0 || r.Commit() != 0) return die("commit"); }   // a held-back add still owed
  if(o.drop_linked > 0 && !links.empty()) {
    // removes of objects that links point to: the targets become tombstones, the links keep naming them
    std::set<std::pair<std::string, uint64_t>> drop;
    for(auto &l : links) {
      if(l.target == l.db) continue;
      for(uint64_t id : r.Ids(l.db)) {
	if((long)drop.size() >= o.drop_linked) break;
	std::vector<unsigned char> b;
	if(r.Get(l.db, id, b) != 0 || b.size() < l.off + 8) continue;
	uint64_t v; memcpy(&v, b.data() + l.off, 8);
	uint64_t t = v ? r.IdAt(l.target, (FAU_t)v) : 0;
	if(t && r.IsLive(l.target, t)) drop.insert({l.target, t});
      }
    }
    if(r.Begin() != 0) return die("begin");
    for(auto &d : drop) if(r.Remove(d.first, d.second) != 0) return die("remove");
    if(r.Commit() != 0) return die("commit");
    printf("objrepl primary: removed %zu linked object(s): links now name their tombstones\n", drop.size());
  }
  if(o.unknown_removes > 0) {
    // removes of ids that never existed: a tombstone on each side
    std::vector<std::string> dbs = r.Databases();
    if(r.Begin() != 0) return die("begin");
    for(long i = 0; i < o.unknown_removes && !dbs.empty(); i++) if(r.Remove(dbs.front(), (1ULL << 40) + r.NewId()) != 0) return die("remove");
    if(r.Commit() != 0) return die("commit");
    printf("objrepl primary: removed %ld unknown id(s)\n", o.unknown_removes);
  }
  if(o.purge) {
    // a purge: every tombstone that no link points to is freed (a scan of the link fields, no reverse index)
    std::set<std::pair<std::string, uint64_t>> linked;
    for(auto &l : links)
      for(uint64_t id : r.Ids(l.db)) {
	std::vector<unsigned char> b;
	if(r.Get(l.db, id, b) != 0 || b.size() < l.off + 8) continue;
	uint64_t v; memcpy(&v, b.data() + l.off, 8);
	uint64_t t = v ? r.IdAt(l.target, (FAU_t)v) : 0;
	if(t) linked.insert({l.target, t});
      }
    size_t n = 0;
    if(r.Begin() != 0) return die("begin");
    for(auto &d : r.Databases())
      for(uint64_t id : r.Ids(d, gxObjRepl::TOMBSTONES))
	if(!linked.count({d, id})) { if(r.Purge(d, id) != 0) return die("purge"); n++; }
    if(r.Commit() != 0) return die("commit");
    printf("objrepl primary: purged %zu tombstone(s), %zu kept (links point to them)\n", n, linked.size());
  }
  rc = r.Drain((int)o.drain_secs);
  if(rc == 3) return 3;
  printf("objrepl primary: epoch %llu, last seq %llu, copy applied %llu, %ld snapshot(s), outbox %s\n",
	 (unsigned long long)r.Epoch(), (unsigned long long)r.LastSeq(), (unsigned long long)r.CopyApplied(),
	 r.Snapshots(), rc == 0 ? "drained" : "NOT drained");
  r.Close();
  return rc == 0 ? 0 : 2;
}

static int run_copy(const std::string &root, const gxObjReplConfig &cfg)
{
  gxObjRepl r;
  int rc = r.Open(root, cfg, gxObjRepl::COPY);
  if(rc == 4) return 4;
  if(rc != 0) { fprintf(stderr, "objrepl copy: %s\n", r.Error().c_str()); return 64; }
  serving = &r;
#if defined(_WIN32)
  SetConsoleCtrlHandler(on_ctrl, TRUE);
  signal(SIGTERM, on_term); signal(SIGINT, on_term);
#else
  struct sigaction sa; memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_term;
  sigaction(SIGTERM, &sa, 0); sigaction(SIGINT, &sa, 0); signal(SIGPIPE, SIG_IGN);
#endif
  // ROOT/objrepl.stop (any platform): the copy stops within a second and the file is removed
  std::string stopf = root + "/objrepl.stop";
  std::remove(stopf.c_str());
  std::atomic<int> served{0};
  std::thread watch([&]() { while(!served) { if(access(stopf.c_str(), F_OK) == 0) { r.Stop(); std::remove(stopf.c_str()); break; }
					      std::this_thread::sleep_for(std::chrono::milliseconds(500)); } });
  rc = r.Serve();
  served = 1; watch.join();
  if(rc != 0) fprintf(stderr, "objrepl copy: %s\n", r.Error().c_str());
  serving = 0;
  r.Close();
  return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
  // 11/16/2026: "objrepl engine": the database engine this tool was built with (the test scripts refuse a 32-bit one:
  // its files are not the 64-bit engine's, and a 32-bit build reading 64-bit files misread them)
  // (the file signature size tells the engines apart: FAU is 8 bytes in both on 64-bit Linux, a long in the 32-bit one)
  if(argc == 2 && std::string(argv[1]) == "engine") {
    printf("engine %s (file signature %d bytes)\n", gxSignatureSize == 10 ? "64-bit" : "32-bit", gxSignatureSize);
    return gxSignatureSize == 10 ? 0 : 1;
  }
  if(argc < 3) { fprintf(stderr, "usage: objrepl primary|copy|digest|ixcheck|status|check|clear-alarm|takeover|rejoin|swap ROOT [options]\n"); return 64; }
  std::string cmd = argv[1], root = argv[2], why;
  if(cmd == "swap") {                                          // 7.5 beta 3: objrepl swap OLD NEW [--index SPEC]
    if(argc != 4 && !(argc == 6 && std::string(argv[4]) == "--index")) { fprintf(stderr, "usage: objrepl swap OLD NEW [--index SPEC]\n"); return 64; }
    std::string line;
    int rc = gxObjRepl::Swap(root, argv[3], argc == 6 ? argv[5] : "", line);
    if(rc == 0) printf("objrepl swap: %s\n", line.c_str()); else fprintf(stderr, "objrepl swap: %s\n", line.c_str());
    return rc;
  }
  bool full = false;
  gxObjReplConfig cfg;
  GenOpts o;
  // the config: ROOT/objrepl.conf or --config FILE, then the command line (which wins)
  std::string conf = root + "/objrepl.conf"; bool conf_given = false;
  for(int i = 3; i + 1 < argc; i++) if(std::string(argv[i]) == "--config") { conf = argv[i + 1]; conf_given = true; }
  if((conf_given || access(conf.c_str(), F_OK) == 0) && !cfg.Load(conf, &why)) { fprintf(stderr, "%s\n", why.c_str()); return 64; }
  static const std::set<std::string> keys = {"name", "peer-name", "copy", "listen", "ca", "cert", "key", "allow-cn",
					     "links", "index", "max-lag", "max-behind", "min-cert-days", "idle-timeout",
					     "ack-timeout", "schema", "rejoin-max-share"};
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if(a.compare(0, 2, "--") == 0 && keys.count(a.substr(2))) { if(!cfg.Set(a.substr(2), val(), &why)) { fprintf(stderr, "%s\n", why.c_str()); return 64; } }
    else if(a == "--config") val();
    else if(a == "--commits") o.commits = atol(val().c_str());
    else if(a == "--per") o.per = atol(val().c_str());
    else if(a == "--dbs") o.ndbs = std::max(1L, atol(val().c_str()));
    else if(a == "--sleep") o.sleep_ms = atol(val().c_str());
    else if(a == "--seed") rng = (unsigned)atol(val().c_str());
    else if(a == "--drain") o.drain = true;
    else if(a == "--no-trim") cfg.trim = false;
    else if(a == "--full") full = true;
    else if(a == "--drain-secs") o.drain_secs = std::max(0L, atol(val().c_str()));
    else if(a == "--drop-linked") o.drop_linked = atol(val().c_str());
    else if(a == "--unknown-removes") o.unknown_removes = atol(val().c_str());
    else if(a == "--purge") o.purge = true;
    else if(a == "--defer-target") o.defer_target = true;
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 64; }
  }
  std::string line, detail;
  int rc;
  if(cmd == "primary") return run_primary(root, cfg, o);
  if(cmd == "copy") return run_copy(root, cfg);
  if(cmd == "digest") {
    rc = gxObjRepl::Digest(root, cfg.links, line, &detail);
    printf("%s\n", line.c_str());
    if(rc == 0) fprintf(stderr, "objrepl digest: %s\n", detail.c_str());
    return rc;
  }
  if(cmd == "ixcheck") { rc = gxObjRepl::IndexCheck(root, cfg.index, line); printf("%s", line.c_str()); return rc; }
  if(cmd == "check" || cmd == "monitor") { rc = gxObjRepl::Check(root, cfg, line); printf("%s\n", line.c_str()); return rc; }
  if(cmd == "clear-alarm") { rc = gxObjRepl::ClearAlarm(root, line); printf("%s\n", line.c_str()); return rc; }
  if(cmd == "status") { rc = gxObjRepl::Status(root, line); printf("%s\n", line.c_str()); return rc; }
  if(cmd == "rejoin") {
    rc = gxObjRepl::Rejoin(root, line, !full);
    if(rc == 0) printf("objrepl rejoin: %s\n", line.c_str()); else fprintf(stderr, "objrepl rejoin: %s\n", line.c_str());
    return rc;
  }
  if(cmd == "takeover") {
    rc = gxObjRepl::Takeover(root, line);
    if(rc == 0) printf("objrepl takeover: %s\n", line.c_str()); else fprintf(stderr, "objrepl takeover: %s\n", line.c_str());
    return rc;
  }
  return 64;
}
