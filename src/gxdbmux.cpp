// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdbmux.cpp
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Date Last Modified: 11/08/2026
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

Many databases over one connection (gxdbmux.h).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#if !defined (__WIN32__)

#include "gxdbmux.h"
#include "gxstream.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace datareel {

// ---------------- Encoding ----------------

static void put8(std::vector<unsigned char> &v, uint8_t x) { v.push_back(x); }
static void put32(std::vector<unsigned char> &v, uint32_t x)
{ unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4); }
static void put64(std::vector<unsigned char> &v, uint64_t x)
{ unsigned char b[8]; memcpy(b, &x, 8); v.insert(v.end(), b, b + 8); }
static void putstr(std::vector<unsigned char> &v, const std::string &s)
{ put32(v, (uint32_t)s.size()); v.insert(v.end(), s.begin(), s.end()); }
static void putbytes(std::vector<unsigned char> &v, const std::vector<unsigned char> &b)
{ put64(v, (uint64_t)b.size()); v.insert(v.end(), b.begin(), b.end()); }

struct Rd {
  const unsigned char *p; size_t n, off = 0; bool ok = true;
  Rd(const std::vector<unsigned char> &v) : p(v.data()), n(v.size()) { }
  bool need(size_t k) { if(!ok || n - off < k) { ok = false; return false; } return true; }
  uint32_t u32() { uint32_t x = 0; if(need(4)) { memcpy(&x, p + off, 4); off += 4; } return x; }
  uint64_t u64() { uint64_t x = 0; if(need(8)) { memcpy(&x, p + off, 8); off += 8; } return x; }
  std::string str() {
    uint32_t k = u32();
    if(!need(k)) return std::string();
    std::string s((const char *)p + off, k); off += k; return s;
  }
  std::vector<unsigned char> bytes() {
    uint64_t k = u64();
    if(!need((size_t)k)) return std::vector<unsigned char>();
    std::vector<unsigned char> b(p + off, p + off + k); off += (size_t)k; return b;
  }
};

static const size_t BODY_HDR = 1 + 1 + 4 + 8 + 4;

void gxdMuxEncode(const gxdMuxFrame &f, std::vector<unsigned char> &out)
{
  gxBlockHeader gx;
  uint32_t status = ((uint32_t)(unsigned char)gxSendBlock << 8) & 0xff00;
  status += (unsigned char)gxRemoteDeviceBlock;
  gx.block_check_word = gxCheckWord;
  gx.block_status = status;
  gx.block_length = (gxUINT32)(BODY_HDR + f.payload.size());
  gx.block_nd_fptr = (FAU)0;
  size_t start = out.size();
  out.resize(start + gxSTREAM_WIRE_HEADER);                 // 11/16/2026: the fixed wire form (gxstream.h)
  gxStreamPackHeader(gx, out.data() + start);
  put8(out, gxdMUX_MARK);
  put8(out, f.kind);
  put32(out, f.channel);
  put64(out, f.seq);
  put32(out, f.flags);
  out.insert(out.end(), f.payload.begin(), f.payload.end());
}

int gxdMuxDecodeBody(const unsigned char *p, size_t n, gxdMuxFrame &f)
{
  if(n < BODY_HDR || p[0] != gxdMUX_MARK) return -1;
  f.kind = p[1];
  memcpy(&f.channel, p + 2, 4);
  memcpy(&f.seq, p + 6, 8);
  memcpy(&f.flags, p + 14, 4);
  f.payload.assign(p + BODY_HDR, p + n);
  if(f.kind < gxdMUX_HELLO || f.kind > gxdMUX_CLOSE) return -1;
  return 0;
}

// ---------------- Transports ----------------

