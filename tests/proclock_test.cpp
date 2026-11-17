// proclock_test.cpp - 11/17/2026 (7.4): gxProcessLock (gxproclock.h): one holder at a time across processes, the
// holder's id, a holder killed with SIGKILL frees the lock (no stale lock), the lock is not inherited across exec, and
// Release() then Acquire() in the same process. POSIX (fork). Usage: proclock_test DIR. Exit 0 when all pass.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string>
#include <chrono>
#include <thread>
#include "gxproclock.h"
using namespace datareel;

static int pass = 0, fail = 0;
static void check(bool ok, const std::string &what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); fflush(stdout); (ok ? pass : fail)++; }

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : ".";
  std::string f = dir + "/proclock_test.lock";
  remove(f.c_str());
  // 1 a child holds it: this process cannot, and learns the child's id
  int p2c[2]; if(pipe(p2c) != 0) return 2;
  pid_t k = fork();
  if(k == 0) {
    gxProcessLock l; char ok = l.Acquire(f) ? 'y' : 'n';
    (void)!write(p2c[1], &ok, 1);
    for(;;) pause();
  }
  char r = 0; (void)!read(p2c[0], &r, 1);
  gxProcessLock me;
  bool got = me.Acquire(f);
  long long pid = 0; bool locked = gxProcessLock::IsLocked(f, &pid);
  check(r == 'y' && !got && me.HolderPid() == (long long)k && locked && pid == (long long)k,
	"a child holds the lock: Acquire() fails here, HolderPid() and IsLocked() name the child " + std::to_string((long long)k) + " [" + me.Error() + "]");
  // 2 the holder killed (SIGKILL): the lock is free at once, no stale lock
  kill(k, SIGKILL); waitpid(k, 0, 0);
  got = me.Acquire(f);
  check(got && me.Held() && me.HolderPid() == (long long)getpid(), "the holder SIGKILLed: the lock is taken at once (no stale lock)");
  // 3 not inherited across exec: a child that execs while this process holds it cannot take it, and does not keep
  // it after this process releases it
  pid_t c = fork();
  if(c == 0) {
    execlp("sh", "sh", "-c", "sleep 2", (char *)0); _exit(127);
  }
  me.Release();
  bool again = me.Acquire(f);
  check(again, "Release() and Acquire() again while an exec'd child (sh) runs: the child did not inherit the lock");
  kill(c, SIGKILL); waitpid(c, 0, 0);
  // 4 the same process: a second object on the same file is refused too (flock is per open file)
  gxProcessLock second;
  check(!second.Acquire(f) && second.HolderPid() == (long long)getpid(), "a second gxProcessLock on the file in the same process is refused");
  me.Release();
  check(!gxProcessLock::IsLocked(f) && second.Acquire(f), "after Release(): free (IsLocked() false), and the second object takes it");
  second.Release();
  // 5 a file that cannot be made
  gxProcessLock bad;
  check(!bad.Acquire(dir + "/no/such/folder/x.lock") && bad.Error().find("cannot open") != std::string::npos, "a lock file in a missing folder: Error() says why");
  remove(f.c_str());
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
