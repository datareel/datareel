// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

6.3 tests for the DataReel 7.3.1 security sweep fixes to the support
code (gxConfig, LogFile, the database debug manager). Run with no
arguments for every test, or name the tests to run:

  config      gxConfig: long lines, value lookups, number overflow,
              whole name match, parameter ID strings of more than one
              character, refused names, file rewritten by rename.
  logfile     LogFile: no write through a symbolic link planted at the
              log file name, line breaks in log text escaped.
  dbug        gxDatabaseDebugManager: damaged block lengths, Rebuild()
              onto its own input.
  thread      gxThread: joining a thread pool whose threads could not be
              created (UNIX: forced with RLIMIT_NPROC in a child process).
  threadmsg   gxThread_t: operator!= on POSIX (it was inverted), type
              and priority class messages for values at the end of their
              tables (read one past the table) (11/14/2026).
  systime     SysTimeUnit::c_str_*() (the terminator was written past
              the buffer), MakeFileModTime()/MakeGMTDateTime() of a time
              localtime()/gmtime() cannot convert (null pointer), and a
              SysTime used many times (11/14/2026).

Returns 0 if every check passes; failed checks print FAIL lines.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gxconfig.h"
#include "gxstring.h"
#include "logfile.h"
#include "dbugmgr.h"
#include "gxthread.h"
#include "gthreadt.h"
#include "systime.h"
#include <time.h>

#if defined (__UNIX__)
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok) failures++;
}

static int file_has(const char *fname, const char *s)
{
  FILE *fp = fopen(fname, "rb");
  if(!fp) return 0;
  static char buf[65536];
  size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = 0;
  return strstr(buf, s) != 0;
}

static void write_file(const char *fname, const char *s)
{
  FILE *fp = fopen(fname, "wb");
  if(!fp) return;
  fputs(s, fp);
  fclose(fp);
}

static int str_is(const char *a, const char *b)
{
  if(!a || !b) return a == b;
  return strcmp(a, b) == 0;
}

