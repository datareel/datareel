// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: objrepl_sim.cpp
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

objrepl_sim: a synthetic workload for gxObjRepl (test data only). One
process is one node: the primary writes a working day on a compressed
clock while reader threads look up keys and nearest vectors; the copy
serves while its own reader threads do the same. sim_day.sh runs the
day, the faults and the checkpoints.

Databases (one stream; the link map and indexes below):
  ticks, ticks2  64-byte records: symbol, time, price (3 B-tree indexes)
                 ticks2 is the next day's database, made at rollover
  docs           1 KB to 64 KB documents (a few of 1-4 MB): key, category,
                 author (3 B-trees) and a 16-float vector (vector index)
  links          64-byte link objects: next (links: chains and cycles),
                 doc and doc2 (docs: fan-out); group, kind, seq (3 B-trees)

  objrepl_sim seed ROOT --objects N [config options]
  objrepl_sim primary ROOT --start EPOCH --day-secs S [--readers K]
                 [--base-rate R] [--until-hour H] [--stats FILE]
  objrepl_sim copy ROOT [--readers K] [--stats FILE]
Config options as in objrepl (--copy, --listen, --ca, --cert, --key,
--allow-cn, --name, --peer-name, --idle-timeout); the links and indexes
are the simulator's own. The clock: simulated hour h of the day runs
from START + h * S / 24 seconds; a restarted primary carries on where
the clock is. Stats: one line a second (time, simulated hour, commits,
objects written, last seq, copy applied, lag, reads, read p50/p99 us).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <time.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include "gxobjrepl.h"

using namespace datareel;

static const char *LINKS = "links:0:links,links:8:docs,links:16:docs";
static const char *INDEX =
  "sym:ticks:B:0:8,tm:ticks:B:8:8,px:ticks:B:16:8,"
  "sym:ticks2:B:0:8,tm:ticks2:B:8:8,px:ticks2:B:16:8,"
  "key:docs:B:0:8,cat:docs:B:8:8,auth:docs:B:16:8,vec:docs:V:24:16,"
  "grp:links:B:24:8,kind:links:B:32:8,lseq:links:B:40:8";
static const int NSYM = 500, NCAT = 40, NAUTH = 300;

static std::atomic<int> stopping{0};
static gxObjRepl *node = 0;
static void on_term(int) { stopping = 1; if(node) node->Stop(); }

static double wall_s() { return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); }
static double now_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// ---------- records ----------
static void tick(std::mt19937_64 &r, std::vector<unsigned char> &b, uint64_t t)
{
  b.assign(64, 0);
  uint64_t sym = r() % NSYM, px = 10000 + r() % 90000;
  memcpy(&b[0], &sym, 8); memcpy(&b[8], &t, 8); memcpy(&b[16], &px, 8);
  for(int i = 24; i < 64; i++) b[i] = (unsigned char)r();
}
static void doc(std::mt19937_64 &r, std::vector<unsigned char> &b, uint64_t key, size_t n)
{
  b.resize(std::max<size_t>(n, 88));
  uint64_t cat = r() % NCAT, auth = r() % NAUTH;
  memcpy(&b[0], &key, 8); memcpy(&b[8], &cat, 8); memcpy(&b[16], &auth, 8);
  for(int i = 0; i < 16; i++) { float f = (float)((int)(r() % 2001) - 1000) / 1000.0f; memcpy(&b[24 + 4 * i], &f, 4); }
  for(size_t i = 88; i < b.size(); i += 8) b[i] = (unsigned char)r();
}
static size_t doc_size(std::mt19937_64 &r)
{
  unsigned x = r() % 1000;
  if(x == 0) return 1024 * 1024 + r() % (3 * 1024 * 1024);     // a few multi-MB documents
  if(x < 700) return 1024 + r() % 4096;
  return 4096 + r() % (60 * 1024);
}

struct Sim {
  gxObjRepl r;
  std::mt19937_64 rng;
  std::vector<uint64_t> docs, links, ticks;                    // live ids (best effort: refreshed at start)
  std::mutex mu;                                               // the node is used by the writer and the readers
  std::atomic<long long> reads{0};
  std::vector<double> lat;                                     // reader latencies this second (us)
  std::mutex latmu;
  uint64_t objects = 0;

