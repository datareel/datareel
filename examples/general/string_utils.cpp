// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: string_utils.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
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

String, base 64 and CRC-32 utilities.

The three small toolkits most programs need around a database:

 strutil.h   Header-only helpers on std::string and std::string_view:
             trim, case changes, case-blind compare and find,
             starts_with/ends_with, replace_all, split, printf-style
             format(), number conversion that never throws, safe
             copies into fixed char fields, and natural ("file2"
             before "file10") ordering.
 gxs_b64.h   Base 64 (RFC 4648) for binary data in text: a record in
             a text export, a mail attachment, an HTTP basic
             authentication header.
 gxcrc32.h   CRC-32 checksums of a buffer or a whole file, to detect
             damage when data is stored or sent. (gxDatabase uses the
             same CRC for its optional per-record checksums.)

Each section prints what it does and checks the result against a known
answer, so the program also works as a quick self test.

Usage: string_utils [file]

  With a file name it also prints the file's CRC-32, computed two ways
  (the whole stream, and block by block) to show they agree.

Exits with 0 when every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "gxcrc32.h"
#include "gxs_b64.h"
#include "strutil.h"

using namespace datareel;

static int failures = 0;

// Print a line and count it as a failure if the check did not hold
static void show(const std::string &what, const std::string &value, bool ok)
{
  if(!ok) failures++;
  std::cout << format("  %-34s %s%s\n", what.c_str(), value.c_str(), ok ? "" : "   <-- WRONG");
}

static void strings()
{
  std::cout << "strutil.h\n";
  std::string raw = "   AMTX, buy , 100 ,3.25,  opening flush  ";
  show("trim(\"" + raw.substr(0, 12) + "...\")", "\"" + trim(raw).substr(0, 15) + "...\"",
       trim(raw).front() == 'A' && trim(raw).back() == 'h');

  // Split a CSV line, then trim each field
  std::vector<std::string> fields = split(raw, ',');
  for(auto &f : fields) trim_in_place(f);
  show("split + trim", str_cat(fields.size(), " fields: ", fields[0], "|", fields[1], "|",
			       fields[2], "|", fields[3], "|", fields[4]),
       fields.size() == 5 && fields[1] == "buy" && fields[4] == "opening flush");

  // Numbers: to_int() and friends return 0 for text that is not a
  // number, instead of throwing
  show("to_int(\"100\") + to_double(\"3.25\")", format("%d, %.2f", to_int(fields[2]), to_double(fields[3])),
       to_int(fields[2]) == 100 && to_double(fields[3]) == 3.25);
  show("to_int(\"n/a\")", format("%d", to_int("n/a")), to_int("n/a") == 0);

  // Case
  show("to_upper(\"buy\")", to_upper(fields[1]), to_upper(fields[1]) == "BUY");
  show("iequals(\"Buy\", \"BUY\")", iequals("Buy", "BUY") ? "true" : "false", iequals("Buy", "BUY"));
  show("ifind(\"Opening FLUSH\", \"flush\")", format("%zu", ifind("Opening FLUSH", "flush")),
       ifind("Opening FLUSH", "flush") == 8);

  // Prefixes, suffixes, replace
  show("starts_with / ends_with",
       str_cat(starts_with("osa_2026-11-09.log", "osa_") ? "true" : "false", " ",
	       ends_with("osa_2026-11-09.log", ".log") ? "true" : "false"),
       starts_with("osa_2026-11-09.log", "osa_") && ends_with("osa_2026-11-09.log", ".log"));
  std::string path = "/data/2026/11/09/events.gxd";
  std::size_t n = replace_all(path, "/", "\\");
  show("replace_all(\"/\" -> \"\\\")", str_cat(n, " changes: ", path), n == 5);

  // Cut a string around a marker
  std::string url = "https://example.com/api/v1/quotes?symbol=AMTX";
  std::string query = url;
  delete_before_including(query, "?");
  show("delete_before_including(\"?\")", query, query == "symbol=AMTX");

  // format() is printf with a std::string result
  show("format(\"%-6s|%8.3f|\")", format("%-6s|%8.3f|", "AMTX", 3.25),
       format("%-6s|%8.3f|", "AMTX", 3.25) == "AMTX  |   3.250|");

  // copy_to() fills a fixed char field (as in a database record) and
  // always leaves it null terminated
  char ticker[5];
  std::size_t copied = copy_to(ticker, sizeof(ticker), "TOOLONG");
  show("copy_to(char[5], \"TOOLONG\")", str_cat("\"", ticker, "\" (", copied, " chars)"),
       strcmp(ticker, "TOOL") == 0 && copied == 4);

  // Natural order: numbers inside names sort by value
  std::vector<std::string> files = {"run10.log", "run2.log", "run1.log"};
  std::sort(files.begin(), files.end(), [](const std::string &a, const std::string &b) {
    return natural_compare(a, b) < 0;
  });
  show("natural_compare sort", files[0] + " " + files[1] + " " + files[2],
       files[0] == "run1.log" && files[1] == "run2.log" && files[2] == "run10.log");
}