static void test_config()
{
  const char *cfg_name = "support_sec.cfg";

  // A value longer than the old 1023-byte line buffer must not add a
  // setting from its tail
  {
    gxString text("Long = ");
    // The old reader cut the line after 1023 bytes, here right before
    // "Admin=yes"
    for(int i = 0; i < 1016; i++) text += 'a';
    text += "Admin=yes\nShort = 1\n";
    write_file(cfg_name, text.c_str());
    gxConfig cfg(cfg_name);
    check(cfg.Load() == 1, "config: load a file with a long line");
    check(cfg.GetStrValue("Admin") == 0, "config: the tail of a long value is not read as a setting");
    char *v = cfg.GetStrValue("Long");
    check(v && strlen(v) == 1025, "config: a long value is read whole");
    check(cfg.GetIntValue("Short") == 1, "config: the setting after a long line");
    check(cfg.ChangeConfigValue("Short", "2") == 1, "config: change a setting in a file with a long line");
    gxConfig cfg2(cfg_name);
    cfg2.Load();
    v = cfg2.GetStrValue("Long");
    check(v && strlen(v) == 1025 && cfg2.GetStrValue("Admin") == 0 && cfg2.GetIntValue("Short") == 2,
	  "config: a rewrite keeps a long line whole");
  }

  // Lookups only match setting names
  {
    write_file(cfg_name, "a = b\nb = c\nx = y\n");
    gxConfig cfg(cfg_name);
    cfg.Load();
    check(str_is(cfg.GetStrValue("b"), "c"), "config: a value equal to a name does not hide the setting");
    check(cfg.GetStrValue("y") == 0, "config: the last value is not matched as a name (null pointer read)");
    check(cfg.GetStrValue("c") == 0 && cfg.GetIntValue("y", 0) == 0, "config: values are not found as names");
  }

  // Numbers that do not fit read as 0
  {
    write_file(cfg_name, "Big = 99999999999999999999\nNeg = -99999999999999999999\n"
	       "IntBig = 4294967296\nOk = 42abc\nMinus = -7\n");
    gxConfig cfg(cfg_name);
    cfg.Load();
    check(cfg.GetIntValue("Big") == 0 && cfg.GetIntValue("Neg") == 0, "config: GetIntValue() out of range reads as 0");
    check(cfg.GetLongValue("Big") == 0 && cfg.GetLongValue("Neg") == 0, "config: GetLongValue() out of range reads as 0");
    check(cfg.GetIntValue("IntBig") == 0, "config: GetIntValue() above INT_MAX reads as 0");
    check(cfg.GetIntValue("Ok") == 42 && cfg.GetIntValue("Minus") == -7 && cfg.GetLongValue("Minus") == -7,
	  "config: numbers in range read as before");
  }

  // The whole name must come before the parameter ID string
  {
    write_file(cfg_name, "Port x = 5\nPortal = 6\n");
    {
      gxConfig cfg(cfg_name);
      check(cfg.ChangeConfigValue("Port", "7") == 1, "config: Port added");
    }
    check(file_has(cfg_name, "Port x = 5\n") && file_has(cfg_name, "Portal = 6\n") &&
	  file_has(cfg_name, "Port=7"), "config: \"Port x\" and \"Portal\" are not changed for \"Port\"");
  }

  // A parameter ID string of more than one character
  {
    write_file(cfg_name, "Speed := 1\nSpeed2:=9\n");
    {
      gxConfig cfg(cfg_name, ":=");
      check(cfg.ChangeConfigValue("Speed", "2") == 1, "config: change with a := parameter ID");
    }
    gxConfig cfg(cfg_name, ":=");
    cfg.Load();
    check(str_is(cfg.GetStrValue("Speed"), "2") && cfg.GetIntValue("Speed2") == 9,
	  "config: the new value goes after the whole := ID");
    check(cfg.GetCommentChar() == '#', "config: the parameter ID constructor sets the comment character");
  }

  // Names that would not read back as the same setting
  {
    write_file(cfg_name, "A = 1\n");
    gxConfig cfg(cfg_name);
    check(cfg.WriteConfigLine("x=y", "1") == 0 && cfg.WriteConfigLine("", "1") == 0,
	  "config: WriteConfigLine() refuses an empty name and a name holding the ID");
    check(cfg.ChangeConfigValue("Admin=yes #", "1") == 0, "config: ChangeConfigValue() refuses a name holding the ID");
    check(!file_has(cfg_name, "x=y") && !file_has(cfg_name, "Admin"), "config: nothing was written");
  }

#if defined (__UNIX__)
  // The file is replaced by rename: same mode, a symbolic link is kept
  {
    write_file(cfg_name, "A = 1\nB = 2\n");
    chmod(cfg_name, 0640);
    struct stat st1, st2;
    stat(cfg_name, &st1);
    {
      gxConfig cfg(cfg_name);
      check(cfg.ChangeConfigValue("A", "3") == 1, "config: change for the rewrite test");
    }
    stat(cfg_name, &st2);
    check((st2.st_mode & 07777) == 0640, "config: the rewritten file keeps its mode");
    check(st1.st_ino != st2.st_ino, "config: the file was replaced by rename, not truncated in place");
    check(file_has(cfg_name, "A=3\nB = 2\n"), "config: rewritten contents");

    const char *link_name = "support_sec_link.cfg";
    remove(link_name);
    if(symlink(cfg_name, link_name) == 0) {
      {
	gxConfig cfg(link_name);
	check(cfg.ChangeConfigValue("B", "4") == 1, "config: change through a symbolic link");
      }
      struct stat lst;
      check(lstat(link_name, &lst) == 0 && S_ISLNK(lst.st_mode) && file_has(cfg_name, "B=4"),
	    "config: a symbolic link to the config file is kept, its target changed");
      remove(link_name);
    }
  }
#endif
  remove(cfg_name);
}