class FdTransport : public gxdMuxTransport {
public:
  explicit FdTransport(int f) : fd(f) {
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    if(pipe(wp) != 0) { wp[0] = wp[1] = -1; }
    else {
      for(int i = 0; i < 2; i++) fcntl(wp[i], F_SETFL, fcntl(wp[i], F_GETFL, 0) | O_NONBLOCK);
    }
  }
  ~FdTransport() override {
    if(wp[0] >= 0) { close(wp[0]); close(wp[1]); }
  }
  int Wait(bool want_write, int timeout_ms) override {
    if(extra_ready()) return 1;
    struct pollfd pf[2];
    pf[0].fd = fd; pf[0].events = (short)(POLLIN | (want_write || out_wants() ? POLLOUT : 0)); pf[0].revents = 0;
    pf[1].fd = wp[0]; pf[1].events = POLLIN; pf[1].revents = 0;
    int r = poll(pf, wp[0] >= 0 ? 2 : 1, timeout_ms);
    if(r < 0) return errno == EINTR ? 0 : -1;
    if(wp[0] >= 0 && (pf[1].revents & POLLIN)) {
      char b[64];
      while(read(wp[0], b, sizeof(b)) > 0) { }
    }
    int mask = 0;
    if(pf[0].revents & (POLLIN | POLLHUP | POLLERR)) mask |= 1;
    if(pf[0].revents & POLLOUT) mask |= 2;
    return mask;
  }
  long ReadSome(std::vector<unsigned char> &buf, std::string &err) override {
    long total = 0;
    unsigned char b[65536];
    for(;;) {
      ssize_t r = read(fd, b, sizeof(b));
      if(r > 0) { buf.insert(buf.end(), b, b + r); total += r; continue; }
      if(r == 0) { if(total) return total; err = "connection closed"; return -1; }
      if(errno == EAGAIN || errno == EWOULDBLOCK) return total;
      if(errno == EINTR) continue;
      err = std::string("read: ") + strerror(errno);
      return -1;
    }
  }
  long WriteSome(const unsigned char *p, size_t n, std::string &err) override {
    ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
    if(w >= 0) return (long)w;
    if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
    err = std::string("write: ") + strerror(errno);
    return -1;
  }
  void Wake() override {
    if(wp[1] >= 0) { char c = 1; if(write(wp[1], &c, 1) < 0) { } }
  }
  void Close() override {
    if(fd >= 0 && own) { close(fd); }
    fd = -1;
  }
protected:
  virtual bool extra_ready() { return false; }
  virtual bool out_wants() { return false; }
  int fd;
  int wp[2];
  bool own = true;
};

// The TLS connection of a gxStream. The stream keeps the socket and the
// SSL object; this transport only reads and writes them, without
// blocking, from the session's I/O thread.
class TlsTransport : public FdTransport {
public:
  TlsTransport(gxStream &s) : FdTransport(s.Connection().Socket().GetSocket()), st(s) {
    own = false;
    ssl = (SSL *)st.Connection().TLS().GetSSL();
    if(ssl) SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    std::string b = st.Connection().TakeBuffered();
    pending.assign(b.begin(), b.end());
  }
  long ReadSome(std::vector<unsigned char> &buf, std::string &err) override {
    long total = 0;
    if(!pending.empty()) {
      buf.insert(buf.end(), pending.begin(), pending.end());
      total += (long)pending.size();
      pending.clear();
    }
    if(!ssl) { err = "no TLS session"; return -1; }
    read_wants_write = false;
    unsigned char b[16384];
    for(;;) {
      ERR_clear_error();
      int r = SSL_read(ssl, b, (int)sizeof(b));
      if(r > 0) { buf.insert(buf.end(), b, b + r); total += r; continue; }
      int e = SSL_get_error(ssl, r);
      if(e == SSL_ERROR_WANT_READ) return total;
      if(e == SSL_ERROR_WANT_WRITE) { read_wants_write = true; return total; }
      if(total) return total;
      err = e == SSL_ERROR_ZERO_RETURN ? "connection closed" : "TLS read failed";
      return -1;
    }
  }
  long WriteSome(const unsigned char *p, size_t n, std::string &err) override {
    if(!ssl) { err = "no TLS session"; return -1; }
    if(n > 1024 * 1024) n = 1024 * 1024;
    ERR_clear_error();
    int r = SSL_write(ssl, p, (int)n);
    if(r > 0) return r;
    int e = SSL_get_error(ssl, r);
    if(e == SSL_ERROR_WANT_WRITE || e == SSL_ERROR_WANT_READ) return 0;
    err = "TLS write failed";
    return -1;
  }
protected:
  bool extra_ready() override { return !pending.empty() || (ssl && SSL_pending(ssl) > 0); }
  bool out_wants() override { return read_wants_write; }
private:
  gxStream &st;
  SSL *ssl = 0;
  std::vector<unsigned char> pending;
  bool read_wants_write = false;
};

