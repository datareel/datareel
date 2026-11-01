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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

6.3 tests for the DataReel 7.3.1 security sweep fixes to the serial
port, telnet and ping code. Only local resources are used: a pseudo
terminal for the serial port and a loopback (127.0.0.1) fake server
for telnet. Run with no arguments for every test, or name the tests
to run:

  serial   gxSerialComm/gxSerialCommServer over a pseudo terminal: no
           SIGIO on arriving data, 0-byte reads end Recv(), ReadBlock()
           buffer sizes, error message table bounds, an object that
           never opened a port does not close descriptor 0,
           close-on-exec, settings restored by Close().
  telnet   gxsTelnetClient against a fake server on 127.0.0.1 (a forked
           process): RecvString() buffer bounds, stale and overlapping
           matches, null pattern, sub-negotiation, option loops, init
           sequence, terminal type length, total time limit.

Returns 0 if every check passes; failed checks print FAIL lines. The
serial tests are UNIX only.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <time.h>

#if defined (__UNIX__)
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#endif

#include "scomserv.h"
#include "gxtelnet.h"

#if defined (__UNIX__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#endif

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  fflush(stdout);
  if(!ok) failures++;
}

#if defined (__UNIX__)
static const char *alarm_test = "";

static void on_alarm(int)
{
  printf("FAIL %s (still running after the time limit)\n", alarm_test);
  fflush(stdout);
  _exit(1);
}

static void on_sigio(int)
{
  const char msg[] = "FAIL serial: SIGIO raised by data arriving on the port\n";
  if(write(1, msg, sizeof(msg) - 1) < 0) { }
  _exit(1);
}

static void limit(const char *what, int secs)
{
  alarm_test = what;
  alarm(secs);
}

static int open_fds()
{
  DIR *d = opendir("/proc/self/fd");
  if(!d) return -1;
  int n = 0;
  while(readdir(d)) n++;
  closedir(d);
  return n;
}

static int open_pty(int &master, char *slave_name, size_t len)
{
  master = posix_openpt(O_RDWR | O_NOCTTY);
  if(master < 0) return 0;
  if(grantpt(master) != 0 || unlockpt(master) != 0) return 0;
  const char *n = ptsname(master);
  if(!n || strlen(n) >= len) return 0;
  strcpy(slave_name, n);
  return 1;
}

