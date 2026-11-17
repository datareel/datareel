// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: serial_read.cpp
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

serial_read: reads a serial port (gxSerialComm) and writes what
arrives to standard output or appends it to a capture file, until
Ctrl+C, a time limit or a byte count. From dougg's ttycat.

  serial_read DEVICE [--baud N] [--parity N|E|O] [--bits 7|8]
	      [--stop 1|2] [--flow none|hard|soft] [--out FILE]
	      [--hex] [--seconds N] [--bytes N] [--quiet]

DEVICE: /dev/ttyS0, /dev/ttyUSB0, a pseudo terminal (/dev/pts/N) on
Linux; COM1, COM2 ... on Windows. Defaults:
9600 baud, 8 data bits, no parity, 1 stop bit, no flow control, raw
(binary) mode. --out appends to FILE and flushes after every read, so
the capture survives a crash or a pulled cable. --hex writes each
byte as two hex digits (16 a line) instead of the bytes themselves.
Exit 0, 1 on an open or read error (a hangup ends the read: exit 0).
Windows has no hangup on a serial line: the read goes on until Ctrl+C,
Ctrl+Break, --seconds or --bytes; a USB adapter pulled out is a read
error (exit 1). COM10 and up are opened as \\.\COMn for you.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#if defined (_WIN32)
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <poll.h>
#endif
#include "gxscomm.h"

using namespace datareel;

static std::atomic<int> g_stop{0};
static void on_signal(int) { g_stop = 1; }
#if defined (_WIN32)
static BOOL WINAPI on_ctrl(DWORD) { g_stop = 1; return TRUE; }
#endif

static int usage()
{
  fprintf(stderr, "usage: serial_read DEVICE [--baud N] [--parity N|E|O] [--bits 7|8] [--stop 1|2]\n"
		  "                   [--flow none|hard|soft] [--out FILE] [--hex] [--seconds N] [--bytes N] [--quiet]\n");
  return 1;
}

