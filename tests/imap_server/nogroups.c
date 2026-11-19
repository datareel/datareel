/* DataReel 7.4 tests (tests/imap_container_check.sh): the IMAP test server container only, never anything else.
   From the DataReel 6.4 IMAP test server (11/16/2026).
   Rootless podman with no subordinate ids maps one id (root in the container), so Dovecot cannot switch users,
   groups or owners. Preloaded into Dovecot (LD_PRELOAD and import_environment), this makes those calls succeed
   without changing anything, and tells imap-login and imap (which refuse to run as root) that they are uid 1000.
   Built with: gcc -shared -fPIC -nostdlib -O2 -o nogroups.so nogroups.c */
#include <stddef.h>
#include <sys/types.h>
extern char *program_invocation_short_name;
static int same(const char *a, const char *b) {
  while(*a && *a == *b) { a++; b++; }
  return *a == 0 && *b == 0;
}
/* The processes that run as the login user or the mail user (1000) */
static int is_login(void) {
  const char *a = program_invocation_short_name;
  if(!a) return 0;
  return same(a, "imap-login") || same(a, "imap");
}
int setgroups(size_t n, const gid_t *g) { (void)n; (void)g; return 0; }
int initgroups(const char *u, gid_t g) { (void)u; (void)g; return 0; }
extern int *__errno_location(void);
/* imap-login checks that it cannot get root back */
int setuid(uid_t u) { if(u == 0 && is_login()) { *__errno_location() = 1; return -1; } return 0; }
int setgid(gid_t g) { if(g == 0 && is_login()) { *__errno_location() = 1; return -1; } return 0; }
int seteuid(uid_t u) { if(u == 0 && is_login()) { *__errno_location() = 1; return -1; } return 0; }
int setegid(gid_t g) { (void)g; return 0; }
int setreuid(uid_t a, uid_t b) { (void)a; (void)b; return 0; }
int setregid(gid_t a, gid_t b) { (void)a; (void)b; return 0; }
int setresuid(uid_t a, uid_t b, uid_t c) { (void)a; (void)b; (void)c; return 0; }
int setresgid(gid_t a, gid_t b, gid_t c) { (void)a; (void)b; (void)c; return 0; }
uid_t getuid(void) { return is_login() ? 1000 : 0; }
uid_t geteuid(void) { return is_login() ? 1000 : 0; }
int chown(const char *p, uid_t u, gid_t g) { (void)p; (void)u; (void)g; return 0; }
int fchown(int fd, uid_t u, gid_t g) { (void)fd; (void)u; (void)g; return 0; }
int lchown(const char *p, uid_t u, gid_t g) { (void)p; (void)u; (void)g; return 0; }
int fchownat(int d, const char *p, uid_t u, gid_t g, int f) { (void)d; (void)p; (void)u; (void)g; (void)f; return 0; }
gid_t getgid(void) { return is_login() ? 1000 : 0; }
gid_t getegid(void) { return is_login() ? 1000 : 0; }
int getresuid(uid_t *r, uid_t *e, uid_t *s) { uid_t v = is_login() ? 1000 : 0; if(r) *r = v; if(e) *e = v; if(s) *s = v; return 0; }
int getresgid(gid_t *r, gid_t *e, gid_t *s) { gid_t v = is_login() ? 1000 : 0; if(r) *r = v; if(e) *e = v; if(s) *s = v; return 0; }
/* The login folder must look owned by group imaptest (1000) */
#include <sys/stat.h>
extern void *dlsym(void *, const char *);
static int is_login_dir(const char *p) {
  /* /run/dovecot/login, /run/dovecot/token-login, ... */
  const char *w = "/run/dovecot/";
  while(*p && *w && *p == *w) { p++; w++; }
  if(*w) return 0;
  for(; *p; p++) {
    if(p[0] == 'l' && p[1] == 'o' && p[2] == 'g' && p[3] == 'i' && p[4] == 'n') return 1;
  }
  return 0;
}
int stat(const char *p, struct stat *st) {
  int (*real)(const char *, struct stat *) = (int (*)(const char *, struct stat *))dlsym((void *)-1, "stat");
  int rv = real(p, st);
  if(rv == 0 && is_login_dir(p)) { st->st_gid = 1000; }
  return rv;
}
int lstat(const char *p, struct stat *st) {
  int (*real)(const char *, struct stat *) = (int (*)(const char *, struct stat *))dlsym((void *)-1, "lstat");
  int rv = real(p, st);
  if(rv == 0 && is_login_dir(p)) { st->st_gid = 1000; }
  return rv;
}
