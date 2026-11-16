// gxrepl_testwriter.cpp - DataReel 7.3 replication test writer.
//
// Writes objects to two gxDatabase files (data.gxd and index.gxd) in a
// replication set folder through a gxdJournal with a ship log, the way
// a real writer does, and checks a copy: every object's checksum, and
// the index object (count and a sum of every object's id and version)
// against the data file, so a copy holding half a transaction fails.
//
//   gxrepl_testwriter write ROOT [--commits N] [--per K] [--sleep MS]
//                     [--seed S] [--no-ship]
//   gxrepl_testwriter check ROOT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <algorithm>
#include <string>
#include <map>
#include <vector>
#include "gxdbase.h"
#include "gxbtree.h"
#include <limits>
#include <math.h>
#include "gxvset.h"
#include "gpersist.h"
#include "gxdjrnl.h"
#include "gxuint64.h" // gxUINT64; the 32-bit database headers do not include it
#include "gxrepl.h"   // 11/08/2026: the raw commands
#include "gxdstamp.h" // 11/14/2026: stamp / readstamp

// 11/08/2026: gxrepl_testwriter rawapply|rawdiff CONF SET PATH
// Connects with the client config CONF and asks the server, as a client
// that skipped its own checks would, to accept PATH as a copy (rawapply: a
// one-piece file update) or to send PATH as a source (rawdiff). Prints "ok"
// or "error: ..." (exit 2).
// 11/08/2026: gxrepl_testwriter rawblock CONF STATUS: a TLS client with
// CONF's certificate sends one block with net status STATUS (a letter,
// e.g. A) and 4 bytes, then prints "closed" if the server closes the
// session, else "answered".
static int rawblock(const char *conf, const char *status)
{
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  datareel::gxStream st;
  st.SetTimeOut(10);
  datareel::gxsTLSConnection &c = st.Connection();
  c.SetCAFile(cfg.ca);
  c.SetClientCert(cfg.cert, cfg.key, cfg.key_passphrase);
  c.SetVerifyPeer(true);
  datareel::gxSocket sock;
  if(sock.Connect(cfg.connect_host, cfg.connect_port, 10) != 0) { printf("connect failed\n"); return 2; }
  sock.SetIOTimeout(10);
  if(c.AttachClient(std::move(sock), cfg.server_name) != 0) { printf("TLS: %s\n", c.Error().c_str()); return 2; }
  datareel::gxBlockHeader gx;
  st.SetBlockStatus(gx, (__SBYTE__)status[0]);
  gx.block_length = 4;
  if(st.WriteBlock("abcd", gx) != datareel::gxSOCKET_NO_ERROR) { printf("closed\n"); return 0; }
  datareel::gxBlockHeader back;
  if(st.ReadHeader(back) != datareel::gxSOCKET_NO_ERROR) { printf("closed\n"); return 0; }
  printf("answered\n");
  return 0;
}

// 11/08/2026: The object service (gxdbobject.h) as a client sees it
struct ObjClient {
  datareel::gxReplRemote r;
  uint32_t ch = 2;
  explicit ObjClient(const datareel::gxReplConfig &c) : r(c) { }
  // 0 ok (reply = result), 1 the server answered an error (err), -1 failed
  int call(uint8_t op, const std::vector<unsigned char> &body, std::vector<unsigned char> &reply, std::string &err) {
    std::vector<unsigned char> m(1, op);
    m.insert(m.end(), body.begin(), body.end());
    if(r.Mux()->Send(ch, m, datareel::gxdMUX_LAST, err) != 0) return -1;
    std::vector<unsigned char> back; uint64_t seq;
    if(r.Mux()->ReceiveMessage(ch, back, seq, 30000, err) != 0) return -1;
    if(back.empty()) { err = "empty reply"; return -1; }
    if(back[0] != 0) { uint32_t n = 0; if(back.size() >= 5) memcpy(&n, back.data() + 1, 4);
      err.assign((const char *)back.data() + 5, std::min<size_t>(n, back.size() - 5)); return 1; }
    reply.assign(back.begin() + 1, back.end());
    return 0;
  }
};
static void q64(std::vector<unsigned char> &v, uint64_t x) { unsigned char b[8]; memcpy(b, &x, 8); v.insert(v.end(), b, b + 8); }
static uint64_t r64(const std::vector<unsigned char> &v, size_t off = 0) { uint64_t x = 0; if(v.size() >= off + 8) memcpy(&x, v.data() + off, 8); return x; }
static std::vector<unsigned char> obj_data(int i, size_t n) {
  std::vector<unsigned char> v(n); for(size_t k = 0; k < n; k++) v[k] = (unsigned char)(i * 13 + k * 7); return v; }

// 11/09/2026: The power loss test. objstream CONF DB [START] adds objects (and
// changes every third one) until killed, ids from START (default 0), printing "A i addr size" once each
// add is answered and "C i addr size" once each change is. objverify CONF DB
// FILE then reads every object FILE says was answered: the last answered
// version of each must be there, and the database must walk end to end.
static int objstream(const char *conf, const char *dbname, int start)
{
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  ObjClient c(cfg);
  if(c.r.ConnectMux(err) != 0) { printf("connect: %s\n", err.c_str()); return 2; }
  if(c.r.Mux()->Open(c.ch, "object", dbname, "write", 0, err) != 0) { printf("open refused: %s\n", err.c_str()); return 3; }
  std::vector<unsigned char> rep;
  std::map<int, uint64_t> addrs;
  for(int i = start; ; i++) {
    size_t n = 10 + ((size_t)i * 37) % 3000;
    std::vector<unsigned char> d = obj_data(i, n), b;
    q64(b, d.size()); b.insert(b.end(), d.begin(), d.end());
    if(c.call(datareel::gxOBJ_ADD, b, rep, err) != 0) { printf("add: %s\n", err.c_str()); return 1; }
    addrs[i] = r64(rep);
    printf("A %d %llu %zu\n", i, (unsigned long long)addrs[i], n); fflush(stdout);
    if(i - start >= 2 && i % 3 == 2) {
      // Change an earlier object, same size (so its address stays)
      int j = i - 2;
      size_t m = 10 + ((size_t)j * 37) % 3000;
      std::vector<unsigned char> e = obj_data(j + 100000, m), cb;
      q64(cb, addrs[j]); q64(cb, e.size()); cb.insert(cb.end(), e.begin(), e.end());
      if(c.call(datareel::gxOBJ_CHANGE, cb, rep, err) != 0) { printf("change: %s\n", err.c_str()); return 1; }
      printf("C %d %llu %zu\n", j, (unsigned long long)addrs[j], m); fflush(stdout);
    }
  }
}