gxdMuxTransport *gxdMuxStreamTransport(gxStream &s) { return new TlsTransport(s); }

void gxdMuxPipe(std::unique_ptr<gxdMuxTransport> &a, std::unique_ptr<gxdMuxTransport> &b)
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { a.reset(); b.reset(); return; }
  a.reset(new FdTransport(sv[0]));
  b.reset(new FdTransport(sv[1]));
}

// ---------------- Session ----------------

struct gxdMuxSession::Channel {
  uint32_t id = 0;
  std::string service, name, mode;
  enum State { OPENING, OPEN, CLOSED } state = OPENING;
  std::string error;
  std::deque<gxdMuxFrame> inq;
  size_t in_bytes = 0;
  std::deque<gxdMuxFrame> outq;
  size_t out_bytes = 0;
  uint64_t credit = 0;        // Bytes the peer allows us to send
  uint64_t granted = 0;       // Bytes we allowed the peer in total
  uint64_t received = 0;      // DATA bytes the peer sent in total
  uint64_t consumed = 0;      // Bytes taken since the last CREDIT
  uint64_t deficit = 0;       // Round-robin
  uint64_t seq_out = 0;
  uint64_t sent_frames = 0, received_frames = 0, ack_seq = 0, ack_lsn = 0;
  std::vector<unsigned char> extra;
};

gxdMuxSession::gxdMuxSession(gxdMuxTransport *t, const gxdMuxOptions &o) : tr(t), opt(o)
{
  if(opt.max_frame > opt.queue_bytes) opt.max_frame = opt.queue_bytes;
  if(opt.max_frame < 1024) opt.max_frame = 1024;
  if(opt.quantum < 1024) opt.quantum = 1024;
}

gxdMuxSession::~gxdMuxSession() { Shutdown(); }

std::string gxdMuxSession::Error()
{
  std::lock_guard<std::mutex> g(m);
  return dead_reason;
}

gxdMuxSession::Channel *gxdMuxSession::get(uint32_t ch)
{
  auto it = chans.find(ch);
  return it == chans.end() ? 0 : it->second.get();
}

int gxdMuxSession::Start(const gxdMuxFrame *first)
{
  if(!tr) return -1;
  if(first) {
    gxdMuxFrame f = *first;
    std::lock_guard<std::mutex> g(m);
    handle(f);
  }
  iot = std::thread(&gxdMuxSession::io, this);
  return 0;
}

void gxdMuxSession::fail_locked(const std::string &why)
{
  if(dead) return;
  dead = true;
  dead_reason = why.empty() ? "session closed" : why;
  for(auto &e : chans) {
    Channel *c = e.second.get();
    if(c->state != Channel::CLOSED) { c->state = Channel::CLOSED; if(c->error.empty()) c->error = dead_reason; }
  }
  cv.notify_all();
}

void gxdMuxSession::Shutdown(const std::string &why)
{
  {
    std::lock_guard<std::mutex> g(m);
    fail_locked(why);
  }
  if(tr) tr->Wake();
  if(iot.joinable() && iot.get_id() != std::this_thread::get_id()) iot.join();
  if(tr) tr->Close();
}

