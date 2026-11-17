// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdbmux_test.cpp
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Date Last Modified: 11/17/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
Tests of the database object server's multiplex (gxdbmux.h) over a
socketpair: two sessions, one opening channels and one accepting them.
11/17/2026 (7.4): Windows too (a loopback connection in place of the
socketpair), and both_ways_large over TLS: both sides send large
messages at once over one connection (OpenSSL on POSIX, Schannel on
Windows), the case a blocking TLS write would deadlock. It needs
GXDBMUX_PKI=DIR with the files of utils/objrepl/objrepl_certs.sh DIR
--copy-host 127.0.0.1 (ca.pem, copy.pem/.key, primary.pem/.key).

  gxdbmux_test [name]      run every test, or the ones whose name has "name"

Prints PASS/FAIL lines; exit code 0 when every test passed.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"
#include "gxdbmux.h"
#include "gxheader.h"   // 11/16/2026: gxBlockHeader, gxWirePackBlockHeader()
#include "gxstream.h"   // 11/17/2026: both_ways_large over TLS
#include "gxstls.h"
#include <stdlib.h>
#include <memory>

#include <stdio.h>
#include <string.h>
#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace datareel;

static double now_s()
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// A message with a checkable content: channel, index and length
static std::vector<unsigned char> make_msg(uint32_t ch, uint32_t i, size_t n)
{
  std::vector<unsigned char> v(n < 12 ? 12 : n);
  memcpy(v.data(), &ch, 4);
  memcpy(v.data() + 4, &i, 4);
  uint32_t len = (uint32_t)v.size();
  memcpy(v.data() + 8, &len, 4);
  for(size_t k = 12; k < v.size(); k++) v[k] = (unsigned char)(ch * 31 + i * 7 + k);
  return v;
}

static bool check_msg(const std::vector<unsigned char> &v, uint32_t ch, uint32_t i)
{
  if(v.size() < 12) return false;
  uint32_t c, j, len;
  memcpy(&c, v.data(), 4); memcpy(&j, v.data() + 4, 4); memcpy(&len, v.data() + 8, 4);
  if(c != ch || j != i || len != v.size()) return false;
  for(size_t k = 12; k < v.size(); k++) if(v[k] != (unsigned char)(ch * 31 + i * 7 + k)) return false;
  return true;
}

struct Pair {
  std::unique_ptr<gxdMuxSession> cli, srv;
  Pair(const gxdMuxOptions &o, gxdMuxSession::OpenHandler h) {
    std::unique_ptr<gxdMuxTransport> a, b;
    gxdMuxPipe(a, b);
    cli.reset(new gxdMuxSession(a.release(), o));
    srv.reset(new gxdMuxSession(b.release(), o));
    srv->SetOpenHandler(h);
    srv->Start();
    cli->Start();
  }
};

static gxdMuxSession::OpenHandler accept_all()
{
  return [](uint32_t, const std::string &, const std::string &name, const std::string &,
	    std::vector<unsigned char> &, std::string &why) {
    if(name == "refuse-me") { why = "refused by the test"; return false; }
    return true;
  };
}

static std::string fails;
#define CHECK(c, msg) do { if(!(c)) { fails = msg; return false; } } while(0)

// Several channels at once, messages of mixed sizes (some larger than a
// frame), each consumer checks order and content
static bool test_interleaved()
{
  gxdMuxOptions o; o.queue_bytes = 2 * 1024 * 1024; o.max_frame = 256 * 1024; o.quantum = 64 * 1024;
  Pair p(o, accept_all());
  std::string err;
  CHECK(p.cli->Hello(err) == 0, "hello: " + err);
  const uint32_t NCH = 6, NMSG = 400;
  for(uint32_t ch = 1; ch <= NCH; ch++)
    CHECK(p.cli->Open(ch, "repl", "db" + std::to_string(ch), "push", 0, err) == 0, "open: " + err);
  std::atomic<int> bad{0};
  std::vector<std::thread> consumers;
  for(uint32_t k = 0; k < NCH; k++) {
    uint32_t ch; std::string sv, nm, md;
    CHECK(p.srv->AcceptChannel(ch, sv, nm, md, 5000) == 0, "accept");
    consumers.emplace_back([&, ch] {
      std::string e;
      for(uint32_t i = 0; i < NMSG; i++) {
	std::vector<unsigned char> msg; uint64_t seq;
	if(p.srv->ReceiveMessage(ch, msg, seq, 20000, e) != 0 || !check_msg(msg, ch, i)) { bad++; return; }
      }
    });
  }
  std::vector<std::thread> senders;
  for(uint32_t ch = 1; ch <= NCH; ch++) {
    senders.emplace_back([&, ch] {
      std::string e;
      for(uint32_t i = 0; i < NMSG; i++) {
	size_t n = (i % 13 == 0) ? 600 * 1024 : 100 + (i * 977 + ch * 131) % 20000;
	if(p.cli->Send(ch, make_msg(ch, i, n), gxdMUX_LAST, e) != 0) { bad++; return; }
      }
    });
  }
  for(auto &t : senders) t.join();
  for(auto &t : consumers) t.join();
  CHECK(bad == 0, "a message was lost, out of order or damaged");
  return true;
}