static void test_serial()
{
  int master;
  char slave[256];
  if(!open_pty(master, slave, sizeof(slave))) {
    printf("SKIP serial: no pseudo terminal\n");
    return;
  }

  // Data arriving on an initialized port must not raise SIGIO (the
  // default action ends the program); see the handler for the FAIL
  signal(SIGIO, on_sigio);
  {
    gxSerialCommServer s;
    check(s.InitCommServer(slave, 9600) == gxSerialComm::scomm_NO_ERROR, "serial: open and initialize the pty");
    int fl = fcntl(s.DeviceHandle(), F_GETFL);
    check(fl != -1 && !(fl & O_ASYNC), "serial: the port is not set to O_ASYNC");
    check(fl != -1 && !(fl & O_NONBLOCK), "serial: reads block after InitSerialPort()");
    int fd_fl = fcntl(s.DeviceHandle(), F_GETFD);
    check(fd_fl != -1 && (fd_fl & FD_CLOEXEC), "serial: the port is close-on-exec");

    // A block sent from the other end
    gxBlockHeader gx;
    s.SetBlockStatus(gx, gxSendBlock);
    char data[32];
    memset(data, 'x', sizeof(data));
    gx.block_length = sizeof(data);
    limit("serial: block over the pty", 10);
    if(write(master, &gx, sizeof(gx)) != (ssize_t)sizeof(gx) ||
       write(master, data, sizeof(data)) != (ssize_t)sizeof(data)) {
      check(0, "serial: write to the pty master");
    }
    usleep(100000);
    gxBlockHeader rx;
    check(s.ReadHeader(rx) == gxSerialComm::scomm_NO_ERROR && rx.block_length == sizeof(data),
	  "serial: block header read over the pty");
    char small[16];
    check(s.ReadBlock(small, sizeof(small), rx) == gxSerialComm::scomm_BUFFER_OVERFLOW,
	  "serial: ReadBlock() with a buffer too small is refused");
    char big[64];
    memset(big, 0, sizeof(big));
    check(s.ReadBlock(big, sizeof(big), rx) == gxSerialComm::scomm_NO_ERROR &&
	  memcmp(big, data, sizeof(data)) == 0, "serial: ReadBlock() into a buffer large enough");

    // The old overload refuses a length a small buffer cannot hold
    gxBlockHeader huge;
    s.SetBlockStatus(huge, gxSendBlock);
    huge.block_length = 0x100000;
    char *p = new char[0x100000];
    alarm_test = "serial: ReadBlock(buf, gx) refuses a block over 64 KB";
    check(s.ReadBlock(p, huge) == gxSerialComm::scomm_BUFFER_OVERFLOW,
	  "serial: ReadBlock(buf, gx) refuses a block over 64 KB");
    delete[] p;
    alarm(0);
    s.Close();
  }
  signal(SIGIO, SIG_DFL);

  // Recv() ends on a 0-byte read (end of file) instead of looping
  {
    gxSerialComm c;
    int fd = open("/dev/null", O_RDONLY);
    char buf[8];
    limit("serial: Recv() on end of file", 5);
    int rv = c.Recv(fd, buf, sizeof(buf));
    alarm(0);
    check(rv == -1 && c.GetSerialCommError() == gxSerialComm::scomm_TIMEOUT_ERROR,
	  "serial: Recv() returns a timeout error on end of file");
    check(c.RawRead(fd, buf, -1) == -1 && c.RawWrite(fd, 0, 4) == -1,
	  "serial: RawRead()/RawWrite() refuse a negative count and a null buffer");
    close(fd);
  }

  // Error message table bounds
  {
    gxSerialComm c;
    c.SetSerialCommError(gxSerialComm::scomm_TIMEOUT_ERROR);
    const char *m = c.SerialCommExceptionMessage();
    check(m && strstr(m, "timeout") != 0, "serial: the last error code has a message");
    c.SetSerialCommError(-3);
    m = c.SerialCommExceptionMessage();
    check(m && strstr(m, "Invalid error code") != 0, "serial: a negative error code reads as invalid");
    c.SetSerialCommError(1000);
    m = c.SerialCommExceptionMessage();
    check(m && strstr(m, "Invalid error code") != 0, "serial: a large error code reads as invalid");
  }

  // An object that never opened a port must not close descriptor 0 (the
  // handle was left uninitialized; zeroed memory made it 0)
  {
    int saved = dup(0);
    int nul = open("/dev/null", O_RDONLY);
    dup2(nul, 0);
    close(nul);
    void *mem = malloc(sizeof(gxSerialComm));
    memset(mem, 0, sizeof(gxSerialComm));
    gxSerialComm *c = new(mem) gxSerialComm;
    c->~gxSerialComm();
    free(mem);
    check(fcntl(0, F_GETFD) != -1, "serial: destroying an unopened object leaves descriptor 0 open");
    dup2(saved, 0);
    close(saved);
  }

  // Opening twice does not leak the first descriptor; Close() restores
  // the settings saved at open
  {
    struct termios t;
    int sfd = open(slave, O_RDWR | O_NOCTTY);
    tcgetattr(sfd, &t);
    cfsetispeed(&t, B38400);
    cfsetospeed(&t, B38400);
    tcsetattr(sfd, TCSANOW, &t);
    int before = open_fds();
    gxSerialComm c;
    c.OpenSerialPort(slave);
    c.OpenSerialPort(slave);
    check(open_fds() == before + 1, "serial: a second OpenSerialPort() does not leak a descriptor");
    c.SetBaudRate(9600);
    c.InitSerialPort();
    c.Close();
    tcgetattr(sfd, &t);
    check(cfgetospeed(&t) == B38400, "serial: Close() restores the saved settings");
    close(sfd);
  }
  close(master);
}
#else
static void test_serial()
{
  printf("SKIP serial: UNIX only (pseudo terminal)\n");
}
#endif


