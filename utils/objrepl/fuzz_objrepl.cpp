// fuzz_objrepl.cpp - 11/16/2026: a wire fuzzer for the gxObjRepl copy (meant for a -fsanitize=address,undefined build).
// A copy serves in a thread; a raw TLS client with the primary's certificate (as in test_gxobjrepl_api's review 9
// case) replays a valid recorded session and then mutated frames: bit flips, truncation, extension, wrong lengths
// and counts, huge capacities, wrong epochs and seqs, frames out of order, duplicated (replayed) frames, dropped
// frames, frames from the other session, garbage frames and raw bytes outside the block framing.
//
// The sessions are recorded from a real primary: a recording server answers its hello as a new copy and records
// what it sends (session A: the hello, F_EPOCH and commits 1..N; session B: the hello, F_EPOCH and a full send,
// F_SNAP_BEGIN/BATCH/DLOG/END). Each case starts the copy from a template folder (T0 a new copy, T3 a copy that has
// commits 1..3), runs the case, and checks:
//   the copy thread is alive and Serve() still running (no crash; ASan/UBSan stop the process with a report)
//   its Digest is the template's, or that of a valid prefix of the session ("unchanged"/"prefix"); else the case
//     applied changed data ("mutated") - possible only for well-formed frames, as the stream has no per-frame MAC
//   IndexCheck consistent on the copy
//   a clean primary afterwards: Pump until 0 (then Digest = the primary's) or 3 (refused: then Rejoin the copy and
//     drain again, Digest = the primary's)
// and counts, for cases where the copy dropped the connection, those with an alarm and those without (silent).
//
//   fuzz_objrepl --pki DIR --work DIR --port P [--start K] [--cases N] [--seed S] [--failures DIR] [--verbose]
//
// Ports P (the copy) and P+1 (the recorder) on 127.0.0.1. One line per case with --verbose; "case K start" lines
// always (a crash leaves the case number last). Summary at the end. Exit 0 when no case found a problem.
#include "gxdlcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "gxobjrepl.h"
#include "gxstream.h"
using namespace datareel;
namespace fs = std::filesystem;

static const char *LINKS = "lk:0:db0,lk:8:lk,db0:40:lk";
static const char *INDEX = "k0:db0:B:0:8,v0:db0:V:8:8,k1:lk:B:16:4";
static std::string g_pki, g_work, g_fail;
static int g_port = 0;
static bool g_verbose = false;
typedef std::vector<unsigned char> Bytes;

static bool g_timing = getenv("FUZZ_TIMING") != 0;
static std::chrono::steady_clock::time_point g_tt;
static void tmark(const char *what) { if(!g_timing) return; auto n = std::chrono::steady_clock::now(); fprintf(stdout, "  t %-14s %6.1f ms\n", what, std::chrono::duration<double, std::milli>(n - g_tt).count()); g_tt = n; }
static void logf(const char *fmt, ...)
{
  char b[8192]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
  fprintf(stdout, "%s\n", b); fflush(stdout);
}
static uint64_t mix(uint64_t a, uint64_t b) { uint64_t x = a * 0x9E3779B97F4A7C15ULL ^ (b + 0x632BE59BD9B4E019ULL + (a << 6) + (a >> 2)); x ^= x >> 31; x *= 0xBF58476D1CE4E5B9ULL; x ^= x >> 29; return x; }
struct Rng {
  std::mt19937_64 m; explicit Rng(uint64_t s) : m(s) {}
  uint64_t u() { return m(); } int r(int n) { return n <= 0 ? 0 : (int)(m() % (uint64_t)n); }
  bool p(double x) { return (double)(m() >> 11) * (1.0 / 9007199254740992.0) < x; }
};
static std::string slurp(const std::string &f) { std::ifstream i(f); std::stringstream s; s << i.rdbuf(); return s.str(); }
static size_t alarm_lines(const std::string &root) { std::string t = slurp(root + "/objrepl.alarm"); size_t k = 0; for(char c : t) if(c == '\n') k++; return k; }

// ---------- configs ----------
static gxObjReplConfig copy_cfg()
{
  gxObjReplConfig c; c.name = "C"; c.ca = g_pki + "/ca.pem"; c.cert = g_pki + "/copy.pem"; c.key = g_pki + "/copy.key";
  c.listen_port = g_port; c.allow_cn = "objrepl-primary"; c.index = INDEX; c.idle_timeout = 5;
  c.links = LINKS;   // 11/16/2026: each node configures its own map; a hello with another map is refused
  c.log = [](const std::string &m) { if(g_verbose) logf("    copy: %s", m.c_str()); };
  return c;
}
static gxObjReplConfig prim_cfg(int port)
{
  gxObjReplConfig c; c.name = "P"; c.ca = g_pki + "/ca.pem"; c.cert = g_pki + "/primary.pem"; c.key = g_pki + "/primary.key";
  c.copy = "127.0.0.1:" + std::to_string(port); c.links = LINKS; c.index = INDEX; c.trim = false; c.idle_timeout = 30;
  c.log = [](const std::string &m) { if(g_verbose) logf("    primary: %s", m.c_str()); };
  return c;
}