static void test_logfile()
{
#if defined (__UNIX__)
  const char *target = "support_sec_target.txt";
  const char *log_name = "support_sec.log";
  const char *gone = "support_sec_gone.txt";
  struct stat st;

  // A new log file is owner-only, an existing one keeps its mode and is
  // appended to
  remove(log_name);
  {
    LogFile log;
    check(log.Open(log_name) == 0, "logfile: open a new log file");
    log << "first" << "\n";
    log.Close();
    check(stat(log_name, &st) == 0 && (st.st_mode & 07777) == 0600, "logfile: a new log file is mode 0600");
    chmod(log_name, 0644);
    check(log.Open(log_name) == 0, "logfile: open an existing log file");
    log << "second" << "\n";
    log.Close();
    check(file_has(log_name, "first\nsecond\n") && stat(log_name, &st) == 0 &&
	  (st.st_mode & 07777) == 0644, "logfile: an existing log file is appended to and keeps its mode");
    check(log.OverWrite(log_name) == 0, "logfile: OverWrite() an existing log file");
    log << "third" << "\n";
    log.Close();
    check(!file_has(log_name, "first") && file_has(log_name, "third"), "logfile: OverWrite() truncates");
  }
  remove(log_name);

  // A symbolic link planted at the log file name is not followed
  write_file(target, "precious\n");
  if(symlink(target, log_name) == 0) {
    LogFile log;
    int rv = log.Open(log_name);
    if(rv == 0) { log << "planted" << "\n"; log.Close(); }
    check(rv != 0, "logfile: Open() refuses a symbolic link");
    check(!file_has(target, "planted"), "logfile: nothing written through the link");
    rv = log.OverWrite(log_name);
    if(rv == 0) log.Close();
    check(rv != 0 && file_has(target, "precious"), "logfile: OverWrite() does not truncate through a link");
    remove(log_name);
  }
  remove(target);
  remove(gone);
  if(symlink(gone, log_name) == 0) {
    LogFile log;
    int rv = log.Open(log_name);
    if(rv == 0) log.Close();
    check(rv != 0 && access(gone, F_OK) != 0, "logfile: a dangling link does not create its target");
    remove(log_name);
  }
  remove(gone);

  // Character devices still work
  {
    LogFile log;
    int rv = log.Open("/dev/null");
    check(rv == 0, "logfile: /dev/null can be used as a log file");
    if(rv == 0) { log << "x" << "\n"; log.Close(); }
  }
#else
  printf("SKIP logfile: symbolic link tests are UNIX only\n");
#endif
}

static const int DBG_OBJECTS = 50;

static int dbg_make(const char *fname, FAU_t *objects)
// A rev 'C' file of DBG_OBJECTS 100-byte objects
{
  gxDatabase f;
  if(f.Create(fname, (FAU_t)0, 'C') != gxDBASE_NO_ERROR) return 0;
  char buf[100];
  for(int i = 0; i < DBG_OBJECTS; i++) {
    memset(buf, 'a' + (i % 26), sizeof(buf));
    FAU_t a = f.Alloc(sizeof(buf));
    if(!a || f.Write(buf, sizeof(buf), a) != gxDBASE_NO_ERROR) return 0;
    f.WriteObjectChecksum(a);
    objects[i] = a;
  }
  return f.Close() == gxDBASE_NO_ERROR;
}

static int dbg_set_length(const char *fname, FAU_t object, gxUINT32 len)
{
  gxDatabase f;
  if(f.Open(fname) != gxDBASE_NO_ERROR) return 0;
  gxBlockHeader blk;
  FAU_t a = object - f.BlockHeaderSize();
  if(f.Read(&blk, sizeof(blk), a) != gxDBASE_NO_ERROR) return 0;
  blk.block_length = len;
  return f.Write(&blk, sizeof(blk), a) == gxDBASE_NO_ERROR;
}

static FAU_t dbg_ref_search(gxDatabaseDebugManager &f, FAU_t addr)
// The old byte by byte BlockSearch()
{
  FAU_t eof = f.FileSize(f.DatabaseName());
  if(addr < (FAU_t)0) addr = (FAU_t)0;
  gxBlockHeader blk;
  for(; (FAU_t)(addr + sizeof(blk)) < eof; addr++) {
    if(f.Read(&blk, sizeof(blk), addr) != gxDBASE_NO_ERROR) return (FAU_t)0;
    if(f.TestBlockHeader(blk)) return addr;
  }
  return (FAU_t)0;
}

static long dbg_file_size(const char *fname)
{
  FILE *fp = fopen(fname, "rb");
  if(!fp) return -1;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fclose(fp);
  return n;
}