// A refused channel and a channel the other side closes with an error
// end only themselves
static bool test_refused_and_error()
{
  gxdMuxOptions o; o.queue_bytes = 1024 * 1024;
  Pair p(o, accept_all());
  std::string err;
  CHECK(p.cli->Hello(err) == 0, "hello");
  CHECK(p.cli->Open(1, "repl", "good", "push", 0, err) == 0, "open good: " + err);
  CHECK(p.cli->Open(2, "repl", "refuse-me", "push", 0, err) != 0, "a refused open succeeded");
  CHECK(err.find("refused by the test") != std::string::npos, "refusal reason lost: " + err);
  CHECK(p.cli->Open(3, "repl", "doomed", "push", 0, err) == 0, "open doomed: " + err);
  uint32_t a, b; std::string sv, nm, md;
  CHECK(p.srv->AcceptChannel(a, sv, nm, md, 5000) == 0 && p.srv->AcceptChannel(b, sv, nm, md, 5000) == 0, "accept");
  p.srv->CloseChannel(3, "boom");
  // The doomed channel ends with the reason; the good one goes on
  std::vector<unsigned char> m = make_msg(1, 0, 5000);
  for(int i = 0; i < 50; i++) {
    if(p.cli->Send(3, m, gxdMUX_LAST, err) != 0) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  CHECK(p.cli->Send(3, m, gxdMUX_LAST, err) != 0 && err.find("boom") != std::string::npos,
	"the closed channel still sends: " + err);
  for(uint32_t i = 0; i < 100; i++) CHECK(p.cli->Send(1, make_msg(1, i, 3000), gxdMUX_LAST, err) == 0, "good send: " + err);
  for(uint32_t i = 0; i < 100; i++) {
    std::vector<unsigned char> msg; uint64_t seq;
    CHECK(p.srv->ReceiveMessage(1, msg, seq, 5000, err) == 0 && check_msg(msg, 1, i), "good receive");
  }
  CHECK(p.cli->Alive() && p.srv->Alive(), "the session died");
  return true;
}

// A large, slowly consumed channel does not hold up a small one
static bool test_slow_large_does_not_block_small()
{
  gxdMuxOptions o; o.queue_bytes = 4 * 1024 * 1024; o.max_frame = 512 * 1024; o.quantum = 128 * 1024;
  Pair p(o, accept_all());
  std::string err;
  CHECK(p.cli->Hello(err) == 0, "hello");
  CHECK(p.cli->Open(1, "repl", "large", "push", 0, err) == 0, "open large");
  CHECK(p.cli->Open(2, "repl", "small", "push", 0, err) == 0, "open small");
  uint32_t a, b; std::string sv, nm, md;
  p.srv->AcceptChannel(a, sv, nm, md, 5000); p.srv->AcceptChannel(b, sv, nm, md, 5000);
  const uint32_t NL = 120, NS = 300;           // 120 x 1 MB, 300 x 2 KB
  std::atomic<bool> large_done{false};
  double small_done = 0, large_end = 0, small_sent = 0, t0 = now_s();
  std::atomic<int> bad{0};
  std::thread cl([&] {                          // Slow consumer: 20 ms a message
    std::string e;
    for(uint32_t i = 0; i < NL; i++) {
      std::vector<unsigned char> msg; uint64_t seq;
      if(p.srv->ReceiveMessage(1, msg, seq, 60000, e) != 0 || !check_msg(msg, 1, i)) { bad++; return; }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    large_end = now_s(); large_done = true;
  });
  std::thread cs([&] {
    std::string e;
    for(uint32_t i = 0; i < NS; i++) {
      std::vector<unsigned char> msg; uint64_t seq;
      if(p.srv->ReceiveMessage(2, msg, seq, 60000, e) != 0 || !check_msg(msg, 2, i)) { bad++; return; }
    }
    small_done = now_s();
  });
  std::thread sl([&] { std::string e; for(uint32_t i = 0; i < NL; i++) p.cli->Send(1, make_msg(1, i, 1024 * 1024), gxdMUX_LAST, e); });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));  // The large backlog first
  std::thread ss([&] {
    std::string e;
    for(uint32_t i = 0; i < NS; i++) { p.cli->Send(2, make_msg(2, i, 2048), gxdMUX_LAST, e); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    small_sent = now_s();
  });
  sl.join(); ss.join(); cs.join(); cl.join();
  CHECK(bad == 0, "a message was lost or damaged");
  double small_t = small_done - t0, large_t = large_end - t0, small_lag = small_done - small_sent;
  printf("    small channel done in %.2fs (%.3fs after its last send), large in %.2fs\n", small_t, small_lag, large_t);
  // 11/17/2026 (7.4): judged by how soon the last small message arrives after it is sent, not against the large
  // channel's time: the small sender's 2 ms sleeps take about 15 ms each on Windows (its timer), so its 300 messages
  // alone take longer than the large channel there. Stuck behind the large backlog, it would wait seconds.
  CHECK(small_lag < 0.5, "the small channel waited behind the large one");
  return true;
}

// A channel whose consumer stops fills its queue; only its own sender
// waits, the others keep flowing, and it catches up when its consumer
// comes back
static bool test_queue_full_holds_only_its_sender()
{
  gxdMuxOptions o; o.queue_bytes = 1024 * 1024; o.max_frame = 128 * 1024;
  Pair p(o, accept_all());
  std::string err;
  CHECK(p.cli->Hello(err) == 0, "hello");
  CHECK(p.cli->Open(1, "repl", "stuck", "push", 0, err) == 0, "open stuck");
  CHECK(p.cli->Open(2, "repl", "free", "push", 0, err) == 0, "open free");
  uint32_t a, b; std::string sv, nm, md;
  p.srv->AcceptChannel(a, sv, nm, md, 5000); p.srv->AcceptChannel(b, sv, nm, md, 5000);
  std::atomic<uint32_t> stuck_sent{0};
  std::thread s1([&] { std::string e; for(uint32_t i = 0; i < 64; i++) { if(p.cli->Send(1, make_msg(1, i, 256 * 1024), gxdMUX_LAST, e) != 0) return; stuck_sent = i + 1; } });
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  uint32_t at_block = stuck_sent;
  // The free channel moves 20 MB meanwhile
  std::thread c2([&] { std::string e; for(uint32_t i = 0; i < 80; i++) { std::vector<unsigned char> m; uint64_t s; p.srv->ReceiveMessage(2, m, s, 10000, e); } });
  double t0 = now_s();
  for(uint32_t i = 0; i < 80; i++) CHECK(p.cli->Send(2, make_msg(2, i, 256 * 1024), gxdMUX_LAST, err) == 0, "free send");
  c2.join();
  double free_t = now_s() - t0;
  CHECK(free_t < 5, "the free channel was held up");
  auto st = p.srv->Status();
  size_t stuck_in = 0;
  for(auto &x : st) if(x.channel == 1) stuck_in = x.in_bytes;
  CHECK(stuck_in <= o.queue_bytes, "the stuck queue grew past its bound");
  CHECK(at_block < 64 && stuck_sent < 64, "the stuck sender was not held back");
  printf("    stuck sender held at %u of 64 messages, queue %zu bytes; free channel moved 20 MB in %.2fs\n",
	 (unsigned)stuck_sent.load(), stuck_in, free_t);
  // Its consumer comes back: everything arrives, in order
  for(uint32_t i = 0; i < 64; i++) {
    std::vector<unsigned char> m; uint64_t s;
    CHECK(p.srv->ReceiveMessage(1, m, s, 10000, err) == 0 && check_msg(m, 1, i), "stuck channel catch-up");
  }
  s1.join();
  return true;
}

// Closing the connection ends every waiting call
static bool test_shutdown_wakes_waiters()
{
  gxdMuxOptions o;
  Pair p(o, accept_all());
  std::string err;
  CHECK(p.cli->Hello(err) == 0, "hello");
  CHECK(p.cli->Open(1, "repl", "x", "push", 0, err) == 0, "open");
  uint32_t a; std::string sv, nm, md;
  p.srv->AcceptChannel(a, sv, nm, md, 5000);
  std::atomic<int> rc{9};
  std::thread w([&] { std::string e; gxdMuxFrame f; rc = p.srv->Receive(1, f, 30000, e); });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  p.cli->Shutdown("test over");
  w.join();
  CHECK(rc == -1, "a waiting receive did not end when the peer closed");
  CHECK(!p.srv->Alive(), "the server session did not notice the close");
  return true;
}

// 11/15/2026 (beta 5): the frame header on the wire is the canonical gxBlockHeader (gxheader.h): 32 bytes, on
// 64-bit Linux the struct's own bytes (what beta 3 sent), unpacked as packed, and gxdMuxEncode() starts with them
static bool test_wire_frame_header_canonical()
{
  gxBlockHeader h;
  h.block_check_word = gxCheckWord;
  h.block_length = (gxUINT32)123456;
  h.block_status = (gxUINT32)0x0201;
  h.block_nd_fptr = (FAU)0;
  unsigned char w[gxWireBlockHeaderSize];
  gxWirePackBlockHeader(h, w);
  CHECK(w[0] == 0xfe && w[1] == 0xfe && w[8] == (123456 & 0xff) && w[16] == 0x01 && w[17] == 0x02, "the packed words");
  for(int i = 2; i < 8; i++) CHECK(w[i] == 0, "the check word is 8 bytes");
  if(sizeof(gxBlockHeader) == gxWireBlockHeaderSize && sizeof(long) == 8)
    CHECK(memcmp(w, &h, gxWireBlockHeaderSize) == 0, "on 64-bit Linux the wire header is the struct's bytes (beta 3)");
  gxBlockHeader b;
  gxWireUnpackBlockHeader(w, b);
  CHECK(b.block_check_word == h.block_check_word && b.block_length == h.block_length &&
	b.block_status == h.block_status && (FAU_t)b.block_nd_fptr == (FAU_t)h.block_nd_fptr, "unpacked as packed");
  gxdMuxFrame f;
  f.kind = 1; f.channel = 7; f.seq = 9; f.flags = 0;
  f.payload.assign(10, 'x');
  std::vector<unsigned char> out;
  gxdMuxEncode(f, out);
  CHECK(out.size() > gxWireBlockHeaderSize, "an encoded frame");
  gxBlockHeader e;
  gxWireUnpackBlockHeader(out.data(), e);
  CHECK(e.block_check_word == gxCheckWord && (size_t)(__ULWORD__)e.block_length == out.size() - gxWireBlockHeaderSize,
	"the frame's header and length");
  return true;
}

// 11/17/2026 (7.4): both sessions send NMSG messages of 900 KB at the
// same time (channel 1 client to server, channel 2 server to client)
// while their consumers read; every message arrives whole and in order
static bool both_ways(gxdMuxSession &cli, gxdMuxSession &srv)
{
  std::string err;
  CHECK(cli.Hello(err) == 0, "hello: " + err);
  CHECK(cli.Open(1, "repl", "up", "push", 0, err) == 0, "open 1: " + err);
  CHECK(cli.Open(2, "repl", "down", "push", 0, err) == 0, "open 2: " + err);
  uint32_t a, b; std::string sv, nm, md;
  CHECK(srv.AcceptChannel(a, sv, nm, md, 5000) == 0 && srv.AcceptChannel(b, sv, nm, md, 5000) == 0, "accept");
  const uint32_t NMSG = 40;
  const size_t SZ = 900 * 1024;
  std::atomic<int> bad{0};
  std::string why;
  std::mutex wm;
  auto note = [&](const std::string &w) { std::lock_guard<std::mutex> g(wm); if(why.empty()) why = w; bad++; };
  std::thread up([&] { std::string e; for(uint32_t i = 0; i < NMSG; i++)
      if(cli.Send(1, make_msg(1, i, SZ), gxdMUX_LAST, e) != 0) { note("client send: " + e); return; } });
  std::thread down([&] { std::string e; for(uint32_t i = 0; i < NMSG; i++)
      if(srv.Send(2, make_msg(2, i, SZ), gxdMUX_LAST, e) != 0) { note("server send: " + e); return; } });
  std::thread rs([&] { std::string e; for(uint32_t i = 0; i < NMSG; i++) {
      std::vector<unsigned char> m; uint64_t q;
      if(srv.ReceiveMessage(1, m, q, 60000, e) != 0 || !check_msg(m, 1, i)) { note("server receive " + std::to_string(i) + ": " + e); return; } } });
  std::thread rc([&] { std::string e; for(uint32_t i = 0; i < NMSG; i++) {
      std::vector<unsigned char> m; uint64_t q;
      if(cli.ReceiveMessage(2, m, q, 60000, e) != 0 || !check_msg(m, 2, i)) { note("client receive " + std::to_string(i) + ": " + e); return; } } });
  up.join(); down.join(); rs.join(); rc.join();
  CHECK(bad == 0, "both ways: " + why);
  printf("    %u messages of %zu KB each way\n", NMSG, SZ / 1024);
  return true;
}

static bool test_both_ways_large_pipe()
{
  gxdMuxOptions o; o.queue_bytes = 4 * 1024 * 1024; o.max_frame = 1024 * 1024;
  Pair p(o, accept_all());
  return both_ways(*p.cli, *p.srv);
}

static bool test_both_ways_large_tls()
{
  const char *pki = getenv("GXDBMUX_PKI");
  CHECK(pki && *pki, "set GXDBMUX_PKI=DIR (utils/objrepl/objrepl_certs.sh DIR --copy-host 127.0.0.1 --apply)");
  std::string d = pki;
  gxsTLSServer server;
  server.SetClientCAFile(d + "/ca.pem");
  server.SetRequireClientAuth(true);
  server.AllowClientCN("objrepl-primary");
  CHECK(server.Listen(0, d + "/copy.pem", d + "/copy.key", std::string(), "127.0.0.1") == 0 && server.Port() > 0,
	"listen: " + server.Error());
  gxStream ss, cs;
  std::string serr;
  std::thread acc([&] { if(server.Accept(ss.Connection()) != 0) serr = server.Error(); });
  gxsTLSConnection &c = cs.Connection();
  c.SetCAFile(d + "/ca.pem");
  c.SetClientCert(d + "/primary.pem", d + "/primary.key");
  c.SetVerifyPeer(true);
  gxSocket sock;
  bool ok = sock.Connect("127.0.0.1", server.Port(), 10) == 0 && c.AttachClient(std::move(sock), "127.0.0.1") == 0;
  acc.join();
  CHECK(ok, "connect: " + c.Error());
  CHECK(serr.empty(), "accept: " + serr);
  gxdMuxOptions o; o.queue_bytes = 4 * 1024 * 1024; o.max_frame = 1024 * 1024;
  std::unique_ptr<gxdMuxSession> srv(new gxdMuxSession(gxdMuxStreamTransport(ss), o));
  std::unique_ptr<gxdMuxSession> cli(new gxdMuxSession(gxdMuxStreamTransport(cs), o));
  srv->SetOpenHandler(accept_all());
  srv->Start();
  cli->Start();
  bool r = both_ways(*cli, *srv);
  cli->Shutdown(); srv->Shutdown();
  cli.reset(); srv.reset();
  return r;
}

// 11/17/2026 (7.4): one side of both_ways_large in its own process, for two machines (a Windows and a Linux peer):
//   gxdbmux_test --peer-serve PKI_DIR PORT        the server (copy.pem/copy.key, client CN objrepl-primary)
//   gxdbmux_test --peer-connect PKI_DIR HOST PORT the client (primary.pem/primary.key; HOST in the server's certificate)
// Each sends NMSG messages of 900 KB on its channel while it receives the other's; prints PASS/FAIL, exit 0 on PASS.
static int peer(bool serve, const std::string &d, const std::string &host, int port)
{
  const uint32_t NMSG = 40; const size_t SZ = 900 * 1024;
  gxStream st;
  gxsTLSServer server;
  if(serve) {
    server.SetClientCAFile(d + "/ca.pem"); server.SetRequireClientAuth(true); server.AllowClientCN("objrepl-primary");
    if(server.Listen(port, d + "/copy.pem", d + "/copy.key", std::string(), "127.0.0.1") != 0) {
      printf("FAIL peer listen: %s\n", server.Error().c_str()); return 1;
    }
    printf("listening on %d\n", server.Port()); fflush(stdout);
    if(server.Accept(st.Connection()) != 0) { printf("FAIL peer accept: %s\n", server.Error().c_str()); return 1; }
  }
  else {
    gxsTLSConnection &c = st.Connection();
    c.SetCAFile(d + "/ca.pem"); c.SetClientCert(d + "/primary.pem", d + "/primary.key"); c.SetVerifyPeer(true);
    gxSocket sock;
    if(sock.Connect(host, port, 10) != 0 || c.AttachClient(std::move(sock), host) != 0) {
      printf("FAIL peer connect: %s\n", c.Error().c_str()); return 1;
    }
  }
  gxdMuxOptions o; o.queue_bytes = 4 * 1024 * 1024; o.max_frame = 1024 * 1024;
  gxdMuxSession mux(gxdMuxStreamTransport(st), o);
  if(serve) mux.SetOpenHandler(accept_all());
  mux.Start();
  std::string err;
  uint32_t mine = serve ? 2 : 1, theirs = serve ? 1 : 2;
  if(!serve) {
    if(mux.Hello(err) != 0 || mux.Open(1, "repl", "up", "push", 0, err) != 0 || mux.Open(2, "repl", "down", "push", 0, err) != 0) {
      printf("FAIL peer open: %s\n", err.c_str()); return 1;
    }
  }
  else {
    uint32_t a, b; std::string sv, nm, md;
    if(mux.AcceptChannel(a, sv, nm, md, 30000) != 0 || mux.AcceptChannel(b, sv, nm, md, 30000) != 0) { printf("FAIL peer accept channels\n"); return 1; }
  }
  std::atomic<int> bad{0};
  std::string why; std::mutex wm;
  auto note = [&](const std::string &w) { std::lock_guard<std::mutex> g(wm); if(why.empty()) why = w; bad++; };
  double t0 = now_s();
  std::thread snd([&] { std::string e; for(uint32_t i = 0; i < NMSG; i++)
      if(mux.Send(mine, make_msg(mine, i, SZ), gxdMUX_LAST, e) != 0) { note("send: " + e); return; } });
  std::thread rcv([&] { std::string e; for(uint32_t i = 0; i < NMSG; i++) {
      std::vector<unsigned char> m; uint64_t q;
      if(mux.ReceiveMessage(theirs, m, q, 120000, e) != 0 || !check_msg(m, theirs, i)) { note("receive " + std::to_string(i) + ": " + e); return; } } });
  snd.join(); rcv.join();
  // both sides done: a short wait so the peer's last frames are read before this side closes
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  mux.Shutdown();
  if(bad) { printf("FAIL peer %s: %s\n", serve ? "server" : "client", why.c_str()); return 1; }
  printf("PASS peer %s: %u messages of %zu KB each way in %.1f s\n", serve ? "server" : "client", NMSG, SZ / 1024, now_s() - t0);
  return 0;
}

int main(int argc, char **argv)
{
  if(argc == 4 && strcmp(argv[1], "--peer-serve") == 0) return peer(true, argv[2], "", atoi(argv[3]));
  if(argc == 5 && strcmp(argv[1], "--peer-connect") == 0) return peer(false, argv[2], argv[3], atoi(argv[4]));
  struct T { const char *name; bool (*fn)(); } tests[] = {
    { "wire_frame_header_canonical", test_wire_frame_header_canonical },
    { "interleaved", test_interleaved },
    { "refused_and_error", test_refused_and_error },
    { "slow_large_does_not_block_small", test_slow_large_does_not_block_small },
    { "queue_full_holds_only_its_sender", test_queue_full_holds_only_its_sender },
    { "shutdown_wakes_waiters", test_shutdown_wakes_waiters },
    { "both_ways_large_pipe", test_both_ways_large_pipe },
    { "both_ways_large_tls", test_both_ways_large_tls },
  };
  int failed = 0;
  for(auto &t : tests) {
    if(argc > 1 && !strstr(t.name, argv[1])) continue;
    fails.clear();
    double t0 = now_s();
    bool ok = t.fn();
    printf("%s mux_%s (%.1fs)%s%s\n", ok ? "PASS" : "FAIL", t.name, now_s() - t0,
	   ok ? "" : ": ", ok ? "" : fails.c_str());
    fflush(stdout);
    if(!ok) failed++;
  }
  return failed ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