static int objverify(const char *conf, const char *dbname, const char *file)
{
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  ObjClient c(cfg);
  if(c.r.ConnectMux(err) != 0) { printf("connect: %s\n", err.c_str()); return 2; }
  if(c.r.Mux()->Open(c.ch, "object", dbname, "read", 0, err) != 0) { printf("open refused: %s\n", err.c_str()); return 3; }
  struct Want { uint64_t addr; size_t n; bool changed; };   // changed: the CHANGE was answered
  std::map<int, Want> want;
  FILE *f = fopen(file, "r");
  if(!f) { printf("cannot read %s\n", file); return 2; }
  char ty; int i; unsigned long long a; size_t n;
  while(fscanf(f, " %c %d %llu %zu", &ty, &i, &a, &n) == 4) {
    if(ty == 'A') want[i] = Want{(uint64_t)a, n, false};
    else if(ty == 'C') want[i] = Want{(uint64_t)a, n, true};
  }
  fclose(f);
  std::vector<unsigned char> rep;
  for(auto &w : want) {
    std::vector<unsigned char> q; q64(q, w.second.addr);
    if(c.call(datareel::gxOBJ_REQUEST, q, rep, err) != 0) { printf("object %d (answered) is gone: %s\n", w.first, err.c_str()); return 1; }
    std::vector<unsigned char> d = obj_data(w.second.changed ? w.first + 100000 : w.first, w.second.n);
    bool match = r64(rep) == d.size() && rep.size() >= 8 + d.size() && memcmp(rep.data() + 8, d.data(), d.size()) == 0;
    if(!match && !w.second.changed) {
      // A change that was sent but never answered may or may not be there: both are fine
      std::vector<unsigned char> d2 = obj_data(w.first + 100000, w.second.n);
      match = r64(rep) == d2.size() && rep.size() >= 8 + d2.size() && memcmp(rep.data() + 8, d2.data(), d2.size()) == 0;
    }
    if(!match) { printf("object %d differs from its last answered version\n", w.first); return 1; }
  }
  if(c.call(datareel::gxOBJ_FIRST, std::vector<unsigned char>(), rep, err) != 0) { printf("first: %s\n", err.c_str()); return 1; }
  uint64_t x = r64(rep); size_t walked = 0;
  while(x) { std::vector<unsigned char> q; q64(q, x); walked++;
    if(c.call(datareel::gxOBJ_REQUEST, q, rep, err) != 0) { printf("walk: %s\n", err.c_str()); return 1; }
    if(c.call(datareel::gxOBJ_NEXT, q, rep, err) != 0) { printf("next: %s\n", err.c_str()); return 1; }
    x = r64(rep); }
  if(walked < want.size()) { printf("walk found %zu objects, %zu were answered\n", walked, want.size()); return 1; }
  printf("objverify ok %zu answered, %zu walked\n", want.size(), walked);
  return 0;
}

// gxrepl_testwriter objects CONF DB write|read COUNT
static int objects(const char *conf, const char *dbname, const std::string &mode, int count)
{
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  ObjClient c(cfg);
  if(c.r.ConnectMux(err) != 0) { printf("connect: %s\n", err.c_str()); return 2; }
  if(c.r.Mux()->Open(c.ch, "object", dbname, mode, 0, err) != 0) { printf("open refused: %s\n", err.c_str()); return 3; }
  std::vector<unsigned char> rep;
  if(mode == "read") {
    std::vector<unsigned char> b; q64(b, 5); b.insert(b.end(), 5, 'x');
    int rc = c.call(datareel::gxOBJ_ADD, b, rep, err);
    if(rc != 1) { printf("a read channel could add\n"); return 1; }
    if(c.call(datareel::gxOBJ_FIRST, std::vector<unsigned char>(), rep, err) != 0) { printf("first: %s\n", err.c_str()); return 1; }
    uint64_t a = r64(rep); int n = 0;
    while(a) {
      std::vector<unsigned char> q; q64(q, a);
      if(c.call(datareel::gxOBJ_REQUEST, q, rep, err) != 0) { printf("request: %s\n", err.c_str()); return 1; }
      n++;
      if(c.call(datareel::gxOBJ_NEXT, q, rep, err) != 0) { printf("next: %s\n", err.c_str()); return 1; }
      a = r64(rep);
    }
    printf("readonly ok %d\n", n);
    return 0;
  }
  std::vector<uint64_t> addr(count);
  for(int i = 0; i < count; i++) {
    std::vector<unsigned char> d = obj_data(i, 10 + (i * 37) % 3000), b;
    q64(b, d.size()); b.insert(b.end(), d.begin(), d.end());
    if(c.call(datareel::gxOBJ_ADD, b, rep, err) != 0) { printf("add: %s\n", err.c_str()); return 1; }
    addr[i] = r64(rep);
  }
  for(int i = 0; i < count; i++) {
    std::vector<unsigned char> q; q64(q, addr[i]);
    if(c.call(datareel::gxOBJ_REQUEST, q, rep, err) != 0) { printf("request: %s\n", err.c_str()); return 1; }
    std::vector<unsigned char> d = obj_data(i, 10 + (i * 37) % 3000);
    if(r64(rep) != d.size() || memcmp(rep.data() + 8, d.data(), d.size()) != 0) { printf("object %d differs\n", i); return 1; }
  }
  for(int i = 0; i < count; i += 2) {
    std::vector<unsigned char> d = obj_data(i + 1000, 10 + (i * 37) % 3000), b;
    q64(b, addr[i]); q64(b, d.size()); b.insert(b.end(), d.begin(), d.end());
    if(c.call(datareel::gxOBJ_CHANGE, b, rep, err) != 0) { printf("change: %s\n", err.c_str()); return 1; }
    std::vector<unsigned char> q; q64(q, addr[i]);
    if(c.call(datareel::gxOBJ_REQUEST, q, rep, err) != 0 || memcmp(rep.data() + 8, d.data(), d.size()) != 0) { printf("changed object %d differs\n", i); return 1; }
  }
  int deleted = 0;
  for(int i = 1; i < count; i += 4) {
    std::vector<unsigned char> q; q64(q, addr[i]);
    if(c.call(datareel::gxOBJ_DELETE, q, rep, err) != 0) { printf("delete: %s\n", err.c_str()); return 1; }
    deleted++;
  }
  if(c.call(datareel::gxOBJ_FIRST, std::vector<unsigned char>(), rep, err) != 0) { printf("first: %s\n", err.c_str()); return 1; }
  uint64_t a = r64(rep); int n = 0;
  while(a) { std::vector<unsigned char> q; q64(q, a); n++;
    if(c.call(datareel::gxOBJ_NEXT, q, rep, err) != 0) { printf("next: %s\n", err.c_str()); return 1; } a = r64(rep); }
  if(n < count - deleted) { printf("walk found %d objects, expected at least %d\n", n, count - deleted); return 1; }
  printf("objects ok %d %d\n", count, n);
  return 0;
}

// gxrepl_testwriter legacyobj CONF: a 6.x style client ('A' add, 'R' request)
static int legacyobj(const char *conf)
{
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  datareel::gxStream st;
  st.SetTimeOut(10);
  datareel::gxsTLSConnection &cn = st.Connection();
  cn.SetCAFile(cfg.ca); cn.SetClientCert(cfg.cert, cfg.key, cfg.key_passphrase); cn.SetVerifyPeer(true);
  datareel::gxSocket sock;
  if(sock.Connect(cfg.connect_host, cfg.connect_port, 10) != 0) { printf("connect failed\n"); return 2; }
  sock.SetIOTimeout(10);
  if(cn.AttachClient(std::move(sock), cfg.server_name) != 0) { printf("TLS: %s\n", cn.Error().c_str()); return 2; }
  const char *msg = "a 6.x style object";
  datareel::gxBlockHeader gx;
  st.SetBlockStatus(gx, datareel::gxAddRemoteBlock);
  gx.block_length = (datareel::gxUINT32)strlen(msg);
  if(st.WriteBlock(msg, gx) != datareel::gxSOCKET_NO_ERROR) { printf("closed\n"); return 1; }
  FAU_t a = (FAU_t)0;
  if(st.ReadResultBlock(a) != datareel::gxSOCKET_NO_ERROR || a == (FAU_t)0) { printf("no address\n"); return 1; }
  int64_t x = (int64_t)a;
  st.SetBlockStatus(gx, datareel::gxRequestBlock);
  gx.block_length = 8;
  if(st.WriteBlock(&x, gx) != datareel::gxSOCKET_NO_ERROR) { printf("closed\n"); return 1; }
  datareel::gxBlockHeader back;
  std::vector<unsigned char> data;
  if(st.ReadHeader(back) != datareel::gxSOCKET_NO_ERROR || st.ReadBlock(data, back) != datareel::gxSOCKET_NO_ERROR) { printf("no object\n"); return 1; }
  if(std::string(data.begin(), data.end()) != msg) { printf("object differs\n"); return 1; }
  st.CloseConnection();
  printf("legacy ok\n");
  return 0;
}

