/* eio_shim.c - 11/16/2026: TEST ONLY. Never link it into a program.
   Loaded with LD_PRELOAD into an objrepl process, it makes I/O calls on matching files fail with EIO:
     EIO_MATCH  a path substring (e.g. "/n2/"): only files whose path contains it are counted
     EIO_CALL   pwrite (default) or fsync (fsync and fdatasync)
     EIO_AFTER  the number of matching calls that succeed first (default 50)
     EIO_MODE   once (default: one failure, then all succeed) or always (every call after EIO_AFTER fails)
   The library writes with pwrite() and syncs with fsync()/fdatasync() (gxdfp64.cpp, gxdjrnl.cpp). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

static long counted = 0;
static int failed = 0;

static int matches(int fd)
{
  const char *m = getenv("EIO_MATCH");
  char link[64], p[4096];
  ssize_t n;
  if(!m || !*m) return 0;
  snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
  n = readlink(link, p, sizeof(p) - 1);
  if(n <= 0) return 0;
  p[n] = 0;
  return strstr(p, m) != 0;
}
static int should_fail(int fd, const char *call)
{
  const char *c = getenv("EIO_CALL"), *mode = getenv("EIO_MODE"), *a = getenv("EIO_AFTER");
  long after = a ? atol(a) : 50;
  if(strcmp(c ? c : "pwrite", call) != 0 || !matches(fd)) return 0;
  if(++counted <= after) return 0;
  if(mode && strcmp(mode, "always") == 0) return 1;
  if(failed) return 0;
  failed = 1;
  return 1;
}
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off)
{
  static ssize_t (*real)(int, const void *, size_t, off_t) = 0;
  if(!real) real = (ssize_t (*)(int, const void *, size_t, off_t))dlsym(RTLD_NEXT, "pwrite");
  if(should_fail(fd, "pwrite")) { fprintf(stderr, "eio_shim: pwrite EIO (call %ld)\n", counted); errno = EIO; return -1; }
  return real(fd, buf, n, off);
}
ssize_t pwrite64(int fd, const void *buf, size_t n, off_t off)
{
  static ssize_t (*real)(int, const void *, size_t, off_t) = 0;
  if(!real) real = (ssize_t (*)(int, const void *, size_t, off_t))dlsym(RTLD_NEXT, "pwrite64");
  if(should_fail(fd, "pwrite")) { fprintf(stderr, "eio_shim: pwrite64 EIO (call %ld)\n", counted); errno = EIO; return -1; }
  return real(fd, buf, n, off);
}
int fsync(int fd)
{
  static int (*real)(int) = 0;
  if(!real) real = (int (*)(int))dlsym(RTLD_NEXT, "fsync");
  if(should_fail(fd, "fsync")) { fprintf(stderr, "eio_shim: fsync EIO (call %ld)\n", counted); errno = EIO; return -1; }
  return real(fd);
}
int fdatasync(int fd)
{
  static int (*real)(int) = 0;
  if(!real) real = (int (*)(int))dlsym(RTLD_NEXT, "fdatasync");
  if(should_fail(fd, "fsync")) { fprintf(stderr, "eio_shim: fdatasync EIO (call %ld)\n", counted); errno = EIO; return -1; }
  return real(fd);
}