void gxdMuxSession::push_ctrl(uint8_t kind, uint32_t ch, const std::vector<unsigned char> &payload)
{
  gxdMuxFrame f;
  f.kind = kind; f.channel = ch; f.payload = payload;
  ctrl.push_back(std::move(f));
}

// Under the lock: the next frame to send, encoded onto "out"
bool gxdMuxSession::next_out(std::vector<unsigned char> &out)
{
  if(!ctrl.empty()) {
    gxdMuxEncode(ctrl.front(), out);
    ctrl.pop_front();
    return true;
  }
  if(chans.empty()) return false;
  // Deficit round-robin over the channels that have data and credit,
  // starting after the last one served
  for(int pass = 0; pass < 2; pass++) {
    auto it = chans.upper_bound(rr_last);
    for(size_t k = 0; k < chans.size(); k++, ++it) {
      if(it == chans.end()) it = chans.begin();
      Channel *c = it->second.get();
      if(c->state != Channel::OPEN || c->outq.empty()) { c->deficit = 0; continue; }
      size_t sz = c->outq.front().payload.size();
      if(sz > c->credit) continue;                 // Waits for credit
      if(c->deficit < sz) {
	c->deficit += opt.quantum;
	if(c->deficit < sz) continue;              // A frame larger than a quantum: next round
      }
      gxdMuxFrame &f = c->outq.front();
      gxdMuxEncode(f, out);
      c->deficit -= sz;
      c->credit -= sz;
      c->out_bytes -= sz;
      c->sent_frames++;
      c->outq.pop_front();
      if(c->outq.empty()) c->deficit = 0;
      // Stay on this channel while its deficit covers its next frame
      if(c->outq.empty() || c->deficit < c->outq.front().payload.size()) rr_last = c->id;
      else rr_last = c->id ? c->id - 1 : 0;
      cv.notify_all();                             // Room for Send()
      return true;
    }
  }
  return false;
}

void gxdMuxSession::io()
{
  std::vector<unsigned char> in, out;
  size_t in_off = 0, out_off = 0;
  std::string err;
  while(!dead) {
    bool progress = false;
    if(out_off >= out.size()) {
      out.clear(); out_off = 0;
      std::lock_guard<std::mutex> g(m);
      while(out.size() < 512 * 1024 && next_out(out)) { }
    }
    if(out_off < out.size()) {
      long w = tr->WriteSome(out.data() + out_off, out.size() - out_off, err);
      if(w < 0) { std::lock_guard<std::mutex> g(m); fail_locked(err); break; }
      if(w > 0) { out_off += (size_t)w; progress = true; }
    }
    size_t before = in.size();
    long r = tr->ReadSome(in, err);
    if(r < 0) { std::lock_guard<std::mutex> g(m); fail_locked(err); break; }
    if(in.size() > before) {
      progress = true;
      std::lock_guard<std::mutex> g(m);
      while(in.size() - in_off >= gxSTREAM_WIRE_HEADER) {
	gxBlockHeader gx;
	int uh = gxStreamUnpackHeader(in.data() + in_off, gx);
	if(uh != 0 || gx.block_check_word != gxCheckWord || gx.block_length > opt.max_block) {
	  fail_locked(uh == 1 || uh == 2 ? "the peer uses another gxStream block header (DataReel before 7.4?)" : "bad block from the peer"); break;
	}
	size_t need = gxSTREAM_WIRE_HEADER + (size_t)gx.block_length;
	if(in.size() - in_off < need) break;
	unsigned char net = (unsigned char)((gx.block_status >> 8) & 0xff);
	if(net == (unsigned char)gxCloseConnection) { fail_locked("closed by the peer"); break; }
	gxdMuxFrame f;
	if(net != (unsigned char)gxSendBlock ||
	   gxdMuxDecodeBody(in.data() + in_off + gxSTREAM_WIRE_HEADER, gx.block_length, f) != 0) {
	  fail_locked("not a multiplex frame"); break;
	}
	in_off += need;
	handle(f);
      }
      if(in_off > 0 && (in_off == in.size() || in_off > 4 * 1024 * 1024)) {
	in.erase(in.begin(), in.begin() + (long)in_off);
	in_off = 0;
      }
    }
    if(!progress && !dead) tr->Wait(out_off < out.size(), 200);
  }
  std::lock_guard<std::mutex> g(m);
  cv.notify_all();
}

