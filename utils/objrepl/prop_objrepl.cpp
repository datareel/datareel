// prop_objrepl.cpp - 11/16/2026: a randomized property test for gxObjRepl. For each seed, two nodes A and B (the
// primary and the copy swap roles) go through a seeded random sequence of steps: commits (adds, growing updates
// that move objects, removes, purges, rollbacks, link fields with forward references inside a commit and cycles,
// B-tree and vector indexes), SIGKILLs of the copy and the primary (each runs in a forked child), stream drops
// (OBJREPL_TEST_DROP_EVERY on the copy), copy rejoins, planned and crash takeovers with the old primary's rejoin.
// After every step that ends drained (Drain() returns 0) both nodes are stopped and checked: Digest equal,
// IndexCheck consistent on both with equal index digests, seqs and epochs equal, every object's own checksum and
// links good, no unexpected alarm. Invariants checked whenever both are stopped: the copy's applied seq never
// exceeds the primary's last seq, its epoch never exceeds the primary's; after a crash failover the commits lost
// (old primary's last seq - copy applied) are >= 0 and <= what Rejoin() reports dropped; a node of the wrong role
// is refused at Open() with an alarm. Needs a build with -DGXREPL_TEST_HOOKS=ON.
//
//   prop_objrepl --pki DIR --work DIR --port P --seed N [--count C] [--failures DIR] [--verbose] [--keep]
//
// PKI: ca.pem, copy.pem/key, primary.pem/key (objrepl_certs.sh DIR --copy-host 127.0.0.1 --apply). Ports P and P+1
// on 127.0.0.1. Children are fork+exec of this program (--child ...). Each seed runs in its own forked process group (killed whole after 300 s); a failing seed's log,
// node folders (tar.gz) and the command that re-runs it go to FAILURES/seed_N/. One line per seed on stdout:
//   seed N PASS|FAIL steps S checks K secs T [reason]
#include "gxdlcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "gxobjrepl.h"
using namespace datareel;
namespace fs = std::filesystem;

static const char *LINKS = "lk:0:db0,lk:8:lk,db0:40:lk";
static const char *INDEX = "k0:db0:B:0:8,v0:db0:V:8:8,k1:lk:B:16:4";
static const char *DBS[3] = {"db0", "lk", "db1"};

static std::string g_pki, g_work, g_fail, g_self;
static int g_port = 0;
static bool g_verbose = false, g_keep = false;
static std::chrono::steady_clock::time_point g_t0;
static std::string g_dir;            // this seed's folder

static void logf(const char *fmt, ...)
{
  char b[8192];
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
  long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_t0).count();
  fprintf(stdout, "%6ld [%d] %s\n", ms, (int)getpid(), b);
  fflush(stdout);
}

struct Rng {
  std::mt19937_64 m;
  explicit Rng(uint64_t s) : m(s) {}
  uint64_t u() { return m(); }
  int r(int n) { return n <= 0 ? 0 : (int)(m() % (uint64_t)n); }
  bool p(double x) { return (double)(m() >> 11) * (1.0 / 9007199254740992.0) < x; }
};
static uint64_t mix(uint64_t a, uint64_t b) { uint64_t x = a * 0x9E3779B97F4A7C15ULL ^ (b + 0x632BE59BD9B4E019ULL + (a << 6) + (a >> 2)); x ^= x >> 31; x *= 0xBF58476D1CE4E5B9ULL; x ^= x >> 29; return x; }

static uint32_t fnv32(const unsigned char *p, size_t n, uint32_t h = 2166136261u) { for(size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; } return h; }

// ---------- objects: self-describing (id and a checksum over everything but the link fields) ----------
// db0: [0,8) key  [8,40) 8 floats  [40,48) link -> lk  [48,56) id  [56,60) sum of [0,40)+[60,n)    n >= 64
// lk : [0,8) link -> db0  [8,16) link -> lk  [16,20) key  [20,28) id  [28,32) sum of [16,20)+[32,n)   n >= 40
// db1: [0,8) id  [8,12) sum of [12,n)                                                                n >= 16
static int db_index(const std::string &db) { for(int i = 0; i < 3; i++) if(db == DBS[i]) return i; return -1; }
static size_t min_size(int d) { return d == 0 ? 64 : d == 1 ? 40 : 16; }
static void seal(int d, std::vector<unsigned char> &b, uint64_t id)
{
  uint32_t s;
  if(d == 0) { memcpy(&b[48], &id, 8); s = fnv32(b.data(), 40); s = fnv32(b.data() + 60, b.size() - 60, s); memcpy(&b[56], &s, 4); }
  else if(d == 1) { memcpy(&b[20], &id, 8); s = fnv32(b.data() + 16, 4); s = fnv32(b.data() + 32, b.size() - 32, s); memcpy(&b[28], &s, 4); }
  else { memcpy(&b[0], &id, 8); s = fnv32(b.data() + 12, b.size() - 12); memcpy(&b[8], &s, 4); }
}
static bool sealed_ok(int d, const std::vector<unsigned char> &b, uint64_t id)
{
  if(b.size() < min_size(d)) return false;
  std::vector<unsigned char> c = b; seal(d, c, id);
  return c == b;
}
static void make_obj(Rng &R, int d, size_t n, std::vector<unsigned char> &b)
{
  b.assign(n, 0);
  for(size_t i = 0; i < n; i++) b[i] = (unsigned char)R.u();
  if(d == 0) {
    for(int i = 0; i < 8; i++) b[i] = (unsigned char)('a' + R.r(i == 0 ? 4 : 2));   // few keys: duplicates in the B-tree
    for(int i = 0; i < 8; i++) { float f = (float)R.r(1000) / 100.0f; memcpy(&b[8 + 4 * i], &f, 4); }
    memset(&b[40], 0, 8);
  } else if(d == 1) {
    memset(&b[0], 0, 16);
    for(int i = 0; i < 4; i++) b[16 + i] = (unsigned char)('a' + R.r(3));
  }
}
struct LinkF { int d; size_t off; int target; };
static const LinkF LF[3] = {{1, 0, 0}, {1, 8, 1}, {0, 40, 1}};

