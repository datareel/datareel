// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: support_security_test.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/01/2026
// Date Last Modified: 11/01/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

support_security_test.cpp - regression tests for the DataReel 7.3.1
security sweep of the support classes: telnet, serial block I/O,
config files, log files, the thread pool, ping reply parsing and
the db_debug block walk. Prints "PASS <name>" per test and returns
non-zero on a failure. The telnet and serial tests use socket pairs
and are skipped on Windows.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "gxconfig.h"
#include "gxsping.h"
#include "logfile.h"
#include "thrpool.h"
#include "dbugmgr.h"
#include "../utils/db_debug/db_walk.h"

#if !defined (_WIN32)
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gxtelnet.h"
#include "scomserv.h"
#endif

using namespace datareel;
namespace fs = std::filesystem;

static int failures = 0;

#define CHECK(cond) do { if(!(cond)) { \
  printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
  failures++; return; } } while(0)

static void pass(const char *name) { printf("PASS %s\n", name); }

static fs::path tmp_dir;

static std::string read_file(const fs::path &p)
{
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
		     std::istreambuf_iterator<char>());
}

#if !defined (_WIN32)
// ----------------------------------------------------------- //
// Telnet
// ----------------------------------------------------------- //

struct TelnetPair {
  gxsTelnetClient client;
  int peer = -1;
  TelnetPair() {
    int sv[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0) {
      static_cast<gxSocket &>(client) = gxSocket(sv[0]);
      peer = sv[1];
      fcntl(peer, F_SETFL, fcntl(peer, F_GETFL) | O_NONBLOCK);
    }
    client.SetTimeOut(0, 300000); // 0.3 s per read
  }
  ~TelnetPair() { if(peer >= 0) close(peer); }
  void put(const std::string &s) {
    if(write(peer, s.data(), s.size()) != (ssize_t)s.size()) { }
  }
  std::string replies() { // What the client sent back
    std::string out;
    char b[512];
    ssize_t n;
    while((n = read(peer, b, sizeof(b))) > 0) out.append(b, (size_t)n);
    return out;
  }
};

static std::string iac(std::initializer_list<int> v)
{
  std::string s;
  for(int c : v) s += (char)(unsigned char)c;
  return s;
}

static void test_telnet_recvstring()
{
  {
    TelnetPair t;
    CHECK(t.peer >= 0);
    t.put("Welcome\r\nlogin: ");
    char buf[64];
    CHECK(t.client.RecvString(buf, sizeof(buf), "login:") == gxSOCKET_NO_ERROR);
    CHECK(strstr(buf, "login: ") != 0);
  }
  {
    // Stale bytes in the caller's buffer must not complete a match
    TelnetPair t;
    char buf[64];
    strcpy(buf, "XXXXXXXXXXlogin:");
    t.put("lo");
    CHECK(t.client.RecvString(buf, sizeof(buf), "login:") ==
	  gxSOCKET_REQUEST_TIMEOUT);
    CHECK(strcmp(buf, "lo") == 0);
  }
  {
    // Overlapping prefix ("aab" in "aaab") used to be missed
    TelnetPair t;
    char buf[64];
    t.put("xaaab");
    CHECK(t.client.RecvString(buf, sizeof(buf), "aab") == gxSOCKET_NO_ERROR);
  }
  {
    // Null pattern: no crash, reads until the timeout
    TelnetPair t;
    char buf[16];
    t.put("data");
    CHECK(t.client.RecvString(buf, sizeof(buf), 0) == gxSOCKET_REQUEST_TIMEOUT);
    CHECK(strcmp(buf, "data") == 0);
  }
  {
    // A full buffer is terminated in bounds
    TelnetPair t;
    char buf[8];
    t.put("0123456789abcdef");
    CHECK(t.client.RecvString(buf, sizeof(buf), "zz") == gxSOCKET_BUFOVER_ERROR);
    CHECK(strlen(buf) == 7);
  }
  {
    // Closed connection terminates the buffer
    TelnetPair t;
    char buf[16];
    memset(buf, 'Q', sizeof(buf));
    t.put("ab");
    shutdown(t.peer, SHUT_WR);
    CHECK(t.client.RecvString(buf, sizeof(buf), "zz") ==
	  gxSOCKET_DISCONNECTED_ERROR);
    CHECK(strcmp(buf, "ab") == 0);
  }
  pass("telnet_recvstring");
}