static int raw(const std::string &mode, const char *conf, const char *set, const char *path)
{
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  datareel::gxReplRemote r(cfg);
  if(r.Connect(err) != 0) { fprintf(stderr, "connect: %s\n", err.c_str()); return 2; }
  int rc;
  if(mode == "rawapply") {
    datareel::gxReplFileUpdate u;
    u.path = path; u.size = 5; u.mode = 0644; u.mtime_ns = 1000000000LL;
    datareel::gxReplPiece p; p.offset = 0; p.data.assign((const unsigned char *)"hello", (const unsigned char *)"hello" + 5);
    u.pieces.push_back(p);
    std::vector<datareel::gxReplFileUpdate> v(1, u);
    std::vector<int> res;
    rc = r.ApplyFiles(set, v, res, err);
  }
  else {
    datareel::gxReplDiffRequest rq; rq.path = path;
    datareel::gxReplDiffResult res;
    rc = r.Diff(set, rq, res, err);
  }
  if(rc != 0) { printf("error: %s\n", err.c_str()); return 2; }
  printf("ok\n");
  return 0;
}

using namespace datareel;

namespace datareel {
struct gxdShipLogTest {   // 11/14/2026: gxdShipLog's friend (gxdrepl.h): a writer crash between a pair
  static void Split(gxdShipLog *l, int how) { l->test_split_pair = how; }
};
}

struct ObjHdr {
  gxUINT64 id;
  gxUINT64 version;
  gxUINT32 len;     // Payload bytes after the header
  gxUINT32 check;   // Hash of id, version, len and the payload
};

struct IndexObj {
  gxUINT64 count;
  gxUINT64 sum;     // Sum of id * 1000003 + version over every object
  gxUINT64 commits;
  gxUINT64 next_id;
};

static unsigned long long rng_state = 88172645463325252ULL;
static unsigned long long rnd()
{
  rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
  return rng_state;
}

static gxUINT32 obj_check(const ObjHdr &h, const unsigned char *p)
{
  unsigned long long x = 1469598103934665603ULL;
  const unsigned char *hp = (const unsigned char *)&h;
  for(size_t i = 0; i < 20; i++) { x ^= hp[i]; x *= 1099511628211ULL; }
  for(gxUINT32 i = 0; i < h.len; i++) { x ^= p[i]; x *= 1099511628211ULL; }
  return (gxUINT32)(x ^ (x >> 32));
}

// 11/10/2026: a file's content hash (gxdReplHash over 1 MB reads), 0 if missing
static uint64_t content_hash(const std::string &path)
{
  int fd = open(path.c_str(), O_RDONLY);
  if(fd < 0) return 0;
  std::vector<unsigned char> b(1 << 20);
  uint64_t h = 0x5EA1ULL;
  ssize_t n;
  while((n = read(fd, b.data(), b.size())) > 0) h = datareel::gxdReplHash(b.data(), (size_t)n, h);
  close(fd);
  return h;
}

static uint64_t read_gen(const std::string &root)
{
  uint64_t g = ~0ULL;
  int fd = open((root + "/commit.bin").c_str(), O_RDONLY);
  if(fd >= 0) { if(pread(fd, &g, 8, 8) != 8) g = ~0ULL; close(fd); }
  return g;
}

// hashes DIR: "<generation> <data.gxd hash> <index.gxd hash>", or "unstable" when the generation moved meanwhile
static int hashes(const std::string &root)
{
  uint64_t g0 = read_gen(root);
  uint64_t hd = content_hash(root + "/data.gxd"), hi = content_hash(root + "/index.gxd");
  uint64_t g1 = read_gen(root);
  if(g0 != g1) { printf("unstable\n"); return 0; }
  printf("%llu %016llx %016llx\n", (unsigned long long)g0, (unsigned long long)hd, (unsigned long long)hi);
  return 0;
}