// ---------- the history (deterministic): adds, growing updates, removes, purges, link cycles ----------
static void put_obj(gxObjRepl &p, Rng &R, const char *db, uint64_t id, size_t n, std::vector<std::pair<size_t, uint64_t>> links)
{
  Bytes b(n); for(auto &x : b) x = (unsigned char)R.u();
  if(!strcmp(db, "db0")) { for(int i = 0; i < 8; i++) b[i] = (unsigned char)('a' + R.r(3)); for(int i = 0; i < 8; i++) { float f = (float)R.r(100); memcpy(&b[8 + 4 * i], &f, 4); } memset(&b[40], 0, 8); }
  if(!strcmp(db, "lk")) memset(&b[0], 0, 16);
  for(auto &l : links) memcpy(&b[l.first], &l.second, 8);
  if(p.Put(db, id, b.data(), (uint32_t)b.size()) != 0) { logf("SETUP Put failed: %s", p.Error().c_str()); exit(2); }
}
static void make_history(gxObjRepl &p, int commits)
{
  Rng R(12345);
  for(const char *d : {"db0", "lk", "db1"}) p.OpenDatabase(d);
  std::vector<uint64_t> d0, l, d1;
  for(int c = 0; c < commits; c++) {
    p.Begin();
    for(int i = 0; i < 3; i++) { uint64_t id = p.NewId(); put_obj(p, R, "db0", id, 64 + R.r(100), {}); d0.push_back(id); }
    for(int i = 0; i < 2; i++) { uint64_t id = p.NewId(); put_obj(p, R, "db1", id, 16 + R.r(200), {}); d1.push_back(id); }
    // a cycle and a forward reference: x, then y -> x, then x -> y
    uint64_t x = p.NewId(), y = p.NewId();
    put_obj(p, R, "lk", x, 40, {});
    put_obj(p, R, "lk", y, 48, {{8, (uint64_t)p.AddressOf("lk", x)}, {0, (uint64_t)p.AddressOf("db0", d0.back())}});
    put_obj(p, R, "lk", x, 120 + R.r(100), {{8, (uint64_t)p.AddressOf("lk", y)}});
    l.push_back(x); l.push_back(y);
    put_obj(p, R, "db0", d0[d0.size() - 2], 300 + 50 * c, {{40, (uint64_t)p.AddressOf("lk", x)}});   // grows: moves
    if(c >= 1) { p.Remove("db1", d1[c - 1]); p.Remove("db0", d0[(c - 1) * 3]); }
    if(c >= 2) { p.Purge("db1", d1[c - 2]); p.Remove("lk", p.NewId()); }
    if(p.Commit() != 0) { logf("SETUP Commit failed: %s", p.Error().c_str()); exit(2); }
  }
}

// ---------- frames ----------
static void put8(Bytes &b, uint8_t v) { b.push_back(v); }
static void put16(Bytes &b, uint16_t v) { b.push_back((unsigned char)v); b.push_back((unsigned char)(v >> 8)); }
static void put32(Bytes &b, uint32_t v) { for(int i = 0; i < 4; i++) b.push_back((unsigned char)(v >> (8 * i))); }
static void put64(Bytes &b, uint64_t v) { for(int i = 0; i < 8; i++) b.push_back((unsigned char)(v >> (8 * i))); }
static void putstr(Bytes &b, const std::string &s) { put16(b, (uint16_t)s.size()); b.insert(b.end(), s.begin(), s.end()); }

