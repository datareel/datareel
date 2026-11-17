// tls_key_test.cpp - 11/17/2026 (7.4): TLS with PEM private keys where no user is signed in. Run it from a console,
// over SSH, or (Windows) as a service under LocalService, the cases DataReel 7.3 failed (the key went to the signed-in
// user's key store). PKI_DIR holds the files of utils/objrepl/objrepl_certs.sh PKI_DIR --copy-host 127.0.0.1:
// ca.pem, copy.pem/copy.key (server), primary.pem/primary.key (client, CN objrepl-primary).
//
//   tls_key_test PKI_DIR PORT [--log FILE]                  console, SSH
//   tls_key_test PKI_DIR PORT --log FILE --service NAME     Windows: run by the service manager (sc start NAME)
//
// Checks: a server with a PEM key, a client that verifies it, data both ways; client certificates (mutual TLS) with a
// PEM key on both sides; a client certificate that is not allowed is refused; two servers with the same certificate,
// one closed while the other keeps serving (7.3 gave both the same stored key name, so closing one deleted the other's
// key); with Schannel, where each key is kept (KeyStorage()). Prints PASS/FAIL lines; exit 0 when all pass.
#include "gxdlcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <vector>
#if defined (_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif
#include "gxstls.h"

using namespace datareel;

static FILE *g_log = 0;
static int g_pass = 0, g_fail = 0;
static void out(const std::string &s)
{
  printf("%s\n", s.c_str()); fflush(stdout);
  if(g_log) { fprintf(g_log, "%s\n", s.c_str()); fflush(g_log); }
}
static void check(bool ok, const std::string &what) { out(std::string(ok ? "PASS " : "FAIL ") + what); (ok ? g_pass : g_fail)++; }

// One server accepting COUNT clients on a thread: each client's line is sent back
struct EchoServer {
  gxsTLSServer srv;
  std::thread t;
  std::string err, storage;
  int port = 0;
  bool start(const std::string &d, int p, bool mutual, int count) {
    if(mutual) { srv.SetClientCAFile(d + "/ca.pem"); srv.SetRequireClientAuth(true); srv.AllowClientCN("objrepl-primary"); }
    if(srv.Listen(p, d + "/copy.pem", d + "/copy.key", std::string(), "127.0.0.1") != 0) { err = srv.Error(); return false; }
    storage = srv.TLS().KeyStorage();
    port = srv.Port();
    t = std::thread([this, count] {
      for(int i = 0; i < count; i++) {
	gxsTLSConnection c;
	if(srv.Accept(c) != 0) { if(err.empty()) err = srv.Error(); continue; }
	std::string line;
	if(c.ReadLine(line)) c.SendLine(line);
	c.Close();
      }
    });
    return true;
  }
  void stop() { if(t.joinable()) t.join(); srv.Close(); }
};

static bool client(const std::string &d, int port, bool with_cert, const std::string &cert, const std::string &key,
		   const std::string &msg, std::string &got, std::string &err, std::string *storage = 0)
{
  gxsTLSConnection c;
  c.SetTimeout(15);
  c.SetCAFile(d + "/ca.pem");
  c.SetVerifyPeer(true);
  if(with_cert) c.SetClientCert(d + "/" + cert, d + "/" + key);
  if(c.Connect("127.0.0.1", port) != 0) { err = c.Error(); return false; }
  if(storage) *storage = c.TLS().KeyStorage();
  if(c.SendLine(msg) < 0 || !c.ReadLine(got)) { err = c.Error(); return false; }
  while(!got.empty() && (got.back() == '\r' || got.back() == '\n')) got.pop_back();
  c.Close();
  return true;
}