int main(int argc, char **argv)
{
  if(argc < 2) return usage();
  std::string dev = argv[1], out_file;
  int baud = 9600, bits = 8, stop = 1, flow = gxSerialComm::scommNO_FLOW_CONTROL;
  char parity = 'N';
  bool hex = false, quiet = false;
  double seconds = 0;
  unsigned long long max_bytes = 0;
  for(int i = 2; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() -> std::string { if(i + 1 >= argc) { usage(); exit(1); } return argv[++i]; };
    if(a == "--baud") baud = atoi(val().c_str());
    else if(a == "--parity") { std::string p = val(); parity = p.empty() ? 'N' : (char)toupper((unsigned char)p[0]); }
    else if(a == "--bits") bits = atoi(val().c_str());
    else if(a == "--stop") stop = atoi(val().c_str());
    else if(a == "--flow") {
      std::string f = val();
      flow = f == "hard" ? gxSerialComm::scommHARD_FLOW : f == "soft" ? gxSerialComm::scommSOFT_FLOW : gxSerialComm::scommNO_FLOW_CONTROL;
    }
    else if(a == "--out") out_file = val();
    else if(a == "--hex") hex = true;
    else if(a == "--seconds") seconds = atof(val().c_str());
    else if(a == "--bytes") max_bytes = strtoull(val().c_str(), 0, 10);
    else if(a == "--quiet") quiet = true;
    else return usage();
  }
  signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
#if defined (_WIN32)
  SetConsoleCtrlHandler(on_ctrl, TRUE);
#endif
  gxSerialComm port;
  std::string devname = dev;
#if defined (_WIN32)
  // 11/17/2026: COM10 and up only open as \\.\COM10 (the short name works for COM1 to COM9 only)
  if(devname.size() > 3 && _strnicmp(devname.c_str(), "COM", 3) == 0 && atoi(devname.c_str() + 3) >= 10)
    devname = "\\\\.\\" + devname;
#endif
  // InitSerialPort() returns the port's access mode (scommREAD_WRITE ...), or -1
  if(port.InitSerialPort((char *)devname.c_str(), baud, parity, bits, stop, flow) < 0) {
    fprintf(stderr, "serial_read: %s: %s\n", dev.c_str(), port.SerialCommExceptionMessage());
    return 1;
  }
#if defined (_WIN32)
  // 11/17/2026: reads that return at once with what has arrived, or after 100 ms with nothing (the library's
  // timeouts wait about 4 s for a full 4 KB buffer), so --seconds and Ctrl+C are seen within 100 ms, as on POSIX
  {
    COMMTIMEOUTS ct; memset(&ct, 0, sizeof(ct));
    ct.ReadIntervalTimeout = MAXDWORD; ct.ReadTotalTimeoutMultiplier = MAXDWORD; ct.ReadTotalTimeoutConstant = 100;
    ct.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(port.DeviceHandle(), &ct);
  }
  // 11/17/2026: binary bytes to standard output unchanged (text mode turns \n into \r\n)
  if(out_file.empty()) _setmode(_fileno(stdout), _O_BINARY);
#endif
  FILE *out = stdout;
  if(!out_file.empty()) {
    out = fopen(out_file.c_str(), "ab");
    if(!out) { fprintf(stderr, "serial_read: cannot open the capture file %s\n", out_file.c_str()); port.Close(); return 1; }
  }
  if(!quiet) fprintf(stderr, "serial_read: %s at %d %d%c%d, %s; Ctrl+C to stop\n", dev.c_str(), baud, bits, parity, stop,
		     out_file.empty() ? "to standard output" : ("appending to " + out_file).c_str());
  auto t0 = std::chrono::steady_clock::now();
  unsigned long long total = 0, col = 0;
  int rc = 0;
  char buf[4096];
  bool hung_up = false;                                        // POSIX: the other end closed (a pty's master, a modem)
  while(!g_stop) {
    if(seconds > 0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() >= seconds) break;
#if !defined (_WIN32)
    // wait up to 100 ms for data, so Ctrl+C and the time limit are seen (the port's reads block)
    struct pollfd pfd; pfd.fd = port.DeviceHandle(); pfd.events = POLLIN; pfd.revents = 0;
    int pr = poll(&pfd, 1, 100);
    if(pr == 0) continue;
    if(pr < 0) continue;                                       // a signal: the loop checks g_stop
    if(pfd.revents & (POLLHUP | POLLERR)) { hung_up = true; if(!(pfd.revents & POLLIN)) break; }   // hangup: the end
#endif
    size_t want = sizeof(buf);
    if(max_bytes && max_bytes - total < want) want = (size_t)(max_bytes - total);
    int n = port.RawRead(buf, (int)want);
    if(n < 0) {
      if(port.GetSerialCommError() == gxSerialComm::scomm_TIMEOUT_ERROR) {   // Windows: the read timeout; UNIX: a hangup
#if defined (_WIN32)
	continue;
#else
	break;
#endif
      }
      if(hung_up) break;                                       // EIO after a hangup (a pty): the end, not an error
      fprintf(stderr, "serial_read: %s: %s\n", dev.c_str(), port.SerialCommExceptionMessage()); rc = 1; break;
    }
    if(n == 0) {
#if defined (_WIN32)
      continue;
#else
      break;                                                   // end of file: the other end closed
#endif
    }
    if(hex) {
      for(int i = 0; i < n; i++) {                             // "XX XX ... XX" 16 a line, no space at a line end
	if(col % 16) fputc(' ', out);
	fprintf(out, "%02X", (unsigned char)buf[i]);
	if(++col % 16 == 0) fputc('\n', out);
      }
    }
    else fwrite(buf, 1, (size_t)n, out);
    fflush(out);                                               // the capture is on disk after every read
    total += (unsigned long long)n;
    if(max_bytes && total >= max_bytes) break;
  }
  if(hex && col % 16) fprintf(out, "\n");
  if(out != stdout) fclose(out); else fflush(out);
  port.Close();
  if(!quiet) fprintf(stderr, "serial_read: %llu byte(s) read\n", total);
  return rc;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