enum FK { K_KIND, K_MAGIC, K_EPOCH, K_SEQ, K_COUNT, K_STRLEN, K_OP, K_ID, K_CAP, K_FLAG, K_LEN, K_DIGEST, K_ROLE };
static const char *FKN[] = {"kind", "magic", "epoch", "seq", "count", "strlen", "op", "id", "cap", "flag", "len", "digest", "role"};
struct Field { size_t off; int size; int kind; };
struct Walk {
  const Bytes &b; size_t o = 0; std::vector<Field> f; bool ok = true;
  explicit Walk(const Bytes &x) : b(x) {}
  bool need(size_t k) { if(o + k > b.size()) ok = false; return ok; }
  uint64_t num(int sz, int kind) { if(!need(sz)) return 0; uint64_t v = 0; for(int i = 0; i < sz; i++) v |= (uint64_t)b[o + i] << (8 * i); f.push_back({o, sz, kind}); o += sz; return v; }
  void str() { uint64_t n = num(2, K_STRLEN); if(need(n)) o += n; }
  void raw(uint64_t n) { if(need(n)) o += n; }
};
// the fields of a frame (hello: index 0 of a session)
static std::vector<Field> fields(const Bytes &b, bool hello)
{
  Walk w(b);
  if(hello) { w.num(4, K_MAGIC); w.num(8, K_EPOCH); w.str(); w.str(); w.num(1, K_ROLE); w.num(8, K_SEQ); w.num(8, K_DIGEST); return w.f; }
  int kind = (int)w.num(1, K_KIND);
  if(kind == 1) {
    w.num(8, K_SEQ); w.num(8, K_EPOCH); uint64_t n = w.num(4, K_COUNT);
    for(uint64_t i = 0; i < n && w.ok; i++) {
      int op = (int)w.num(1, K_OP); w.str(); w.num(8, K_ID);
      if(op == 1 || op == 2) { w.num(4, K_CAP); w.num(1, K_FLAG); uint64_t l = w.num(4, K_LEN); w.raw(l); }
    }
  } else if(kind == 2) { w.num(8, K_SEQ); w.num(8, K_EPOCH); }
  else if(kind == 3) {
    uint64_t n = w.num(4, K_COUNT);
    for(uint64_t i = 0; i < n && w.ok; i++) { w.str(); w.num(8, K_ID); w.num(4, K_FLAG); w.num(4, K_FLAG); w.num(4, K_FLAG); w.num(8, K_SEQ); w.num(4, K_CAP); uint64_t l = w.num(4, K_LEN); w.raw(l); }
  } else if(kind == 6) { uint64_t n = w.num(4, K_COUNT); for(uint64_t i = 0; i < n && w.ok; i++) { w.num(8, K_SEQ); w.num(8, K_DIGEST); } }
  else if(kind == 4) { w.num(8, K_SEQ); w.num(8, K_DIGEST); w.num(8, K_COUNT); w.num(8, K_DIGEST); }   // a74479f: records count, hash
  else if(kind == 5) { w.num(8, K_EPOCH); }
  return w.f;
}
static uint64_t getn(const Bytes &b, const Field &f) { uint64_t v = 0; for(int i = 0; i < f.size; i++) v |= (uint64_t)b[f.off + i] << (8 * i); return v; }
static void setn(Bytes &b, const Field &f, uint64_t v) { for(int i = 0; i < f.size; i++) b[f.off + i] = (unsigned char)(v >> (8 * i)); }
static uint64_t interesting(Rng &R, int size, int kind, uint64_t v)
{
  uint64_t mx = size == 8 ? ~0ULL : ((1ULL << (8 * size)) - 1);
  std::vector<uint64_t> c = {0, 1, mx, mx - 1, v + 1, v - 1, v + 2, v * 2, mx / 2, mx / 2 + 1, R.u() & mx};
  if(kind == K_CAP || kind == K_LEN) { c.insert(c.end(), {200000000ULL, 0xFFFFFFFFULL, 32u * 1024 * 1024 + 1, 32u * 1024 * 1024 + 64, 33554432ULL + 100, 64, 0x7FFFFFFFULL, v + 64, v - 8}); }
  if(kind == K_EPOCH || kind == K_SEQ) { c.insert(c.end(), {v + 1000, v + 1, v - 1, (uint64_t)1 << 40, 0, mx}); }
  if(kind == K_COUNT) { c.insert(c.end(), {v + 1, v - 1, 65536, 65537, 1000000, 0xFFFFFFFFULL}); }
  if(kind == K_OP || kind == K_KIND) { c.insert(c.end(), {0, 1, 2, 3, 4, 5, 6, 7, 255}); }
  if(kind == K_FLAG) { c.insert(c.end(), {1, 2, 3, 0xFFFFFFFFULL}); }
  return c[R.r((int)c.size())] & mx;
}