// ---------- configs ----------
static std::string node_dir(int n) { return g_dir + (n == 0 ? "/A" : "/B"); }
static const char *node_name(int n) { return n == 0 ? "A" : "B"; }
static gxObjReplConfig cfg_for(int n, bool primary)
{
  gxObjReplConfig c;
  c.name = node_name(n); c.peer_name = node_name(1 - n);
  c.ca = g_pki + "/ca.pem";
  if(primary) { c.cert = g_pki + "/primary.pem"; c.key = g_pki + "/primary.key"; c.copy = "127.0.0.1:" + std::to_string(g_port + 1 - n); }
  else { c.cert = g_pki + "/copy.pem"; c.key = g_pki + "/copy.key"; c.listen_port = g_port + n; c.allow_cn = "objrepl-primary"; }
  c.links = LINKS; c.index = INDEX; c.idle_timeout = 30;
  c.log = [](const std::string &m) { logf("    lib: %s", m.c_str()); };
  return c;
}

struct St { bool ok = false; uint64_t epoch = 0, next_seq = 0, acked = 0, applied = 0, in_snap = 0; std::string role, digest, line; };
static St status_of(int n)
{
  St s; std::string line;
  if(gxObjRepl::Status(node_dir(n), line) != 0) { s.line = line; return s; }
  s.line = line;
  std::istringstream is(line); std::string k, v;
  while(is >> k >> v) {
    if(k == "epoch") s.epoch = strtoull(v.c_str(), 0, 10);
    else if(k == "next_seq") s.next_seq = strtoull(v.c_str(), 0, 10);
    else if(k == "acked_seq") s.acked = strtoull(v.c_str(), 0, 10);
    else if(k == "applied_seq") s.applied = strtoull(v.c_str(), 0, 10);
    else if(k == "in_snapshot") s.in_snap = strtoull(v.c_str(), 0, 10);
    else if(k == "role") s.role = v;
    else if(k == "digest") s.digest = v;
  }
  s.ok = true;
  return s;
}
static size_t alarm_lines(int n)
{
  std::ifstream f(node_dir(n) + "/objrepl.alarm"); std::string l; size_t k = 0;
  while(std::getline(f, l)) if(!l.empty()) k++;
  return k;
}
static std::string alarm_text(int n) { std::ifstream f(node_dir(n) + "/objrepl.alarm"); std::stringstream s; s << f.rdbuf(); return s.str(); }

// ---------- checks inside an open node ----------
static std::string selfcheck(gxObjRepl &r, bool links)
{
  for(int d = 0; d < 3; d++) {
    for(uint64_t id : r.Ids(DBS[d])) {
      std::vector<unsigned char> b;
      if(r.Get(DBS[d], id, b) != 0) return std::string("Get failed for live ") + DBS[d] + " " + std::to_string(id);
      if(!sealed_ok(d, b, id)) return std::string("bad contents (id/checksum) ") + DBS[d] + " " + std::to_string(id) + " len " + std::to_string(b.size());
      if(!links) continue;
      for(auto &l : LF) {
	if(l.d != d || b.size() < l.off + 8) continue;
	uint64_t v; memcpy(&v, &b[l.off], 8);
	if(v && r.IdAt(DBS[l.target], (FAU_t)v) == 0)
	  return std::string("a link in ") + DBS[d] + " " + std::to_string(id) + " at " + std::to_string(l.off) + " names no object in " + DBS[l.target];
      }
    }
  }
  return "";
}

// exit codes of the children
enum { PX_OK = 0, PX_NOTDRAINED = 2, PX_REFUSED = 3, PX_OPEN4 = 4, PX_OPENFAIL = 40, PX_SELF = 41, PX_API = 42, PX_NOALARM = 43 };

// 11/16/2026: (fixed in a74479f: PROP_SALT_COLLIDE=1 now checks the fix) gxdJournal's header salt was time(0) ^ a per-process counter ^ the journal object's address. Two children
// forked from this controller in the same second open their node at the same heap address with the same counter, so
// they write the same salt, and Recover() then chains the stale records of the earlier session (left past the new
// ones: a clear only zeroes the magic) onto the new ones and replays old transactions (BUG 1 in the report). Each
// child moves its heap by a pid-dependent amount so the salts differ; PROP_SALT_COLLIDE=1 turns that off (reproducer)
static void heap_jitter()
{
  if(getenv("PROP_SALT_COLLIDE")) return;
  void *volatile pad = malloc(16 * (1 + (size_t)(getpid() % 4093)));           // below the mmap threshold: the heap
  void *volatile map = mmap(0, 4096 * (1 + (size_t)(getpid() % 997)), PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);   // and the mmap area
  (void)pad; (void)map;
}

// ---------- the copy, a child ----------
static volatile sig_atomic_t g_stop = 0;
static gxObjRepl *g_copy = 0;
static void on_term(int) { g_stop = 1; if(g_copy) g_copy->Stop(); }

struct Kid { pid_t pid = 0; int node = -1; long drop = 0; };

static int copy_main(int n, long drop)
{
  heap_jitter();
  if(drop) setenv("OBJREPL_TEST_DROP_EVERY", std::to_string(drop).c_str(), 1); else unsetenv("OBJREPL_TEST_DROP_EVERY");
  static gxObjRepl c;
  int rc = c.Open(node_dir(n), cfg_for(n, false), gxObjRepl::COPY);
  if(rc == 4) { logf("copy %s: Open rc 4 (%s)", node_name(n), c.Error().c_str()); return PX_OPEN4; }
  if(rc != 0) { logf("copy %s: Open failed: %s", node_name(n), c.Error().c_str()); return PX_OPENFAIL; }
  St s = status_of(n);   // read while open: the state file is only read
  std::string e = selfcheck(c, !s.in_snap);
  if(!e.empty()) { logf("FAILCHECK copy %s selfcheck at open: %s", node_name(n), e.c_str()); return PX_SELF; }
  g_copy = &c;
  struct sigaction sa; memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_term; sigaction(SIGTERM, &sa, 0);
  if(g_stop) c.Stop();
  rc = c.Serve();
  if(rc != 0) logf("copy %s: Serve returned %d: %s", node_name(n), rc, c.Error().c_str());
  c.Close();
  return rc == 0 ? PX_OK : PX_OPENFAIL;
}

