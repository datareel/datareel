// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: GCC
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

6.4.1 checks for the thread classes (POSIX only). Each check runs in
a child process so a crash or a hang (alarm) is reported as a FAIL.

  state     The state is RUNNING as soon as CreateThread() returns. The
            new thread used to set it itself, so the caller could see
            NEW for a thread that was starting.
  cancel    A thread canceled at once after CreateThread() keeps the
            CANCELED state (it was overwritten with RUNNING).
  sleep     gxThread::mSleep(), uSleep() and nSleep() sleep for the
            time asked (they used a condition wait with a mutex left
            locked) and finish the sleep after a signal.
  timedwait gxCondition::ConditionTimedWait() waits for the time asked,
            including a nanosecond part of one second or more.

The clock used for the timed waits (CLOCK_MONOTONIC on Linux) is not
tested here, because that needs the system time to be changed.

Returns 0 if every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxthread.h"
#include "gxmutex.h"
#include "gxcond.h"

#if defined (__POSIX__)
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  fflush(stdout);
  if(!ok) failures++;
}

typedef int (*TestFunc)();

static void run(const char *name, TestFunc f, int max_seconds = 60)
{
  fflush(stdout);
  pid_t pid = fork();
  if(pid == 0) {
    alarm(max_seconds);
    int rv = f();
    fflush(stdout);
    _exit(rv);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if(WIFSIGNALED(status)) {
    char sbuf[256];
    sprintf(sbuf, "%s (killed by signal %d)", name, WTERMSIG(status));
    check(0, sbuf);
    return;
  }
  check(WIFEXITED(status) && WEXITSTATUS(status) == 0, name);
}

static double now_ms()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

class QuickThread : public gxThread
{
private:
  void *ThreadEntryRoutine(gxThread_t *thread) { return 0; }
};

class WaitThread : public gxThread
{
private:
  void *ThreadEntryRoutine(gxThread_t *thread) {
    for(;;) sleep(1); // sleep() is a cancellation point
    return 0;
  }
};

static int test_state()
{
  const int rounds = 2000;
  int seen_new = 0;
  QuickThread t;
  for(int i = 0; i < rounds; i++) {
    gxThread_t *thread = t.CreateThread();
    if(!thread || thread->GetThreadError() != gxTHREAD_NO_ERROR) return 2;
    if(thread->GetThreadState() == gxTHREAD_STATE_NEW) seen_new++;
    t.JoinThread(thread);
    delete thread;
  }
  if(seen_new) {
    printf("  %d of %d threads were NEW after CreateThread() returned\n",
	   seen_new, rounds);
    return 1;
  }
  return 0;
}

static int test_cancel()
{
  const int rounds = 500;
  int lost = 0;
  WaitThread t;
  for(int i = 0; i < rounds; i++) {
    gxThread_t *thread = t.CreateThread();
    if(!thread || thread->GetThreadError() != gxTHREAD_NO_ERROR) return 2;
    if(t.CancelThread(thread) != 0) return 2;
    t.JoinThread(thread);
    if(thread->GetThreadState() != gxTHREAD_STATE_CANCELED) lost++;
    delete thread;
  }
  if(lost) {
    printf("  %d of %d canceled threads did not end CANCELED\n", lost, rounds);
    return 1;
  }
  return 0;
}

static void on_alarm(int) { }

static int test_sleep()
{
  QuickThread t;
  int rv = 0;
  for(int i = 0; i < 200; i++) t.mSleep(1); // Many calls, each its own objects
  double s = now_ms();
  t.mSleep(300);
  double e = now_ms() - s;
  if(e < 300 || e > 1300) { printf("  mSleep(300) took %.0f ms\n", e); rv = 1; }
  s = now_ms();
  t.uSleep(1200000); // 1.2 seconds: more than a second in microseconds
  e = now_ms() - s;
  if(e < 1200 || e > 2200) { printf("  uSleep(1200000) took %.0f ms\n", e); rv = 1; }
  s = now_ms();
  t.nSleep(250000000UL);
  e = now_ms() - s;
  if(e < 250 || e > 1250) { printf("  nSleep(250000000) took %.0f ms\n", e); rv = 1; }

  // A signal in the middle of the sleep: the sleep goes on
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_alarm;
  sigaction(SIGALRM, &sa, 0);
  struct itimerval it;
  memset(&it, 0, sizeof(it));
  it.it_value.tv_usec = 100000;
  setitimer(ITIMER_REAL, &it, 0);
  s = now_ms();
  t.mSleep(500);
  e = now_ms() - s;
  if(e < 500) { printf("  mSleep(500) with a signal took %.0f ms\n", e); rv = 1; }
  return rv;
}

static int test_timedwait()
{
  int rv = 0;
  gxMutex m;
  gxCondition c;
  if(m.MutexLock() != 0) return 2;
  double s = now_ms();
  c.ConditionTimedWait(&m, 1, 0);
  double e = now_ms() - s;
  if(e < 1000 || e > 2000) { printf("  1 s timed wait took %.0f ms\n", e); rv = 1; }
  s = now_ms();
  c.ConditionTimedWait(&m, 0, 1500000000UL);
  e = now_ms() - s;
  if(e < 1500 || e > 2500) { printf("  1.5e9 ns timed wait took %.0f ms\n", e); rv = 1; }
  m.MutexUnlock();
  return rv;
}

int main()
{
  run("state: RUNNING when CreateThread() returns", test_state);
  run("cancel: a thread canceled at once stays CANCELED", test_cancel);
  run("sleep: mSleep(), uSleep() and nSleep() sleep the time asked", test_sleep);
  run("timedwait: ConditionTimedWait() waits the time asked", test_timedwait);
  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

#else // Not POSIX
int main()
{
  printf("SKIP: the thread checks use fork()\n");
  return 0;
}
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
