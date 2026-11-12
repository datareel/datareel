/* ------------------------------- */
/* -------- Start of File -------- */
/* ------------------------------- */
/* ----------------------------------------------------------- */
/* C Source Code File Name: powerloss_shim.c                    */
/* Produced By: DataReel Software Development Team              */
/* File Creation Date: 11/09/2026                               */
/* Copyright (c) 2001-2026 DataReel Software Development        */
/* ----------------------------------------------------------- */
/*
TEST ONLY. A power loss simulator for the replication tests
(test_gxrepl.py, test_power_loss_*). Never link it into a program.

Loaded with LD_PRELOAD into the writer, the gxrepl server and the gxrepl
client, it keeps a "durable image" of the folders it watches: what a
machine would still have after a power cut.

  - A file's data is durable only once it was fsynced (fsync or
    fdatasync on the file): each such call saves the inode's current
    bytes to PL_DURABLE/inodes/<dev>_<ino>.
  - A folder entry (a create, a rename, an unlink) is durable only once
    the folder was fsynced: each such call saves the folder's entries
    (type, inode, name) to PL_DURABLE/dirs/<dev>_<ino>. Folders are kept
    by inode too, so a renamed folder keeps its entries.

Everything else (writes, renames, page cache) is lost in the image. The
test kills every process at a random moment and rebuilds the folders
from the image only, then checks that each database is a whole commit
and that replication catches up with no gap.

  PL_ROOTS    folders to watch, ':' separated (absolute)
  PL_DURABLE  where the image is kept
  PL_TRACE    optional: a file that gets one line per call seen

Saves are a temporary file and a rename, made under a lock on
PL_DURABLE/.lock, so several processes and threads can share one image.
A save copies the bytes right after the real fsync returns. The test
(not this file) takes the starting state and rebuilds the folders.
*/
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int (*real_fsync)(int) = 0;
static int (*real_fdatasync)(int) = 0;

static int watched(const char *path)
{
  const char *roots = getenv("PL_ROOTS");
  if(!roots || !*roots) return 0;
  const char *p = roots;
  while(*p) {
    const char *e = strchr(p, ':');
    size_t n = e ? (size_t)(e - p) : strlen(p);
    if(n && strncmp(path, p, n) == 0 && (path[n] == '/' || path[n] == 0)) return 1;
    if(!e) break;
    p = e + 1;
  }
  return 0;
}

/* Write a file atomically: a temporary name, then one rename */
static void save(const char *dir, const char *name, const char *data, size_t n, int src_fd)
{
  char tmp[PATH_MAX], dst[PATH_MAX];
  snprintf(dst, sizeof(dst), "%s/%s", dir, name);
  snprintf(tmp, sizeof(tmp), "%s/.tmp.%d.%ld.%s", dir, (int)getpid(), (long)syscall(SYS_gettid), name);
  int o = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if(o < 0) return;
  if(data) {
    if(write(o, data, n) != (ssize_t)n) { close(o); unlink(tmp); return; }
  }
  else {
    char b[65536];
    off_t off = 0;
    ssize_t r;
    while((r = pread(src_fd, b, sizeof(b), off)) > 0) {
      if(write(o, b, (size_t)r) != r) { close(o); unlink(tmp); return; }
      off += r;
    }
  }
  close(o);
  rename(tmp, dst);
}

static void trace(const char *what, int fd, int r)
{
  const char *tf = getenv("PL_TRACE");
  if(!tf) return;
  int saved = errno;
  char link[64], path[PATH_MAX], line[PATH_MAX + 96];
  snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
  ssize_t n = readlink(link, path, sizeof(path) - 1);
  path[n > 0 ? n : 0] = 0;
  int k = snprintf(line, sizeof(line), "%d %s %d %s\n", (int)getpid(), what, r, path);
  int o = open(tf, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
  if(o >= 0) { if(write(o, line, (size_t)k) < 0) { } close(o); }
  errno = saved;
}

static void record(int fd)
{
  const char *img = getenv("PL_DURABLE");
  if(!img) return;
  int saved = errno;
  char link[64], path[PATH_MAX];
  snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
  ssize_t n = readlink(link, path, sizeof(path) - 1);
  if(n <= 0) { errno = saved; return; }
  path[n] = 0;
  if(!watched(path)) { errno = saved; return; }
  struct stat st;
  if(fstat(fd, &st) != 0) { errno = saved; return; }
  char sub[PATH_MAX], name[64], lockp[PATH_MAX];
  snprintf(name, sizeof(name), "%llu_%llu", (unsigned long long)st.st_dev, (unsigned long long)st.st_ino);
  snprintf(lockp, sizeof(lockp), "%s/.lock", img);
  int lk = open(lockp, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if(lk >= 0) flock(lk, LOCK_EX);
  if(S_ISREG(st.st_mode)) {
    snprintf(sub, sizeof(sub), "%s/inodes", img);
    mkdir(sub, 0700);
    int rfd = open(link, O_RDONLY | O_CLOEXEC);
    if(rfd >= 0) { save(sub, name, 0, 0, rfd); close(rfd); }
  }
  else if(S_ISDIR(st.st_mode)) {
    DIR *d = opendir(path);
    if(!d) { if(lk >= 0) close(lk); errno = saved; return; }
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    struct dirent *e;
    while(buf && (e = readdir(d)) != 0) {
      if(!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
      struct stat es;
      if(fstatat(dirfd(d), e->d_name, &es, AT_SYMLINK_NOFOLLOW) != 0) continue;
      char t = S_ISDIR(es.st_mode) ? 'd' : S_ISREG(es.st_mode) ? 'f' : 'o';
      char line[PATH_MAX + 64];
      int k = snprintf(line, sizeof(line), "%c\t%llu_%llu\t%s\n", t,
		       (unsigned long long)es.st_dev, (unsigned long long)es.st_ino, e->d_name);
      if(len + (size_t)k + 1 > cap) { cap = (cap + k) * 2; char *nb = realloc(buf, cap); if(!nb) break; buf = nb; }
      memcpy(buf + len, line, (size_t)k); len += (size_t)k;
    }
    closedir(d);
    snprintf(sub, sizeof(sub), "%s/dirs", img);
    mkdir(sub, 0700);
    if(buf) { save(sub, name, buf, len, -1); free(buf); }
  }
  if(lk >= 0) close(lk);  /* Closing it drops the lock */
  errno = saved;
}

int fsync(int fd)
{
  if(!real_fsync) real_fsync = (int (*)(int))dlsym(RTLD_NEXT, "fsync");
  if(!real_fsync) { errno = ENOSYS; return -1; }
  int r = real_fsync(fd);
  trace("fsync", fd, r);
  if(r == 0) record(fd);
  return r;
}

int fdatasync(int fd)
{
  if(!real_fdatasync) real_fdatasync = (int (*)(int))dlsym(RTLD_NEXT, "fdatasync");
  if(!real_fdatasync) { errno = ENOSYS; return -1; }
  int r = real_fdatasync(fd);
  trace("fdatasync", fd, r);
  if(r == 0) record(fd);
  return r;
}
/* ----------------------------------------------------------- */
/* ------------------------------- */
/* --------- End of File --------- */
/* ------------------------------- */
