// gxrepl.cpp - DataReel 7.3 database replication over gxStream.
//
// One node runs "gxrepl serve" (listens on TCP 2073, mutual TLS); the
// other runs "gxrepl run" and dials it. See gxrepl.h and README.md.
//
//   gxrepl serve  --config FILE [--key-fd N]
//   gxrepl run    --config FILE [--key-fd N]
//   gxrepl once   --config FILE [--set NAME] [--key-fd N]
//   gxrepl status --config FILE
//   gxrepl check  --config FILE
//   gxrepl torn   --config FILE [--set NAME]   the copies of commit_block sets that are not one whole
//                 commit (exit 1: a fail over must not promote them)
//   gxrepl fingerprint CERT.pem
//   gxrepl shiplog STREAM_DIR [--from LSN]
//   gxrepl version
//
// The private key (config "key", or --key-fd) is "fd:N" (read from an
// open file descriptor, for example a pipe from drsecrets), "env:VAR"
// (read and removed from the environment) or a file path. A key read
// from a descriptor or the environment lives only in an anonymous
// memory file (memfd), never in a file on disk.
//
// Exit codes: 0 ok, 1 a set is not in step (once), 2 error, 64 usage.
#ifndef GXREPL_GIT_HEAD
#define GXREPL_GIT_HEAD "unknown"    // 11/10/2026: set by gxrepl_version.cmake
#endif
#ifndef GXREPL_BUILD_TIME
#define GXREPL_BUILD_TIME "unknown"
#endif
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include "gxrepl.h"
#include "gxdrepl.h"

using namespace datareel;

static gxReplServer *g_server = 0;
static gxReplClient *g_client = 0;

static void on_stop(int)
{
  if(g_server) g_server->Stop();
  if(g_client) g_client->Stop();
}

static void on_hup(int)
{
  if(g_client) g_client->RequestVerify();
}

static int usage()
{
  fprintf(stderr,
	  "usage: gxrepl serve|run|once|status|check|torn --config FILE [--set NAME] [--key-fd N]\n"
	  "       gxrepl fingerprint CERT.pem\n"
	  "       gxrepl shiplog STREAM_DIR [--from LSN]\n"
	  "       gxrepl version\n");
  return 64;
}

// Put a key that is not a plain file into a memfd. Returns 0 and the
// path OpenSSL can open (/proc/self/fd/N), or -1.
static int key_path(const std::string &spec, std::string &path, std::string &err)
{
  std::string pem;
  if(spec.compare(0, 3, "fd:") == 0) {
    int fd = atoi(spec.c_str() + 3);
    if(fd < 0) { err = "bad key descriptor"; return -1; }
    char b[4096];
    ssize_t n;
    while((n = read(fd, b, sizeof(b))) > 0) {
      pem.append(b, (size_t)n);
      if(pem.size() > 256 * 1024) { err = "key too large"; return -1; }
    }
    close(fd);
    // gxrepl.sh writes the key from a shell it forked before exec'ing us
    // (process substitution): reap it so it is not left as a zombie
    for(int i = 0; i < 100; i++) {
      pid_t r = waitpid(-1, 0, WNOHANG);
      if(r < 0) break;          // no child left
      if(r == 0) usleep(10000); // still writing its last bytes
    }
  }
  else if(spec.compare(0, 4, "env:") == 0) {
    const char *v = getenv(spec.c_str() + 4);
    if(!v || !*v) { err = "no key in " + spec.substr(4); return -1; }
    pem = v;
    unsetenv(spec.c_str() + 4);
  }
  else {
    path = spec;
    struct stat st;
    if(stat(path.c_str(), &st) != 0) { err = "no key file " + path; return -1; }
    if(st.st_mode & 077)
      fprintf(stderr, "gxrepl: warning: %s can be read by others (chmod 600)\n", path.c_str());
    return 0;
  }
  if(pem.find("PRIVATE KEY") == std::string::npos) { err = "the key is not a PEM private key"; return -1; }
  int mfd = memfd_create("gxrepl-key", MFD_CLOEXEC);
  if(mfd < 0) { err = "memfd_create failed"; return -1; }
  size_t off = 0;
  while(off < pem.size()) {
    ssize_t w = write(mfd, pem.data() + off, pem.size() - off);
    if(w <= 0) { close(mfd); err = "memfd write failed"; return -1; }
    off += (size_t)w;
  }
  // Overwrite our copy; the memfd is the only one left
  volatile char *p = &pem[0];
  for(size_t i = 0; i < pem.size(); i++) p[i] = 0;
  path = "/proc/self/fd/" + std::to_string(mfd);
  return 0;
}