// 11/10/2026: reindex DIR: rebuild index.gxd (the derived index of a test
// folder) from data.gxd: the objects' count, sum and next id, and the
// commit count from commit.bin (a --commit-block folder). Exit 0, or 1.
static int check(const std::string &root);
static unsigned long long stamp_lsn = 0;   // 11/10/2026: reindex DIR [LSN]
static int reindex(const std::string &root)
{
  gxDatabase db, ix;
  if(db.Open((root + "/data.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { printf("reindex: no data.gxd\n"); return 1; }
  IndexObj io;
  memset(&io, 0, sizeof(io));
  std::vector<unsigned char> buf;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
    __ULWORD__ n = db.ObjectLength(a);
    if(n < sizeof(ObjHdr)) continue;
    buf.resize(n);
    if(db.Read(buf.data(), n, a) != gxDBASE_NO_ERROR) continue;
    ObjHdr h; memcpy(&h, buf.data(), sizeof(h));
    if(sizeof(h) + h.len > n || obj_check(h, buf.data() + sizeof(h)) != h.check) { printf("reindex: a bad object\n"); return 1; }
    io.count++;
    io.sum += (unsigned long long)h.id * 1000003ULL + h.version;
    if(h.id + 1 > io.next_id) io.next_id = h.id + 1;
  }
  FILE *cf = fopen((root + "/commit.bin").c_str(), "rb");
  if(!cf) { printf("reindex: no commit.bin\n"); return 1; }
  unsigned char blk[24] = {0};
  size_t got = fread(blk, 1, sizeof(blk), cf);
  fclose(cf);
  if(got != sizeof(blk)) { printf("reindex: commit.bin short\n"); return 1; }
  uint64_t cc = 0; memcpy(&cc, blk + 16, 8);
  io.commits = cc;
  std::string tmp = root + "/.index.gxd.rebuild";
  unlink(tmp.c_str());
  if(ix.Create(tmp.c_str()) != gxDBASE_NO_ERROR) { printf("reindex: cannot create\n"); return 1; }
  FAU_t ia = ix.Alloc(sizeof(io));
  if(!ia || ix.Write(&io, sizeof(io), ia) != gxDBASE_NO_ERROR) { printf("reindex: cannot write\n"); return 1; }
  ix.Close();
  if(rename(tmp.c_str(), (root + "/index.gxd").c_str()) != 0) { printf("reindex: cannot rename\n"); return 1; }
  db.Close();
  if(check(root) != 0) return 1;               // the rebuilt index must check against the data
  // 11/10/2026: the stamp gxrepl checks: "gen G lsn L records N keys K" (keys: what the index holds)
  uint64_t g = 0; memcpy(&g, blk + 8, 8);
  FILE *sf = fopen((root + "/.derived.stamp.tmp").c_str(), "w");
  if(!sf) { printf("reindex: cannot stamp\n"); return 1; }
  // 11/14/2026: GXREPL_TEST_STAMP_TAIL: the rebuild's own fields after "keys K" (OSA writes keys_version N)
  const char *tail = getenv("GXREPL_TEST_STAMP_TAIL");
  fprintf(sf, "gen %llu lsn %llu records %llu keys %llu%s\n", (unsigned long long)g, stamp_lsn,
	  (unsigned long long)io.count, (unsigned long long)io.count, tail ? tail : "");
  fflush(sf); fsync(fileno(sf)); fclose(sf);
  if(rename((root + "/.derived.stamp.tmp").c_str(), (root + "/derived.stamp").c_str()) != 0) return 1;
  return 0;
}

static int check(const std::string &root)
{
  gxDatabase db, ix;
  if(db.Open((root + "/data.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR ||
     ix.Open((root + "/index.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) {
    printf("check: cannot open the files\n");
    return 2;
  }
  FAU_t ia = ix.FindFirstObject();
  IndexObj io;
  if(ia == (FAU_t)0 || ix.Read(&io, sizeof(io), ia) != gxDBASE_NO_ERROR) {
    printf("check: no index object\n");
    return 2;
  }
  unsigned long long count = 0, sum = 0, bad = 0;
  std::vector<unsigned char> buf;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
    __ULWORD__ n = db.ObjectLength(a);
    if(n < sizeof(ObjHdr)) { bad++; continue; }
    buf.resize(n);
    if(db.Read(buf.data(), n, a) != gxDBASE_NO_ERROR) { bad++; continue; }
    ObjHdr h; memcpy(&h, buf.data(), sizeof(h));
    if(sizeof(h) + h.len > n || obj_check(h, buf.data() + sizeof(h)) != h.check) { bad++; continue; }
    count++;
    sum += (unsigned long long)h.id * 1000003ULL + h.version;
  }
  // 11/10/2026: a commit block, when there is one, must be at rest (even) and name this commit
  FILE *cf = fopen((root + "/commit.bin").c_str(), "rb");
  if(cf) {
    unsigned char blk[24] = {0};
    size_t got = fread(blk, 1, sizeof(blk), cf);
    fclose(cf);
    uint64_t g = 0, cc = 0;
    memcpy(&g, blk + 8, 8); memcpy(&cc, blk + 16, 8);
    uint32_t magic = 0;
    memcpy(&magic, blk, 4);
    if(magic == 0x42435754 && (got != sizeof(blk) || (g & 1) || cc != io.commits)) {   // only a "TWCB" block
      printf("check: commit.bin generation %llu commits %llu, the files hold commit %llu: MISMATCH\n",
	     (unsigned long long)g, (unsigned long long)cc, (unsigned long long)io.commits);
      return 1;
    }
  }
  printf("check: %llu objects, %llu bad, index count %llu, commits %llu, %s\n",
	 count, bad, (unsigned long long)io.count, (unsigned long long)io.commits,
	 (bad == 0 && count == io.count && sum == io.sum) ? "OK" : "MISMATCH");
  return (bad == 0 && count == io.count && sum == io.sum) ? 0 : 1;
}

// ---------------- 11/14/2026 (7.4): vector searches of the object service ----------------
// vmake DIR DAYS PER: a set DIR/news.vixset of DAYS day files (dim 16, tag 2 = 20261110 + day), deterministic
static unsigned vrng = 12345;
static float vrand() { vrng = vrng * 1103515245u + 12345u; return ((vrng >> 8) & 0xFFFF) / 32768.0f - 1.0f; }
static void vquery(int qi, std::vector<float> &q) { for(size_t d = 0; d < q.size(); d++) q[d] = (float)sin(qi * 0.77 + d * 0.41); }
static int vmake(const std::string &dir, int days, int per)
{
  datareel::gxVectorSet set;
  std::string man = dir + "/news.vixset";
  if(set.Create(man.c_str(), "news", 16, datareel::gxvFLOAT32, datareel::gxvL2, "vec-test") != datareel::gxv_NO_ERROR) { printf("create set\n"); return 1; }
  for(int d = 0; d < days; d++) {
    char rel[64]; snprintf(rel, sizeof(rel), "day%d.vix", d);
    datareel::gxVectorIndex v;
    if(v.Create((dir + "/" + rel).c_str(), 16, datareel::gxvFLOAT32, datareel::gxvL2, 8, "vec-test", 100) != datareel::gxv_NO_ERROR) { printf("create\n"); return 1; }
    std::vector<float> x(16);
    for(int i = 0; i < per; i++) {
      for(auto &e : x) e = vrand();
      unsigned t[4] = { (unsigned)(i % 3), 0, 20261110u + (unsigned)d, 0 };
      v.Insert((long long)d * 100000 + i, x.data(), t);
    }
    v.Close();
    if(set.AddFile(rel) != datareel::gxv_NO_ERROR) { printf("add: %s\n", set.Error().c_str()); return 1; }
  }
  printf("vmake ok\n");
  return 0;
}

static void p32v(std::vector<unsigned char> &v, uint32_t x) { unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4); }
static uint32_t g32v(const std::vector<unsigned char> &v, size_t &off) { uint32_t x = 0; if(off + 4 <= v.size()) memcpy(&x, v.data() + off, 4); off += 4; return x; }

static std::vector<unsigned char> vbody(uint32_t k, uint32_t ef, uint32_t dim, uint32_t flags, uint32_t dlo, uint32_t dhi,
					 const std::vector<float> &q, uint32_t ver = 1)
{
  std::vector<unsigned char> b;
  p32v(b, ver); p32v(b, k); p32v(b, ef); p32v(b, dim); p32v(b, flags); p32v(b, 0);
  for(int i = 0; i < 8; i++) p32v(b, 0);
  p32v(b, dlo); p32v(b, dhi);
  const unsigned char *qb = (const unsigned char *)q.data();
  b.insert(b.end(), qb, qb + q.size() * 4);
  return b;
}

// The canonical print of results: one line per result "id distance file path"
static void vprint(const std::vector<std::pair<long long, std::pair<float, std::string> > > &r, unsigned long long skipped,
		   unsigned long long missing)
{
  printf("skipped %llu missing %llu\n", skipped, missing);
  for(auto &x : r) printf("%lld %.9g %s\n", x.first, x.second.first, x.second.second.c_str());
}

// vsearch CONF DB K QI [DLO DHI [strict]] / vlocal MANIFEST K QI [DLO DHI [strict]]
static int vsearch(int argc, char **argv, bool local)
{
  std::string target = argv[2];
  int a = local ? 3 : 4;
  uint32_t k = (uint32_t)atoi(argv[a]); int qi = atoi(argv[a + 1]);
  uint32_t flags = 0, dlo = 0, dhi = 0;
  if(argc > a + 3) { flags |= 1; dlo = (uint32_t)strtoul(argv[a + 2], 0, 10); dhi = (uint32_t)strtoul(argv[a + 3], 0, 10); }
  if(argc > a + 4) flags |= 2;
  std::vector<float> q(16);
  vquery(qi, q);
  std::vector<std::pair<long long, std::pair<float, std::string> > > out;
  if(local) {
    datareel::gxVectorSet set;
    if(set.Open(target.c_str()) != datareel::gxv_NO_ERROR) { printf("open: %s\n", set.Error().c_str()); return 1; }
    datareel::gxvSetSearch o; o.k = k; o.ef = 64; o.day_range = flags & 1; o.day_lo = dlo; o.day_hi = dhi; o.strict = (flags & 2) ? 1 : 0;
    std::vector<datareel::gxvSetResult> r;
    datareel::gxvSetStats st;
    if(set.Search(q.data(), o, r, &st) != datareel::gxv_NO_ERROR) { printf("error %s\n", st.error.c_str()); return 3; }
    for(auto &x : r) out.push_back(std::make_pair(x.id, std::make_pair(x.distance, set.Files()[x.file].path)));
    vprint(out, st.skipped, st.missing);
    return 0;
  }
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(target, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  ObjClient c(cfg);
  if(c.r.ConnectMux(err) != 0) { printf("connect: %s\n", err.c_str()); return 2; }
  if(c.r.Mux()->Open(c.ch, "object", argv[3], "read", 0, err) != 0) { printf("open refused: %s\n", err.c_str()); return 2; }
  std::vector<unsigned char> rep;
  int rc = c.call(datareel::gxOBJ_VECTOR_SEARCH, vbody(k, 64, 16, flags, dlo, dhi, q), rep, err);
  if(rc != 0) { printf("error %s\n", err.c_str()); return 3; }
  size_t off = 0;
  uint32_t ver = g32v(rep, off), n = g32v(rep, off), skipped = g32v(rep, off), missing = g32v(rep, off);
  if(ver != 1) { printf("bad version %u\n", ver); return 1; }
  struct R { long long id; float d; uint32_t file; };
  std::vector<R> rs;
  for(uint32_t i = 0; i < n; i++) {
    R x; memcpy(&x.id, rep.data() + off, 8); off += 8; memcpy(&x.d, rep.data() + off, 4); off += 4; off += 16; x.file = g32v(rep, off);
    rs.push_back(x);
  }
  std::map<uint32_t, std::string> names;
  uint32_t nn = g32v(rep, off);
  for(uint32_t i = 0; i < nn; i++) { uint32_t f = g32v(rep, off), l = g32v(rep, off); names[f] = std::string((const char *)rep.data() + off, l); off += l; }
  if(off != rep.size()) { printf("bad reply length\n"); return 1; }
  for(auto &x : rs) {
    if(!names.count(x.file) || names[x.file].empty() || names[x.file][0] == '/') { printf("bad path for file %u\n", x.file); return 1; }
    out.push_back(std::make_pair(x.id, std::make_pair(x.d, names[x.file])));
  }
  vprint(out, skipped, missing);
  return 0;
}

// vinfo CONF DB; vbad CONF DB: every malformed request is refused (and the channel still works)
static int vinfo_bad(const char *conf, const char *db, bool bad)
{
  datareel::gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
  ObjClient c(cfg);
  if(c.r.ConnectMux(err) != 0) { printf("connect: %s\n", err.c_str()); return 2; }
  if(c.r.Mux()->Open(c.ch, "object", db, "read", 0, err) != 0) { printf("open refused: %s\n", err.c_str()); return 2; }
  std::vector<unsigned char> rep;
  if(!bad) {
    int rc = c.call(datareel::gxOBJ_VECTOR_INFO, std::vector<unsigned char>(), rep, err);
    if(rc != 0) { printf("error %s\n", err.c_str()); return 3; }
    size_t off = 0;
    uint32_t ver = g32v(rep, off), dim = g32v(rep, off), el = g32v(rep, off), met = g32v(rep, off), nf = g32v(rep, off);
    uint64_t nodes = 0, del = 0; memcpy(&nodes, rep.data() + off, 8); off += 8; memcpy(&del, rep.data() + off, 8); off += 8;
    uint32_t sl = g32v(rep, off);
    std::string spec((const char *)rep.data() + off, sl);
    printf("vinfo version %u dim %u element %u metric %u files %u nodes %llu deleted %llu spec %s\n", ver, dim, el, met, nf,
	   (unsigned long long)nodes, (unsigned long long)del, spec.c_str());
    return 0;
  }
  std::vector<float> q(16, 0.1f);
  struct Case { const char *what; std::vector<unsigned char> body; const char *want; } cases[] = {
    { "k 0", vbody(0, 64, 16, 0, 0, 0, q), "k must be" },
    { "k 1001", vbody(1001, 64, 16, 0, 0, 0, q), "k must be" },
    { "ef 5000", vbody(10, 5000, 16, 0, 0, 0, q), "ef must be" },
    { "wrong dim", vbody(10, 64, 8, 0, 0, 0, std::vector<float>(8, 0.1f)), "dimension" },
    { "version 2", vbody(10, 64, 16, 0, 0, 0, q, 2), "version" },
    { "unknown flags", vbody(10, 64, 16, 8, 0, 0, q), "flags" },
  };
  int fails = 0;
  for(auto &cs : cases) {
    int rc = c.call(datareel::gxOBJ_VECTOR_SEARCH, cs.body, rep, err);
    if(rc != 1 || err.find(cs.want) == std::string::npos) { printf("%s: not refused (%d %s)\n", cs.what, rc, err.c_str()); fails++; }
  }
  std::vector<unsigned char> b = vbody(10, 64, 16, 0, 0, 0, q);
  std::vector<unsigned char> shortb(b.begin(), b.end() - 4), longb = b, nanb = b;
  longb.push_back(0);
  float nan = std::numeric_limits<float>::quiet_NaN();
  memcpy(nanb.data() + nanb.size() - 4, &nan, 4);
  std::vector<unsigned char> tiny(b.begin(), b.begin() + 10);
  for(auto *bb : { &shortb, &longb, &nanb, &tiny }) {
    int rc = c.call(datareel::gxOBJ_VECTOR_SEARCH, *bb, rep, err);
    if(rc != 1) { printf("a bad body was answered\n"); fails++; }
  }
  int rc = c.call(datareel::gxOBJ_VECTOR_INFO, std::vector<unsigned char>(1, 0), rep, err);
  if(rc != 1) { printf("vinfo with a body answered\n"); fails++; }
  rc = c.call(datareel::gxOBJ_VECTOR_SEARCH, b, rep, err);
  if(rc != 0) { printf("the channel no longer answers: %s\n", err.c_str()); fails++; }
  printf(fails ? "vbad FAILED\n" : "vbad ok\n");
  return fails ? 1 : 0;
}

// 11/14/2026 (7.4): vwrite ROOT --ship-root SET [--commits N] [--per P] [--seed S] [--sleep MS]: InfoHog-layout
// records (object header, 3 members: text, a vector member, the day as u32) in ROOT/data.gxd, the primary's own
// derived index ROOT/vectors.vix in the same journal transaction (id = the record's address, tag 2 = the day),
// commit block ROOT/commit.bin, ship log with *.vix left out. Adds, vector updates and deletes (blocks reused).
static int vwrite(int argc, char **argv)
{
  std::string root = argv[2], ship_root = root;
  long commits = 20, per = 10, sleep_ms = 0;
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--ship-root" && i + 1 < argc) ship_root = argv[++i];
    else if(a == "--commits" && i + 1 < argc) commits = atol(argv[++i]);
    else if(a == "--per" && i + 1 < argc) per = atol(argv[++i]);
    else if(a == "--seed" && i + 1 < argc) vrng ^= (unsigned)strtoul(argv[++i], 0, 10) * 2654435761u;
    else if(a == "--sleep" && i + 1 < argc) sleep_ms = atol(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 64; }
  }
  std::string dn = root + "/data.gxd", vn = root + "/vectors.vix", jn = root + "/writer.jnl";
  datareel::gxdJournal j;
  if(j.Open(jn.c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "journal\n"); return 1; }
  gxDatabase db;
  if(db.Open(dn.c_str()) != gxDBASE_NO_ERROR && db.Create(dn.c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "data\n"); return 1; }
  datareel::gxVectorIndex vix;
  if(vix.Open(vn.c_str()) != datareel::gxv_NO_ERROR &&
     vix.Create(vn.c_str(), 16, datareel::gxvFLOAT32, datareel::gxvL2, 8, "vtest", 100) != datareel::gxv_NO_ERROR) { fprintf(stderr, "vix\n"); return 1; }
  if(j.Attach(&db) != gxDBASE_NO_ERROR || j.Attach(vix.Database()) != gxDBASE_NO_ERROR) { fprintf(stderr, "attach\n"); return 1; }
  if(j.EnableShipLog(ship_root.c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "ship log\n"); return 1; }
  j.ShipLog()->SetExclude(std::vector<std::string>(1, "*.vix"));
  int cbfd = open((root + "/commit.bin").c_str(), O_RDWR | O_CREAT, 0644);
  unsigned char blk[4096];
  memset(blk, 0, sizeof(blk));
  if(pread(cbfd, blk, sizeof(blk), 0) != (ssize_t)sizeof(blk)) {
    uint32_t magic = 0x42435756, ver = 1;     // "VWCB"
    memcpy(blk, &magic, 4); memcpy(blk + 4, &ver, 4);
    if(pwrite(cbfd, blk, sizeof(blk), 0) != (ssize_t)sizeof(blk)) return 1;
  }
  uint64_t gen = 0; memcpy(&gen, blk + 8, 8);
  if(gen & 1) gen++;
  std::vector<FAU_t> live;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) live.push_back(a);
  auto record = [&](FAU_t addr, unsigned day, const std::vector<unsigned char> &mem, std::vector<unsigned char> &rec) {
    datareel::gxObjectHeader oh; memset(&oh, 0, sizeof(oh)); oh.ClassID = 7; oh.ObjectID = addr;
    rec.assign((const unsigned char *)&oh, (const unsigned char *)&oh + sizeof(oh));
    // the count and lengths as InfoHog writes them: gxUINT32, sizeof(gxUINT32) bytes wide
    auto pw = [&](unsigned long v) { gxUINT32 w = (gxUINT32)v; rec.insert(rec.end(), (unsigned char *)&w, (unsigned char *)&w + sizeof(w)); };
    pw(3);
    pw(20); for(int i = 0; i < 20; i++) rec.push_back((unsigned char)('a' + (day + i) % 26));
    pw(mem.size()); rec.insert(rec.end(), mem.begin(), mem.end());
    pw(4); p32v(rec, day);
  };
  const size_t rec_size = sizeof(datareel::gxObjectHeader) + 4 * sizeof(gxUINT32) + 20 + vix.MemberSize() + 4;
  std::vector<float> v(16);
  std::vector<unsigned char> mem, rec;
  for(long c = 0; c < commits; c++) {
    uint64_t g = ++gen; if(pwrite(cbfd, &g, 8, 8) != 8) { }
    if(j.Begin() != gxDBASE_NO_ERROR) return 1;
    for(long k = 0; k < per; k++) {
      unsigned r = (vrng = vrng * 1103515245u + 12345u) >> 16;
      unsigned day = 20261110u + (unsigned)(c % 5);
      if(r % 10 < 7 || live.size() < 5) {             // add (a deleted block is reused when one fits)
	FAU_t a = db.Alloc((__ULWORD__)rec_size);
	for(auto &e : v) e = vrand();
	vix.MakeMember(v.data(), mem);
	record(a, day, mem, rec);
	if(!a || db.Write(rec.data(), (__ULWORD__)rec.size(), a) != gxDBASE_NO_ERROR) return 1;
	unsigned t[4] = { 0, 0, day, 0 };
	if(vix.InsertMember((long long)a, mem.data(), (unsigned)mem.size(), t) != datareel::gxv_NO_ERROR) return 1;
	live.push_back(a);
      }
      else if(r % 10 < 9) {                             // a new vector for a record
	FAU_t a = live[r % live.size()];
	for(auto &e : v) e = vrand();
	vix.MakeMember(v.data(), mem);
	record(a, day, mem, rec);
	if(db.Write(rec.data(), (__ULWORD__)rec.size(), a) != gxDBASE_NO_ERROR) return 1;
	unsigned t[4] = { 0, 0, day, 0 };
	if(vix.UpdateMember((long long)a, mem.data(), (unsigned)mem.size(), t) != datareel::gxv_NO_ERROR) return 1;
      }
      else {                                            // delete
	size_t i = r % live.size();
	FAU_t a = live[i];
	db.Delete(a);
	vix.Delete((long long)a);
	live.erase(live.begin() + (long)i);
      }
    }
    unsigned char nb[4096];
    if(pread(cbfd, nb, sizeof(nb), 0) != (ssize_t)sizeof(nb)) return 1;
    uint64_t g2 = gen + 1; memcpy(nb + 8, &g2, 8);
    j.ShipLog()->SetCommitBlock(root + "/commit.bin", nb, sizeof(nb));
    if(j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "commit\n"); return 1; }
    g = ++gen; if(pwrite(cbfd, &g, 8, 8) != 8) { }
    if(sleep_ms) usleep((useconds_t)sleep_ms * 1000);
  }
  close(cbfd);
  j.Close();
  vix.Close();
  db.Close();
  printf("vwrite ok %zu live\n", live.size());
  return 0;
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    fprintf(stderr, "usage: gxrepl_testwriter write|check ROOT [options]\n");
    return 64;
  }
  std::string mode = argv[1], root = argv[2];
  if((mode == "rawapply" || mode == "rawdiff") && argc == 5) return raw(mode, argv[2], argv[3], argv[4]);
  if(mode == "rawblock" && argc == 4) return rawblock(argv[2], argv[3]);
  if(mode == "objects" && argc == 6) return objects(argv[2], argv[3], argv[4], atoi(argv[5]));
  if(mode == "legacyobj" && argc == 3) return legacyobj(argv[2]);
  if(mode == "purge" && (argc == 3 || argc == 4)) {
    // 11/10/2026: purge SETROOT [STREAM]: one stream, or every stream whose journal is gone
    if(argc == 4) {
      std::string why;
      int rc = datareel::gxdShipLogPurgeStream(argv[2], argv[3], why);
      if(rc == 0) printf("removed %s\n", argv[3]); else printf("kept %s: %s\n", argv[3], why.c_str());
      return rc < 0 ? 2 : 0;
    }
    std::vector<std::string> removed, held;
    datareel::gxdShipLogPurgeOrphans(argv[2], &removed, &held);
    for(auto &r : removed) printf("removed %s\n", r.c_str());
    for(auto &h : held) printf("kept %s\n", h.c_str());
    return 0;
  }
  if(mode == "objstream" && (argc == 4 || argc == 5)) return objstream(argv[2], argv[3], argc == 5 ? atoi(argv[4]) : 0);
  if(mode == "objverify" && argc == 5) return objverify(argv[2], argv[3], argv[4]);
  if(mode == "vwrite" && argc >= 3) return vwrite(argc, argv);
  if(mode == "vmake" && argc == 5) return vmake(argv[2], atoi(argv[3]), atoi(argv[4]));   // 11/14/2026 (7.4)
  if(mode == "vsearch" && argc >= 6) return vsearch(argc, argv, false);
  if(mode == "vlocal" && argc >= 5) return vsearch(argc, argv, true);
  if(mode == "vinfo" && argc == 4) return vinfo_bad(argv[2], argv[3], false);
  if(mode == "vbad" && argc == 4) return vinfo_bad(argv[2], argv[3], true);
  // 11/14/2026: stamp PATH LOGID LSN: a fresh <index>.stamp; readstamp PATH: print it
  if(mode == "stamp" && argc == 5) {
    datareel::gxdStampData d;
    d.log_id = argv[3]; d.lsn = strtoull(argv[4], 0, 10); d.digest_kind = datareel::gxdStampDigestBytes;
    return datareel::gxdDerivedStamp::WriteFresh(argv[2], d) == 0 ? 0 : 1;
  }
  if(mode == "readstamp" && argc == 3) {
    datareel::gxdStampData d;
    int rc = datareel::gxdDerivedStamp::ReadFile(argv[2], d);
    if(rc == 0) printf("stamp log %s lsn %llu\n", d.log_id.c_str(), d.lsn);
    else printf("stamp %s\n", rc == 1 ? "none" : "damaged");
    return rc == 0 ? 0 : 1;
  }
  // 11/14/2026: lookup DIR APPLY_JNL: a reader of a copy (the stress test's searches): under the copy's apply
  // read lock, "<generation> <index count> <index sum>" (or "busy")
  if(mode == "lookup" && argc == 4) {
    gxdJournalReadLock rl;
    if(rl.Lock(argv[3]) != gxDBASE_NO_ERROR) { printf("busy\n"); return 1; }
    uint64_t g = ~0ULL;
    int fd = open((root + "/commit.bin").c_str(), O_RDONLY);
    if(fd >= 0) { if(pread(fd, &g, 8, 8) != 8) g = ~0ULL; close(fd); }
    gxDatabase ix;
    if(ix.Open((root + "/index.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { printf("noindex\n"); return 1; }
    FAU_t ia = ix.FindFirstObject();
    IndexObj io;
    if(ia == (FAU_t)0 || ix.Read(&io, sizeof(io), ia) != gxDBASE_NO_ERROR) { printf("noindex\n"); return 1; }
    printf("%llu %llu %llu\n", (unsigned long long)g, (unsigned long long)io.count, (unsigned long long)io.sum);
    return 0;
  }
  // 11/15/2026: btree ROOT ORDER N: a B-tree of N random 64-bit keys (numeric order) of node order ORDER in
  // ROOT/keys.btx, for gxrepl digest's in-order walk (an order below 5 is made 5: a node then holds 4 keys)
  if(mode == "btree" && argc > 4) {
    struct U64Key : public DatabaseKeyB {
      uint64_t v;
      U64Key(uint64_t x = 0) : DatabaseKeyB((char *)&v), v(x) { }
      size_t KeySize() { return sizeof(v); }
      int CompareKey(const DatabaseKeyB &o) const { uint64_t w; memcpy(&w, o.db_key, sizeof(w)); return v < w ? -1 : v > w ? 1 : 0; }
      int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
      int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
    };
    unsigned order = (unsigned)strtoul(argv[3], 0, 10);
    unsigned long n = strtoul(argv[4], 0, 10);
    U64Key t, c;
    gxBtree bt(t, (BtreeNodeOrder_t)order);
    std::string f = root + "/keys.btx";
    if(bt.Create(f.c_str()) != gxDBASE_NO_ERROR) { printf("create failed\n"); return 1; }
    unsigned long long x = 88172645463325252ULL;
    for(unsigned long i = 0; i < n; i++) {
      x ^= x << 13; x ^= x >> 7; x ^= x << 17;
      U64Key k(x);
      if(!bt.Insert(k, c)) { printf("insert failed\n"); return 1; }
    }
    bt.Close();
    printf("%lu keys, order %u\n", n, order);
    return 0;
  }
  if(mode == "check") return check(root);
  if(mode == "hashes") return hashes(root);
  if(mode == "reindex") { if(argc > 3) stamp_lsn = strtoull(argv[3], 0, 10); return reindex(root); }
  long commits = 100, per = 5, sleep_ms = 0;
  unsigned long long seg_bytes = 0, max_bytes = 0;  // 11/08/2026: ship log retention tests
  int keep_days = -1;
  bool ship = true;
  std::string ship_root = root;   // 11/10/2026: --ship-root SET: the day folder layout (ROOT inside SET)
  std::string stream_name;        // 11/14/2026: --stream NAME: one stream feeds several folders (seal per folder)
  // 11/10/2026: --commit-block: keep ROOT/commit.bin, a seqlock commit block like OSA's (u32 magic, u32
  // version, u64 generation at 8: odd while a commit writes the files, u64 commits at 16), written in
  // place (pwrite). --big N: the first commit adds one object of N bytes. --latency-out FILE: the
  // slowest commit (Begin to Commit) in ms and the count, written at the end.
  bool commit_block = false;
  unsigned long long big = 0;
  bool big_zero = false;
  bool old_ship_format = false;    // 11/10/2026: no commit block in the records
  std::string latency_out;
  std::string gen_log;            // 11/10/2026: after each commit "<generation> <data hash> <index hash>"
  std::string answers_log;        // 11/14/2026: after each commit "<generation> <index count> <index sum>"
  long split_at = -1; int split_how = 0;   // 11/14/2026
  unsigned long size_max = 12000;          // 11/14/2026: --size-max: a new object's payload is size_min .. + this bytes
  bool adds_only = false;                  // 11/15/2026: --adds-only: every object a new one (no change, no delete)
  unsigned long size_min = 20;             // 11/14/2026: --size-min
  // 11/14/2026 (stress): --burst-at K --burst-per N [--burst-split M]: from commit K, N objects in commits of at
  // most M (one commit when M is 0); --wait-at K --wait-file F: before commit K, wait until F exists
  long burst_at = -1, burst_per = 0, burst_split = 0, wait_at = -1;
  std::string wait_file;
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--commits" && i + 1 < argc) commits = atol(argv[++i]);
    else if(a == "--per" && i + 1 < argc) per = atol(argv[++i]);
    else if(a == "--sleep" && i + 1 < argc) sleep_ms = atol(argv[++i]);
    else if(a == "--seed" && i + 1 < argc) rng_state ^= strtoull(argv[++i], 0, 10) * 2654435761ULL;
    else if(a == "--no-ship") ship = false;
    else if(a == "--ship-root" && i + 1 < argc) ship_root = argv[++i];
    else if(a == "--stream" && i + 1 < argc) stream_name = argv[++i];
    else if(a == "--commit-block") commit_block = true;
    else if(a == "--big" && i + 1 < argc) big = strtoull(argv[++i], 0, 10);
    else if(a == "--old-ship-format") old_ship_format = true;
    else if(a == "--big-zero" && i + 1 < argc) { big = strtoull(argv[++i], 0, 10); big_zero = true; }
    else if(a == "--latency-out" && i + 1 < argc) latency_out = argv[++i];
    else if(a == "--gen-log" && i + 1 < argc) gen_log = argv[++i];
    else if(a == "--answers-log" && i + 1 < argc) answers_log = argv[++i];
    else if(a == "--size-max" && i + 1 < argc) { size_max = strtoul(argv[++i], 0, 10); if(size_max == 0) size_max = 1; }
    else if(a == "--size-min" && i + 1 < argc) size_min = strtoul(argv[++i], 0, 10);
    else if(a == "--adds-only") adds_only = true;
    else if(a == "--burst-at" && i + 1 < argc) burst_at = atol(argv[++i]);
    else if(a == "--burst-per" && i + 1 < argc) burst_per = atol(argv[++i]);
    else if(a == "--burst-split" && i + 1 < argc) burst_split = atol(argv[++i]);
    else if(a == "--wait-at" && i + 1 < argc) wait_at = atol(argv[++i]);
    else if(a == "--wait-file" && i + 1 < argc) wait_file = argv[++i];
    else if(a == "--segment" && i + 1 < argc) seg_bytes = strtoull(argv[++i], 0, 10);
    else if(a == "--keep-days" && i + 1 < argc) keep_days = atoi(argv[++i]);
    else if(a == "--max-bytes" && i + 1 < argc) max_bytes = strtoull(argv[++i], 0, 10);
    // 11/14/2026: commit K (from 0) writes only its objects record: --split-pair-at goes on (a gap follows),
    // --split-crash-at ends the writer there (its pages never reach the files)
    else if(a == "--split-pair-at" && i + 1 < argc) { split_at = atol(argv[++i]); split_how = 1; }
    else if(a == "--split-crash-at" && i + 1 < argc) { split_at = atol(argv[++i]); split_how = 2; }
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 64; }
  }
  std::string dn = root + "/data.gxd", in = root + "/index.gxd", jn = root + "/writer.jnl";
  gxdJournal j;
  if(j.Open(jn.c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "journal open failed\n"); return 1; }
  gxDatabase db, ix;
  bool fresh = false;
  if(db.Open(dn.c_str()) != gxDBASE_NO_ERROR) {
    if(db.Create(dn.c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "create failed\n"); return 1; }
    fresh = true;
  }
  if(ix.Open(in.c_str()) != gxDBASE_NO_ERROR) {
    if(ix.Create(in.c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "create failed\n"); return 1; }
    fresh = true;
  }
  if(j.Attach(&db) != gxDBASE_NO_ERROR || j.Attach(&ix) != gxDBASE_NO_ERROR) {
    fprintf(stderr, "attach failed\n"); return 1;
  }
  if(ship && j.EnableShipLog(ship_root.c_str(), stream_name.empty() ? 0 : stream_name.c_str()) != gxDBASE_NO_ERROR) {
    fprintf(stderr, "EnableShipLog failed\n"); return 1;
  }
  if(ship && j.ShipLog()) {
    if(seg_bytes) j.ShipLog()->SetSegmentSize(seg_bytes);
    if(keep_days >= 0 || max_bytes) j.ShipLog()->SetRetention(keep_days >= 0 ? keep_days : 7, max_bytes ? max_bytes : 2ULL * 1024 * 1024 * 1024);
  }
  IndexObj io;
  memset(&io, 0, sizeof(io));
  FAU_t ia = ix.FindFirstObject();
  if(fresh || ia == (FAU_t)0) {
    j.Begin();
    ia = ix.Alloc(sizeof(io));
    io.next_id = 1;
    ix.Write(&io, sizeof(io), ia);
    if(j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "commit failed\n"); return 1; }
  }
  else ix.Read(&io, sizeof(io), ia);

  int cbfd = -1;
  uint64_t gen = 0;
  if(commit_block) {
    cbfd = open((root + "/commit.bin").c_str(), O_RDWR | O_CREAT, 0644);
    if(cbfd < 0) { fprintf(stderr, "commit.bin\n"); return 1; }
    unsigned char blk[4096];
    memset(blk, 0, sizeof(blk));
    if(pread(cbfd, blk, sizeof(blk), 0) != (ssize_t)sizeof(blk)) {
      uint32_t magic = 0x42435754, ver = 1;     // "TWCB"
      memcpy(blk, &magic, 4); memcpy(blk + 4, &ver, 4);
      uint64_t c0 = io.commits; memcpy(blk + 16, &c0, 8);
      if(pwrite(cbfd, blk, sizeof(blk), 0) != (ssize_t)sizeof(blk)) { fprintf(stderr, "commit.bin\n"); return 1; }
    }
    memcpy(&gen, blk + 8, 8);
    if(gen & 1) gen++;                           // a writer died in a batch: the files were recovered
  }
  auto cb_write = [&](uint64_t g, bool with_commits) {
    if(cbfd < 0) return;
    if(pwrite(cbfd, &g, 8, 8) != 8) { }
    if(with_commits) { uint64_t c = io.commits; if(pwrite(cbfd, &c, 8, 16) != 8) { } }
  };
  double worst_ms = 0;
  long timed = 0;
  std::vector<unsigned char> buf;
  long burst_left = burst_per;
  for(long c = 0; c < commits; c++) {
    if(c == wait_at && !wait_file.empty()) while(access(wait_file.c_str(), F_OK) != 0) usleep(100000);
    long per_now = per;
    if(burst_at >= 0 && c >= burst_at && burst_left > 0) {
      per_now = burst_split > 0 && burst_split < burst_left ? burst_split : burst_left;
      burst_left -= per_now;
    }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    cb_write(++gen, false);                      // odd: the files are changing
    if(j.Begin() != gxDBASE_NO_ERROR) { fprintf(stderr, "begin failed\n"); return 1; }
    if(big && c == 0) {
      ObjHdr h;
      memset(&h, 0, sizeof(h));
      h.id = io.next_id++; h.version = 1; h.len = (gxUINT32)big;
      buf.assign(sizeof(h) + big, 0);
      // random bytes, so it does not compress (a ship-log record over max_message stays over it)
      if(!big_zero)                    // 11/10/2026: --big-zero: zeros (compresses to almost nothing)
	for(unsigned long long i = 0; i + 8 <= big; i += 8) { unsigned long long r8 = rnd(); memcpy(&buf[sizeof(h) + i], &r8, 8); }
      h.check = obj_check(h, buf.data() + sizeof(h));
      memcpy(buf.data(), &h, sizeof(h));
      FAU_t a = db.Alloc((__ULWORD__)buf.size());
      if(a == (FAU_t)0 || db.Write(buf.data(), (__ULWORD__)buf.size(), a) != gxDBASE_NO_ERROR) {
	fprintf(stderr, "big write failed\n"); return 1;
      }
      io.count++;
      io.sum += (unsigned long long)h.id * 1000003ULL + h.version;
    }
    for(long k = 0; k < per_now; k++) {
      ObjHdr h;
      memset(&h, 0, sizeof(h));
      unsigned long long r = rnd() % 10;
      if(r < 7 || io.count == 0 || adds_only) {
	// A new object, 20 bytes to 12 KB
	h.id = io.next_id++;
	h.version = 1;
	h.len = (gxUINT32)(size_min + rnd() % size_max);
	buf.resize(sizeof(h) + h.len);
	for(gxUINT32 i = 0; i < h.len; i++) buf[sizeof(h) + i] = (unsigned char)rnd();
	h.check = obj_check(h, buf.data() + sizeof(h));
	memcpy(buf.data(), &h, sizeof(h));
	FAU_t a = db.Alloc((__ULWORD__)buf.size());
	if(a == (FAU_t)0 || db.Write(buf.data(), (__ULWORD__)buf.size(), a) != gxDBASE_NO_ERROR) {
	  fprintf(stderr, "write failed\n"); return 1;
	}
	io.count++;
	io.sum += (unsigned long long)h.id * 1000003ULL + h.version;
      }
      else {
	// Change or delete the first object at or after a random spot
	FAU_t a = db.FindFirstObject();
	long skip = (long)(rnd() % (io.count < 50 ? io.count : 50));
	for(long s = 0; s < skip && a != (FAU_t)0; s++) a = db.FindNextObject(a);
	if(a == (FAU_t)0) continue;
	__ULWORD__ n = db.ObjectLength(a);
	buf.resize(n);
	if(db.Read(buf.data(), n, a) != gxDBASE_NO_ERROR) continue;
	memcpy(&h, buf.data(), sizeof(h));
	io.sum -= (unsigned long long)h.id * 1000003ULL + h.version;
	if(r < 9) {
	  h.version++;
	  for(gxUINT32 i = 0; i < h.len; i += 7) buf[sizeof(h) + i] = (unsigned char)rnd();
	  h.check = obj_check(h, buf.data() + sizeof(h));
	  memcpy(buf.data(), &h, sizeof(h));
	  db.Write(buf.data(), n, a);
	  io.sum += (unsigned long long)h.id * 1000003ULL + h.version;
	}
	else {
	  db.Delete(a);
	  io.count--;
	}
      }
    }
    io.commits++;
    ix.Write(&io, sizeof(io), ia);
    // 11/10/2026: the commit block as it will be once this commit lands goes in the record (as OSA's
    // writers do), unless --old-ship-format (a log from before that: the copy reports the unit unsealed)
    if(cbfd >= 0 && j.ShipLog() && !old_ship_format) {
      unsigned char nb[4096];
      memset(nb, 0, sizeof(nb));
      if(pread(cbfd, nb, sizeof(nb), 0) != (ssize_t)sizeof(nb)) { fprintf(stderr, "commit.bin\n"); return 1; }
      uint64_t g2 = gen + 1, c2 = io.commits;
      memcpy(nb + 8, &g2, 8); memcpy(nb + 16, &c2, 8);
      j.ShipLog()->SetCommitBlock(root + "/commit.bin", nb, sizeof(nb));
    }
    if(c == split_at && j.ShipLog()) gxdShipLogTest::Split(j.ShipLog(), split_how);
    if(j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "commit failed\n"); return 1; }
    cb_write(++gen, true);                       // even: the files are this commit
    if(!answers_log.empty()) {
      FILE *al = fopen(answers_log.c_str(), "a");
      if(al) { fprintf(al, "%llu %llu %llu\n", (unsigned long long)gen, (unsigned long long)io.count, (unsigned long long)io.sum); fclose(al); }
    }
    if(!gen_log.empty()) {
      FILE *gl = fopen(gen_log.c_str(), "a");
      if(gl) {
	fprintf(gl, "%llu %016llx %016llx\n", (unsigned long long)gen,
		(unsigned long long)content_hash(dn), (unsigned long long)content_hash(in));
	fclose(gl);
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    if(c > 0 || !big) { if(ms > worst_ms) worst_ms = ms; timed++; }   // the big first commit is not timed
    if(!latency_out.empty() && timed > 0) {      // after every commit, so a test may stop the writer at any time
      std::string tmp = latency_out + ".tmp";
      FILE *lf = fopen(tmp.c_str(), "w");
      if(lf) { fprintf(lf, "%.1f %ld\n", worst_ms, timed); fclose(lf); rename(tmp.c_str(), latency_out.c_str()); }
    }
    if(sleep_ms) usleep((useconds_t)sleep_ms * 1000);
  }
  if(cbfd >= 0) close(cbfd);
  if(!latency_out.empty()) {
    FILE *lf = fopen(latency_out.c_str(), "w");
    if(lf) { fprintf(lf, "%.1f %ld\n", worst_ms, timed); fclose(lf); }
  }
  j.Close();
  ix.Close();
  db.Close();
  printf("wrote %ld commits, %llu objects\n", commits, (unsigned long long)io.count);
  return 0;
}