static void test_dbug()
{
  const char *fname = "support_sec.gxd";
  const char *out = "support_sec_out.gxd";
  FAU_t objects[DBG_OBJECTS];
  check(dbg_make(fname, objects), "dbug: create the test file");

  // BlockSearch() finds the same blocks as the old byte by byte search
  {
    gxDatabaseDebugManager f;
    f.BlindOpen(fname);
    FAU_t eof = f.FileSize(fname);
    int same = 1;
    for(FAU_t a = (FAU_t)-3; a < eof + 3; a += 5) {
      if(f.BlockSearch(a) != dbg_ref_search(f, a)) {
	same = 0;
	break;
      }
    }
    check(same, "dbug: BlockSearch() gives the same results as before");
  }

  // Rebuild() refuses to write onto the file being rebuilt
  {
    long before = dbg_file_size(fname);
    gxDatabaseDebugManager f;
    f.BlindOpen(fname);
    FAU_t n = 0;
    int rv = f.Rebuild(fname, &n, gxDatabase::gxVersion, 'C', (FAU_t)0, 1);
    check(rv == 0 && dbg_file_size(fname) == before, "dbug: Rebuild() onto its own input is refused");
    gxString other;
    other << "./" << fname;
    rv = f.Rebuild(other.c_str(), &n, gxDatabase::gxVersion, 'C', (FAU_t)0, 1);
    check(rv == 0 && dbg_file_size(fname) == before, "dbug: Rebuild() onto another spelling of the input is refused");
#if defined (__UNIX__)
    const char *link_name = "support_sec_link.gxd";
    remove(link_name);
    if(symlink(fname, link_name) == 0) {
      rv = f.Rebuild(link_name, &n, gxDatabase::gxVersion, 'C', (FAU_t)0, 1);
      check(rv == 0 && dbg_file_size(fname) == before, "dbug: Rebuild() onto a symbolic link to the input is refused");
      remove(link_name);
    }
    if(link(fname, link_name) == 0) {
      rv = f.Rebuild(link_name, &n, gxDatabase::gxVersion, 'C', (FAU_t)0, 1);
      check(rv == 0 && dbg_file_size(fname) == before, "dbug: Rebuild() onto a hard link to the input is refused");
      remove(link_name);
    }
#endif
    remove(out);
    rv = f.Rebuild(out, &n, gxDatabase::gxVersion, 'C', (FAU_t)0, 1);
    check(rv == 1 && n == (FAU_t)DBG_OBJECTS, "dbug: Rebuild() to a new file copies every block");
    remove(out);
  }

  // Damaged block lengths: short, zero and huge
  {
    dbg_set_length(fname, objects[10], 1);
    dbg_set_length(fname, objects[20], 0);
    dbg_set_length(fname, objects[30], (gxUINT32)0xfffffff0UL);
    gxDatabaseDebugManager f;
    f.BlindOpen(fname);
    FAU_t n = 0;
    remove(out);
    f.Rebuild(out, &n, gxDatabase::gxVersion, 'C', (FAU_t)0, 1);
    check(n == (FAU_t)(DBG_OBJECTS - 3), "dbug: Rebuild() skips the three damaged blocks and keeps the rest");
    remove(out);
  }

  // A large file with no blocks is searched in reasonable time
  {
    gxDatabaseDebugManager f;
    f.BlindOpen(fname);
    FAU_t start_addr = f.FileSize(fname);
    f.Close();
    FILE *fp = fopen(fname, "ab");
    if(fp) {
      static char zeros[65536];
      for(int i = 0; i < 128; i++) fwrite(zeros, 1, sizeof(zeros), fp); // 8 MB
      fclose(fp);
    }
    f.BlindOpen(fname);
    clock_t t0 = clock();
    FAU_t a = f.BlockSearch(start_addr);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("     dbug: searched 8 MB with no blocks in %.2f seconds\n", secs);
    check(a == (FAU_t)0 && secs < 2.0, "dbug: BlockSearch() of 8 MB with no blocks takes under 2 seconds (4.5 before on a fast host)");
  }
  remove(fname);
  remove(out);
}


class SupportSecThread : public gxThread
{
public:
  void *ThreadEntryRoutine(gxThread_t *) { return 0; }
};