static int fingerprint(const char *fname)
{
  FILE *f = fopen(fname, "r");
  if(!f) { fprintf(stderr, "gxrepl: cannot open %s\n", fname); return 2; }
  X509 *x = PEM_read_X509(f, 0, 0, 0);
  fclose(f);
  if(!x) { fprintf(stderr, "gxrepl: %s is not a PEM certificate\n", fname); return 2; }
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n = 0;
  X509_digest(x, EVP_sha256(), md, &n);
  char cn[256] = "";
  X509_NAME_get_text_by_NID(X509_get_subject_name(x), NID_commonName, cn, sizeof(cn));
  X509_free(x);
  for(unsigned i = 0; i < n; i++) printf("%s%02X", i ? ":" : "", md[i]);
  printf("  CN=%s\n", cn);
  return 0;
}

static int shiplog(const std::string &dir, unsigned long long from)
{
  gxdShipLogReader rd;
  if(rd.Open(dir) != 0) { fprintf(stderr, "gxrepl: %s\n", rd.Error().c_str()); return 2; }
  gxdShipLogReader::Info info;
  if(rd.GetInfo(info) != 0) { fprintf(stderr, "gxrepl: %s\n", rd.Error().c_str()); return 2; }
  printf("log %s  first %llu  next %llu  journal %s\n", info.log_id.c_str(),
	 info.first_lsn, info.next_lsn, info.journal.c_str());
  for(auto &f : info.files) printf("  file %s\n", f.c_str());
  if(from == 0) from = info.first_lsn ? info.first_lsn : 1;
  while(1) {
    std::vector<unsigned char> out;
    unsigned long long next = 0;
    unsigned long count = 0;
    int rc = rd.Read(from, 4 * 1024 * 1024, out, next, count);
    if(rc == 1) { printf("lsn %llu is no longer kept\n", from); return 1; }
    if(rc != 0) { fprintf(stderr, "gxrepl: %s\n", rd.Error().c_str()); return 2; }
    if(count == 0) break;
    std::vector<gxdShipRecordView> recs;
    gxdParseShipRecords(out.data(), out.size(), recs);
    for(auto &r : recs) {
      time_t t = (time_t)(r.time_us / 1000000);
      struct tm tmv; localtime_r(&t, &tmv);
      char tb[32]; strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tmv);
      const char *ty = r.type == gxdShipCommit ? "commit" : r.type == gxdShipOpen ? "open" :
	r.type == gxdShipGap ? "gap" : "?";
      printf("%llu %s.%06llu %s", r.lsn, tb, r.time_us % 1000000, ty);
      if(r.type == gxdShipCommit) {
	std::vector<gxdShipCommitFile> files;
	if(gxdParseShipCommit(r.payload, r.len, files) == 0) {
	  for(auto &f : files) printf("  %s %zu page(s) size %llu", f.name.c_str(), f.pages.size(), f.size);
	}
	else printf("  (damaged)");
      }
      printf("\n");
    }
    from = next;
  }
  return 0;
}

