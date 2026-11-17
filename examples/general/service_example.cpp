// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: service_example.cpp
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

service_example: the frame of a long-running program. One instance at
a time (gxProcessLock on --lock), work every --interval seconds (here:
a heartbeat line in --log), and a clean stop on SIGTERM/SIGINT or
Ctrl+C, a stop file, or the Windows service manager. From dougg's
app_framework and is_file_locked.

  service_example [--name NAME] --lock FILE --log FILE
		  [--stop-file FILE] [--interval S] [--run-for S]
		  [--daemon]                       (POSIX: detach)
  service_example --service ...               (Windows: under the SCM)
  service_example --install ... / --uninstall  (Windows, as admin)

Exit codes: 0 stopped cleanly, 2 another instance holds the lock
(its process id is printed), 1 another error. --run-for: stop after S
seconds (tests). --install registers a demand-start service that runs
"service_example --service" with the other options under the
LocalService account (never auto-start); --uninstall removes it.
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
#include <atomic>
#include <chrono>
#include <thread>
#if defined (_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
#include "gxproclock.h"

using namespace datareel;

static std::atomic<int> g_stop{0};
static void on_signal(int) { g_stop = 1; }

struct Opts {
  std::string name = "DataReelTestSvc", lock, log, stop_file;
  int interval = 1;
  double run_for = 0;
  bool daemon = false, service = false, install = false, uninstall = false;
  std::vector<std::string> pass_on;          // the options a service is installed with
};
static Opts O;

static void logline(const char *fmt, ...)
{
  FILE *f = O.log.empty() ? stderr : fopen(O.log.c_str(), "a");
  if(!f) return;
  char ts[32]; time_t t = time(0); strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
  fprintf(f, "%s %s: ", ts, O.name.c_str());
  va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
  fputc('\n', f);
  if(f != stderr) fclose(f); else fflush(f);
}

