// odirect_test.cpp - 11/17/2026 (7.4): what a closed gxObjRepl node left on the disk, read past the page cache.
// From dougg's direct_io (2016): aligned O_DIRECT reads of every file of a node (databases, B-trees, the journal, the
// outbox, the state, the digest log, the index files) after its close (the close checkpoints and syncs them), with
// each file's clean pages dropped first (posix_fadvise DONTNEED), compared with an ordinary read; then the bytes read
// directly are copied to a new folder, which must give the same Digest() and a consistent IndexCheck(). A second
// round does the same after more commits, a purge and a reopen. Linux (O_DIRECT); a file system that refuses O_DIRECT
// is a FAIL, not a skip. Usage: odirect_test DIR. Prints PASS/FAIL lines; exit 0 when all pass.
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "gxdlcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <random>
#include <filesystem>
#include "gxobjrepl.h"
using namespace datareel;
namespace fs = std::filesystem;

static int pass = 0, fail = 0;
static void check(bool ok, const std::string &what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); fflush(stdout); (ok ? pass : fail)++; }

static const size_t ALIGN = 4096;   // the buffer, the offsets and the sizes of each O_DIRECT read

// the file read with O_DIRECT in aligned pieces (the last read returns the tail); false and WHY on an error
static bool read_direct(const std::string &fn, std::string &out, std::string &why)
{
  out.clear();
  int fd = ::open(fn.c_str(), O_RDONLY | O_DIRECT | O_CLOEXEC);
  if(fd < 0) { why = fn + ": open with O_DIRECT: " + strerror(errno); return false; }
  const size_t piece = 64 * ALIGN;
  void *buf = 0;
  if(posix_memalign(&buf, ALIGN, piece) != 0) { ::close(fd); why = "posix_memalign"; return false; }
  off_t off = 0;
  for(;;) {
    ssize_t n = pread(fd, buf, piece, off);
    if(n < 0) { why = fn + ": pread with O_DIRECT at " + std::to_string((long long)off) + ": " + strerror(errno); free(buf); ::close(fd); return false; }
    if(n == 0) break;
    out.append((const char *)buf, (size_t)n);
    off += n;
    if((size_t)n < piece) break;                              // the end of the file (a short read is the tail)
  }
  free(buf); ::close(fd);
  return true;
}