static void test_telnet_deadline()
{
  TelnetPair t;
  t.client.SetTimeOut(2, 0);
  t.client.SetMaxTotalTime(1);
  std::atomic<bool> stop(false);
  int fd = t.peer;
  std::thread trickle([&] {
    for(int i = 0; i < 40 && !stop; i++) {
      if(write(fd, "a", 1) != 1) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  char buf[256];
  auto start = std::chrono::steady_clock::now();
  gxSocketError err = t.client.RecvString(buf, sizeof(buf), "never");
  double secs = std::chrono::duration<double>
    (std::chrono::steady_clock::now() - start).count();
  stop = true;
  trickle.join();
  CHECK(err == gxSOCKET_REQUEST_TIMEOUT);
  CHECK(secs < 2.5);
  pass("telnet_deadline");
}

static void test_telnet_iac()
{
  {
    // SendInitSeq() sends 5 complete commands, no trailing lone IAC
    TelnetPair t;
    CHECK(t.client.SendInitSeq() == gxSOCKET_NO_ERROR);
    std::string r = t.replies();
    CHECK(r.size() == 15);
    CHECK((unsigned char)r[14] == gxsTELOPT_NAWS);
  }
  {
    // A non terminal-type SB holding 1 bytes is discarded silently
    TelnetPair t;
    t.put(iac({255, 250, 31, 1, 1, 1, 0, 1, 255, 240}) + "ok:");
    char buf[64];
    CHECK(t.client.RecvString(buf, sizeof(buf), "ok:") == gxSOCKET_NO_ERROR);
    CHECK(strcmp(buf, "ok:") == 0); // No SB bytes leaked into the data
    CHECK(t.replies().empty());
  }
  {
    // TERMINAL-TYPE SEND is answered exactly once, with IAC SE
    TelnetPair t;
    t.put(iac({255, 250, 24, 1, 255, 240}) + "ok:");
    char buf[64];
    CHECK(t.client.RecvString(buf, sizeof(buf), "ok:") == gxSOCKET_NO_ERROR);
    std::string r = t.replies();
    CHECK(r == iac({255, 250, 24, 0}) + "NVT" + iac({255, 240}));
  }
  {
    // Malformed terminal-type SB (extra bytes) gets no reply; an IAC
    // other than SE inside the SB does not end it
    TelnetPair t;
    t.put(iac({255, 250, 24, 1, 1, 255, 251, 1, 255, 240}) + "ok:");
    char buf[64];
    CHECK(t.client.RecvString(buf, sizeof(buf), "ok:") == gxSOCKET_NO_ERROR);
    CHECK(strcmp(buf, "ok:") == 0);
    CHECK(t.replies().empty());
  }
  {
    // A repeated WILL ECHO is acknowledged once (RFC 1143)
    TelnetPair t;
    t.put(iac({255, 251, 1, 255, 251, 1, 255, 251, 1}) + "ok:");
    char buf[64];
    CHECK(t.client.RecvString(buf, sizeof(buf), "ok:") == gxSOCKET_NO_ERROR);
    CHECK(t.replies() == iac({255, 253, 1}));
    // A change of state is answered again
    t.put(iac({255, 252, 1}) + "ok:");
    CHECK(t.client.RecvString(buf, sizeof(buf), "ok:") == gxSOCKET_NO_ERROR);
    CHECK(!t.replies().empty());
  }
  {
    // IAC IAC is a data byte
    TelnetPair t;
    t.put(std::string("a") + iac({255, 255}) + "b");
    char buf[64];
    t.client.RecvString(buf, sizeof(buf), "b");
    CHECK(strlen(buf) == 3 && (unsigned char)buf[1] == 255);
  }
  pass("telnet_iac");
}

// ----------------------------------------------------------- //
// Serial block server (socket pair in place of a tty)
// ----------------------------------------------------------- //

class TestCommServer : public gxSerialCommServer
{
public:
  void Attach(int fd) { device_handle = fd; }
};

static void test_scomserv()
{
  int sv[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  TestCommServer s;
  s.Attach(sv[0]);
  gxBlockHeader gx;
  char buf[16];

  // Peer claims a 100 byte block for a 16 byte buffer
  gx.block_length = 100;
  CHECK(s.ReadBlock(buf, sizeof(buf), gx) == gxSerialComm::scomm_BUFFER_OVERFLOW);
  // The unsized overload uses the internal cap
  gx.block_length = (gxUINT32)(gxsSCOMM_MAX_UNSIZED_BLOCK + 1);
  std::vector<char> big(gxsSCOMM_MAX_UNSIZED_BLOCK + 16);
  CHECK(s.ReadBlock(big.data(), gx) == gxSerialComm::scomm_BUFFER_OVERFLOW);
  gx.block_length = 0xFFFFFFF0u;
  CHECK(s.ReadBlock(big.data(), gx) == gxSerialComm::scomm_BUFFER_OVERFLOW);

  // A block that fits is read
  CHECK(write(sv[1], "0123456789", 10) == 10);
  gx.block_length = 10;
  CHECK(s.ReadBlock(buf, sizeof(buf), gx) == gxSerialComm::scomm_NO_ERROR);
  CHECK(memcmp(buf, "0123456789", 10) == 0);

  // End of file ends Recv() instead of looping forever
  CHECK(write(sv[1], "abc", 3) == 3);
  close(sv[1]);
  CHECK(s.Recv(buf, 10) == -1);
  CHECK(s.GetSerialCommError() == gxSerialComm::scomm_TIMEOUT_ERROR);

  // Bad sizes and error codes
  CHECK(s.RawRead(buf, 0) == -1);
  CHECK(s.RawRead(buf, -5) == -1);
  CHECK(s.RawWrite(buf, -1) == -1);
  s.SetSerialCommError(-3);
  CHECK(s.SerialCommExceptionMessage() != 0);
  s.SetSerialCommError(gxSerialComm::scomm_TIMEOUT_ERROR);
  const char *m = s.SerialCommExceptionMessage();
  CHECK(m != 0 && strstr(m, "timeout") != 0);
  s.SetSerialCommError(1000);
  CHECK(s.SerialCommExceptionMessage() != 0);
  s.Close(); // Settings never saved: must not touch termios

  // A constructed but never opened port closes nothing
  {
    gxSerialComm unused;
  }
  // Opening a non-tty: works, nothing to restore
  {
    gxSerialComm p;
    fs::path f = tmp_dir / "not_a_tty";
    { std::ofstream o(f); o << "x"; }
    std::string name = f.string();
    CHECK(p.OpenSerialPort((char *)name.c_str()) == gxSerialComm::scommREAD_WRITE);
    int fd = p.DeviceHandle();
    CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
    p.Close();
    CHECK(p.DeviceHandle() == -1);
    CHECK(p.OpenSerialPort(0) == -1);
  }
  pass("scomserv_readblock");
}
#endif // !_WIN32

// ----------------------------------------------------------- //
// Config files
// ----------------------------------------------------------- //

static void test_config()
{
  fs::path f = tmp_dir / "test.cfg";
  {
    std::ofstream o(f, std::ios::binary);
    o << "# Port = 1\n"
      << "AdminPort = 90\n"
      << "Port = 80\n"
      << "  #Port=2\n"
      << "PortRange = 5\n"
      << "Big = 99999999999999999999\n"
      << "Neg = -12abc\n";
  }
  gxConfig cfg(f.string());
  CHECK(cfg.Load());
  CHECK(cfg.ChangeConfigValue("Port", 81) == 1);
  std::string text = read_file(f);
  CHECK(text.find("AdminPort = 90") != std::string::npos);
  CHECK(text.find("# Port = 1") != std::string::npos);
  CHECK(text.find("#Port=2") != std::string::npos);
  CHECK(text.find("PortRange = 5") != std::string::npos);
  CHECK(text.find("Port=81") != std::string::npos);
  CHECK(cfg.GetIntValue("Port") == 81);
  CHECK(cfg.GetIntValue("AdminPort") == 90);
  CHECK(!fs::exists(f.string() + ".tmp"));

  // CR/LF injection
  CHECK(cfg.ChangeConfigValue("Port", "1\nAdmin=yes") == 0);
  CHECK(cfg.ChangeConfigValue("Evil\r\nX", "1") == 0);
  CHECK(!cfg.WriteConfigLine("Name", "a\rb"));
  CHECK(!cfg.WriteConfigLine("Na=me", "x"));
  CHECK(!cfg.WriteCommentLine("note\nAdmin=yes"));
  CHECK(cfg.ReLoad());
  CHECK(!cfg.HasValue("Admin"));
  CHECK(cfg.GetIntValue("Port") == 81);

  // Range checked numbers
  CHECK(cfg.GetIntValue("Big") == 0);
  CHECK(cfg.GetLongValue("Big") == 0);
  CHECK(cfg.GetIntValue("Neg") == -12);

  // Append path still works
  CHECK(cfg.ChangeConfigValue("NewKey", "v") == 1);
  CHECK(cfg.ReLoad());
  CHECK(cfg.GetString("NewKey") == "v");
  pass("config_keys_crlf");
}

// ----------------------------------------------------------- //
// Log files
// ----------------------------------------------------------- //

static void test_logfile()
{
  fs::path f = tmp_dir / "test.log";
#if !defined (_WIN32)
  mode_t old = umask(022);
#endif
  {
    LogFile log;
    CHECK(log.OverWrite(f.string()));
    CHECK(log.WriteLine("user bob\nOct 26, 2026 00:00:00 admin login ok\r"));
    CHECK(log.WriteLine("second"));
    log.Close();
  }
  std::string text = read_file(f);
  int lines = 0;
  for(char c : text) if(c == '\n') lines++;
  CHECK(lines == 2);
  CHECK(text.find("user bob\\nOct 26") != std::string::npos);
  CHECK(text.find("login ok\\r") != std::string::npos);
#if !defined (_WIN32)
  umask(old);
  struct stat st;
  CHECK(stat(f.c_str(), &st) == 0);
  CHECK((st.st_mode & 0777) == 0640);

  // Symbolic links are refused
  fs::path target = tmp_dir / "target.txt";
  fs::path link = tmp_dir / "link.log";
  { std::ofstream o(target); o << "keep\n"; }
  fs::create_symlink(target, link);
  {
    LogFile log;
    CHECK(!log.Open(link.string()));
    CHECK(!log.OverWrite(link.string()));
  }
  CHECK(read_file(target) == "keep\n");
  // Append to an existing file keeps working
  {
    LogFile log;
    CHECK(log.Open(f.string()));
    CHECK(log.WriteLine("third"));
  }
  CHECK(read_file(f).find("third") != std::string::npos);
#endif
  pass("logfile_crlf_mode");
}

// ----------------------------------------------------------- //
// Thread pool
// ----------------------------------------------------------- //

static void test_thread_pool()
{
  ThreadPool pool(2);
  auto f1 = pool.Submit([] { return 7; });
  CHECK(f1.get() == 7);

  // Wait() from a task throws instead of deadlocking
  auto f2 = pool.Submit([&pool] { pool.Wait(); return 1; });
  bool threw = false;
  try { f2.get(); } catch(const std::logic_error &) { threw = true; }
  CHECK(threw);
  pool.Wait();

  pool.Shutdown();
  pool.Shutdown(); // Second call does nothing
  CHECK(pool.IsStopping());
  threw = false;
  try { pool.Submit([] { return 1; }); }
  catch(const std::runtime_error &) { threw = true; }
  CHECK(threw);

  // Queued tasks still run before Shutdown() returns
  std::atomic<int> n(0);
  {
    ThreadPool p2(1);
    for(int i = 0; i < 20; i++) p2.Submit([&n] { n++; });
  }
  CHECK(n == 20);
  pass("thread_pool");
}

// ----------------------------------------------------------- //
// Ping reply parser
// ----------------------------------------------------------- //

static void test_ping_parser()
{
  unsigned char pkt[64];
  memset(pkt, 0, sizeof(pkt));
  int ttl = 0;
  // Echo reply without IP header, seq 5
  pkt[0] = 0; pkt[6] = 0; pkt[7] = 5;
  CHECK(gxsPing::ParseEchoReply(pkt, 8, false, false, 0, 5, false, ttl) == 1);
  CHECK(ttl == -1);
  CHECK(gxsPing::ParseEchoReply(pkt, 7, false, false, 0, 5, false, ttl) == 0);
  CHECK(gxsPing::ParseEchoReply(pkt, 8, false, false, 0, 6, false, ttl) == 0);
  // IPv4 raw: short packets and a bad IHL are rejected before buf[8]
  unsigned char ip[64];
  memset(ip, 0, sizeof(ip));
  ip[0] = 0x40; // IHL 0
  CHECK(gxsPing::ParseEchoReply(ip, 9, false, true, 1, 5, true, ttl) == 0);
  ip[0] = 0x45; ip[8] = 33;
  CHECK(gxsPing::ParseEchoReply(ip, 12, false, true, 1, 5, true, ttl) == 0);
  ip[20] = 0; ip[24] = 0; ip[25] = 1; ip[26] = 0; ip[27] = 5;
  CHECK(gxsPing::ParseEchoReply(ip, 28, false, true, 1, 5, true, ttl) == 1);
  CHECK(ttl == 33);
  ip[0] = 0x4F; // IHL 60 > length
  CHECK(gxsPing::ParseEchoReply(ip, 28, false, true, 1, 5, true, ttl) == 0);
  // Source address check
  sockaddr_in a, b;
  memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
  a.sin_family = b.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(0x7f000001);
  b.sin_addr.s_addr = htonl(0x7f000002);
  CHECK(gxsPing::SameAddress((sockaddr *)&a, (sockaddr *)&a));
  CHECK(!gxsPing::SameAddress((sockaddr *)&a, (sockaddr *)&b));
  pass("ping_reply_parser");
}

// ----------------------------------------------------------- //
// db_debug block walk on a damaged file
// ----------------------------------------------------------- //

static void test_db_walk()
{
  fs::path f = tmp_dir / "walk.gxd";
  std::string fname = f.string();
  {
    gxDatabase db;
    CHECK(db.Create(fname.c_str()) == gxDBASE_NO_ERROR);
    char rec[100];
    for(int i = 0; i < 10; i++) {
      memset(rec, 'a' + i, sizeof(rec));
      FAU_t a = db.Alloc(sizeof(rec));
      CHECK(db.Write(rec, sizeof(rec), a) == gxDBASE_NO_ERROR);
    }
    db.Close();
  }
  // Find the block headers
  std::vector<FAU_t> hdrs;
  {
    gxDatabaseDebugManager dm;
    CHECK(dm.BlindOpen(fname.c_str()) == gxDBASE_NO_ERROR);
    FAU_t good, bad;
    CHECK(gxdWalkBlocks(&dm, good, bad));
    CHECK(good == 10 && bad == 0);
    FAU_t a = dm.BlockSearch(0);
    while(a) { hdrs.push_back(a); a = dm.BlockSearch(a + 1); }
    CHECK(hdrs.size() == 10);
  }
  // Corrupt lengths: 0, huge, shorter than a header
  auto set_len = [&](FAU_t at, gxUINT32 len) {
    std::fstream io(fname, std::ios::in | std::ios::out | std::ios::binary);
    io.seekp((std::streamoff)at + 4);
    io.write((const char *)&len, sizeof(len));
  };
  set_len(hdrs[2], 0);
  set_len(hdrs[5], 0xFFFFFFF0u);
  set_len(hdrs[8], 3);
  {
    gxDatabaseDebugManager dm;
    CHECK(dm.BlindOpen(fname.c_str()) == gxDBASE_NO_ERROR);
    auto start = std::chrono::steady_clock::now();
    FAU_t good, bad;
    CHECK(gxdWalkBlocks(&dm, good, bad));
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
    CHECK(good == 7 && bad == 3);

    // Rebuild refuses to overwrite the file being rebuilt
    std::string before = read_file(f);
    CHECK(dm.Rebuild(fname.c_str()) == 0);
    std::string other = (tmp_dir / "." / "walk.gxd").string();
    CHECK(dm.Rebuild(other.c_str()) == 0);
#if !defined (_WIN32)
    fs::path hl = tmp_dir / "walk_link.gxd";
    fs::create_symlink(f, hl);
    CHECK(dm.Rebuild(hl.string().c_str()) == 0);
#endif
    CHECK(read_file(f) == before);
    // A different file works (bad blocks make it return 0, but the
    // good ones are copied)
    fs::path out = tmp_dir / "rebuilt.gxd";
    FAU_t n = 0;
    dm.Rebuild(out.string().c_str(), &n, gxDatabase::gxVersion,
	       gxDatabaseRevisionLetter, (FAU_t)0, 1);
    CHECK(n == 7);
  }
  // A file of nothing but check words and zero lengths
  {
    fs::path junk = tmp_dir / "junk.gxd";
    std::string base = read_file(f).substr(0, (size_t)hdrs[0]);
    std::ofstream o(junk, std::ios::binary);
    o << base;
    gxBlockHeader h;
    h.block_length = 0;
    for(int i = 0; i < 2000; i++) o.write((const char *)&h, sizeof(h));
    o.close();
    gxDatabaseDebugManager dm;
    CHECK(dm.BlindOpen(junk.string().c_str()) == gxDBASE_NO_ERROR);
    FAU_t good, bad;
    CHECK(gxdWalkBlocks(&dm, good, bad));
    CHECK(good == 0 && bad > 0);
  }
  pass("db_walk_damaged");
}

int main()
{
  std::error_code ec;
  tmp_dir = fs::temp_directory_path(ec) /
    ("dr_support_test_" + std::to_string((long long)
#if defined (_WIN32)
					 GetCurrentProcessId()
#else
					 getpid()
#endif
					 ));
  fs::remove_all(tmp_dir, ec);
  fs::create_directories(tmp_dir);

#if !defined (_WIN32)
  test_telnet_recvstring();
  test_telnet_deadline();
  test_telnet_iac();
  test_scomserv();
#else
  printf("SKIP telnet and serial tests (POSIX socket pairs)\n");
#endif
  test_config();
  test_logfile();
  test_thread_pool();
  test_ping_parser();
  test_db_walk();

  fs::remove_all(tmp_dir, ec);
  if(failures) {
    printf("%d test(s) FAILED\n", failures);
    return 1;
  }
  printf("PASS support_security_test (all)\n");
  return 0;
}

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