static void base64()
{
  std::cout << "gxs_b64.h\n";
  // The RFC 4648 test vector
  show("encode(\"foobar\")", gxsBase64Encode("foobar"), gxsBase64Encode("foobar") == "Zm9vYmFy");
  show("encode(\"fo\") (padding)", gxsBase64Encode("fo"), gxsBase64Encode("fo") == "Zm8=");

  // Binary data, including zero bytes, survives the round trip
  std::string bin;
  for(int i = 0; i < 256; i++) bin += (char)i;
  std::string text = gxsBase64Encode(bin.data(), bin.size()), back;
  bool ok = gxsBase64Decode(text, back) && back == bin;
  show("256 binary bytes round trip", str_cat(text.size(), " chars of base 64"), ok);

  // White space is ignored, so wrapped lines decode
  std::string wrapped = text.substr(0, 76) + "\r\n" + text.substr(76);
  ok = gxsBase64Decode(wrapped, back) && back == bin;
  show("decode with a line break", ok ? "same bytes" : "different", ok);

  // Characters outside the alphabet are refused
  show("decode(\"not*base64\")", gxsBase64Decode("not*base64", back) ? "accepted" : "refused",
       !gxsBase64Decode("not*base64", back));

  // HTTP basic authentication
  std::string header = gxsBasicAuthenticationEncode("trader", "s3cret");
  std::string user, pass;
  ok = gxsBasicAuthenticationDecode(header, user, pass) && user == "trader" && pass == "s3cret";
  show("basic auth header", header, ok && header == "Basic dHJhZGVyOnMzY3JldA==");
}

static void crc32(const char *fname)
{
  std::cout << "gxcrc32.h\n";
  // The standard check value: CRC-32 of "123456789" is CBF43926
  const char *check = "123456789";
  unsigned long crc = calcCRC32(check, (unsigned)strlen(check));
  show("CRC-32(\"123456789\")", format("%08lX", crc), crc == 0xCBF43926UL);

  // One changed byte gives a different checksum
  char rec[] = "AMTX buy 100 at 3.25";
  unsigned long before = calcCRC32(rec, (unsigned)strlen(rec));
  rec[13] = '9'; // "100" -> "900"
  unsigned long after = calcCRC32(rec, (unsigned)strlen(rec));
  show("one byte changed", format("%08lX -> %08lX", before, after), before != after);

  // A file: the whole stream at once, then block by block with the
  // one-byte form, which carries the running CRC from call to call
  if(fname) {
    std::ifstream in(fname, std::ios::binary);
    if(!in) {
      show(str_cat("file ", fname), "cannot open", false);
      return;
    }
    unsigned long whole = calcCRC32(in);
    std::ifstream in2(fname, std::ios::binary);
    unsigned long running = 0xFFFFFFFFUL;
    char buf[4096];
    while(in2.read(buf, sizeof(buf)) || in2.gcount() > 0) {
      for(std::streamsize i = 0; i < in2.gcount(); i++) running = calcCRC32((unsigned char)buf[i], running);
    }
    running ^= 0xFFFFFFFFUL;
    show(str_cat("file ", fname), format("%08lX (stream) %08lX (blocks)", whole, running), whole == running);
  }
}

int main(int argc, char **argv)
{
  strings();
  base64();
  crc32(argc > 1 ? argv[1] : nullptr);
  std::cout << (failures == 0 ? "All checks pass" : str_cat(failures, " checks failed")) << "\n";
  return failures == 0 ? 0 : 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