static bool read_plain(const std::string &fn, std::string &out)
{
  FILE *f = fopen(fn.c_str(), "rb");
  if(!f) return false;
  out.clear(); char b[65536]; size_t n;
  while((n = fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
  fclose(f);
  return true;
}

// the clean cached pages of FN dropped, so the next read goes to the device (a closed, synced file has no dirty ones)
static void drop_cache(const std::string &fn)
{
  int fd = ::open(fn.c_str(), O_RDONLY | O_CLOEXEC);
  if(fd < 0) return;
  fdatasync(fd);
  posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
  ::close(fd);
}

// every node file: the direct read equals the plain one; the direct bytes copied to OUT. Returns the files compared
static int compare_node(const std::string &root, const std::string &out, int &bad, std::string &first_bad)
{
  int files = 0;
  std::error_code ec;
  fs::remove_all(out, ec); fs::create_directories(out, ec);
  for(auto &e : fs::directory_iterator(root)) {
    if(!e.is_regular_file()) continue;
    std::string fn = e.path().string(), name = e.path().filename().string();
    if(name == "objrepl.lock") continue;                      // the lock file is not data
    drop_cache(fn);
    std::string d, p, why;
    bool ok = read_direct(fn, d, why) && read_plain(fn, p) && d == p;
    if(!ok) { if(!bad) first_bad = why.empty() ? name + ": the direct read differs from the plain read (" + std::to_string(d.size()) + " vs " + std::to_string(p.size()) + " bytes)" : why; bad++; }
    FILE *f = fopen((out + "/" + name).c_str(), "wb");
    if(f) { fwrite(d.data(), 1, d.size(), f); fclose(f); }
    files++;
  }
  return files;
}

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : ".";
  std::string root = dir + "/odirect_node", out = dir + "/odirect_copy";
  std::error_code ec;
  fs::remove_all(root, ec); fs::remove_all(out, ec);
  // O_DIRECT works here at all?
  { std::string probe = dir + "/odirect_probe"; FILE *f = fopen(probe.c_str(), "wb"); if(f) { std::string z(ALIGN * 3 + 100, 'x'); fwrite(z.data(), 1, z.size(), f); fclose(f); }
    std::string d, why; bool ok = read_direct(probe, d, why) && d.size() == ALIGN * 3 + 100; fs::remove(probe, ec);
    check(ok, "O_DIRECT aligned reads work in " + dir + (ok ? "" : ": " + why + " (a file system without O_DIRECT, e.g. tmpfs, cannot run this test)"));
    if(!ok) { printf("%d passed, %d failed\n", pass, fail); return 1; } }

  gxObjReplConfig c;
  c.copy = "127.0.0.1:9"; c.name = "P"; c.links = "lk:0:ev"; c.index = "k:ev:B:0:8,v:ev:V:8:4";
  c.log = [](const std::string &) {};
  // the primary's copy (127.0.0.1:9) is never reached: commits are durable on the primary alone. Open() checks the
  // certificates, made by utils/objrepl/objrepl_certs.sh and named in the environment
  const char *pki = getenv("ODIRECT_PKI");
  if(!pki) { check(false, "ODIRECT_PKI (a folder from utils/objrepl/objrepl_certs.sh) is set"); printf("%d passed, %d failed\n", pass, fail); return 1; }
  c.ca = std::string(pki) + "/ca.pem"; c.cert = std::string(pki) + "/primary.pem"; c.key = std::string(pki) + "/primary.key";
  std::mt19937_64 r(2016);
  std::vector<uint64_t> ids;
  for(int round = 1; round <= 2; round++) {
    gxObjRepl x;
    int orc = x.Open(root, c, gxObjRepl::PRIMARY);
    if(orc != 0) { check(false, "round " + std::to_string(round) + ": Open: " + x.Error()); break; }
    x.OpenDatabase("ev"); x.OpenDatabase("lk");
    for(int k = 0; k < 20; k++) {                              // 2000 objects of 0.1-6 KB a round, links, updates
      x.Begin();
      for(int i = 0; i < 100; i++) {
	std::vector<unsigned char> b(24 + r() % 6000);
	for(auto &ch : b) ch = (unsigned char)r();
	float v[4] = { (float)(r() % 1000), (float)(r() % 1000), 0, 1 }; memcpy(&b[8], v, 16);
	uint64_t id = (round == 2 && !ids.empty() && r() % 4 == 0) ? ids[r() % ids.size()] : x.NewId();
	x.Put("ev", id, b.data(), (uint32_t)b.size());
	ids.push_back(id);
      }
      uint64_t lid = x.NewId(), t = ids[r() % ids.size()];
      uint64_t a = (uint64_t)x.AddressOf("ev", t); unsigned char lb[16]; memcpy(lb, &a, 8); memcpy(lb + 8, &t, 8);
      x.Put("lk", lid, lb, 16);
      if(x.Commit() != 0) { check(false, "round " + std::to_string(round) + ": Commit: " + x.Error()); break; }
    }
    if(round == 2) {                                           // removes and a purge of unlinked tombstones
      std::vector<uint64_t> gone;
      x.Begin(); for(int i = 0; i < 50; i++) { uint64_t g = ids[r() % ids.size()]; if(x.Remove("ev", g) == 0) gone.push_back(g); } x.Commit();
      x.Begin(); for(uint64_t g : gone) x.Purge("ev", g); x.Commit();   // a tombstone a link names is refused: kept
    }
    x.Close();                                                 // checkpoint and sync of every file
    std::string da, db, ia, ib, first;
    int bad = 0, files = compare_node(root, out, bad, first);
    check(bad == 0 && files >= 8, "round " + std::to_string(round) + ": " + std::to_string(files) + " node files read with O_DIRECT (cache dropped) equal the plain reads" + (bad ? " [" + first + "]" : ""));
    int r1 = gxObjRepl::Digest(root, c.links, da), r2 = gxObjRepl::Digest(out, c.links, db);
    int r3 = gxObjRepl::IndexCheck(out, c.index, ib);
    check(r1 == 0 && r2 == 0 && da == db && r3 == 0, "round " + std::to_string(round) + ": the bytes read directly give the node's digest [" + da + "] and consistent indexes" + (r3 ? " [" + ib + "]" : ""));
  }
  fs::remove_all(root, ec); fs::remove_all(out, ec);
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