static int run_checks(const std::string &d, int base)
{
  std::string got, err, st;
  // 1-3: server certificate with a PEM key
  {
    EchoServer s;
    bool ok = s.start(d, base, false, 2);
    check(ok, "a server loads its certificate and PEM key" + (ok ? " (key " + (s.storage.empty() ? std::string("OpenSSL") : s.storage) + ")" : ": " + s.err));
    if(ok) {
      bool c1 = client(d, s.port, false, "", "", "hello one", got, err);
      check(c1 && got == "hello one", "a client verifies the server and gets its line back" + (c1 ? "" : ": " + err));
      bool c2 = client(d, s.port, false, "", "", std::string(5000, 'x'), got, err);
      check(c2 && got == std::string(5000, 'x'), "a 5000-byte line both ways" + (c2 ? "" : ": " + err));
    }
    s.stop();
  }
  // 4-5: client certificates (mutual TLS), PEM keys on both sides; a client not on the allow list refused
  {
    EchoServer s;
    bool ok = s.start(d, base + 1, true, 2);
    check(ok, "a server that requires client certificates starts" + (ok ? "" : ": " + s.err));
    if(ok) {
      bool c1 = client(d, s.port, true, "primary.pem", "primary.key", "mutual", got, err, &st);
      check(c1 && got == "mutual", "a client with a PEM key and an allowed certificate" + (c1 ? " (key " + (st.empty() ? std::string("OpenSSL") : st) + ")" : ": " + err));
      bool c2 = client(d, s.port, true, "copy.pem", "copy.key", "not allowed", got, err);
      check(!c2, "a client certificate that is not allowed is refused");
    }
    s.stop();
  }
  // 6-7: two servers with the same certificate; the first closed, the second still serves
  {
    EchoServer a, b;
    bool ok = a.start(d, base + 2, false, 1) && b.start(d, base + 3, false, 2);
    check(ok, "two servers with the same certificate and key at once" + (ok ? "" : ": " + a.err + b.err));
    if(ok) {
      bool c1 = client(d, a.port, false, "", "", "a", got, err);
      a.stop();                                   // its context and key are freed
      bool c2 = client(d, b.port, false, "", "", "b after a closed", got, err);
      bool c3 = c2 && client(d, b.port, false, "", "", "b again", got, err);
      check(c1 && c2 && c3 && got == "b again", "the second server still serves after the first one closed" + (c3 ? "" : ": " + err));
    }
    a.stop(); b.stop();
  }
  out(std::to_string(g_pass) + " passed, " + std::to_string(g_fail) + " failed");
  return g_fail ? 1 : 0;
}

#if defined (_WIN32)
static std::string g_name, g_dir;
static int g_base = 0;
static SERVICE_STATUS_HANDLE g_ssh = 0;
static SERVICE_STATUS g_ss;
static void report(DWORD state, DWORD code = 0)
{
  g_ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS; g_ss.dwCurrentState = state;
  g_ss.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP : 0;
  g_ss.dwWin32ExitCode = code ? ERROR_SERVICE_SPECIFIC_ERROR : NO_ERROR; g_ss.dwServiceSpecificExitCode = code;
  SetServiceStatus(g_ssh, &g_ss);
}
static DWORD WINAPI handler(DWORD c, DWORD, LPVOID, LPVOID) { if(c == SERVICE_CONTROL_STOP) report(SERVICE_STOP_PENDING); return NO_ERROR; }
static void WINAPI service_main(DWORD, LPSTR *)
{
  g_ssh = RegisterServiceCtrlHandlerExA(g_name.c_str(), handler, 0);
  if(!g_ssh) return;
  memset(&g_ss, 0, sizeof(g_ss));
  report(SERVICE_RUNNING);
  char user[256] = {0}; DWORD n = sizeof(user); GetUserNameA(user, &n);
  out(std::string("running as a service under ") + user);
  int rc = run_checks(g_dir, g_base);
  report(SERVICE_STOPPED, rc ? 1 : 0);
}
#endif

int main(int argc, char **argv)
{
  if(argc < 3) { fprintf(stderr, "usage: tls_key_test PKI_DIR PORT [--log FILE] [--service NAME]\n"); return 64; }
  std::string dir = argv[1], log, service;
  int base = atoi(argv[2]);
  for(int i = 3; i + 1 < argc; i += 2) {
    if(strcmp(argv[i], "--log") == 0) log = argv[i + 1];
    else if(strcmp(argv[i], "--service") == 0) service = argv[i + 1];
  }
  if(!log.empty()) g_log = fopen(log.c_str(), "w");
  if(!service.empty()) {
#if defined (_WIN32)
    g_name = service; g_dir = dir; g_base = base;
    SERVICE_TABLE_ENTRYA t[] = { { (LPSTR)g_name.c_str(), service_main }, { 0, 0 } };
    if(!StartServiceCtrlDispatcherA(t)) { out("FAIL not started by the service manager (error " + std::to_string(GetLastError()) + ")"); return 1; }
    return 0;
#else
    out("FAIL --service is for Windows"); return 64;
#endif
  }
  return run_checks(dir, base);
}