int main(int argc, char **argv)
{
  if(argc < 2) return usage();
  std::string cmd = argv[1];
  if(cmd == "version") {
    // 11/10/2026: the git head and build time come from CMake (gxrepl_version.cmake)
    printf("gxrepl (DataReel %s), protocol %d, default port %d, zlib %s, git %s, built %s\n",
	   DATAREEL_VERSION_STRING, gxREPL_PROTOCOL, gxREPL_DEFAULT_PORT,
	   gxReplHasZlib() ? "yes" : "no", GXREPL_GIT_HEAD, GXREPL_BUILD_TIME);
    return 0;
  }
  if(cmd == "fingerprint") return argc == 3 ? fingerprint(argv[2]) : usage();
  if(cmd == "shiplog") {
    if(argc < 3) return usage();
    unsigned long long from = 0;
    if(argc == 5 && std::string(argv[3]) == "--from") from = strtoull(argv[4], 0, 10);
    return shiplog(argv[2], from);
  }
  std::string conf, only_set, key_fd;
  for(int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--config" && i + 1 < argc) conf = argv[++i];
    else if(a == "--set" && i + 1 < argc) only_set = argv[++i];
    else if(a == "--key-fd" && i + 1 < argc) key_fd = std::string("fd:") + argv[++i];
    else return usage();
  }
  if(conf.empty()) return usage();
  gxReplConfig cfg;
  std::string err;
  if(cfg.Load(conf, err) != 0) { fprintf(stderr, "gxrepl: %s\n", err.c_str()); return 2; }
  if(!only_set.empty() && !cfg.Set(only_set)) {
    fprintf(stderr, "gxrepl: no set %s in %s\n", only_set.c_str(), conf.c_str());
    return 2;
  }

  if(cmd == "status") {
    if(cfg.status_file.empty()) { fprintf(stderr, "gxrepl: no status_file in %s\n", conf.c_str()); return 2; }
    FILE *f = fopen(cfg.status_file.c_str(), "r");
    if(!f) { fprintf(stderr, "gxrepl: no status yet (%s)\n", cfg.status_file.c_str()); return 2; }
    char b[4096]; size_t n;
    while((n = fread(b, 1, sizeof(b), f)) > 0) fwrite(b, 1, n, stdout);
    fclose(f);
    struct stat st;
    if(stat(cfg.status_file.c_str(), &st) == 0 && time(0) - st.st_mtime > 30) {
      fprintf(stderr, "gxrepl: the status is %ld s old: is gxrepl running?\n",
	      (long)(time(0) - st.st_mtime));
      return 1;
    }
    return 0;
  }
  if(cmd == "torn") {
    // 11/10/2026: the copies of commit_block sets that are not one whole
    // commit; exit 1 if any (a fail over must not promote them)
    int bad = 0, any = 0;
    for(auto &s : cfg.sets) {
      if(!only_set.empty() && s.name != only_set) continue;
      if(s.commit_block.empty()) continue;
      any = 1;
      std::vector<std::string> torn;
      int n = datareel::gxReplTornCopies(s, torn);
      printf("set %s: %d folder(s) checked, %zu torn\n", s.name.c_str(), n, torn.size());
      for(auto &x : torn) printf("  TORN %s\n", x.c_str());
      if(!torn.empty()) bad = 1;
    }
    if(!any) printf("no set with commit_block\n");
    return bad;
  }
  if(cmd == "check") {
    printf("node %s, role %s, %zu set(s)\n", cfg.node.c_str(), cfg.role.c_str(), cfg.sets.size());
    int bad = 0;
    for(const char *p : { cfg.cert.c_str(), cfg.ca.c_str() }) {
      if(access(p, R_OK) != 0) { printf("  missing %s\n", p); bad = 1; }
    }
    for(auto &s : cfg.sets) {
      struct stat st;
      bool ok = stat(s.root.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
      printf("  set %-24s %s %s %s\n", s.name.c_str(),
	     cfg.role == "client" ? s.direction.c_str() : s.accept.c_str(),
	     s.root.c_str(), ok ? "" : "(no such folder yet)");
    }
    return bad ? 2 : 0;
  }

  if(!key_fd.empty()) cfg.key = key_fd;
  if(cfg.key.empty()) { fprintf(stderr, "gxrepl: no key: set key in %s or pass --key-fd N\n", conf.c_str()); return 2; }
  std::string kp;
  if(key_path(cfg.key, kp, err) != 0) { fprintf(stderr, "gxrepl: key: %s\n", err.c_str()); return 2; }
  cfg.key = kp;

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_stop;
  sigaction(SIGTERM, &sa, 0);
  sigaction(SIGINT, &sa, 0);
  sa.sa_handler = on_hup;
  sigaction(SIGHUP, &sa, 0);
  signal(SIGPIPE, SIG_IGN);

  if(cmd == "serve") {
    if(cfg.role != "server") { fprintf(stderr, "gxrepl: %s is not a server config\n", conf.c_str()); return 2; }
    gxReplServer s(cfg);
    g_server = &s;
    int rc = s.Run(err);
    g_server = 0;
    if(rc != 0) { fprintf(stderr, "gxrepl: %s\n", err.c_str()); return 2; }
    return 0;
  }
  if(cmd == "run" || cmd == "once") {
    if(cfg.role != "client") { fprintf(stderr, "gxrepl: %s is not a client config\n", conf.c_str()); return 2; }
    gxReplClient c(cfg);
    g_client = &c;
    int rc = c.Run(cmd == "once", only_set, err);
    g_client = 0;
    if(rc != 0 && !err.empty()) fprintf(stderr, "gxrepl: %s\n", err.c_str());
    return rc;
  }
  return usage();
}