// Under the lock: a frame from the peer
void gxdMuxSession::handle(gxdMuxFrame &f)
{
  Rd r(f.payload);
  Channel *c = get(f.channel);
  switch(f.kind) {
  case gxdMUX_HELLO: {
    std::string magic = r.str(); uint32_t ver = r.u32();
    if(!r.ok || magic != "GXDBMUX" || ver != gxdMUX_VERSION) { fail_locked("incompatible multiplex peer"); break; }
    hello_seen = true;
    if(!hello_sent) {
      std::vector<unsigned char> p; putstr(p, "GXDBMUX"); put32(p, gxdMUX_VERSION);
      push_ctrl(gxdMUX_HELLO, 0, p);
      hello_sent = true;
    }
    break;
  }
  case gxdMUX_OPEN: {
    std::string service = r.str(), name = r.str(), mode = r.str();
    uint64_t credit = r.u64();
    std::vector<unsigned char> p;
    if(!r.ok || f.channel == 0) { putstr(p, "bad open"); push_ctrl(gxdMUX_REFUSED, f.channel, p); break; }
    if(c && c->state != Channel::CLOSED) { putstr(p, "channel in use"); push_ctrl(gxdMUX_REFUSED, f.channel, p); break; }
    std::vector<unsigned char> extra;
    std::string why = "no service";
    bool ok = on_open ? on_open(f.channel, service, name, mode, extra, why) : false;
    if(!ok) { putstr(p, why); push_ctrl(gxdMUX_REFUSED, f.channel, p); chans.erase(f.channel); break; }
    std::unique_ptr<Channel> nc(new Channel);
    nc->id = f.channel; nc->service = service; nc->name = name; nc->mode = mode;
    nc->state = Channel::OPEN;
    nc->credit = credit;
    nc->granted = opt.queue_bytes;
    chans[f.channel] = std::move(nc);
    put64(p, opt.queue_bytes); putbytes(p, extra);
    push_ctrl(gxdMUX_OPENED, f.channel, p);
    accepted.push_back(f.channel);
    break;
  }
  case gxdMUX_OPENED:
    if(c && c->state == Channel::OPENING) {
      c->credit = r.u64(); c->extra = r.bytes();
      c->state = r.ok ? Channel::OPEN : Channel::CLOSED;
      if(!r.ok) c->error = "bad opened";
    }
    break;
  case gxdMUX_REFUSED:
    if(c) { c->state = Channel::CLOSED; c->error = r.str(); if(c->error.empty()) c->error = "refused"; }
    break;
  case gxdMUX_DATA:
    if(!c || c->state != Channel::OPEN) break;  // A closed channel's late frames
    c->received += f.payload.size();
    if(c->received > c->granted) {
      std::vector<unsigned char> p; putstr(p, "over credit");
      push_ctrl(gxdMUX_ERROR, f.channel, p);
      c->state = Channel::CLOSED; c->error = "the peer sent over its credit";
      break;
    }
    c->in_bytes += f.payload.size();
    c->received_frames++;
    c->inq.push_back(std::move(f));
    break;
  case gxdMUX_CREDIT:
    if(c) c->credit += r.u64();
    break;
  case gxdMUX_ACK:
    if(c) {
      uint64_t s = r.u64(), l = r.u64();
      if(r.ok) { c->ack_seq = s; c->ack_lsn = l; c->inq.push_back(std::move(f)); }
    }
    break;
  case gxdMUX_ERROR:
    if(c) { c->state = Channel::CLOSED; c->error = r.str(); if(c->error.empty()) c->error = "error from the peer"; }
    break;
  case gxdMUX_CLOSE:
    if(f.channel == 0) fail_locked("closed by the peer");
    else if(c) { c->state = Channel::CLOSED; if(c->error.empty()) c->error = "closed by the peer"; }
    break;
  }
  cv.notify_all();
}

