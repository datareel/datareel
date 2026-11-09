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
#include <string>
#include <vector>
#include "gxdbase.h"
#include "gxdjrnl.h"
#include "gxrepl.h"   // 11/08/2026: the raw commands

// 11/08/2026: gxrepl_testwriter rawapply|rawdiff CONF SET PATH
// Connects with the client config CONF and asks the server, as a client
// that skipped its own checks would, to accept PATH as a copy (rawapply: a
// one-piece file update) or to send PATH as a source (rawdiff). Prints "ok"
// or "error: ..." (exit 2).
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
  printf("check: %llu objects, %llu bad, index count %llu, commits %llu, %s\n",
	 count, bad, (unsigned long long)io.count, (unsigned long long)io.commits,
	 (bad == 0 && count == io.count && sum == io.sum) ? "OK" : "MISMATCH");
  return (bad == 0 && count == io.count && sum == io.sum) ? 0 : 1;
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    fprintf(stderr, "usage: gxrepl_testwriter write|check ROOT [options]\n");
    return 64;
  }
  std::string mode = argv[1], root = argv[2];
  if((mode == "rawapply" || mode == "rawdiff") && argc == 5) return raw(mode, argv[2], argv[3], argv[4]);
  if(mode == "check") return check(root);
  long commits = 100, per = 5, sleep_ms = 0;
  bool ship = true;
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--commits" && i + 1 < argc) commits = atol(argv[++i]);
    else if(a == "--per" && i + 1 < argc) per = atol(argv[++i]);
    else if(a == "--sleep" && i + 1 < argc) sleep_ms = atol(argv[++i]);
    else if(a == "--seed" && i + 1 < argc) rng_state ^= strtoull(argv[++i], 0, 10) * 2654435761ULL;
    else if(a == "--no-ship") ship = false;
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
  if(ship && j.EnableShipLog(root.c_str()) != gxDBASE_NO_ERROR) {
    fprintf(stderr, "EnableShipLog failed\n"); return 1;
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

  std::vector<unsigned char> buf;
  for(long c = 0; c < commits; c++) {
    if(j.Begin() != gxDBASE_NO_ERROR) { fprintf(stderr, "begin failed\n"); return 1; }
    for(long k = 0; k < per; k++) {
      ObjHdr h;
      memset(&h, 0, sizeof(h));
      unsigned long long r = rnd() % 10;
      if(r < 7 || io.count == 0) {
	// A new object, 20 bytes to 12 KB
	h.id = io.next_id++;
	h.version = 1;
	h.len = (gxUINT32)(20 + rnd() % 12000);
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
    if(j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "commit failed\n"); return 1; }
    if(sleep_ms) usleep((useconds_t)sleep_ms * 1000);
  }
  j.Close();
  ix.Close();
  db.Close();
  printf("wrote %ld commits, %llu objects\n", commits, (unsigned long long)io.count);
  return 0;
}
