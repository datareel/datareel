// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_proto.cpp
// Compiler Used: clang (libFuzzer)
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

6.3 libFuzzer targets for the 6.x HTTP, URL, SMTP, POP3 and FTP code
that reads network input (the 6.x security sweep; DataReel 7 replaced
this code):

  FUZZ_HTTPHDR  gxsHTTPClient::ParseHTTPHeader() on a response header
  FUZZ_URL      gxsURL::ParseURL() (with the dynamic page code),
                GetProtocolType(), HasFile(), FilterQueryCharacters(),
                DecodeStrict()
  FUZZ_REPLY    the buffer reply parsers: gxsSMTPClient::ParseReply()
                and NormalizeAddress(), gxsPOP3Client::CheckReply(),
                gxsFTPClient::ParseReply() and ParsePASVPort(), and
                ParseServiceFileEntry()
  FUZZ_RECV     the SMTP, POP3 and FTP RecvResponse() loops, reading
                the input from a socket pair into a small buffer (the
                first byte picks the protocol and the expected reply,
                the second the buffer size)

Build with examples_socket/fuzz/build_fuzz.sh (clang, ASan, UBSan).
UNIX only (FUZZ_RECV uses socketpair()).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "gxshttp.h"
#include "gxshttpc.h"
#include "gxsurl.h"
#include "gxsmtp.h"
#include "gxspop3.h"
#include "gxsftp.h"
#include "gxsutils.h"

#if defined (FUZZ_RECV)
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined (FUZZ_HTTPHDR)
static void fuzz_httphdr(const uint8_t *data, size_t size)
{
  gxsHTTPClient client;
  gxsURLInfo u;
  u.host = "fuzz.example";
  gxString header;
  if(size > 0) header.Cat((const char *)data, (unsigned)size);
  gxsHTTPHeader hdr;
  client.ParseHTTPHeader(u, header, hdr);
  gxsHTTPHeader copy(hdr);
  (void)copy;
}
#endif

#if defined (FUZZ_URL)
static void fuzz_url(const uint8_t *data, size_t size)
{
  std::string s((const char *)data, size);
  gxString g;
  if(size > 0) g.SetString(s.data(), (unsigned)size);
  gxsURL url;
  gxsURLInfo u;
  url.ParseURL(s.c_str(), u);
  url.ParseURL(g, u, 1);
  url.GetProtocolType(g);
  url.GetProtocolString(size > 0 ? (int)(signed char)data[0] : 0);
  gxString d, f;
  url.HasFile(g, d, f);
  gxString q(g);
  url.FilterQueryCharacters(q);
  gxString out;
  gxsURL::DecodeStrict(g, out, size & 1);
  gxsURL::HasControlChars(g);
  url.IsDynamicPage(g);
}
#endif

#if defined (FUZZ_REPLY)
static void fuzz_reply(const uint8_t *data, size_t size)
{
  const char *p = (const char *)data;
  int len = (int)size;
  int code = 0, used = 0;
  gxsSMTPClient::ParseReply(p, len, &code, &used);
  if(used < 0 || used > len) __builtin_trap();
  gxsFTPClient::ParseReply(p, len, &code, &used);
  if(used < 0 || used > len) __builtin_trap();
  gxsPOP3Client::CheckReply(p, len, "+OK");
  gxsPOP3Client::CheckReply(p, len, "\r\n.\r\n");

  std::string c(p, size); // As a C string
  std::string cs(c.c_str());
  gxString a;
  gxsSMTPClient::NormalizeAddress(cs.c_str(), a, size & 1);
  if(a.Find("\r") != -1 || a.Find("\n") != -1 || a.Find(" ") != -1) {
    __builtin_trap(); // Must not get into a command
  }
  int port = gxsFTPClient::ParsePASVPort(cs.c_str());
  if(port == 0 || port > 65535) __builtin_trap();

  std::vector<char> entry(cs.begin(), cs.end());
  entry.push_back(0);
  size_t n = entry.size();
  std::vector<char> b1(n), b2(n), b3(n), b4(n);
  int sport = 0;
  ParseServiceFileEntry(&entry[0], &b1[0], &sport, &b2[0], &b3[0], &b4[0]);
}
#endif

#if defined (FUZZ_RECV)
template <class T>
static void recv_one(T &client, const uint8_t *data, size_t size,
		     int bufsize, const char *response)
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return;
  if(size > 0 && write(sv[1], data, size) != (ssize_t)size) {
    close(sv[0]); close(sv[1]);
    return;
  }
  shutdown(sv[1], SHUT_WR);
  client.SetSocket(sv[0]);
  client.SetTimeOut(1, 0);
  std::vector<char> buf(bufsize + 16, 'G');
  client.RecvResponse(&buf[0], bufsize, response);
  for(int i = bufsize; i < bufsize + 16; i++) {
    if(buf[i] != 'G') __builtin_trap(); // Wrote past the buffer
  }
  if(memchr(&buf[0], 0, bufsize) == 0) __builtin_trap(); // No NUL
  client.ReleaseSocket();
  close(sv[0]);
  close(sv[1]);
}

static void fuzz_recv(const uint8_t *data, size_t size)
{
  if(size < 2 || size > 16384) return;
  int which = data[0] % 7;
  int bufsize = 2 + data[1];
  data += 2; size -= 2;
  switch(which) {
    case 0: { gxsSMTPClient c; recv_one(c, data, size, bufsize, "250"); break; }
    case 1: { gxsSMTPClient c; recv_one(c, data, size, bufsize, "3"); break; }
    case 2: { gxsPOP3Client c; recv_one(c, data, size, bufsize, "+OK"); break; }
    case 3: { gxsPOP3Client c; recv_one(c, data, size, bufsize, "\r\n.\r\n"); break; }
    case 4: { gxsFTPClient c; recv_one(c, data, size, bufsize, "220 "); break; }
    case 5: { gxsFTPClient c; recv_one(c, data, size, bufsize, "227"); break; }
    default: { gxsFTPClient c; recv_one(c, data, size, bufsize, "150"); break; }
  }
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
#if defined (FUZZ_HTTPHDR)
  fuzz_httphdr(data, size);
#elif defined (FUZZ_URL)
  fuzz_url(data, size);
#elif defined (FUZZ_REPLY)
  fuzz_reply(data, size);
#elif defined (FUZZ_RECV)
  fuzz_recv(data, size);
#else
#error Define FUZZ_HTTPHDR, FUZZ_URL, FUZZ_REPLY or FUZZ_RECV
#endif
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
