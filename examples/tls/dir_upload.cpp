// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: dir_upload.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
// Date Last Modified: 11/17/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
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

dir_upload: watches a folder and uploads each new file by FTPS
(gxsFTPClient), then deletes it (or moves it to a done folder) once
the server holds the whole file. From dougg's dirscanf.

  dir_upload --dir DIR --host HOST [--port N] --user USER
	     [--password-file FILE] [--ca FILE] [--implicit]
	     [--remote-dir DIR] [--pattern GLOB] [--settle S]
	     [--interval S] [--done DIR] [--once] [--log FILE]

The password: DR_FTP_PASSWORD in the environment, or the first line
of --password-file (which must not be readable by group or others on
POSIX). Never on the command line, never in the log.

A file is taken once it has not changed for --settle seconds (default
5: a file still being written is left alone). It is sent as NAME.part,
renamed to NAME on the server, and its size there (SIZE) must equal
the local size before the local file is deleted (or moved to --done).
On any error the file stays and is tried again at the next scan.
--once: one scan, then exit (0 if every file was uploaded, 1 if any
failed). Otherwise it scans every --interval seconds (default 10)
until Ctrl+C or SIGTERM. Explicit FTPS (AUTH TLS, port 21) by
default; --implicit for implicit FTPS (port 990).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include <filesystem>
#if !defined (_WIN32)
#include <sys/stat.h>
#endif
#include "gxsftp.h"

using namespace datareel;
namespace fs = std::filesystem;

static std::atomic<int> g_stop{0};
static void on_signal(int) { g_stop = 1; }

struct Opts {
  std::string dir, host, user, password, ca, remote_dir, pattern = "*", done, log;
  int port = 0, settle = 5, interval = 10;
  bool implicit = false, once = false;
};

static FILE *g_log = 0;
static void logf(const char *fmt, ...)
{
  char ts[32]; time_t t = time(0); strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
  va_list ap;
  for(FILE *f : {stderr, g_log}) {
    if(!f) continue;
    fprintf(f, "%s dir_upload: ", ts);
    va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fflush(f);
  }
}

static bool glob_match(const std::string &p, const std::string &n)
{
  std::function<bool(size_t, size_t)> m = [&](size_t i, size_t j) -> bool {
    if(i == p.size()) return j == n.size();
    if(p[i] == '*') { for(size_t k = j; k <= n.size(); k++) if(m(i + 1, k)) return true; return false; }
    if(j == n.size()) return false;
    return (p[i] == '?' || p[i] == n[j]) && m(i + 1, j + 1);
  };
  return m(0, 0);
}

static bool read_password(Opts &o, const std::string &file)
{
  if(file.empty()) {
    const char *e = getenv("DR_FTP_PASSWORD");
    if(!e || !*e) { fprintf(stderr, "dir_upload: no password: set DR_FTP_PASSWORD or use --password-file\n"); return false; }
    o.password = e;
    return true;
  }
#if !defined (_WIN32)
  struct stat sb;
  if(stat(file.c_str(), &sb) != 0) { fprintf(stderr, "dir_upload: cannot read %s\n", file.c_str()); return false; }
  if(sb.st_mode & 077) { fprintf(stderr, "dir_upload: %s is readable by group or others (chmod 600)\n", file.c_str()); return false; }
#endif
  FILE *f = fopen(file.c_str(), "rb");
  if(!f) { fprintf(stderr, "dir_upload: cannot read %s\n", file.c_str()); return false; }
  char b[1024]; size_t n = fread(b, 1, sizeof(b) - 1, f); fclose(f); b[n] = 0;
  o.password.assign(b, strcspn(b, "\r\n"));
  memset(b, 0, sizeof(b));
  if(o.password.empty()) { fprintf(stderr, "dir_upload: %s is empty\n", file.c_str()); return false; }
  return true;
}

static int usage()
{
  fprintf(stderr, "usage: dir_upload --dir DIR --host HOST [--port N] --user USER [--password-file FILE] [--ca FILE]\n"
		  "                  [--implicit] [--remote-dir DIR] [--pattern GLOB] [--settle S] [--interval S]\n"
		  "                  [--done DIR] [--once] [--log FILE]   (password: DR_FTP_PASSWORD or --password-file)\n");
  return 64;
}