static void test_thread()
{
#if defined (__UNIX__)
  // A child process with no room for new threads creates a pool and
  // joins it. The never-started threads used to be passed to
  // pthread_join() with an uninitialized thread ID.
  fflush(stdout);
  pid_t pid = fork();
  if(pid == 0) {
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = 1;
    if(getuid() == 0 || setrlimit(RLIMIT_NPROC, &rl) != 0) _exit(3);
    alarm(20);
    SupportSecThread t;
    thrPool *pool = t.CreateThreadPool(4);
    if(!pool) _exit(4);
    int created = 0;
    thrPoolNode *n = pool->GetHead();
    while(n) {
      if(n->GetThreadPtr()->GetThreadError() != gxTHREAD_CREATE_ERROR) created++;
      n = n->GetNext();
    }
    if(created) _exit(3); // The limit did not stop thread creation
    int rv = t.JoinThread(pool);
    t.DestroyThreadPool(pool);
    _exit(rv != 0 ? 0 : 5);
  }
  int st = 0;
  waitpid(pid, &st, 0);
  if(WIFEXITED(st) && WEXITSTATUS(st) == 3) {
    printf("SKIP thread: cannot make thread creation fail (root, or the limit is not enforced)\n");
  }
  else {
    check(WIFEXITED(st) && WEXITSTATUS(st) == 0,
	  "thread: joining a pool whose threads could not be created returns an error");
  }

  // A normal pool still starts, joins and is destroyed
  {
    SupportSecThread t;
    thrPool *pool = t.CreateThreadPool(4);
    int rv = t.JoinThread(pool);
    check(pool && rv == 0 && t.DestroyThreadPool(pool) == 0, "thread: a pool of 4 threads runs and joins");
  }
#else
  printf("SKIP thread: UNIX only (fork)\n");
#endif
}

static void test_threadmsg()
// 11/14/2026
{
#if defined (__POSIX__)
  {
    SupportSecThread t;
    gxThread_t *th = t.CreateThread();
    int ok = (th != 0);
    if(th) t.JoinThread(th);
    check(ok && (*th == *th) && !(*th != *th),
	  "threadmsg: operator!= of the same thread ID is false");
    if(th) t.DestroyThread(th);
  }
#endif
  gxThread_t a, b;
  a.SetThreadType((gxThreadType)2);
  a.SetThreadPriorityClass((gxThreadPriorityClass)4);
  b.SetThreadPriorityClass(gxTHREAD_PRIORITY_CLASS_INVALID);
  const char *m2 = a.ThreadTypeMessage();
  const char *m4 = a.ThreadPriorityClassMessage();
  check(m2 && strcmp(m2, "Thread type: JOINABLE") == 0 &&
	m4 && strcmp(m4, b.ThreadPriorityClassMessage()) == 0,
	"threadmsg: values one past the message tables give the default message");
}

static void test_systime()
// 11/14/2026
{
  SysTimeUnit u;
  u.hours = 4294967295U; u.minutes = 7; u.seconds = 0; u.milliseconds = 999;
  char *h = u.c_str_Hours(), *m = u.c_str_Minutes();
  char *sec = u.c_str_Seconds(), *ms = u.c_str_Milliseconds();
  check(strcmp(h, "4294967295") == 0 && strcmp(m, "7") == 0 &&
	strcmp(sec, "0") == 0 && strcmp(ms, "999") == 0,
	"systime: c_str_*() strings (run under ASan for the overflow)");
  delete[] h; delete[] m; delete[] sec; delete[] ms;

  SysTime t;
  time_t bad = (time_t)0x7fffffffffffffffLL;
  if(sizeof(time_t) < 8) bad = (time_t)-1;
  const char *f = t.MakeFileModTime(bad);
  const char *g = t.MakeGMTDateTime(bad);
  check(f != 0 && g != 0, "systime: a time localtime()/gmtime() cannot convert does not crash");
  const char *ok = t.MakeGMTDateTime((time_t)0);
  check(ok && strstr(ok, "1970") != 0, "systime: MakeGMTDateTime(0) as before");
  for(int i = 0; i < 1000; i++) t.GetStrTime(0);
  check(1, "systime: GetStrTime() 1000 times (no buffer per call, check under ASan/LSan)");
}

int main(int argc, char **argv)
{
#if defined (__UNIX__)
  umask(022);
#endif
  int all = (argc < 2);
  const char *names[] = { "config", "logfile", "dbug", "thread", "threadmsg",
			  "systime", 0 };
  void (*tests[])() = { test_config, test_logfile, test_dbug, test_thread,
			test_threadmsg, test_systime };
  for(int i = 1; i < argc; i++) {
    int known = 0;
    for(int t = 0; names[t]; t++) if(strcmp(argv[i], names[t]) == 0) known = 1;
    if(!known) {
      printf("Unknown test %s\n", argv[i]);
      return 2;
    }
  }
  for(int t = 0; names[t]; t++) {
    int run = all;
    for(int i = 1; i < argc; i++) if(strcmp(argv[i], names[t]) == 0) run = 1;
    if(run) tests[t]();
  }

  if(failures) printf("%d check(s) FAILED\n", failures);
  else printf("All checks passed\n");
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