// ---------- the primary, a child: commits, then a drain ----------
struct Plan {
  int commits = 1;          // how many commits
  int kill_at = -1;         // SIGKILL itself: inside commit k's transaction (before Commit())
  int kill_after = -1;      // SIGKILL itself right after commit k
  int drain = 2;            // 0 none, 1 one Pump(), 2 until drained
  int drain_secs = 20;
  uint64_t sub = 0;         // the content seed
};

static int pump_until(gxObjRepl &p, int secs, int n)
{
  auto until = std::chrono::steady_clock::now() + std::chrono::seconds(secs);
  size_t a0 = alarm_lines(n);
  for(;;) {
    int rc = p.Pump();
    if(rc == 3) {
      bool alarmed = alarm_lines(n) > a0 || !alarm_text(n).empty();
      logf("primary %s: Pump refused (3): %s; alarm %s", node_name(n), p.Error().c_str(), alarmed ? "written" : "MISSING");
      return alarmed ? PX_REFUSED : PX_NOALARM;
    }
    if(rc < 0) { logf("primary %s: Pump -1: %s", node_name(n), p.Error().c_str()); }
    if(rc == 0) {
      int d = p.Drain(5);
      if(d == 0) return PX_OK;
      logf("primary %s: Pump 0 but Drain %d", node_name(n), d);
    }
    if(std::chrono::steady_clock::now() >= until) return PX_NOTDRAINED;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

static int primary_main(int n, const Plan &pl)
{
  heap_jitter();
  gxObjRepl p;
  int rc = p.Open(node_dir(n), cfg_for(n, true), gxObjRepl::PRIMARY);
  if(rc == 4) { logf("primary %s: Open rc 4 (%s)", node_name(n), p.Error().c_str()); return PX_OPEN4; }
  if(rc != 0) { logf("primary %s: Open failed: %s", node_name(n), p.Error().c_str()); return PX_OPENFAIL; }
  std::string e = selfcheck(p, true);
  if(!e.empty()) { logf("FAILCHECK primary %s selfcheck at open: %s", node_name(n), e.c_str()); return PX_SELF; }
  for(int d = 0; d < 3; d++) p.OpenDatabase(DBS[d]);
  Rng R(pl.sub);
  long purge_refused = 0, purged = 0, rollbacks = 0;
  auto api = [&](int r, const char *what) { if(r != 0) { logf("FAILCHECK primary %s: %s failed: %s", node_name(n), what, p.Error().c_str()); _exit(PX_API); } };
  for(int k = 0; k < pl.commits; k++) {
    bool rollback = R.p(0.07);
    api(p.Begin(), "Begin");
    int nops = 1 + R.r(8);
    for(int o = 0; o < nops; o++) {
      int d = R.r(3);
      std::vector<uint64_t> live = p.Ids(DBS[d]), tombs = p.Ids(DBS[d], gxObjRepl::TOMBSTONES);
      size_t total = 0; for(int x = 0; x < 3; x++) total += p.Ids(DBS[x]).size();
      // a random link value for a field: 0, or the address of any record (live or tombstone) in the target database
      auto pick_addr = [&](int target) -> uint64_t {
	if(R.p(0.25)) return 0;
	std::vector<uint64_t> c = p.Ids(DBS[target]); std::vector<uint64_t> t = p.Ids(DBS[target], gxObjRepl::TOMBSTONES);
	c.insert(c.end(), t.begin(), t.end());
	if(c.empty()) return 0;
	return (uint64_t)p.AddressOf(DBS[target], c[R.r((int)c.size())]);
      };
      auto set_links = [&](int dd, std::vector<unsigned char> &b) {
	for(auto &l : LF) if(l.d == dd && b.size() >= l.off + 8) { uint64_t a = pick_addr(l.target); memcpy(&b[l.off], &a, 8); }
      };
      int what = R.r(100);
      if(total > 45 && what < 40) what = 50 + R.r(30);           // many objects: remove / purge more
      if(what < 30 || live.empty()) {                             // add
	uint64_t id = R.p(0.03) && !tombs.empty() ? tombs[R.r((int)tombs.size())] : p.NewId();   // sometimes revive a tombstone
	std::vector<unsigned char> b; make_obj(R, d, min_size(d) + R.r(200), b); set_links(d, b); seal(d, b, id);
	api(p.Put(DBS[d], id, b.data(), (uint32_t)b.size()), "Put(add)");
      } else if(what < 50) {                                      // update: mostly grows (moves)
	uint64_t id = live[R.r((int)live.size())];
	std::vector<unsigned char> old; p.Get(DBS[d], id, old);
	size_t n2 = R.p(0.7) ? old.size() + 1 + R.r(400) : min_size(d) + R.r(old.size() > min_size(d) ? (int)(old.size() - min_size(d)) : 1);
	std::vector<unsigned char> b; make_obj(R, d, n2, b);
	if(R.p(0.5)) { for(auto &l : LF) if(l.d == d && old.size() >= l.off + 8 && b.size() >= l.off + 8) memcpy(&b[l.off], &old[l.off], 8); }
	else set_links(d, b);
	seal(d, b, id);
	api(p.Put(DBS[d], id, b.data(), (uint32_t)b.size()), "Put(update)");
      } else if(what < 65) {                                      // remove
	api(p.Remove(DBS[d], live[R.r((int)live.size())]), "Remove");
      } else if(what < 68) {                                      // remove of an id never seen: a tombstone
	api(p.Remove(DBS[d], p.NewId()), "Remove(unknown)");
      } else if(what < 80) {                                      // purge a tombstone (refused while a live link names it)
	if(tombs.empty()) continue;
	uint64_t id = tombs[R.r((int)tombs.size())];
	if(p.Purge(DBS[d], id) == 0) purged++;
	else if(p.Error().find("still named by a link") != std::string::npos) purge_refused++;
	else { logf("FAILCHECK primary %s: Purge %s %llu failed: %s", node_name(n), DBS[d], (unsigned long long)id, p.Error().c_str()); _exit(PX_API); }
      } else if(what < 90) {                                      // a forward reference and a cycle inside one commit
	uint64_t x = p.NewId(), y = p.NewId();
	std::vector<unsigned char> bx, by;
	make_obj(R, 1, 40 + R.r(100), bx); seal(1, bx, x);
	api(p.Put("lk", x, bx.data(), (uint32_t)bx.size()), "Put(cycle x)");    // x first, its link to y not set yet
	make_obj(R, 1, 40 + R.r(100), by);
	uint64_t ax = (uint64_t)p.AddressOf("lk", x); memcpy(&by[8], &ax, 8);
	uint64_t a0 = pick_addr(0); memcpy(&by[0], &a0, 8); seal(1, by, y);
	api(p.Put("lk", y, by.data(), (uint32_t)by.size()), "Put(cycle y)");
	uint64_t ay = (uint64_t)p.AddressOf("lk", y); memcpy(&bx[8], &ay, 8);   // x -> y: the record names a later add
	if(R.p(0.5)) bx.resize(bx.size() + 1 + R.r(300));                         // and x grows (moves) too
	seal(1, bx, x);
	api(p.Put("lk", x, bx.data(), (uint32_t)bx.size()), "Put(cycle x2)");
	if(R.p(0.3)) {                                                            // a db0 object <-> lk cross cycle
	  uint64_t z = p.NewId(); std::vector<unsigned char> bz; make_obj(R, 0, 64 + R.r(100), bz);
	  ax = (uint64_t)p.AddressOf("lk", x); memcpy(&bz[40], &ax, 8); seal(0, bz, z);
	  api(p.Put("db0", z, bz.data(), (uint32_t)bz.size()), "Put(cross z)");
	  uint64_t az = (uint64_t)p.AddressOf("db0", z); memcpy(&bx[0], &az, 8); seal(1, bx, x);
	  api(p.Put("lk", x, bx.data(), (uint32_t)bx.size()), "Put(cross x)");
	}
      } else if(what < 95) {                                      // a self link
	uint64_t x = p.NewId(); std::vector<unsigned char> bx; make_obj(R, 1, 40 + R.r(60), bx); seal(1, bx, x);
	api(p.Put("lk", x, bx.data(), (uint32_t)bx.size()), "Put(self)");
	uint64_t ax = (uint64_t)p.AddressOf("lk", x); memcpy(&bx[8], &ax, 8); seal(1, bx, x);
	api(p.Put("lk", x, bx.data(), (uint32_t)bx.size()), "Put(self2)");
      } else {                                                    // remove then purge in the same commit
	uint64_t id = live[R.r((int)live.size())];
	api(p.Remove(DBS[d], id), "Remove(+purge)");
	if(p.Purge(DBS[d], id) == 0) purged++; else if(p.Error().find("still named by a link") != std::string::npos) purge_refused++;
	else { logf("FAILCHECK primary %s: Purge after remove failed: %s", node_name(n), p.Error().c_str()); _exit(PX_API); }
      }
    }
    if(pl.kill_at == k) { logf("primary %s: SIGKILL itself inside commit %d's transaction", node_name(n), k); fflush(stdout); raise(SIGKILL); }
    if(rollback) { api(p.Rollback(), "Rollback"); rollbacks++; continue; }
    api(p.Commit(), "Commit");
    if(pl.kill_after == k) { logf("primary %s: SIGKILL itself after commit %d (seq %llu)", node_name(n), k, (unsigned long long)p.LastSeq()); fflush(stdout); raise(SIGKILL); }
    if(R.p(0.3)) p.Pump();                                       // sometimes stream as it goes
  }
  e = selfcheck(p, true);
  if(!e.empty()) { logf("FAILCHECK primary %s selfcheck after commits: %s", node_name(n), e.c_str()); return PX_SELF; }
  logf("primary %s: last seq %llu, copy applied %llu, purged %ld, purge refused (linked) %ld, rollbacks %ld", node_name(n),
       (unsigned long long)p.LastSeq(), (unsigned long long)p.CopyApplied(), purged, purge_refused, rollbacks);
  int rc2 = PX_OK;
  if(pl.drain == 1) p.Pump();
  else if(pl.drain == 2) rc2 = pump_until(p, pl.drain_secs, n);
  if(rc2 == PX_OK && pl.drain == 2 && p.CopyApplied() != p.LastSeq()) { logf("FAILCHECK drained but copy applied %llu != last %llu", (unsigned long long)p.CopyApplied(), (unsigned long long)p.LastSeq()); rc2 = PX_SELF; }
  p.Close();
  return rc2;
}

// ---------- children: fork, then exec this program again (--child), so each has its own ASLR layout ----------
static long ms_since_t0() { return (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_t0).count(); }
static void exec_child(const std::vector<std::string> &extra)
{
  std::vector<std::string> a = {g_self, "--child-pki", g_pki, "--child-dir", g_dir, "--child-port", std::to_string(g_port), "--child-t0", std::to_string(ms_since_t0())};
  a.insert(a.end(), extra.begin(), extra.end());
  std::vector<char *> v; for(auto &x : a) v.push_back((char *)x.c_str()); v.push_back(0);
  execv(g_self.c_str(), v.data());
  fprintf(stdout, "exec %s failed: %s\n", g_self.c_str(), strerror(errno)); fflush(stdout);
  _exit(127);
}
static bool g_collide() { return getenv("PROP_SALT_COLLIDE") != 0; }

// ---------- the controller ----------
static std::string g_fail_reason;
static int g_checks = 0;
#define FAIL(...) do { char _b[4096]; snprintf(_b, sizeof(_b), __VA_ARGS__); g_fail_reason = _b; logf("FAIL %s", _b); return false; } while(0)

struct Ctl {
  Rng R;
  int prim = 0;              // the node that is primary now
  Kid copy;                  // the copy child, if running
  pid_t ppid = 0;            // a primary child
  explicit Ctl(uint64_t s) : R(s) {}

  int copyn() const { return 1 - prim; }
  static int wait_kid(pid_t pid, int timeout_ms, bool *timed_out = 0) {
    int st = 0;
    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for(;;) {
      pid_t w = waitpid(pid, &st, WNOHANG);
      if(w == pid) break;
      if(w < 0) return -1000;
      if(std::chrono::steady_clock::now() >= until) { if(timed_out) *timed_out = true; kill(pid, SIGKILL); waitpid(pid, &st, 0); break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(WIFEXITED(st)) return WEXITSTATUS(st);
    if(WIFSIGNALED(st)) return -WTERMSIG(st);
    return -999;
  }
  // the copy: started, and up (listening) before this returns
  bool start_copy(long drop) {
    int n = copyn();
    fflush(stdout);
    pid_t k = fork();
    if(k == 0) { if(g_collide()) _exit(copy_main(n, drop)); exec_child({"copy", std::to_string(n), std::to_string(drop)}); }
    copy.pid = k; copy.node = n; copy.drop = drop;
    logf("copy %s started pid %d%s", node_name(n), (int)k, drop ? (" drop every " + std::to_string(drop)).c_str() : "");
    std::string sf = node_dir(n) + "/objrepl.status";
    auto up_until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while(std::chrono::steady_clock::now() < up_until) {
      int st = 0;
      if(waitpid(k, &st, WNOHANG) == k) {
	copy.pid = 0;
	int code = WIFEXITED(st) ? WEXITSTATUS(st) : -WTERMSIG(st);
	FAIL("copy %s exited at start (code %d)", node_name(n), code);
      }
      std::ifstream f(sf); std::string l; bool pid_ok = false, up = false;
      while(std::getline(f, l)) {
	if(l == "pid " + std::to_string(k)) pid_ok = true;
	if(l == "state listening" || l == "state streaming") up = true;
      }
      if(pid_ok && up) return true;
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    FAIL("copy %s did not come up in 30 s", node_name(n));
  }
  void kill_copy(const char *why) {
    if(!copy.pid) return;
    kill(copy.pid, SIGKILL); int rc = wait_kid(copy.pid, 5000);
    logf("copy %s pid %d SIGKILLed (%s), rc %d", node_name(copy.node), (int)copy.pid, why, rc);
    if(getenv("PROP_SAVE_JOURNALS")) { static int k = 0; std::error_code ec; fs::copy_file(node_dir(copy.node) + "/node.jnl", g_dir + "/jnl_" + std::to_string(++k) + "_" + node_name(copy.node), ec); logf("journal saved as jnl_%d_%s", k, node_name(copy.node)); }
    copy.pid = 0;
  }
  bool stop_copy_clean() {
    if(!copy.pid) return true;
    kill(copy.pid, SIGTERM); bool to = false; int rc = wait_kid(copy.pid, 90000, &to);   // 11/17/2026: 90 s (was 30: the close's checkpoint fsyncs)
    logf("copy %s pid %d stopped (TERM) rc %d", node_name(copy.node), (int)copy.pid, rc);
    copy.pid = 0;
    if(to) FAIL("the copy did not stop within 90 s of SIGTERM");
    if(rc != 0) FAIL("the copy exited %d on SIGTERM", rc);
    return true;
  }
  // a primary child with PLAN; KILL_MS >= 0: the controller SIGKILLs it after that many ms (and the copy after
  // COPY_KILL_MS >= 0). Returns the child's exit code (negative: the signal)
  int run_primary(const Plan &pl, int kill_ms = -1, int copy_kill_ms = -1) {
    logf("primary %s: commits %d kill_at %d kill_after %d drain %d%s%s", node_name(prim), pl.commits, pl.kill_at, pl.kill_after, pl.drain,
	 kill_ms >= 0 ? (", SIGKILL at " + std::to_string(kill_ms) + " ms").c_str() : "",
	 copy_kill_ms >= 0 ? (", copy SIGKILL at " + std::to_string(copy_kill_ms) + " ms").c_str() : "");
    fflush(stdout);
    pid_t k = fork();
    if(k == 0) {
      if(g_collide()) _exit(primary_main(prim, pl));
      exec_child({"primary", std::to_string(prim), std::to_string(pl.commits), std::to_string(pl.kill_at), std::to_string(pl.kill_after),
		  std::to_string(pl.drain), std::to_string(pl.drain_secs), std::to_string(pl.sub)});
    }
    auto t0 = std::chrono::steady_clock::now();
    bool pk = false, ck = false;
    int st = 0;
    for(;;) {
      pid_t w = waitpid(k, &st, WNOHANG);
      if(w == k) break;
      long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
      if(!pk && kill_ms >= 0 && ms >= kill_ms) { kill(k, SIGKILL); pk = true; logf("primary SIGKILLed by the controller at %ld ms", ms); }
      if(!ck && copy_kill_ms >= 0 && ms >= copy_kill_ms) { ck = true; kill_copy("during the primary's step"); }
      // 11/17/2026: 180 s (was 60 s: a drain of 30 s plus fsync-heavy commits went past it under disk load); the
      // child's stacks are saved before the kill, to tell a hang from a slow disk
      if(ms > 180000) {
	std::string cmd = "timeout 60 gdb -p " + std::to_string((long)k) + " -batch -ex 'thread apply all bt' 2>&1 | head -n 400";
	if(FILE *g = popen(cmd.c_str(), "r")) { char line[512]; while(fgets(line, sizeof(line), g)) logf("  stack: %s", line); pclose(g); }
	kill(k, SIGKILL); waitpid(k, &st, 0); logf("primary child hung 180 s: killed"); return -1001;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    int rc = WIFEXITED(st) ? WEXITSTATUS(st) : -WTERMSIG(st);
    logf("primary child rc %d", rc);
    return rc;
  }
  bool bad_child(int rc) { return rc == PX_OPENFAIL || rc == PX_SELF || rc == PX_API || rc == PX_NOALARM || rc == PX_OPEN4 || rc == -1001; }

  // invariants that hold whenever both nodes are stopped
  bool invariants(const char *when) {
    St P = status_of(prim), C = status_of(copyn());
    if(!P.ok) FAIL("%s: no state on the primary %s (%s)", when, node_name(prim), P.line.c_str());
    if(!C.ok) return true;                                      // a copy that never ran yet
    if(P.role != "primary") FAIL("%s: %s should be primary: %s", when, node_name(prim), P.line.c_str());
    if(C.role != "copy") FAIL("%s: %s should be copy: %s", when, node_name(copyn()), C.line.c_str());
    if(!C.in_snap && C.applied > P.next_seq - 1) FAIL("%s: copy applied %llu > primary last seq %llu", when, (unsigned long long)C.applied, (unsigned long long)(P.next_seq - 1));
    if(C.epoch > P.epoch) FAIL("%s: copy epoch %llu > primary epoch %llu", when, (unsigned long long)C.epoch, (unsigned long long)P.epoch);
    if(P.acked > P.next_seq - 1) FAIL("%s: primary acked %llu > last %llu", when, (unsigned long long)P.acked, (unsigned long long)(P.next_seq - 1));
    return true;
  }
  // both stopped and drained: the full comparison
  bool compare(const char *when) {
    if(!invariants(when)) return false;
    St P = status_of(prim), C = status_of(copyn());
    if(C.in_snap || C.applied != P.next_seq - 1 || C.epoch != P.epoch || C.digest != P.digest)
      FAIL("%s: drained but primary [%s] copy [%s]", when, P.line.c_str(), C.line.c_str());
    std::string da, db, xa, xb, ra, rb;
    int r1 = gxObjRepl::Digest(node_dir(prim), LINKS, da, &xa), r2 = gxObjRepl::Digest(node_dir(copyn()), LINKS, db, &xb);
    if(r1 || r2 || da != db) FAIL("%s: DIGEST primary %s [%s] (%s), copy %s [%s] (%s)", when, node_name(prim), da.c_str(), xa.c_str(), node_name(copyn()), db.c_str(), xb.c_str());
    int i1 = gxObjRepl::IndexCheck(node_dir(prim), INDEX, ra), i2 = gxObjRepl::IndexCheck(node_dir(copyn()), INDEX, rb);
    if(i1 || i2 || ra != rb) FAIL("%s: INDEXCHECK primary rc %d\n%s copy rc %d\n%s", when, i1, ra.c_str(), i2, rb.c_str());
    if(alarm_lines(0) || alarm_lines(1)) FAIL("%s: unexpected alarm: A [%s] B [%s]", when, alarm_text(0).c_str(), alarm_text(1).c_str());
    g_checks++;
    if(g_verbose) logf("compare %s: OK %s; %s; index %s", when, da.c_str(), xa.c_str(), ra.c_str());
    else logf("compare %s: OK %s (%s)", when, da.c_str(), xa.c_str());
    return true;
  }
  // bring both to drained and compare: the copy up (restarted if needed, no drops), a drain child
  bool settle(const char *when) {
    if(!copy.pid && !start_copy(R.p(0.2) ? 6 + R.r(10) : 0)) return false;
    Plan pl; pl.commits = 0; pl.drain = 2; pl.drain_secs = 30;
    int rc = run_primary(pl);
    if(rc != PX_OK) {
      if(rc == PX_NOTDRAINED) FAIL("%s: not drained in 30 s (copy drop %ld)", when, copy.drop);
      FAIL("%s: drain child rc %d", when, rc);
    }
    // the copy is idle once the drain returned 0 (every commit applied and acknowledged): compared as it runs, or
    // stopped first (killed, or now and then a clean stop)
    double x = (double)(R.u() % 1000) / 1000.0;
    if(x < 0.05) { if(!stop_copy_clean()) return false; } else if(x < 0.35) kill_copy("stop for the compare");
    return compare(when);
  }
  // a takeover of the copy (both stopped). CRASH: the primary died with commits the copy may not have
  bool takeover(bool crash) {
    int oldp = prim, newp = copyn();
    if(copy.pid) kill_copy("takeover");
    St P = status_of(oldp), C = status_of(newp);
    if(!C.ok) { logf("takeover skipped: the copy has no state yet"); return true; }
    if(!invariants("before takeover")) return false;
    std::string msg;
    if(getenv("PROP_SAVE_BEFORE_TAKEOVER")) { std::error_code ec; fs::remove_all(g_dir + "/before_takeover", ec); fs::create_directories(g_dir + "/before_takeover");
      fs::copy(node_dir(0), g_dir + "/before_takeover/A", fs::copy_options::recursive, ec); fs::copy(node_dir(1), g_dir + "/before_takeover/B", fs::copy_options::recursive, ec); }
    int rc = gxObjRepl::Takeover(node_dir(newp), msg);
    logf("Takeover %s rc %d: %s", node_name(newp), rc, msg.c_str());
    if(rc != 0) {
      if(C.in_snap && msg.find("full send") != std::string::npos) { logf("takeover refused mid full send (expected): skipped"); return true; }
      FAIL("Takeover refused: %s [copy %s]", msg.c_str(), C.line.c_str());
    }
    uint64_t last = P.next_seq - 1;
    if(C.applied > last) FAIL("takeover: copy applied %llu > old primary's last %llu", (unsigned long long)C.applied, (unsigned long long)last);
    long long lost = (long long)last - (long long)C.applied;
    logf("failover (%s): old primary %s last %llu, copy applied %llu: %lld commit(s) lost", crash ? "crash" : "planned", node_name(oldp),
	 (unsigned long long)last, (unsigned long long)C.applied, lost);
    if(lost < 0) FAIL("negative commits lost %lld", lost);
    if(!crash && lost != 0) FAIL("planned takeover lost %lld commits", lost);
    St N = status_of(newp);
    if(N.role != "primary" || N.epoch != C.epoch + 1 || N.next_seq != C.applied + 1) FAIL("after takeover: %s", N.line.c_str());
    // the old primary will not run as a copy before its rejoin: Open() rc 4 and an alarm
    {
      prim = newp;                                             // copyn() is the old primary now
      size_t a0 = alarm_lines(oldp);
      fflush(stdout);
      pid_t k = fork();
      if(k == 0) { gxObjRepl c; int r = c.Open(node_dir(oldp), cfg_for(oldp, false), gxObjRepl::COPY); _exit(r == 4 ? 4 : 50 + r); }
      int r = wait_kid(k, 10000);
      size_t a1 = alarm_lines(oldp);
      if(r != 4) FAIL("the old primary opened as a copy before rejoin: rc %d (want 4)", r);
      if(a1 <= a0) FAIL("Open rc 4 left no alarm");
      std::string m; gxObjRepl::ClearAlarm(node_dir(oldp), m);
    }
    msg.clear();
    rc = gxObjRepl::Rejoin(node_dir(oldp), msg);
    logf("Rejoin %s rc %d: %s", node_name(oldp), rc, msg.c_str());
    if(rc != 0) FAIL("Rejoin failed: %s", msg.c_str());
    St O = status_of(oldp);
    // a74479f: a former primary whose copy acknowledged every commit follows on from its last seq (no full send)
    if(msg.find("following on from seq") != std::string::npos) {
      if(lost != 0) FAIL("fast rejoin (no full send) but %lld commit(s) were lost", lost);
      if(O.role != "copy" || O.in_snap || O.applied != last) FAIL("after a fast rejoin: %s (old last %llu)", O.line.c_str(), (unsigned long long)last);
      logf("fast rejoin: %s follows on from seq %llu", node_name(oldp), (unsigned long long)last);
      return true;
    }
    size_t at = msg.find(" commit(s) past seq ");
    if(at == std::string::npos) FAIL("Rejoin message without the count: %s", msg.c_str());
    size_t b = msg.rfind(' ', at - 1);
    long long dropped = atoll(msg.substr(b + 1, at - b - 1).c_str());
    if(dropped < lost) FAIL("Rejoin says %lld dropped, but %lld were lost", dropped, lost);
    if(!crash && dropped != 0) FAIL("planned: Rejoin says %lld dropped", dropped);
    if(O.role != "copy" || !O.in_snap || O.applied != 0) FAIL("after rejoin: %s", O.line.c_str());
    return true;
  }

  bool run(int steps) {
    // first contact
    if(!start_copy(0)) return false;
    { Plan pl; pl.commits = 1 + R.r(3); pl.sub = R.u(); int rc = run_primary(pl); if(rc != PX_OK) FAIL("first step rc %d", rc); }
    kill_copy("stop for the compare");
    if(!compare("start")) return false;
    for(int s = 0; s < steps; s++) {
      int what = R.r(100);
      char when[64]; snprintf(when, sizeof(when), "step %d", s);
      logf("---- step %d (primary %s) what %d", s, node_name(prim), what);
      if(what < 40) {                                          // commits, maybe with a drop on the copy and kills
	if(!copy.pid && !start_copy(R.p(0.3) ? 6 + R.r(10) : 0)) return false;
	Plan pl; pl.commits = 1 + R.r(5); pl.sub = R.u(); pl.drain = R.r(3);
	int pk = -1, ck = -1;
	int k = R.r(10);
	if(k == 0) pl.kill_at = R.r(pl.commits);
	else if(k == 1) pl.kill_after = R.r(pl.commits);
	else if(k == 2) pk = R.r(40);
	if(R.p(0.2)) ck = R.r(40);
	if(ck >= 0 && pl.drain == 2) pl.drain = 1;               // the copy will be gone: no drain to wait for
	int rc = run_primary(pl, pk, ck);
	if(bad_child(rc)) FAIL("%s: primary child rc %d", when, rc);
	if(rc == PX_REFUSED) FAIL("%s: the stream was refused", when);
	if(pl.drain == 2 && rc == PX_NOTDRAINED) FAIL("%s: not drained in %d s (copy drop %ld)", when, pl.drain_secs, copy.drop);
	if(R.p(0.25)) kill_copy("random");
      } else if(what < 50) {                                   // the copy restarts (maybe with drops)
	kill_copy("restart");
	if(!invariants(when)) return false;
	if(!start_copy(R.p(0.5) ? 6 + R.r(10) : 0)) return false;
      } else if(what < 57) {                                   // the copy rejoins: a full send
	kill_copy("rejoin");
	if(!invariants(when)) return false;
	St C = status_of(copyn());
	if(C.ok) {
	  std::string msg; int rc = gxObjRepl::Rejoin(node_dir(copyn()), msg);
	  logf("Rejoin (copy) %s rc %d: %s", node_name(copyn()), rc, msg.c_str());
	  if(rc != 0) FAIL("copy rejoin failed: %s", msg.c_str());
	}
      } else if(what < 67) {                                   // planned takeover
	if(!settle(when)) return false;
	if(!takeover(false)) return false;
      } else if(what < 77) {                                   // crash takeover: the primary dies with unsent commits
	if(!copy.pid && !start_copy(0)) return false;
	Plan pl; pl.commits = 1 + R.r(5); pl.sub = R.u(); pl.drain = R.r(2);
	int pk = -1;
	if(R.p(0.5)) pl.kill_after = R.r(pl.commits); else if(R.p(0.5)) pl.kill_at = R.r(pl.commits); else pk = R.r(30);
	int rc = run_primary(pl, pk, R.p(0.3) ? R.r(30) : -1);
	if(bad_child(rc)) FAIL("%s: primary child rc %d", when, rc);
	if(!takeover(true)) return false;
      } else {                                                 // nothing new: settle only
      }
      // the step ends drained (most of the time): compare
      if(what >= 57 || R.p(0.5)) { if(!settle(when)) return false; }
      else { if(copy.pid && R.p(0.5)) kill_copy("between steps"); if(!copy.pid && !invariants(when)) return false; }
    }
    return settle("end");
  }
};

static int run_seed(uint64_t seed, int &steps_out)
{
  g_t0 = std::chrono::steady_clock::now();
  Rng R0(mix(seed, 1));
  int steps = 10 + R0.r(21);
  steps_out = steps;
  logf("seed %llu: %d steps, ports %d %d, dir %s", (unsigned long long)seed, steps, g_port, g_port + 1, g_dir.c_str());
  Ctl c(mix(seed, 2));
  bool ok = c.run(steps);
  if(c.copy.pid) c.kill_copy("end");
  logf("seed %llu: %s, %d compares%s%s", (unsigned long long)seed, ok ? "PASS" : "FAIL", g_checks, ok ? "" : ": ", g_fail_reason.c_str());
  return ok ? 0 : 1;
}

static int sh(const std::string &cmd) { int r = system(cmd.c_str()); return WIFEXITED(r) ? WEXITSTATUS(r) : -1; }
static std::string q(const std::string &s) { std::string o = "'"; for(char c : s) { if(c == '\'') o += "'\\''"; else o += c; } return o + "'"; }

int main(int argc, char **argv)
{
  uint64_t seed = 0; long count = 1;
  g_self = argv[0];
  if(argc >= 10 && std::string(argv[1]) == "--child-pki") {          // a child: copy N DROP | primary N PLAN...
    g_pki = argv[2]; g_dir = argv[4]; g_port = atoi(argv[6]);
    g_t0 = std::chrono::steady_clock::now() - std::chrono::milliseconds(atol(argv[8]));
    std::string what = argv[9];
    if(what == "copy" && argc >= 12) return copy_main(atoi(argv[10]), atol(argv[11]));
    if(what == "primary" && argc >= 17) {
      Plan pl; pl.commits = atoi(argv[11]); pl.kill_at = atoi(argv[12]); pl.kill_after = atoi(argv[13]);
      pl.drain = atoi(argv[14]); pl.drain_secs = atoi(argv[15]); pl.sub = strtoull(argv[16], 0, 10);
      return primary_main(atoi(argv[10]), pl);
    }
    return 64;
  }
  for(int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto nx = [&]() -> std::string { if(i + 1 >= argc) { fprintf(stderr, "%s needs a value\n", a.c_str()); exit(64); } return argv[++i]; };
    if(a == "--pki") g_pki = nx();
    else if(a == "--work") g_work = nx();
    else if(a == "--port") g_port = atoi(nx().c_str());
    else if(a == "--seed") seed = strtoull(nx().c_str(), 0, 10);
    else if(a == "--count") count = atol(nx().c_str());
    else if(a == "--failures") g_fail = nx();
    else if(a == "--verbose") g_verbose = true;
    else if(a == "--keep") g_keep = true;
    else { fprintf(stderr, "usage: prop_objrepl --pki DIR --work DIR --port P --seed N [--count C] [--failures DIR] [--verbose] [--keep]\n"); return 64; }
  }
  if(g_pki.empty() || g_work.empty() || g_port < 1024) { fprintf(stderr, "--pki, --work and --port are required\n"); return 64; }
  g_self = fs::absolute(fs::path(g_self)).string();
  g_pki = fs::absolute(g_pki).string(); g_work = fs::absolute(g_work).string();
  if(!g_fail.empty()) g_fail = fs::absolute(g_fail).string();
  fs::create_directories(g_work);
  long pass = 0, failn = 0;
  for(long i = 0; i < count; i++) {
    uint64_t s = seed + (uint64_t)i;
    g_dir = g_work + "/seed_" + std::to_string(s);
    std::error_code ec; fs::remove_all(g_dir, ec); fs::create_directories(g_dir);
    std::string logp = g_dir + ".log";
    for(;!getenv("PROP_NO_LOAD_GATE");) { double la[1]; if(getloadavg(la, 1) != 1 || la[0] <= 12.0) break; std::this_thread::sleep_for(std::chrono::seconds(5)); }   // the workstation load rule
    auto t0 = std::chrono::steady_clock::now();
    fflush(stdout);
    pid_t k = fork();
    if(k == 0) {
      setpgid(0, 0);
      int fd = open(logp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if(fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
      int steps = 0; int r = run_seed(s, steps);
      fflush(stdout);
      _exit(r == 0 ? 0 : 1);
    }
    setpgid(k, k);
    int st = 0; bool timeout = false;
    for(;;) {
      pid_t w = waitpid(k, &st, WNOHANG);
      if(w == k) break;
      if(std::chrono::steady_clock::now() - t0 > std::chrono::seconds(300)) { timeout = true; kill(-k, SIGKILL); waitpid(k, &st, 0); break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    kill(-k, SIGKILL);                                         // any child of the seed still there (its process group)
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    bool ok = !timeout && WIFEXITED(st) && WEXITSTATUS(st) == 0;
    std::string reason;
    {
      std::ifstream f(logp); std::string l;
      while(std::getline(f, l)) if(reason.empty() && (l.find("] FAIL ") != std::string::npos || l.find("] FAILCHECK ") != std::string::npos)) reason = l.substr(l.find("] FAIL") + 2);
      if(reason.size() > 400) reason.resize(400);
      for(char &ch : reason) if(ch == '\n') ch = '|';
      if(timeout) reason = "TIMEOUT (300 s)";
      else if(!WIFEXITED(st)) reason = "the seed process died, signal " + std::to_string(WTERMSIG(st));
    }
    int steps = 0; { Rng R0(mix(s, 1)); steps = 10 + R0.r(21); }
    int compares = 0; { std::ifstream f(logp); std::string l; while(std::getline(f, l)) if(l.find(" compare ") != std::string::npos && l.find(": OK") != std::string::npos) compares++; }
    printf("seed %llu %s steps %d checks %d secs %.2f%s%s\n", (unsigned long long)s, ok ? "PASS" : "FAIL", steps, compares, secs, ok ? "" : " ", reason.c_str());
    fflush(stdout);
    if(ok) pass++; else failn++;
    if(!ok && !g_fail.empty()) {
      std::string fd = g_fail + "/seed_" + std::to_string(s);
      fs::create_directories(fd);
      fs::copy_file(logp, fd + "/log.txt", fs::copy_options::overwrite_existing, ec);
      sh("tar -C " + q(g_work) + " -czf " + q(fd + "/nodes.tar.gz") + " " + q("seed_" + std::to_string(s)));
      std::ofstream r(fd + "/repro.sh");
      r << "#!/bin/bash\n# seed " << s << ": " << reason << "\n"
	<< "# re-runs exactly this seed (the same steps; kill timings may land differently). PKI: objrepl_certs.sh DIR --copy-host 127.0.0.1 --apply\n"
	<< (getenv("PROP_SALT_COLLIDE") ? "export PROP_SALT_COLLIDE=1   # children forked without exec: the gxdJournal salt collision (BUG 1)\n" : "")
	<< "W=$(mktemp -d /tmp/objrepl_prop_repro.XXXXXX)\n"
	<< "PKI=${PKI:-$W/pki}; [ -f \"$PKI/ca.pem\" ] || " << q((fs::path(__FILE__).parent_path() / "objrepl_certs.sh").string()) << " \"$PKI\" --copy-host 127.0.0.1 --apply >/dev/null\n"
	<< "nice -n 10 " << q(g_self) << " --pki \"$PKI\" --work \"$W/work\" --port ${PORT:-" << g_port << "} --seed " << s << " --count 1 --verbose --keep\n"
	<< "echo \"log: $W/work/seed_" << s << ".log\"\n";
      r.close();
      chmod((fd + "/repro.sh").c_str(), 0755);
    }
    if(!g_keep) { fs::remove_all(g_dir, ec); if(ok) fs::remove(logp, ec); }
  }
  printf("summary: seeds %ld passed %ld failed %ld\n", pass + failn, pass, failn);
  return failn ? 1 : 0;
}
