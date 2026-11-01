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

Returns 0 if every check passes; failed checks print FAIL lines. The
serial tests are UNIX only.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>

#if defined (__UNIX__)
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#endif

#include "scomserv.h"

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

int main(int argc, char **argv)
{
#if defined (__UNIX__)
  signal(SIGALRM, on_alarm);
  signal(SIGPIPE, SIG_IGN);
#endif
  const char *names[] = { "serial", 0 };
  void (*tests[])() = { test_serial };
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