#if defined (__UNIX__)
// Fake telnet server: a forked process accepts one connection, runs a
// script, then collects what the client sent (until EOF or 1 second of
// silence) and passes it back through a pipe.
typedef void (*server_script)(int fd);

struct fake_server {
  int port;
  pid_t pid;
  int pipe_rd;
};

static void put(int fd, const void *p, size_t n)
{
  if(write(fd, p, n) != (ssize_t)n) { }
}

static int start_server(fake_server &fs, server_script script)
{
  int ls = socket(AF_INET, SOCK_STREAM, 0);
  if(ls < 0) return 0;
  int on = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = 0;
  if(bind(ls, (struct sockaddr *)&sa, sizeof(sa)) != 0 || listen(ls, 1) != 0) {
    close(ls);
    return 0;
  }
  socklen_t len = sizeof(sa);
  getsockname(ls, (struct sockaddr *)&sa, &len);
  fs.port = ntohs(sa.sin_port);
  int pfd[2];
  if(pipe(pfd) != 0) { close(ls); return 0; }
  fflush(stdout);
  fs.pid = fork();
  if(fs.pid == 0) {
    close(pfd[0]);
    alarm(30);
    int c = accept(ls, 0, 0);
    close(ls);
    if(c < 0) _exit(1);
    script(c);
    char buf[4096];
    for(;;) {
      struct pollfd p;
      p.fd = c; p.events = POLLIN; p.revents = 0;
      if(poll(&p, 1, 1000) <= 0) break;
      ssize_t n = read(c, buf, sizeof(buf));
      if(n <= 0) break;
      put(pfd[1], buf, (size_t)n);
    }
    close(c);
    close(pfd[1]);
    _exit(0);
  }
  close(ls);
  close(pfd[1]);
  fs.pipe_rd = pfd[0];
  return fs.pid > 0;
}

static int finish_server(fake_server &fs, unsigned char *got, int max)
// Returns the number of bytes the client sent
{
  int n = 0;
  for(;;) {
    ssize_t r = read(fs.pipe_rd, got + n, (size_t)(max - n));
    if(r <= 0) break;
    n += (int)r;
    if(n >= max) break;
  }
  close(fs.pipe_rd);
  int st;
  waitpid(fs.pid, &st, 0);
  return n;
}

static int count_seq(const unsigned char *p, int n, const unsigned char *seq, int len)
{
  int c = 0;
  for(int i = 0; i + len <= n; i++) if(memcmp(p + i, seq, len) == 0) c++;
  return c;
}

static void script_flood(int fd)
{
  char a[3000];
  memset(a, 'A', sizeof(a));
  put(fd, a, sizeof(a));
  put(fd, "$ ", 2);
  usleep(300000);
}

static void script_overlap(int fd)
{
  put(fd, "xaaab", 5);
  usleep(1500000);
}

static void script_short(int fd)
{
  put(fd, "lo", 2);
  usleep(2500000);
}

static void script_hi(int fd)
{
  put(fd, "hi", 2);
}

static void script_sb(int fd)
{
  // A NAWS sub-negotiation holding 1 bytes, then TERMINAL-TYPE SEND
  // twice, then data
  const unsigned char naws[] = { 255, 250, 31, 1, 1, 1, 1, 255, 240 };
  const unsigned char ttype[] = { 255, 250, 24, 1, 255, 240 };
  put(fd, naws, sizeof(naws));
  put(fd, ttype, sizeof(ttype));
  put(fd, ttype, sizeof(ttype));
  put(fd, "ok$", 3);
  usleep(300000);
}

