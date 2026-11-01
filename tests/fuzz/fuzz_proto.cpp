// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_proto.cpp
// Compiler Used: Clang (libFuzzer)
// Produced By: DataReel Software Development Team
// File Creation Date: 11/01/2026
// Date Last Modified: 11/01/2026
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

libFuzzer target for the protocol parsers. The first input byte
picks the parser and the rest of the input is fed to it through the
buffer functions, which run the same code as the TLS connection:

  0  gxsHTTPParseResponse() (the second byte's low bit is HEAD)
  1  gxsHTTPParseRequest()
  2  gxsFTPClient::ParsePASVPort(), ParseEPSVPort(), ParseSizeReply()
  3  gxsURL::Parse(), Resolve(), Decode(), DecodeStrict(), Encode()
  4  gxsBase64Decode(), and decode(encode(x)) == x
  5  gxsSMTPClient::ParseReply(), NormalizeAddress(),
     gxsFTPClient::ParseReply(), gxsPOP3Client::ParseMultiline()

Properties that must hold are checked and abort() when they do not.
Build with clang++ -fsanitize=fuzzer,address,undefined against the
library (see build_fuzz.sh for the flags).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "gxs_b64.h"
#include "gxsftp.h"
#include "gxshttp.h"
#include "gxsmtp.h"
#include "gxspop3.h"
#include "gxsurl.h"

using namespace datareel;

static void require(bool cond)
{
  if(!cond) std::abort();
}

static bool has_ctl(std::string_view s, bool allow_tab)
{
  for(unsigned char c : s) {
    if(c == '\t' && allow_tab) continue;
    if(c < 0x20 || c == 0x7F) return true;
  }
  return false;
}

static void check_message(const gxsHTTPMessage &m, std::size_t max_body)
{
  require(m.body.size() <= max_body);
  for(const auto &h : m.headers) {
    require(gxsHTTPIsToken(h.first));
    require(!has_ctl(h.second, true));
  }
}

static void fuzz_http(std::string_view in, bool response)
{
  const std::size_t max_body = 64*1024;
  gxsHTTPMessage m;
  std::size_t used = 0;
  if(response) {
    bool head = !in.empty() && (in[0] & 1);
    if(!in.empty()) in.remove_prefix(1);
    if(gxsHTTPParseResponse(in, m, head, max_body, &used) == 0) {
      require(m.status >= 200 && m.status <= 999);
      require(!has_ctl(m.reason, true));
      check_message(m, max_body);
    }
  }
  else {
    if(gxsHTTPParseRequest(in, m, max_body, &used) == 0) {
      require(gxsHTTPIsToken(m.method));
      require(!m.target.empty() && !has_ctl(m.target, false));
      require(m.target.find(' ') == std::string::npos);
      check_message(m, max_body);
    }
  }
  require(used <= in.size());
}

static void fuzz_ftp_ports(std::string_view in)
{
  int p = gxsFTPClient::ParsePASVPort(in);
  require(p == -1 || (p >= 1 && p <= 65535));
  p = gxsFTPClient::ParseEPSVPort(in);
  require(p == -1 || (p >= 1 && p <= 65535));
  long long n = 0;
  if(gxsFTPClient::ParseSizeReply(in, n)) require(n >= 0);
  else require(n == -1);
}

static void fuzz_url(std::string_view in)
{
  gxsURLInfo u;
  if(gxsURL::Parse(in, u)) {
    require(!u.host.empty());
    require(u.port >= 0 && u.port <= 65535);
    (void)u.ToString();
    (void)u.PathAndQuery();
    (void)u.HostAndPort();
  }
  gxsURLInfo base;
  if(gxsURL::Parse("https://example.com:8443/a/b?q", base)) {
    std::string r = gxsURL::Resolve(base, in);
    gxsURLInfo v;
    (void)gxsURL::Parse(r, v);
  }
  (void)gxsURL::Decode(in);
  (void)gxsURL::Decode(in, true);
  std::string out;
  if(gxsURL::DecodeStrict(in, out)) require(!gxsURL::HasControlChars(out));
  else require(out.empty());
  // Decode(Encode(x)) == x
  require(gxsURL::Decode(gxsURL::Encode(in)) == in);
}

static void fuzz_base64(std::string_view in)
{
  std::string out;
  (void)gxsBase64Decode(in, out);
  std::string back;
  require(gxsBase64Decode(gxsBase64Encode(in), back));
  require(back == in);
}

static void fuzz_replies(std::string_view in)
{
  std::string r;
  std::size_t used = 0;
  int code = gxsSMTPClient::ParseReply(in, r, &used);
  require(code == -1 || (code >= 100 && code <= 599));
  require(used <= in.size());
  code = gxsFTPClient::ParseReply(in, r, &used);
  require(code == -1 || (code >= 100 && code <= 599));
  require(used <= in.size());
  std::string d;
  const std::size_t max_size = 4096;
  if(gxsPOP3Client::ParseMultiline(in, d, max_size, &used) == 0) {
    require(d.size() <= max_size);
  }
  require(used <= in.size());
  std::string a;
  if(gxsSMTPClient::NormalizeAddress(std::string(in), a, true)) {
    require(a.size() >= 2 && a.front() == '<' && a.back() == '>');
    std::string_view inner = std::string_view(a).substr(1, a.size() - 2);
    for(unsigned char c : inner) {
      require(c > 0x20 && c != 0x7F && c != '<' && c != '>');
    }
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if(size < 1) return 0;
  std::string_view in(reinterpret_cast<const char *>(data) + 1, size - 1);
  switch(data[0] % 6) {
    case 0: fuzz_http(in, true); break;
    case 1: fuzz_http(in, false); break;
    case 2: fuzz_ftp_ports(in); break;
    case 3: fuzz_url(in); break;
    case 4: fuzz_base64(in); break;
    default: fuzz_replies(in); break;
  }
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