int gxdMuxSession::Hello(std::string &err, int timeout_ms)
{
  std::unique_lock<std::mutex> l(m);
  if(!hello_sent) {
    std::vector<unsigned char> p; putstr(p, "GXDBMUX"); put32(p, gxdMUX_VERSION);
    push_ctrl(gxdMUX_HELLO, 0, p);
    hello_sent = true;
  }
  tr->Wake();
  if(!cv.wait_for(l, std::chrono::milliseconds(timeout_ms), [&] { return hello_seen || dead; }) ||
     !hello_seen) {
    err = dead ? dead_reason : "no hello from the server";
    return -1;
  }
  return 0;
}

int gxdMuxSession::Open(uint32_t ch, const std::string &service, const std::string &name,
			const std::string &mode, std::vector<unsigned char> *extra,
			std::string &err, int timeout_ms)
{
  std::unique_lock<std::mutex> l(m);
  if(dead) { err = dead_reason; return -1; }
  Channel *old = get(ch);
  if(ch == 0 || (old && old->state != Channel::CLOSED)) { err = "channel in use"; return -1; }
  std::unique_ptr<Channel> nc(new Channel);
  nc->id = ch; nc->service = service; nc->name = name; nc->mode = mode;
  nc->granted = opt.queue_bytes;
  Channel *c = nc.get();
  chans[ch] = std::move(nc);
  std::vector<unsigned char> p;
  putstr(p, service); putstr(p, name); putstr(p, mode); put64(p, opt.queue_bytes);
  push_ctrl(gxdMUX_OPEN, ch, p);
  tr->Wake();
  cv.wait_for(l, std::chrono::milliseconds(timeout_ms), [&] { return c->state != Channel::OPENING || dead; });
  if(c->state == Channel::OPEN) {
    if(extra) *extra = c->extra;
    return 0;
  }
  err = c->state == Channel::OPENING ? "no answer to the open" : c->error;
  if(c->state == Channel::OPENING) { c->state = Channel::CLOSED; c->error = err; }
  return -1;
}

int gxdMuxSession::Send(uint32_t ch, const void *vp, size_t n, uint32_t flags, std::string &err)
{
  const unsigned char *p = (const unsigned char *)vp;
  std::unique_lock<std::mutex> l(m);
  Channel *c = get(ch);
  if(!c) { err = "no channel"; return -1; }
  size_t off = 0;
  do {
    cv.wait(l, [&] { return dead || c->state != Channel::OPEN || c->out_bytes < opt.queue_bytes; });
    if(c->state != Channel::OPEN) { err = c->error.empty() ? "channel closed" : c->error; return -1; }
    size_t k = std::min(n - off, opt.max_frame);
    gxdMuxFrame f;
    f.kind = gxdMUX_DATA; f.channel = ch; f.seq = ++c->seq_out;
    f.flags = (off + k >= n) ? flags : 0;
    f.payload.assign(p + off, p + off + k);
    c->out_bytes += k;
    c->outq.push_back(std::move(f));
    off += k;
    tr->Wake();
  } while(off < n);
  return 0;
}