static void script_loop(int fd)
{
  const unsigned char will_echo[] = { 255, 251, 1 };
  const unsigned char do_x[] = { 255, 253, 99 };
  for(int i = 0; i < 50; i++) put(fd, will_echo, sizeof(will_echo));
  for(int i = 0; i < 50; i++) put(fd, do_x, sizeof(do_x));
  put(fd, "$", 1);
  usleep(300000);
}

static void script_none(int fd)
{
  (void)fd;
  usleep(300000);
}

static void script_trickle(int fd)
{
  for(int i = 0; i < 20; i++) {
    put(fd, "z", 1);
    usleep(400000);
  }
}

static void test_telnet()
{
  fake_server fs;
  unsigned char got[8192];

  // RecvString() stores at most bytes-1 characters
  if(start_server(fs, script_flood)) {
    gxsTelnetClient t;
    t.SetTimeOut(2, 0);
    t.ConnectClient("127.0.0.1", fs.port);
    char *mem = new char[8192];
    memset(mem, 0x5a, 8192);
    limit("telnet: RecvString() buffer bounds", 20);
    gxSocketError e = t.RecvString(mem, 64, "$ ");
    alarm(0);
    int guard_ok = 1;
    for(int i = 64; i < 8192; i++) if(mem[i] != 0x5a) { guard_ok = 0; break; }
    check(guard_ok, "telnet: RecvString() does not write past the buffer");
    check(e == gxSOCKET_BUFOVER_ERROR && strlen(mem) == 63, "telnet: a full buffer holds 63 characters and is terminated");
    delete[] mem;
    t.Close();
    finish_server(fs, got, sizeof(got));
  }

  // Overlapping match
  if(start_server(fs, script_overlap)) {
    gxsTelnetClient t;
    t.SetTimeOut(1, 0);
    t.ConnectClient("127.0.0.1", fs.port);
    char buf[64];
    limit("telnet: overlapping match", 20);
    gxSocketError e = t.RecvString(buf, sizeof(buf), "aab");
    alarm(0);
    check(e == gxSOCKET_NO_ERROR, "telnet: \"aab\" is found in \"xaaab\"");
    t.Close();
    finish_server(fs, got, sizeof(got));
  }

  // No match on stale bytes past the received data
  if(start_server(fs, script_short)) {
    gxsTelnetClient t;
    t.SetTimeOut(1, 0);
    t.ConnectClient("127.0.0.1", fs.port);
    char buf[64];
    strcpy(buf, "login: from an earlier call");
    limit("telnet: stale bytes", 20);
    gxSocketError e = t.RecvString(buf, sizeof(buf), "login:");
    alarm(0);
    check(e == gxSOCKET_REQUEST_TIMEOUT && strcmp(buf, "lo") == 0,
	  "telnet: stale bytes in the buffer are not matched");
    t.Close();
    finish_server(fs, got, sizeof(got));
  }

  // A null pattern and a disconnect: no crash, buffer terminated
  if(start_server(fs, script_hi)) {
    gxsTelnetClient t;
    t.SetTimeOut(2, 0);
    t.ConnectClient("127.0.0.1", fs.port);
    char buf[64];
    memset(buf, 'q', sizeof(buf));
    limit("telnet: null pattern", 20);
    gxSocketError e = t.RecvString(buf, sizeof(buf), 0);
    alarm(0);
    check(e == gxSOCKET_DISCONNECTED_ERROR && strcmp(buf, "hi") == 0,
	  "telnet: a null pattern does not crash; the buffer is terminated on disconnect");
    t.Close();
    finish_server(fs, got, sizeof(got));
  }

  // Sub-negotiation: only TERMINAL-TYPE SEND is answered, once per SB,
  // and SB bytes are not output
  if(start_server(fs, script_sb)) {
    gxsTelnetClient t;
    t.SetTimeOut(2, 0);
    t.ConnectClient("127.0.0.1", fs.port);
    char buf[64];
    limit("telnet: sub-negotiation", 20);
    gxSocketError e = t.RecvString(buf, sizeof(buf), "$");
    alarm(0);
    check(e == gxSOCKET_NO_ERROR && strcmp(buf, "ok$") == 0, "telnet: sub-negotiation bytes are not output");
    t.Close();
    int n = finish_server(fs, got, sizeof(got));
    const unsigned char reply[] = { 255, 250, 24, 0, 'N', 'V', 'T', 255, 240 };
    check(count_seq(got, n, reply, sizeof(reply)) == 2, "telnet: one terminal type reply per TERMINAL-TYPE SEND");
    const unsigned char sb[] = { 255, 250, 24, 0 };
    check(count_seq(got, n, sb, sizeof(sb)) == 2, "telnet: a NAWS sub-negotiation holding 1 bytes gets no reply");
  }

  // Repeated option requests are acknowledged once
  if(start_server(fs, script_loop)) {
    gxsTelnetClient t;
    t.SetTimeOut(2, 0);
    t.ConnectClient("127.0.0.1", fs.port);
    char buf[64];
    limit("telnet: option loops", 20);
    t.RecvString(buf, sizeof(buf), "$");
    alarm(0);
    t.Close();
    int n = finish_server(fs, got, sizeof(got));
    const unsigned char do_echo[] = { 255, 253, 1 };
    const unsigned char wont_x[] = { 255, 252, 99 };
    check(count_seq(got, n, do_echo, 3) == 1 && count_seq(got, n, wont_x, 3) == 1,
	  "telnet: 50 repeated WILL ECHO and DO requests are answered once each");
  }

  // The init sequence has no lone IAC; a Windows server line ends CR LF
  if(start_server(fs, script_none)) {
    gxsTelnetClient t;
    t.ConnectClient("127.0.0.1", fs.port);
    t.SendInitSeq();
    t.SetServerTypeWindows();
    t.SendString("cmd", 3);
    t.Close();
    int n = finish_server(fs, got, sizeof(got));
    check(n == 20 && got[14] == 31 && memcmp(got + 15, "cmd\r\n", 5) == 0,
	  "telnet: SendInitSeq() sends 15 bytes; SendString() to a Windows server ends with CR LF");
  }

  // Terminal type names are cut to fit
  {
    gxsTelnetClient t;
    char name[200];
    memset(name, 'T', sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    t.SetTermType(name);
    check(strlen(t.GetTermType()) == 15 && t.GetMaxTotalTime() == 0, "telnet: a long terminal type is cut to 15 characters");
    setenv("TERM", name, 1);
    t.SetTermType(gxsTEL_ENV_TERM);
    check(strlen(t.GetTermType()) == 15, "telnet: a long TERM variable is cut to 15 characters");
  }

  // Total time limit against a server that trickles one byte at a time
  if(start_server(fs, script_trickle)) {
    gxsTelnetClient t;
    t.SetTimeOut(2, 0);
    t.SetMaxTotalTime(2);
    t.ConnectClient("127.0.0.1", fs.port);
    char buf[256];
    time_t t0 = time(0);
    limit("telnet: total time limit", 20);
    gxSocketError e = t.RecvString(buf, sizeof(buf), "never");
    alarm(0);
    check(e == gxSOCKET_REQUEST_TIMEOUT && (time(0) - t0) <= 4, "telnet: SetMaxTotalTime() ends RecvString()");
    t.Close();
    kill(fs.pid, SIGKILL);
    finish_server(fs, got, sizeof(got));
  }
}
#else
static void test_telnet()
{
  printf("SKIP telnet: UNIX only (fork)\n");
}
#endif

int main(int argc, char **argv)
{
#if defined (__UNIX__)
  signal(SIGALRM, on_alarm);
  signal(SIGPIPE, SIG_IGN);
#endif
  const char *names[] = { "serial", "telnet", 0 };
  void (*tests[])() = { test_serial, test_telnet };
  int all = (argc < 2);
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