// one file: up as NAME.part, renamed, size checked; then the local copy goes. 0 uploaded, 1 failed
static int upload_one(const Opts &o, const fs::path &p)
{
  std::string name = p.filename().string();
  std::error_code ec;
  long long local = (long long)fs::file_size(p, ec);
  if(ec) { logf("%s: cannot read its size: %s", name.c_str(), ec.message().c_str()); return 1; }
  gxsFTPClient ftp;
  if(!o.ca.empty()) ftp.SetCAFile(o.ca);
  ftp.SetTimeout(60);
  int port = o.port ? o.port : (o.implicit ? 990 : 21);
  if(ftp.Connect(o.host, port, o.implicit ? gxsMAIL_IMPLICIT_TLS : gxsMAIL_STARTTLS) != 0) { logf("%s: connect to %s:%d: %s", name.c_str(), o.host.c_str(), port, ftp.Error().c_str()); return 1; }
  if(ftp.Login(o.user, o.password) != 0) { logf("%s: login as %s refused: %s", name.c_str(), o.user.c_str(), ftp.LastReply().c_str()); ftp.Close(); return 1; }
  if(!o.remote_dir.empty() && ftp.ChDir(o.remote_dir) != 0) { logf("%s: CWD %s: %s", name.c_str(), o.remote_dir.c_str(), ftp.LastReply().c_str()); ftp.Quit(); return 1; }
  long long sent = 0, remote = -1;
  std::string part = name + ".part";
  int rc = 1;
  if(ftp.Put(p.string(), part, sent) != 0) logf("%s: upload failed: %s", name.c_str(), ftp.Error().empty() ? ftp.LastReply().c_str() : ftp.Error().c_str());
  else if(sent != local) logf("%s: %lld of %lld bytes sent", name.c_str(), sent, local);
  else if(ftp.Rename(part, name) != 0) logf("%s: rename of %s on the server failed: %s", name.c_str(), part.c_str(), ftp.LastReply().c_str());
  else if(ftp.Size(name, remote) != 0 || remote != local) logf("%s: the server holds %lld bytes, the file has %lld: kept", name.c_str(), remote, local);
  else rc = 0;
  if(rc) ftp.Delete(part);                                     // a partial upload is not left behind (if the server allows)
  ftp.Quit();
  if(rc) return 1;
  // the server holds the whole file: the local one goes
  if(!o.done.empty()) {
    fs::path to = fs::path(o.done) / name;
    fs::rename(p, to, ec);
    if(ec) { fs::copy_file(p, to, fs::copy_options::overwrite_existing, ec); if(!ec) fs::remove(p, ec); }
    if(ec) { logf("%s: uploaded, but cannot move it to %s: %s", name.c_str(), o.done.c_str(), ec.message().c_str()); return 1; }
    logf("%s: uploaded (%lld bytes), moved to %s", name.c_str(), local, o.done.c_str());
  }
  else {
    fs::remove(p, ec);
    if(ec) { logf("%s: uploaded, but cannot delete it: %s", name.c_str(), ec.message().c_str()); return 1; }
    logf("%s: uploaded (%lld bytes), deleted", name.c_str(), local);
  }
  return 0;
}

int main(int argc, char **argv)
{
  Opts o;
  std::string password_file;
  for(int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if(i + 1 >= argc) { usage(); exit(64); } return argv[++i]; };
    if(a == "--dir") o.dir = val();
    else if(a == "--host") o.host = val();
    else if(a == "--port") o.port = atoi(val().c_str());
    else if(a == "--user") o.user = val();
    else if(a == "--password-file") password_file = val();
    else if(a == "--password") { fprintf(stderr, "dir_upload: --password is not taken (it would show in the process list): DR_FTP_PASSWORD or --password-file\n"); return 64; }
    else if(a == "--ca") o.ca = val();
    else if(a == "--implicit") o.implicit = true;
    else if(a == "--remote-dir") o.remote_dir = val();
    else if(a == "--pattern") o.pattern = val();
    else if(a == "--settle") o.settle = atoi(val().c_str());
    else if(a == "--interval") o.interval = atoi(val().c_str());
    else if(a == "--done") o.done = val();
    else if(a == "--once") o.once = true;
    else if(a == "--log") o.log = val();
    else return usage();
  }
  if(o.dir.empty() || o.host.empty() || o.user.empty()) return usage();
  if(!read_password(o, password_file)) return 64;
  std::error_code ec;
  if(!fs::is_directory(o.dir, ec)) { fprintf(stderr, "dir_upload: %s is not a folder\n", o.dir.c_str()); return 64; }
  if(!o.done.empty()) fs::create_directories(o.done, ec);
  if(!o.log.empty()) { g_log = fopen(o.log.c_str(), "a"); if(!g_log) { fprintf(stderr, "dir_upload: cannot open the log %s\n", o.log.c_str()); return 64; } }
  signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
  logf("watching %s (%s) for %s@%s, settle %d s%s", o.dir.c_str(), o.pattern.c_str(), o.user.c_str(), o.host.c_str(), o.settle, o.once ? ", one scan" : "");
  // size and time of each file at the last scan: a file is taken when both stayed the same for SETTLE seconds
  struct Seen { long long size; fs::file_time_type mtime; std::chrono::steady_clock::time_point since; };
  std::map<std::string, Seen> seen;
  int failed = 0, uploaded = 0;
  for(;;) {
    auto now = std::chrono::steady_clock::now();
    std::vector<fs::path> ready;
    std::map<std::string, Seen> next;
    for(auto &e : fs::directory_iterator(o.dir, ec)) {
      if(!e.is_regular_file(ec)) continue;
      std::string n = e.path().filename().string();
      if(n.size() > 5 && n.compare(n.size() - 5, 5, ".part") == 0) continue;
      if(!glob_match(o.pattern, n)) continue;
      long long sz = (long long)e.file_size(ec); auto mt = e.last_write_time(ec);
      auto it = seen.find(n);
      Seen s{sz, mt, now};
      if(it != seen.end() && it->second.size == sz && it->second.mtime == mt) s.since = it->second.since;
      next[n] = s;
      // the file's age by its time stamp counts too, so --once takes an old file at the first scan
      auto age = fs::file_time_type::clock::now() - mt;
      bool still = std::chrono::duration_cast<std::chrono::seconds>(now - s.since).count() >= o.settle ||
		   std::chrono::duration_cast<std::chrono::seconds>(age).count() >= o.settle;
      if(still) ready.push_back(e.path());
    }
    seen.swap(next);
    for(auto &p : ready) {
      if(g_stop) break;
      if(upload_one(o, p) == 0) { uploaded++; seen.erase(p.filename().string()); } else failed++;
    }
    if(o.once || g_stop) break;
    for(int s = 0; s < o.interval * 10 && !g_stop; s++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if(g_stop) break;
  }
  logf("%d file(s) uploaded, %d failed", uploaded, failed);
  for(auto &c : o.password) c = 0;
  if(g_log) fclose(g_log);
  return failed ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