  void refresh() {
    std::lock_guard<std::mutex> g(mu);
    docs = r.Ids("docs"); links = r.Ids("links"); ticks = r.Ids("ticks");
  }
  // a link object: next (another link, maybe itself later: cycles), doc and doc2 (fan-out to documents)
  bool put_link(uint64_t id) {
    std::vector<unsigned char> b(64, 0);
    uint64_t nx = 0, d1 = 0, d2 = 0;
    if(!links.empty() && rng() % 3) nx = (uint64_t)r.AddressOf("links", links[rng() % links.size()]);
    if(!docs.empty()) { d1 = (uint64_t)r.AddressOf("docs", docs[rng() % docs.size()]); d2 = (uint64_t)r.AddressOf("docs", docs[rng() % docs.size()]); }
    uint64_t grp = rng() % 1000, kind = rng() % 8, seq = id;
    memcpy(&b[0], &nx, 8); memcpy(&b[8], &d1, 8); memcpy(&b[16], &d2, 8);
    memcpy(&b[24], &grp, 8); memcpy(&b[32], &kind, 8); memcpy(&b[40], &seq, 8);
    return r.Put("links", id, b.data(), 64) == 0;
  }
  // one commit of the working day: mostly ticks, then documents (new, grown, changed), links, removes
  bool commit_one(const std::string &tickdb, uint64_t t, int nticks) {
    std::lock_guard<std::mutex> g(mu);
    if(r.Begin() != 0) return false;
    std::vector<unsigned char> b;
    for(int i = 0; i < nticks; i++) { tick(rng, b, t); uint64_t id = r.NewId(); if(r.Put(tickdb, id, b.data(), 64) != 0) return false; if(tickdb == "ticks") ticks.push_back(id); objects++; }
    unsigned x = rng() % 100;
    if(x < 30 || docs.empty()) { uint64_t id = r.NewId(); doc(rng, b, id, doc_size(rng)); if(r.Put("docs", id, b.data(), (uint32_t)b.size()) != 0) return false; docs.push_back(id); objects++; }
    else if(x < 55) {                                          // a document grows (moves, leaves a stub)
      uint64_t id = docs[rng() % docs.size()]; std::vector<unsigned char> old;
      if(r.Get("docs", id, old) == 0) { size_t n = std::min<size_t>(old.size() * 2 + 512, 2 * 1024 * 1024); doc(rng, b, id, n); if(r.Put("docs", id, b.data(), (uint32_t)b.size()) != 0) return false; objects++; }
    } else if(x < 70) {                                        // a document changes in place
      uint64_t id = docs[rng() % docs.size()]; std::vector<unsigned char> old;
      if(r.Get("docs", id, old) == 0) { doc(rng, b, id, old.size()); if(r.Put("docs", id, b.data(), (uint32_t)b.size()) != 0) return false; objects++; }
    }
    if(rng() % 4 == 0) {                                       // links: a chain step, fan-out, sometimes a cycle back
      uint64_t id = r.NewId(); if(!put_link(id)) return false; links.push_back(id); objects++;
      if(rng() % 10 == 0 && links.size() > 2) { if(!put_link(links[rng() % links.size()])) return false; objects++; }   // re-point: cycles
    }
    if(rng() % 5 == 0 && ticks.size() > 1000) {                // removes: tombstones
      size_t k = rng() % ticks.size(); uint64_t id = ticks[k]; ticks[k] = ticks.back(); ticks.pop_back();
      if(r.Remove("ticks", id) != 0) return false;
    }
    if(rng() % 50 == 0 && docs.size() > 100) {
      size_t k = rng() % docs.size(); uint64_t id = docs[k]; docs[k] = docs.back(); docs.pop_back();
      if(r.Remove("docs", id) != 0) return false;
    }
    return r.Commit() == 0;
  }
  // the nightly purge: every tombstone no link names (Purge() refuses the linked ones)
  size_t purge() {
    std::lock_guard<std::mutex> g(mu);
    size_t n = 0;
    if(r.Begin() != 0) return 0;
    for(auto &d : r.Databases()) for(uint64_t id : r.Ids(d, gxObjRepl::TOMBSTONES)) if(r.Purge(d, id) == 0) n++;
    r.Commit();
    return n;
  }
  void reader(unsigned seed) {
    std::mt19937_64 rr(seed);
    while(!stopping) {
      double t0 = now_s();
      // the library locks each call itself; the simulator's own lock is only for its id lists (the writer's)
      if(rr() % 2) { uint64_t sym = rr() % NSYM; std::vector<uint64_t> ids; r.FindKey("sym", &sym, 8, ids); }
      else { float q[16]; for(auto &f : q) f = (float)((int)(rr() % 2001) - 1000) / 1000.0f; std::vector<gxObjReplHit> h; r.Nearest("vec", q, 10, h); }
      double us = (now_s() - t0) * 1e6;
      reads++;
      { std::lock_guard<std::mutex> g(latmu); lat.push_back(us); }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  void pct(double &p50, double &p99) {
    std::vector<double> v; { std::lock_guard<std::mutex> g(latmu); v.swap(lat); }
    p50 = p99 = 0;
    if(v.empty()) return;
    std::sort(v.begin(), v.end());
    p50 = v[v.size() / 2]; p99 = v[std::min(v.size() - 1, (size_t)(v.size() * 0.99))];
  }
};

static int usage() { fprintf(stderr, "usage: objrepl_sim seed|primary|copy ROOT [options] (see the file header)\n"); return 64; }

int main(int argc, char **argv)
{
  if(argc < 3) return usage();
  std::string mode = argv[1], root = argv[2], why, stats;
  gxObjReplConfig cfg;
  cfg.links = LINKS; cfg.index = INDEX;
  long long objects = 0, start = 0; double day_secs = 1440, base_rate = 40; int readers = 0; double until_hour = 24; unsigned seed = 1;
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if(a == "--objects") objects = atoll(val().c_str());
    else if(a == "--start") start = atoll(val().c_str());
    else if(a == "--day-secs") day_secs = atof(val().c_str());
    else if(a == "--base-rate") base_rate = atof(val().c_str());
    else if(a == "--readers") readers = atoi(val().c_str());
    else if(a == "--until-hour") until_hour = atof(val().c_str());
    else if(a == "--stats") stats = val();
    else if(a == "--seed") seed = (unsigned)atol(val().c_str());
    else if(a.compare(0, 2, "--") == 0) { if(!cfg.Set(a.substr(2), val(), &why)) { fprintf(stderr, "%s\n", why.c_str()); return 64; } }
    else return usage();
  }
#if defined(_WIN32)
  signal(SIGTERM, on_term); signal(SIGINT, on_term);
#else
  struct sigaction sa; memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_term;
  sigaction(SIGTERM, &sa, 0); sigaction(SIGINT, &sa, 0); signal(SIGPIPE, SIG_IGN);
#endif
  Sim s; s.rng.seed(seed ^ (uint64_t)time(0));
  bool copy = mode == "copy";
  int rc = s.r.Open(root, cfg, copy ? gxObjRepl::COPY : gxObjRepl::PRIMARY);
  if(rc != 0) { fprintf(stderr, "objrepl_sim %s: %s (rc %d)\n", mode.c_str(), s.r.Error().c_str(), rc); return rc == 4 ? 4 : 64; }
  node = &s.r;
  FILE *sf = stats.empty() ? 0 : fopen(stats.c_str(), "a");
  std::vector<std::thread> rt;
  auto start_readers = [&]() { for(int k = 0; k < readers; k++) rt.emplace_back([&s, k, seed]() { s.reader(seed * 100 + k); }); };
  auto stop_readers = [&]() { stopping = 1; for(auto &t : rt) t.join(); rt.clear(); };

  if(mode == "seed") {
    // the starting database: OBJECTS objects, 90% ticks, 9% documents, 1% links, in commits of about 500
    for(const char *d : {"ticks", "ticks2", "docs", "links"}) s.r.OpenDatabase(d);
    double t0 = now_s(); long long n = 0; long long nd = objects * 9 / 100, nl = objects / 100;
    std::vector<unsigned char> b;
    while(n < objects && !stopping) {
      if(s.r.Begin() != 0) break;
      for(int k = 0; k < 500 && n < objects; k++, n++) {
	uint64_t id = s.r.NewId();
	if(n % 100 < 9 && (long long)s.docs.size() < nd) { doc(s.rng, b, id, s.rng() % 1000 < 3 ? 1024 * 1024 : 256 + s.rng() % 2048); s.r.Put("docs", id, b.data(), (uint32_t)b.size()); s.docs.push_back(id); }
	else if(n % 100 == 9 && (long long)s.links.size() < nl && !s.docs.empty()) { s.put_link(id); s.links.push_back(id); }
	else { tick(s.rng, b, (uint64_t)n); s.r.Put("ticks", id, b.data(), 64); }
      }
      if(s.r.Commit() != 0) { fprintf(stderr, "seed: %s\n", s.r.Error().c_str()); return 1; }
      if(n % 100000 < 500) { printf("seed %lld objects, %.0f s\n", n, now_s() - t0); fflush(stdout); }
    }
    printf("seeded %lld objects in %.1f s (%.0f objects/s), last seq %llu\n", n, now_s() - t0, n / std::max(0.001, now_s() - t0),
	   (unsigned long long)s.r.LastSeq());
    s.r.Close();
    return 0;
  }
  if(copy) {
    start_readers();
    std::thread st([&]() {
      while(!stopping) {
	std::this_thread::sleep_for(std::chrono::seconds(1));
	double p50, p99; s.pct(p50, p99);
	uint64_t ap; { std::lock_guard<std::mutex> g(s.mu); ap = s.r.AppliedSeq(); }
	if(sf) { fprintf(sf, "%lld copy applied %llu reads %lld p50_us %.0f p99_us %.0f\n", (long long)time(0), (unsigned long long)ap, (long long)s.reads, p50, p99); fflush(sf); }
      }
    });
    // Serve() runs here; the readers call the node from their threads (gxObjRepl takes its own lock in each call)
    rc = s.r.Serve();
    stop_readers(); st.join();
    s.r.Close();
    if(sf) fclose(sf);
    return rc == 0 ? 0 : 1;
  }
  // primary: the working day on the compressed clock
  for(const char *d : {"ticks", "ticks2", "docs", "links"}) s.r.OpenDatabase(d);
  s.refresh();
  if(!start) start = (long long)time(0);
  start_readers();
  double hour_secs = day_secs / 24;
  bool purged = false;
  long long commits = 0, last_c = 0; uint64_t last_obj = 0; double last_print = now_s();
  printf("primary: day of %.0f s (an hour = %.1f s), %zu ticks %zu docs %zu links live\n", day_secs, hour_secs, s.ticks.size(), s.docs.size(), s.links.size());
  fflush(stdout);
  while(!stopping) {
    double h = (wall_s() - (double)start) / hour_secs;          // the simulated hour (wall clock: restarts carry on)
    if(h >= until_hour) break;
    // the day: quiet night, steady day, bursts at the open (9:30-10:00, 20x) and the close (15:30-16:00, 40x)
    double mult = (h < 7 || h >= 20) ? 0.2 : (h >= 9.5 && h < 10) ? 20 : (h >= 15.5 && h < 16) ? 40 : 1;
    double rate = base_rate * mult;                            // commits per second
    std::string tickdb = h >= 18 ? "ticks2" : "ticks";         // the next day's database from 18:00 (rollover)
    if(!purged && h >= 22) { size_t n = s.purge(); printf("hour %.1f: nightly purge, %zu tombstone(s)\n", h, n); fflush(stdout); purged = true; }
    double t0 = now_s();
    if(!s.commit_one(tickdb, (uint64_t)(h * 3600), 5 + (int)(s.rng() % 20))) { fprintf(stderr, "commit: %s\n", s.r.Error().c_str()); break; }
    commits++;
    if(s.r.Pump() == 3) { fprintf(stderr, "refused\n"); break; }   // not under the writer's lock: readers go on
    double wait = 1.0 / rate - (now_s() - t0);
    if(wait > 0) std::this_thread::sleep_for(std::chrono::duration<double>(wait));
    if(now_s() - last_print >= 1) {
      double p50, p99; s.pct(p50, p99);
      uint64_t ls, ca; { std::lock_guard<std::mutex> g(s.mu); ls = s.r.LastSeq(); ca = s.r.CopyApplied(); }
      if(sf) { fprintf(sf, "%lld hour %.2f commits %lld objects %llu last %llu applied %llu lag %llu reads %lld p50_us %.0f p99_us %.0f rate_x %.1f\n",
		       (long long)time(0), h, commits - last_c, (unsigned long long)(s.objects - last_obj), (unsigned long long)ls,
		       (unsigned long long)ca, (unsigned long long)(ls - std::min(ls, ca)), (long long)s.reads, p50, p99, mult); fflush(sf); }
      last_c = commits; last_obj = s.objects; last_print = now_s();
    }
  }
  stop_readers();
  printf("primary: stopped at hour %.2f after %lld commits\n", (wall_s() - (double)start) / hour_secs, commits);
  s.r.Close();
  if(sf) fclose(sf);
  return 0;
}