// the work: until a stop. 0 stopped cleanly, 2 another instance, 1 an error
static int run_service()
{
  gxProcessLock lk;
  if(!lk.Acquire(O.lock)) {
    long long pid = lk.HolderPid();
    fprintf(stderr, "service_example: %s\n", lk.Error().c_str());
    logline("not started: another instance (process %lld) holds %s", pid, O.lock.c_str());
    return pid ? 2 : 1;
  }
  if(!O.stop_file.empty()) remove(O.stop_file.c_str());       // a stop file from before is spent
  logline("started (process %lld), work every %d s", lk.HolderPid(), O.interval);
  auto t0 = std::chrono::steady_clock::now();
  unsigned long beats = 0;
  auto next = t0;
  while(!g_stop) {
    auto now = std::chrono::steady_clock::now();
    if(O.run_for > 0 && std::chrono::duration<double>(now - t0).count() >= O.run_for) break;
    if(!O.stop_file.empty()) { FILE *f = fopen(O.stop_file.c_str(), "r"); if(f) { fclose(f); remove(O.stop_file.c_str()); logline("stop file seen"); break; } }
    if(now >= next) { logline("heartbeat %lu", ++beats); next = now + std::chrono::seconds(O.interval); }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  logline("stopped after %lu heartbeat(s)", beats);
  lk.Release();
  return 0;
}

#if defined (_WIN32)
// ---------- Windows: the service control manager ----------
static SERVICE_STATUS_HANDLE g_ssh = 0;
static SERVICE_STATUS g_ss;
static BOOL WINAPI on_ctrl(DWORD) { g_stop = 1; return TRUE; }
static void report(DWORD state, DWORD exit_code = NO_ERROR)
{
  g_ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  g_ss.dwCurrentState = state;
  g_ss.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
  g_ss.dwWin32ExitCode = exit_code == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
  g_ss.dwServiceSpecificExitCode = exit_code;
  g_ss.dwWaitHint = state == SERVICE_RUNNING || state == SERVICE_STOPPED ? 0 : 5000;
  SetServiceStatus(g_ssh, &g_ss);
}
static DWORD WINAPI handler(DWORD ctl, DWORD, LPVOID, LPVOID)
{
  if(ctl == SERVICE_CONTROL_STOP || ctl == SERVICE_CONTROL_SHUTDOWN) { report(SERVICE_STOP_PENDING); g_stop = 1; return NO_ERROR; }
  if(ctl == SERVICE_CONTROL_INTERROGATE) return NO_ERROR;
  return ERROR_CALL_NOT_IMPLEMENTED;
}
static void WINAPI service_main(DWORD, LPSTR *)
{
  g_ssh = RegisterServiceCtrlHandlerExA(O.name.c_str(), handler, 0);
  if(!g_ssh) return;
  report(SERVICE_START_PENDING);
  report(SERVICE_RUNNING);
  int rc = run_service();
  report(SERVICE_STOPPED, (DWORD)rc);
}
static std::string quote(const std::string &s) { return s.find_first_of(" \t\"") == std::string::npos ? s : "\"" + s + "\""; }
static int install()
{
  char exe[MAX_PATH]; GetModuleFileNameA(0, exe, MAX_PATH);
  std::string cmd = quote(exe) + " --service";
  for(auto &a : O.pass_on) cmd += " " + quote(a);
  SC_HANDLE scm = OpenSCManagerA(0, 0, SC_MANAGER_CREATE_SERVICE);
  if(!scm) { fprintf(stderr, "service_example: OpenSCManager: error %lu (run as administrator)\n", GetLastError()); return 1; }
  SC_HANDLE s = CreateServiceA(scm, O.name.c_str(), O.name.c_str(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
			       SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, cmd.c_str(), 0, 0, 0, "NT AUTHORITY\\LocalService", 0);
  if(!s) { fprintf(stderr, "service_example: CreateService %s: error %lu\n", O.name.c_str(), GetLastError()); CloseServiceHandle(scm); return 1; }
  printf("service_example: %s installed (demand start, LocalService): %s\n", O.name.c_str(), cmd.c_str());
  CloseServiceHandle(s); CloseServiceHandle(scm);
  return 0;
}
static int uninstall()
{
  SC_HANDLE scm = OpenSCManagerA(0, 0, SC_MANAGER_CONNECT);
  if(!scm) { fprintf(stderr, "service_example: OpenSCManager: error %lu\n", GetLastError()); return 1; }
  SC_HANDLE s = OpenServiceA(scm, O.name.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
  if(!s) { fprintf(stderr, "service_example: no service %s (error %lu)\n", O.name.c_str(), GetLastError()); CloseServiceHandle(scm); return 1; }
  SERVICE_STATUS st; ControlService(s, SERVICE_CONTROL_STOP, &st);
  for(int i = 0; i < 100 && QueryServiceStatus(s, &st) && st.dwCurrentState != SERVICE_STOPPED; i++) Sleep(100);
  bool ok = DeleteService(s) != 0;
  if(!ok) fprintf(stderr, "service_example: DeleteService %s: error %lu\n", O.name.c_str(), GetLastError());
  else printf("service_example: %s removed\n", O.name.c_str());
  CloseServiceHandle(s); CloseServiceHandle(scm);
  return ok ? 0 : 1;
}
#else
// ---------- POSIX: detach ----------
static bool daemonize()
{
  pid_t p = fork();
  if(p < 0) return false;
  if(p > 0) _exit(0);                                        // the parent returns to the shell
  if(setsid() < 0) return false;
  p = fork();
  if(p < 0) return false;
  if(p > 0) _exit(0);                                        // never a session leader: no controlling terminal again
  umask(022);
  if(chdir("/") != 0) { }
  int d = open("/dev/null", O_RDWR);
  if(d >= 0) { dup2(d, 0); dup2(d, 1); dup2(d, 2); if(d > 2) close(d); }
  return true;
}
#endif

static int usage()
{
  fprintf(stderr, "usage: service_example [--name NAME] --lock FILE --log FILE [--stop-file FILE] [--interval S] [--run-for S]\n"
		  "                       [--daemon] | --service | --install | --uninstall\n");
  return 64;
}

int main(int argc, char **argv)
{
  for(int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if(i + 1 >= argc) { usage(); exit(64); } std::string v = argv[++i]; O.pass_on.push_back(a); O.pass_on.push_back(v); return v; };
    if(a == "--name") O.name = val();
    else if(a == "--lock") O.lock = val();
    else if(a == "--log") O.log = val();
    else if(a == "--stop-file") O.stop_file = val();
    else if(a == "--interval") O.interval = atoi(val().c_str());
    else if(a == "--run-for") O.run_for = atof(val().c_str());
    else if(a == "--daemon") O.daemon = true;
    else if(a == "--service") O.service = true;
    else if(a == "--install") O.install = true;
    else if(a == "--uninstall") O.uninstall = true;
    else return usage();
  }
  if(O.interval < 1) O.interval = 1;
#if defined (_WIN32)
  if(O.uninstall) return uninstall();
  if(O.lock.empty() || O.log.empty()) return usage();
  if(O.install) return install();
  if(O.service) {
    SERVICE_TABLE_ENTRYA t[] = { { (LPSTR)O.name.c_str(), service_main }, { 0, 0 } };
    if(!StartServiceCtrlDispatcherA(t)) { fprintf(stderr, "service_example: not started by the service manager (error %lu)\n", GetLastError()); return 1; }
    return 0;
  }
  SetConsoleCtrlHandler(on_ctrl, TRUE);
  signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
  return run_service();
#else
  if(O.service || O.install || O.uninstall) { fprintf(stderr, "service_example: --service, --install and --uninstall are for Windows; use --daemon (or systemd)\n"); return 64; }
  if(O.lock.empty() || O.log.empty()) return usage();
  // absolute paths: a daemon changes to /
  char cwd[4096];
  if(getcwd(cwd, sizeof(cwd))) for(std::string *p : {&O.lock, &O.log, &O.stop_file}) if(!p->empty() && (*p)[0] != '/') *p = std::string(cwd) + "/" + *p;
  if(O.daemon) {
    // a second instance is refused before detaching, so the shell sees exit 2
    long long pid = 0;
    if(gxProcessLock::IsLocked(O.lock, &pid)) { fprintf(stderr, "service_example: %s is locked by process %lld\n", O.lock.c_str(), pid); return 2; }
    if(!daemonize()) { fprintf(stderr, "service_example: cannot detach\n"); return 1; }
  }
  struct sigaction sa; memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_signal;
  sigaction(SIGTERM, &sa, 0); sigaction(SIGINT, &sa, 0);
  return run_service();
#endif
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
