// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdbmux.h
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Date Last Modified: 11/08/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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

Many databases over one connection (the database object server's
multiplex; docs/DATABASE_OBJECT_SERVER_DESIGN.md sections 4 and 5).

A gxdMuxSession carries channels over one connection. A channel is one
database, or one replication set, and has its own:
  - order: frames arrive in the order they were sent (seq);
  - flow control: the receiver grants credit (bytes) for the free room
    in that channel's bounded queue. A sender with no credit waits, and
    only that channel waits; the others keep flowing;
  - queue: received frames wait in the channel's queue until that
    channel's consumer (an applier thread, a client call) takes them,
    so a slow consumer never blocks the connection;
  - errors: an error or a refused open closes that channel only.

One I/O thread per session owns the connection: a non-blocking socket,
read and written with poll(), so reads and writes never wait for each
other (no deadlock when both sides send at once) and a TLS connection
is only ever used by that one thread (OpenSSL does not allow two). It
sends control frames first, then data frames by deficit round-robin
over the channels that have data and credit (quantum bytes a turn), so
a channel with a large backlog cannot starve a small one. Callers on
other threads only touch queues and wake the I/O thread.

Wire: each frame is one gxStream block (gxBlockHeader with net status
'S', then the body). Body: u8 'M', u8 kind, u32 channel, u64 seq, u32
flags, payload (native byte order). The leading 'M' keeps multiplex
sessions apart from gxrepl protocol 2 messages, whose first byte is an
op code (1 to 101). Payload of each kind:
  HELLO     str "GXDBMUX", u32 version
  OPEN      str service, str name, str mode, u64 credit (the opener's queue)
  OPENED    u64 credit (the accepter's queue), bytes extra (service data)
  REFUSED   str reason
  DATA      service data
  CREDIT    u64 bytes
  ACK       u64 seq, u64 lsn (durable apply, section 6 of the design)
  ERROR     str reason (closes the channel)
  CLOSE     nothing (channel 0: the connection)
(str = u32 length + bytes, bytes = u64 length + bytes)

Changes:
==============================================================
11/08/2026: New in DataReel 7.3.1 (phase 2 step 2).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_MUX_HPP__
#define __GX_DATABASE_MUX_HPP__

#include "gxdlcode.h"

#include <stdint.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace datareel {

class gxStream;

enum gxdMuxKind {
  gxdMUX_HELLO = 1, gxdMUX_OPEN = 2, gxdMUX_OPENED = 3, gxdMUX_REFUSED = 4,
  gxdMUX_DATA = 5, gxdMUX_CREDIT = 6, gxdMUX_ACK = 7, gxdMUX_ERROR = 8,
  gxdMUX_CLOSE = 9
};

const uint32_t gxdMUX_LAST = 1;      // flags: the last frame of a message
const uint32_t gxdMUX_VERSION = 1;
const unsigned char gxdMUX_MARK = 'M';

struct GXDLCODE_API gxdMuxFrame {
  uint8_t kind = 0;
  uint32_t channel = 0;
  uint64_t seq = 0;
  uint32_t flags = 0;
  std::vector<unsigned char> payload;
};

// Non-blocking byte transport used by the session's I/O thread only
class GXDLCODE_API gxdMuxTransport {
public:
  virtual ~gxdMuxTransport() { }
  // Wait up to timeout_ms until there is input, or output room when
  // want_write, or Wake(). Returns a mask (1 input, 2 output room), 0 on
  // a timeout or wake, -1 on an error.
  virtual int Wait(bool want_write, int timeout_ms) = 0;
  // Append the bytes there are now. Returns the count (0: none now), or
  // -1 if the connection closed or failed (err).
  virtual long ReadSome(std::vector<unsigned char> &buf, std::string &err) = 0;
  // Write what fits now. Returns the bytes written (0: none now), or -1.
  virtual long WriteSome(const unsigned char *p, size_t n, std::string &err) = 0;
  virtual void Wake() = 0;   // Any thread: end the current Wait()
  virtual void Close() = 0;
};

// The TLS connection of a gxStream, taken over after its last block
// read with gxStream calls (bytes it had buffered are taken first)
GXDLCODE_API gxdMuxTransport *gxdMuxStreamTransport(gxStream &s);
// Two connected plain transports over a socketpair (tests)
GXDLCODE_API void gxdMuxPipe(std::unique_ptr<gxdMuxTransport> &a,
			     std::unique_ptr<gxdMuxTransport> &b);

// One frame as a gxStream block (header + body), and back
GXDLCODE_API void gxdMuxEncode(const gxdMuxFrame &f, std::vector<unsigned char> &out);
// The body of an 'S' block already read; 0, or -1 if it is not a frame
GXDLCODE_API int gxdMuxDecodeBody(const unsigned char *p, size_t n, gxdMuxFrame &f);

struct GXDLCODE_API gxdMuxOptions {
  size_t queue_bytes = 16 * 1024 * 1024; // Each channel's receive queue
  size_t quantum = 256 * 1024;           // Round-robin bytes a turn
  size_t max_frame = 1024 * 1024;        // Largest data frame payload
  size_t max_block = 17 * 1024 * 1024;   // Largest block accepted
};

struct GXDLCODE_API gxdMuxChannelStatus {
  uint32_t channel = 0;
  std::string service, name, mode, state, error;
  size_t in_frames = 0, in_bytes = 0;     // Waiting in the receive queue
  size_t out_frames = 0, out_bytes = 0;   // Waiting to be sent
  uint64_t credit = 0;                    // Bytes the peer allows now
  uint64_t sent_frames = 0, received_frames = 0;
  uint64_t last_ack_seq = 0, last_ack_lsn = 0;
};

class GXDLCODE_API gxdMuxSession {
public:
  // OPEN on the accepting side: return true to accept (extra goes back
  // in OPENED), false to refuse with "why". Called on the I/O thread:
  // keep it short.
  typedef std::function<bool(uint32_t ch, const std::string &service,
			     const std::string &name, const std::string &mode,
			     std::vector<unsigned char> &extra,
			     std::string &why)> OpenHandler;

  // The session owns the transport
  gxdMuxSession(gxdMuxTransport *t, const gxdMuxOptions &o = gxdMuxOptions());
  ~gxdMuxSession();

  void SetOpenHandler(OpenHandler h) { on_open = h; }
  // Start the I/O thread. A first frame a server already read (the
  // session's first block) is handled first.
  int Start(const gxdMuxFrame *first = 0);
  // Close the connection, end the I/O thread, wake every waiting call
  void Shutdown(const std::string &why = std::string());
  bool Alive() const { return !dead; }
  std::string Error();

  // Client: HELLO, and wait for the server's
  int Hello(std::string &err, int timeout_ms = 10000);
  // Client: open a channel. 0 opened, -1 refused or failed (err).
  int Open(uint32_t ch, const std::string &service, const std::string &name,
	   const std::string &mode, std::vector<unsigned char> *extra,
	   std::string &err, int timeout_ms = 30000);
  // Data on an open channel, split into frames of at most max_frame
  // bytes (flags go on the last frame). Waits while the channel already
  // has queue_bytes waiting to be sent. 0, or -1 if the channel closed.
  int Send(uint32_t ch, const void *p, size_t n, uint32_t flags, std::string &err);
  int Send(uint32_t ch, const std::vector<unsigned char> &v, uint32_t flags, std::string &err) {
    return Send(ch, v.data(), v.size(), flags, err);
  }
  // The next DATA or ACK frame of a channel, in order. 0 a frame, 1 a
  // timeout, -1 the channel is closed and empty (err says why).
  int Receive(uint32_t ch, gxdMuxFrame &f, int timeout_ms, std::string &err);
  // A whole message: DATA frames up to the one with gxdMUX_LAST, joined
  // (ACK frames on the way are only counted in the status)
  int ReceiveMessage(uint32_t ch, std::vector<unsigned char> &msg, uint64_t &last_seq,
		     int timeout_ms, std::string &err);
  int Ack(uint32_t ch, uint64_t seq, uint64_t lsn);
  // Close a channel: with a reason an ERROR frame, without a CLOSE
  void CloseChannel(uint32_t ch, const std::string &why = std::string());
  // Accepting side: the next channel the peer opened and this side
  // accepted. 0, 1 timeout, -1 the session ended.
  int AcceptChannel(uint32_t &ch, std::string &service, std::string &name,
		    std::string &mode, int timeout_ms);

  std::vector<gxdMuxChannelStatus> Status();

private:
  struct Channel;
  Channel *get(uint32_t ch);
  void io();
  void handle(gxdMuxFrame &f);
  void push_ctrl(uint8_t kind, uint32_t ch, const std::vector<unsigned char> &payload);
  bool next_out(std::vector<unsigned char> &out);
  void fail_locked(const std::string &why);

private:
  std::unique_ptr<gxdMuxTransport> tr;
  gxdMuxOptions opt;
  OpenHandler on_open;
  std::mutex m;
  std::condition_variable cv;        // Any change a waiting caller may want
  std::map<uint32_t, std::unique_ptr<Channel> > chans;
  std::deque<gxdMuxFrame> ctrl;      // Control frames, sent first
  std::deque<uint32_t> accepted;     // Channels opened by the peer
  bool hello_seen = false, hello_sent = false;
  std::atomic<bool> dead{false};
  std::string dead_reason;
  std::thread iot;
  uint32_t rr_last = 0;              // Round-robin position
};

} // namespace datareel

#endif // __GX_DATABASE_MUX_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