// ---------- the recorder: answers a primary's hello as a copy, records its frames ----------
struct Session { Bytes hello; std::vector<Bytes> frames; };
static bool record(gxObjRepl &p, bool full_send, Session &out)
{
  gxStreamServer srv;
  srv.SetClientCAFile(g_pki + "/ca.pem");
  srv.AllowClientCN("objrepl-primary");
  if(srv.Listen(g_port + 1, g_pki + "/copy.pem", g_pki + "/copy.key", std::string(), "127.0.0.1") != 0) { logf("SETUP recorder listen: %s", srv.Error().c_str()); return false; }
  std::atomic<bool> done{false}; bool ok = false;
  std::thread t([&]() {
    if(srv.Listener().WaitReadable(10000) <= 0) return;
    gxStream st; st.SetTimeOut(10);
    if(srv.Accept(st) != 0) return;
    gxBlockHeader gx; Bytes b;
    if(st.ReadHeader(gx) != 0 || st.ReadBlock(b, gx) != 0) return;
    out.hello = b;
    Bytes rep; put8(rep, 1); put64(rep, 0); put64(rep, 0); put8(rep, full_send ? 1 : 0); putstr(rep, "C"); put8(rep, 2); put64(rep, 0); putstr(rep, "");
    st.WriteBlock(rep.data(), (int)rep.size());
    for(;;) {
      if(st.ReadHeader(gx) != 0 || st.ReadBlock(b, gx) != 0) break;
      out.frames.push_back(b);
      uint64_t a = 0;
      if(b[0] == 1 || b[0] == 4) for(int i = 0; i < 8; i++) a |= (uint64_t)b[1 + i] << (8 * i);
      Bytes ack; put64(ack, a); st.WriteBlock(ack.data(), (int)ack.size());
      ok = true;
    }
    done = true;
  });
  int rc = 1;
  for(int i = 0; i < 200 && rc == 1; i++) { rc = p.Pump(); if(rc == 1) std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
  p.Close();
  t.join();
  srv.Close();
  return ok && rc == 0;
}

// ---------- the copy in a thread ----------
struct Copy {
  gxObjRepl r; std::thread t; std::atomic<int> rc{-99}; std::atomic<bool> running{false};
  std::string dir;
  bool start(const std::string &d) {
    dir = d;
    fs::remove(d + "/objrepl.status");
    int o = r.Open(d, copy_cfg(), gxObjRepl::COPY);
    if(o != 0) { logf("copy Open rc %d: %s", o, r.Error().c_str()); return false; }
    running = true; rc = -99;
    t = std::thread([this]() { rc = r.Serve(); running = false; });
    for(int i = 0; i < 5000; i++) {
      if(!running) { t.join(); r.Close(); return false; }
      std::string s = slurp(d + "/objrepl.status");
      if(s.find("state listening") != std::string::npos) return true;
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    stop();
    return false;
  }
  ~Copy() { if(t.joinable()) stop(); }
  // stop: Stop(), then a plain TCP connect to the listener so its wait returns at once. false: the thread did not end
  bool stop() {
    if(!t.joinable()) return true;
    r.Stop();
    int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a; memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons((uint16_t)g_port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(s >= 0) { connect(s, (sockaddr *)&a, sizeof(a)); close(s); }
    for(int i = 0; i < 120000 && running; i++) std::this_thread::sleep_for(std::chrono::microseconds(500));   // 60 s (was 5)
    if(running) return false;
    t.join(); r.Close();
    return true;
  }
};

// ---------- a raw client ----------
struct Client {
  gxStream st; bool up = false;
  bool connect() {
    st.Connection().SetCAFile(g_pki + "/ca.pem"); st.SetClientCert(g_pki + "/primary.pem", g_pki + "/primary.key"); st.SetTimeOut(30);   // 11/17/2026: was 3 s (a copy's commit fsyncs under disk load)
    up = st.Connect("127.0.0.1", g_port) == 0;
    if(up) st.Connection().Socket().SetNoDelay(true);        // as the primary does: no Nagle wait per frame
    return up;
  }
  bool send(const Bytes &b) { return st.WriteBlock(b.data(), (int)b.size()) == 0; }
  bool recv(Bytes &b) { gxBlockHeader gx; return st.ReadHeader(gx) == 0 && st.ReadBlock(b, gx) == 0; }
  bool raw(const Bytes &b) { return st.Connection().Send(b.data(), (int)b.size()) == (int)b.size(); }
  void close() { st.Close(); }
};

// a step of a case: a frame (expects a reply/ack) or raw bytes (no reply; the case ends after them)
struct Act { Bytes b; bool raw = false; bool mutated = false; };

static Session S[2];                        // A commits, B full send
static std::string D_P;                     // the primary's digest
static std::map<std::string, std::set<std::string>> valid_dig;   // (template, session) -> digests of valid prefixes
static std::map<std::string, std::string> tmpl_dig;

static void restore(const std::string &tmpl, const std::string &dst)
{
  std::error_code ec; fs::remove_all(dst, ec); fs::create_directories(dst);
  for(auto &e : fs::directory_iterator(tmpl)) if(e.path().filename() != "objrepl.lock") fs::copy(e.path(), fs::path(dst) / e.path().filename(), fs::copy_options::recursive, ec);
}
static std::string digest(const std::string &d) { std::string l; gxObjRepl::Digest(d, LINKS, l); return l; }

// one session's frames through a client: returns how many acts were answered; stops at the first unanswered
static int play(const std::vector<Act> &acts, Bytes *hello_reply = 0, int *mutated_answered = 0)
{
  Client c; if(!c.connect()) return -1;
  int n = 0;
  for(size_t i = 0; i < acts.size(); i++) {
    if(acts[i].raw) { c.raw(acts[i].b); break; }
    if(!c.send(acts[i].b)) break;
    Bytes r;
    if(!c.recv(r)) break;
    if(i == 0 && hello_reply) *hello_reply = r;
    if(i == 0 && (r.empty() || r[0] != 1)) { n++; break; }   // the hello refused
    if(acts[i].mutated && mutated_answered) (*mutated_answered)++;
    n++;
  }
  c.close();
  return n;
}

static std::vector<Act> valid_acts(int s) { std::vector<Act> v; v.push_back({S[s].hello}); for(auto &f : S[s].frames) v.push_back({f}); return v; }

// ---------- the mutations ----------
static std::string mutate(Rng &R, int s, std::vector<Act> &a, int &cut)
{
  std::string what;
  int n = (int)a.size();
  auto pick = [&](bool hello_ok) {
    if(R.p(0.2)) {                                             // F_EPOCH and F_SNAP_DLOG frames on purpose (batch 3)
      std::vector<int> c; for(int i = 1; i < n; i++) if(!a[i].b.empty() && (a[i].b[0] == 5 || a[i].b[0] == 6)) c.push_back(i);
      if(!c.empty()) return c[R.r((int)c.size())];
    }
    int lo = hello_ok && R.p(0.35) ? 0 : 1; return lo + R.r(n - lo);
  };
  int kinds = R.p(0.08) ? 2 + R.r(3) : 1;
  int first = n;
  for(int k = 0; k < kinds; k++) {
    int m = R.r(100), j = pick(true);
    Bytes &b = a[j].b;
    char d[200];
    if(m < 15) { int bits = 1 + R.r(8); for(int i = 0; i < bits && !b.empty(); i++) b[R.r((int)b.size())] ^= (unsigned char)(1 << R.r(8)); snprintf(d, sizeof(d), "bitflip%d@%d", bits, j); }
    else if(m < 40 || (m < 50 && j > 0)) {                    // a field: lengths, counts, capacities, seqs, epochs ...
      auto f = fields(b, j == 0);
      if(f.empty()) { b.push_back((unsigned char)R.u()); snprintf(d, sizeof(d), "extend1@%d", j); }
      else {
	std::vector<int> pref;
	int want = m < 40 ? -1 : (R.p(0.5) ? K_CAP : K_LEN);
	for(int i = 0; i < (int)f.size(); i++) if(want < 0 || f[i].kind == want) pref.push_back(i);
	if(pref.empty()) for(int i = 0; i < (int)f.size(); i++) pref.push_back(i);
	Field &x = f[pref[R.r((int)pref.size())]];
	uint64_t v0 = getn(b, x), v = interesting(R, x.size, x.kind, v0);
	setn(b, x, v);
	snprintf(d, sizeof(d), "field %s@%d+%zu %llu->%llu", FKN[x.kind], j, x.off, (unsigned long long)v0, (unsigned long long)v);
      }
    }
    else if(m < 55) { size_t l = b.empty() ? 0 : R.r((int)b.size()); b.resize(l); snprintf(d, sizeof(d), "truncate@%d to %zu", j, l); }
    else if(m < 62) { int e = R.p(0.9) ? 1 + R.r(64) : 1000 + R.r(100000); for(int i = 0; i < e; i++) b.push_back((unsigned char)R.u()); snprintf(d, sizeof(d), "extend%d@%d", e, j); }
    else if(m < 68 && n > 2) { int i1 = 1 + R.r(n - 1), i2 = 1 + R.r(n - 1); std::swap(a[i1], a[i2]); j = std::min(i1, i2); snprintf(d, sizeof(d), "swap %d %d", i1, i2); }
    else if(m < 76 && n > 1) { int i1 = 1 + R.r(n - 1), at = i1 + 1 + R.r(n - i1); Act c = a[i1]; a.insert(a.begin() + std::min(at, (int)a.size()), c); j = std::min(at, (int)a.size() - 1); snprintf(d, sizeof(d), "dup %d at %d", i1, at); }
    else if(m < 80 && n > 2) { int i1 = 1 + R.r(n - 1); a.erase(a.begin() + i1); j = std::min(i1, (int)a.size() - 1); snprintf(d, sizeof(d), "drop %d", i1); }
    else if(m < 86) {                                          // a garbage frame (sometimes with a good kind byte)
      Bytes g(R.p(0.2) ? 0 : 1 + R.r(300)); for(auto &x : g) x = (unsigned char)R.u();
      if(!g.empty() && R.p(0.6)) g[0] = (unsigned char)(1 + R.r(6));
      int at = 1 + R.r(n); a.insert(a.begin() + std::min(at, (int)a.size()), Act{g}); j = std::min(at, (int)a.size() - 1);
      snprintf(d, sizeof(d), "garbage%zu kind %d at %d", g.size(), g.empty() ? -1 : g[0], at);
    }
    else if(m < 92) {                                          // a frame of the other session
      const Session &o = S[1 - s];
      if(o.frames.empty()) continue;
      int i1 = R.r((int)o.frames.size()); int at = 1 + R.r(n); a.insert(a.begin() + std::min(at, (int)a.size()), Act{o.frames[i1]}); j = std::min(at, (int)a.size() - 1);
      snprintf(d, sizeof(d), "splice other[%d] at %d", i1, at);
    }
    else if(m < 96) {                                          // raw bytes outside the block framing
      Bytes g(1 + R.r(64)); for(auto &x : g) x = (unsigned char)R.u();
      int at = 1 + R.r(n); Act r; r.b = g; r.raw = true; a.insert(a.begin() + std::min(at, (int)a.size()), r); a.resize(std::min(at, (int)a.size() - 1) + 1); j = (int)a.size() - 1;
      snprintf(d, sizeof(d), "raw%zu at %d", g.size(), at);
    }
    else {                                                     // a huge count with a short frame
      int i1 = pick(false); auto f = fields(a[i1].b, false);
      for(auto &x : f) if(x.kind == K_COUNT) { setn(a[i1].b, x, 0xFFFFFFF0u - R.r(16)); break; }
      j = i1; snprintf(d, sizeof(d), "hugecount@%d", i1);
    }
    n = (int)a.size();
    if(j >= 0 && j < n) a[j].mutated = true;
    first = std::min(first, j);
    what += (what.empty() ? "" : "+") + std::string(d);
  }
  // after the (first) mutated frame: sometimes the rest of the session, sometimes stop there
  cut = R.p(0.5) ? (int)a.size() : std::min((int)a.size(), first + 1);
  return what;
}

// ---------- the counts ----------
struct Counts {
  long cases = 0, problems = 0;
  long hello_refused = 0, dropped = 0, dropped_alarm = 0, dropped_silent = 0, accepted_all = 0;
  long unchanged = 0, prefix = 0, mutated_applied = 0;
  long clean_ok = 0, clean_refused_rejoined = 0, fenced_epoch = 0;
  long undetected = 0, index_bad = 0, copy_dead = 0, stuck = 0, not_recovered = 0;
  long slow_back = 0; double max_back_secs = 0;   // 11/17/2026: the copy back to listening after 10 s (not stuck)
  std::map<std::string, long> alarm_reasons;
  std::map<std::string, long> by_kind, by_kind_applied;      // cases by the kind of the (first) mutated frame
};
static Counts C;

static std::string short_alarm(const std::string &line)
{
  // "YYYY-MM-DD HH:MM:SS reason": the reason without numbers, to group them
  std::string r = line.size() > 20 ? line.substr(20) : line, o;
  for(char c : r) { if(isdigit((unsigned char)c)) { if(o.empty() || o.back() != '#') o += '#'; } else o += c; }
  if(o.size() > 90) o.resize(90);
  return o;
}

static void problem(long i, const std::string &what, const std::string &why, const std::string &cdir)
{
  C.problems++;
  logf("PROBLEM case %ld [%s]: %s", i, what.c_str(), why.c_str());
  if(g_fail.empty()) return;
  static std::map<std::string, int> saved;                     // the first 20 of each kind keep their copy folder
  std::string cat = why.substr(0, why.find_first_of(":( "));
  int already = 0;
  std::error_code ec0; fs::create_directories(g_fail, ec0);
  for(auto &e : fs::directory_iterator(g_fail, ec0)) if(e.path().filename().string().rfind("case_", 0) == 0) {
    std::string w = slurp((e.path() / "what.txt").string()); size_t nl = w.find('\n'); if(nl != std::string::npos && w.compare(nl + 1, cat.size(), cat) == 0) already++; }
  if(already + saved[cat] >= 20) return;
  saved[cat]++;
  std::string d = g_fail + "/case_" + std::to_string(i);
  std::error_code ec; fs::create_directories(d, ec);
  std::ofstream(d + "/what.txt") << "case " << i << ": " << what << "\n" << why << "\n";
  std::string cmd = "tar -C " + fs::path(cdir).parent_path().string() + " -czf " + d + "/copy.tar.gz " + fs::path(cdir).filename().string() + " 2>/dev/null";
  if(system(cmd.c_str()) != 0) {}
}

int main(int argc, char **argv)
{
  long start = 0, cases = 1000; uint64_t seed = 1;
  for(int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto nx = [&]() -> std::string { if(i + 1 >= argc) exit(64); return argv[++i]; };
    if(a == "--pki") g_pki = nx(); else if(a == "--work") g_work = nx(); else if(a == "--port") g_port = atoi(nx().c_str());
    else if(a == "--start") start = atol(nx().c_str()); else if(a == "--cases") cases = atol(nx().c_str());
    else if(a == "--seed") seed = strtoull(nx().c_str(), 0, 10); else if(a == "--failures") g_fail = nx();
    else if(a == "--verbose") g_verbose = true;
    else { fprintf(stderr, "usage: fuzz_objrepl --pki DIR --work DIR --port P [--start K] [--cases N] [--seed S] [--failures DIR] [--verbose]\n"); return 64; }
  }
  if(g_pki.empty() || g_work.empty() || g_port < 1024) { fprintf(stderr, "--pki, --work and --port are required\n"); return 64; }
  std::error_code ec; fs::remove_all(g_work, ec); fs::create_directories(g_work);
  std::string W = g_work;

  // ---- the primary and the two recorded sessions ----
  {
    gxObjRepl p;
    if(p.Open(W + "/p", prim_cfg(g_port + 1), gxObjRepl::PRIMARY) != 0) { logf("SETUP primary open: %s", p.Error().c_str()); return 2; }
    make_history(p, 6);
    if(!record(p, false, S[0])) { logf("SETUP recording session A failed"); return 2; }
  }
  {
    gxObjRepl p;
    if(p.Open(W + "/p", prim_cfg(g_port + 1), gxObjRepl::PRIMARY) != 0) { logf("SETUP primary open: %s", p.Error().c_str()); return 2; }
    if(!record(p, true, S[1])) { logf("SETUP recording session B failed"); return 2; }
  }
  D_P = digest(W + "/p");
  logf("setup: session A %zu frames, session B %zu frames; primary %s", S[0].frames.size(), S[1].frames.size(), D_P.c_str());
  for(int s = 0; s < 2; s++) { std::string k; for(auto &f : S[s].frames) k += std::to_string(f[0]) + "(" + std::to_string(f.size()) + ") "; logf("  session %c: hello(%zu) %s", 'A' + s, S[s].hello.size(), k.c_str()); }

  // ---- templates: T0 (new copy), T3 (commits 1..3) ----
  std::string T0 = W + "/T0", T3 = W + "/T3", CD = W + "/c";
  { Copy c; fs::create_directories(T0); if(!c.start(T0) || !c.stop()) { logf("SETUP T0 failed"); return 2; } fs::remove(T0 + "/objrepl.status"); fs::remove(T0 + "/objrepl.lock"); }
  {
    restore(T0, CD); Copy c; if(!c.start(CD)) return 2;
    auto v = valid_acts(0); v.resize(2 + 3);
    int n = play(v); if(!c.stop() || n != 5) { logf("SETUP T3 failed (%d)", n); return 2; }
    restore(CD, T3); fs::remove(T3 + "/objrepl.status");
  }
  tmpl_dig["T0"] = digest(T0); tmpl_dig["T3"] = digest(T3);
  // the digests of every valid prefix of each session from each template
  for(const char *t : {"T0", "T3"}) for(int s = 0; s < 2; s++) {
    auto v = valid_acts(s);
    std::string key = std::string(t) + (char)('A' + s);
    valid_dig[key].insert(tmpl_dig[t]);
    for(size_t k = 1; k <= v.size(); k++) {
      restore(t[1] == '0' ? T0 : T3, CD); Copy c; if(!c.start(CD)) return 2;
      std::vector<Act> pre(v.begin(), v.begin() + k);
      int n = play(pre);
      if(!c.stop()) return 2;
      std::string d = digest(CD);
      valid_dig[key].insert(d);
      if(g_verbose) logf("  valid %s prefix %zu (answered %d): %s", key.c_str(), k, n, d.c_str());
      if(k == v.size() && d != D_P) { logf("SETUP: the full valid session %s does not give the primary's digest (%s vs %s)", key.c_str(), d.c_str(), D_P.c_str()); return 2; }
    }
  }
  logf("setup: templates T0 %s, T3 %s", tmpl_dig["T0"].c_str(), tmpl_dig["T3"].c_str());

  // ---- the cases ----
  auto t_start = std::chrono::steady_clock::now();
  for(long i = start; i < start + cases; i++) {
    Rng R(mix(seed, (uint64_t)i));
    int t = R.r(2), s = R.r(2);
    std::string tn = t ? "T3" : "T0", key = tn + (char)('A' + s);
    std::vector<Act> acts = valid_acts(s);
    int cut = 0;
    std::string what = mutate(R, s, acts, cut);
    std::string mkind = "none";
    for(size_t q = 0; q < acts.size(); q++) if(acts[q].mutated) {
      static const char *KN[] = {"?", "F_COMMIT", "F_SNAP_BEGIN", "F_SNAP_BATCH", "F_SNAP_END", "F_EPOCH", "F_SNAP_DLOG"};
      mkind = q == 0 ? "hello" : acts[q].raw ? "raw" : acts[q].b.empty() ? "empty" : acts[q].b[0] >= 1 && acts[q].b[0] <= 6 ? KN[acts[q].b[0]] : "kind_other";
      break;
    }
    C.by_kind[mkind]++;
    acts.resize(cut);
    std::string label = key + " " + what;
    logf("case %ld start %s", i, label.c_str());
    C.cases++;
    g_tt = std::chrono::steady_clock::now();
    restore(tn == "T0" ? T0 : T3, CD);
    tmark("restore");
    size_t a0 = alarm_lines(CD);
    Copy c;
    if(!c.start(CD)) { problem(i, label, "the copy did not start", CD); C.copy_dead++; continue; }
    tmark("copy start");
    Bytes hrep; int mut_ans = 0;
    fs::remove(CD + "/objrepl.status");
    int answered = play(acts, &hrep, &mut_ans);
    bool hello_refused = !hrep.empty() && hrep[0] != 1;
    tmark("play");
    // the copy is done with the connection once its status says listening (or refused) again: it stays up, idle
    // 11/17/2026: up to 60 s (was 10 s). A copy that rolls back a refused frame reopens the node, and the close's
    // checkpoint fsyncs every file: seconds on a busy disk. Back after 10 s is counted as slow (and its time kept), not
    // stuck; only 60 s without it is stuck, with the process's stacks saved (gdb) to tell a hang from a slow disk
    bool back = false;
    auto bt0 = std::chrono::steady_clock::now();
    for(int w = 0; w < 120000 && c.running && !back; w++) {
      std::string st = slurp(CD + "/objrepl.status");
      back = st.find("state listening") != std::string::npos || st.find("state refused") != std::string::npos;
      if(!back) std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    double bsecs = std::chrono::duration<double>(std::chrono::steady_clock::now() - bt0).count();
    if(back && bsecs > 10) { C.slow_back++; logf("SLOW case %ld [%s]: the copy back to listening after %.1f s", i, label.c_str(), bsecs); }
    if(back && bsecs > C.max_back_secs) C.max_back_secs = bsecs;
    if(!back && c.running && !g_fail.empty()) {
      std::error_code ec; fs::create_directories(g_fail, ec);
      std::string cmd = "timeout 60 gdb -p " + std::to_string((long)getpid()) + " -batch -ex 'thread apply all bt' > '" + g_fail +
			"/stuck_case_" + std::to_string(i) + "_stacks.txt' 2>&1";
      if(system(cmd.c_str()) != 0) logf("  (gdb could not capture the stacks)");
    }
    if(!c.running) { problem(i, label, "the copy's Serve() returned " + std::to_string(c.rc.load()) + " after the case: " + c.r.Error(), CD); C.copy_dead++; c.stop(); continue; }
    if(!back) { problem(i, label, "the copy did not go back to listening within 60 s (stuck; stacks saved)", CD); C.stuck++; if(!c.stop()) { logf("ABORT: a stuck copy thread"); fflush(stdout); _exit(3); } continue; }
    size_t a1 = alarm_lines(CD);
    bool dropped = answered < (int)acts.size();
    if(hello_refused) C.hello_refused++;
    if(dropped) { C.dropped++; if(a1 > a0) C.dropped_alarm++; else C.dropped_silent++; }
    else C.accepted_all++;
    if(a1 > a0) {
      std::string t2 = slurp(CD + "/objrepl.alarm"); std::istringstream is(t2); std::string l;
      while(std::getline(is, l)) if(!l.empty()) C.alarm_reasons[short_alarm(l)]++;
    }
    std::string d1 = digest(CD), state = d1 == tmpl_dig[tn] ? "unchanged" : valid_dig[key].count(d1) ? "prefix" : "mutated";
    if(state == "unchanged") C.unchanged++; else if(state == "prefix") C.prefix++; else { C.mutated_applied++; C.by_kind_applied[mkind]++; }
    tmark("back+digest");
    std::string rep;
    if(gxObjRepl::IndexCheck(CD, INDEX, rep) != 0) { C.index_bad++; problem(i, label, "IndexCheck after the case:\n" + rep, CD); }
    tmark("ixcheck");
    // a clean primary afterwards
    std::string msg; gxObjRepl::ClearAlarm(CD, msg);
    std::string outcome;
    for(int round = 0; round < 2; round++) {
      if(round > 0 && !c.start(CD)) { problem(i, label, "the copy did not start for the clean primary", CD); C.copy_dead++; outcome = "nostart"; break; }
      gxObjRepl p;
      if(p.Open(W + "/p", prim_cfg(g_port), gxObjRepl::PRIMARY) != 0) { logf("primary open: %s", p.Error().c_str()); return 2; }
      int rc = 1;
      auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);   // 11/17/2026: was 15 s
      while(rc == 1 && std::chrono::steady_clock::now() < until) { rc = p.Pump(); if(rc == 1) std::this_thread::sleep_for(std::chrono::milliseconds(3)); }
      tmark("clean pump");
      std::string perr = p.Error();
      p.Close();
      if(!c.stop()) { problem(i, label, "the copy thread did not stop after the clean primary", CD); _exit(3); }
      tmark("close+stop");
      std::string pal = slurp(W + "/p/objrepl.alarm"); gxObjRepl::ClearAlarm(W + "/p", msg);
      if(rc == 0) {
	std::string d2 = digest(CD);
	if(d2 != D_P) { C.undetected++; problem(i, label, "UNDETECTED: the clean primary drained (rc 0) but the copy's digest " + d2 + " != primary " + D_P + " (after-case state " + state + ")", CD); outcome = "undetected"; }
	else if(gxObjRepl::IndexCheck(CD, INDEX, rep) != 0) { C.index_bad++; problem(i, label, "IndexCheck after the clean drain:\n" + rep, CD); outcome = "index"; }
	else outcome = round ? "rejoined" : "ok";
	break;
      }
      if(rc == 3 && round == 0) {
	if(pal.find("older than this copy") != std::string::npos || pal.find("is older") != std::string::npos) {
	  C.fenced_epoch++; outcome = "fenced"; if(g_verbose) logf("  fenced: %s", pal.c_str());
	  break;
	}
	if(g_verbose) logf("  clean primary refused: %s", pal.c_str());
	std::string m2; gxObjRepl::ClearAlarm(CD, m2);
	if(!c.stop()) { problem(i, label, "the copy thread did not stop before the rejoin", CD); _exit(3); }
	if(gxObjRepl::Rejoin(CD, m2) != 0) { problem(i, label, "Rejoin of the copy failed: " + m2, CD); C.not_recovered++; outcome = "norejoin"; break; }
	continue;
      }
      if(rc == 3) { C.not_recovered++; problem(i, label, "refused again after the rejoin: " + pal, CD); outcome = "refused2"; break; }
      C.stuck++; problem(i, label, "the clean primary did not drain in 60 s (rc " + std::to_string(rc) + "): " + perr, CD); outcome = "notdrained"; break;
    }
    if(outcome == "ok") C.clean_ok++; else if(outcome == "rejoined") C.clean_refused_rejoined++;
    if(g_verbose) logf("case %ld %s: answered %d/%zu%s, alarm %s, after-case %s, clean %s", i, label.c_str(), answered, acts.size(),
		       hello_refused ? " (hello refused)" : "", a1 > a0 ? "yes" : "no", state.c_str(), outcome.c_str());
  }
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
  logf("SUMMARY cases %ld (start %ld seed %llu) secs %.1f problems %ld", C.cases, start, (unsigned long long)seed, secs, C.problems);
  logf("SUMMARY stream: hello_refused %ld dropped %ld (alarm %ld silent %ld) all_frames_answered %ld", C.hello_refused, C.dropped, C.dropped_alarm, C.dropped_silent, C.accepted_all);
  logf("SUMMARY copy_after_case: unchanged %ld valid_prefix %ld mutated_applied %ld", C.unchanged, C.prefix, C.mutated_applied);
  logf("SUMMARY slow: %ld case(s) with the copy back to listening after 10 s (max %.1f s)", C.slow_back, C.max_back_secs);
  logf("SUMMARY clean_primary: ok %ld refused_then_rejoined_ok %ld fenced_by_epoch %ld undetected %ld not_recovered %ld stuck %ld index_bad %ld copy_dead %ld",
       C.clean_ok, C.clean_refused_rejoined, C.fenced_epoch, C.undetected, C.not_recovered, C.stuck, C.index_bad, C.copy_dead);
  for(auto &a : C.alarm_reasons) logf("SUMMARY alarm %ld x %s", a.second, a.first.c_str());
  for(auto &a : C.by_kind) logf("SUMMARY kind %s cases %ld applied_changed %ld", a.first.c_str(), a.second, C.by_kind_applied[a.first]);
  fs::remove_all(g_work, ec);
  return C.problems ? 1 : 0;
}