int gxdMuxSession::Receive(uint32_t ch, gxdMuxFrame &f, int timeout_ms, std::string &err)
{
  std::unique_lock<std::mutex> l(m);
  Channel *c = get(ch);
  if(!c) { err = "no channel"; return -1; }
  if(!cv.wait_for(l, std::chrono::milliseconds(timeout_ms),
		  [&] { return !c->inq.empty() || c->state == Channel::CLOSED || dead; }))
    return 1;
  if(c->inq.empty()) { err = c->error.empty() ? "channel closed" : c->error; return -1; }
  f = std::move(c->inq.front());
  c->inq.pop_front();
  if(f.kind == gxdMUX_DATA) {
    c->in_bytes -= f.payload.size();
    c->consumed += f.payload.size();
    // Give the room back: in quarters of the queue, or when it is empty
    if(c->state == Channel::OPEN &&
       (c->consumed >= opt.queue_bytes / 4 || (c->inq.empty() && c->consumed > 0))) {
      std::vector<unsigned char> p; put64(p, c->consumed);
      push_ctrl(gxdMUX_CREDIT, ch, p);
      c->granted += c->consumed;
      c->consumed = 0;
      tr->Wake();
    }
  }
  return 0;
}

int gxdMuxSession::ReceiveMessage(uint32_t ch, std::vector<unsigned char> &msg, uint64_t &last_seq,
				  int timeout_ms, std::string &err)
{
  msg.clear();
  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  for(;;) {
    int left = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
    if(left < 0) left = 0;
    gxdMuxFrame f;
    int rc = Receive(ch, f, left, err);
    if(rc != 0) return rc;
    if(f.kind != gxdMUX_DATA) continue;
    msg.insert(msg.end(), f.payload.begin(), f.payload.end());
    last_seq = f.seq;
    if(f.flags & gxdMUX_LAST) return 0;
  }
}

int gxdMuxSession::Ack(uint32_t ch, uint64_t seq, uint64_t lsn)
{
  std::lock_guard<std::mutex> g(m);
  if(dead) return -1;
  std::vector<unsigned char> p; put64(p, seq); put64(p, lsn);
  push_ctrl(gxdMUX_ACK, ch, p);
  tr->Wake();
  return 0;
}

void gxdMuxSession::CloseChannel(uint32_t ch, const std::string &why)
{
  std::lock_guard<std::mutex> g(m);
  Channel *c = get(ch);
  if(!c || c->state == Channel::CLOSED) return;
  c->state = Channel::CLOSED;
  c->error = why.empty() ? "closed" : why;
  c->outq.clear(); c->out_bytes = 0;
  std::vector<unsigned char> p;
  if(why.empty()) push_ctrl(gxdMUX_CLOSE, ch, p);
  else { putstr(p, why); push_ctrl(gxdMUX_ERROR, ch, p); }
  tr->Wake();
  cv.notify_all();
}

int gxdMuxSession::AcceptChannel(uint32_t &ch, std::string &service, std::string &name,
				 std::string &mode, int timeout_ms)
{
  std::unique_lock<std::mutex> l(m);
  if(!cv.wait_for(l, std::chrono::milliseconds(timeout_ms), [&] { return !accepted.empty() || dead; }))
    return 1;
  if(accepted.empty()) return -1;
  ch = accepted.front();
  accepted.pop_front();
  Channel *c = get(ch);
  if(!c) return 1;
  service = c->service; name = c->name; mode = c->mode;
  return 0;
}

std::vector<gxdMuxChannelStatus> gxdMuxSession::Status()
{
  std::lock_guard<std::mutex> g(m);
  std::vector<gxdMuxChannelStatus> v;
  for(auto &e : chans) {
    Channel *c = e.second.get();
    gxdMuxChannelStatus s;
    s.channel = c->id; s.service = c->service; s.name = c->name; s.mode = c->mode;
    s.state = c->state == Channel::OPEN ? "open" : c->state == Channel::OPENING ? "opening" : "closed";
    s.error = c->error;
    s.in_frames = c->inq.size(); s.in_bytes = c->in_bytes;
    s.out_frames = c->outq.size(); s.out_bytes = c->out_bytes;
    s.credit = c->credit;
    s.sent_frames = c->sent_frames; s.received_frames = c->received_frames;
    s.last_ack_seq = c->ack_seq; s.last_ack_lsn = c->ack_lsn;
    v.push_back(s);
  }
  return v;
}

} // namespace datareel

#endif // !__WIN32__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
