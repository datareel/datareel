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

6.3 tests for the DataReel 7.3 security sweep findings that apply to
6.x:

  config      gxConfig::ChangeConfigValue() changes only the setting
              with that name (not a longer name that ends with it, a
              comment or a value that holds it), and names, values and
              comments with a line break are refused.
  logfile     A new log file is owner-only (UNIX: mode 0600).
  rsa_key     gxSSL::MakeRSAPrivateKey() writes an owner-only key file
              (UNIX: mode 0600).
  base64      gxsBase64Encode() gives the RFC 4648 encoding, also for
              bytes of 0x80 and more (6.x-only code; fcrypt and the
              Encrypted Password Database encode user names with it),
              and decodes back.
  findlast    gxString::FindLast() with an empty pattern returns -1 (it
              read far out of bounds), and finds the last match.
  membuf      MemoryBuffer: copies of empty buffers, searches that end at
              the end of the buffer, InsertAt() past the end, DeleteAt()
              and MemSet() with sizes that wrap, operator[] at the end
              (6.x-only code used by fcrypt and the Encrypted Password
              Database). Most of these show only under ASan/UBSan.
  gxstring    gxString Mid()/Left()/Right() with counts past the end (a
              4 GB allocation), InsertAt() past the end, DeleteAt() with
              counts that wrap, ReplaceChar() with an offset, SetLength()
              on an empty string. ConvertEscapeSeq() with an escape at
              the end, operator<< of a large double.
  strutil     FindMatch()/gxfind_pattern() stay inside the given length,
              64-bit number conversions with too many digits, a bad
              radix and negative values. StringANCompare() digit runs.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxconfig.h"
#include "logfile.h"
#include "dfileb.h"
#include "gxs_b64.h"
#include "gxstring.h"
#include "membuf.h"
#include "strutil.h"
#include "memblock.h"
#if defined (__USE_GX_SSL_EXTENSIONS__)
#include "gxssl.h"
#endif

#if defined (__UNIX__)
#include <sys/stat.h>
#endif

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok) failures++;
}

static int owner_only(const char *fname)
// 1 if only the owner can read or write the file, -1 if not checked
{
#if defined (__UNIX__)
  struct stat st;
  if(stat(fname, &st) != 0) return 0;
  return (st.st_mode & (S_IRWXG|S_IRWXO)) == 0;
#else
  (void)fname;
  return -1;
#endif
}

static int file_has(const char *fname, const char *s)
{
  FILE *fp = fopen(fname, "rb");
  if(!fp) return 0;
  char buf[4096];
  size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = 0;
  return strstr(buf, s) != 0;
}

int main()
{
#if defined (__UNIX__)
  umask(022); // The usual umask: new files readable by every user
#endif

  // config
  const char *cfg_name = "sweep.cfg";
  FILE *fp = fopen(cfg_name, "wb");
  fputs("# Port = 1 is a comment\n", fp);
  fputs("ServerPort = 100\n", fp);
  fputs("Note = the Port = 2 text\n", fp);
  fputs("Port = 3\n", fp);
  fclose(fp);
  {
    gxConfig cfg(cfg_name);
    check(cfg.ChangeConfigValue("Port", "4") == 1, "config: one setting changed");
  }
  check(file_has(cfg_name, "# Port = 1 is a comment") &&
	file_has(cfg_name, "ServerPort = 100") &&
	file_has(cfg_name, "Note = the Port = 2 text") &&
	file_has(cfg_name, "\nPort=4") && !file_has(cfg_name, "Port = 3"),
	"config: only the setting named Port changed");
  {
    gxConfig cfg(cfg_name);
    check(cfg.ChangeConfigValue("Port", "5\nAdmin = yes") == 0, "config: a value with a line break is refused");
    check(cfg.WriteConfigLine("New", "x\rAdmin = yes") == 0, "config: a new value with a line break is refused");
    check(cfg.WriteConfigLine("Bad\nName", "x") == 0, "config: a name with a line break is refused");
    check(cfg.WriteCommentLine("comment\nAdmin = yes") == 0, "config: a comment with a line break is refused");
    check(cfg.ChangeConfigValue("Timeout", "30") == 1, "config: a new setting is added");
  }
  check(!file_has(cfg_name, "Admin") && file_has(cfg_name, "Timeout=30"), "config: no line was injected");
  remove(cfg_name);

  // logfile
  {
    const char *log_name = "sweep.log";
    remove(log_name);
    LogFile log;
    check(log.Open(log_name) == 0, "logfile: open");
    log << "test" << "\n";
    log.Close();
    int o = owner_only(log_name);
    check(o != 0, o < 0 ? "logfile: owner-only (not checked on this system)" : "logfile: a new log file is mode 0600");
    LogFile log2;
    remove("sweep2.log");
    log2.OverWrite("sweep2.log");
    log2.Close();
    o = owner_only("sweep2.log");
    check(o != 0, "logfile: OverWrite() of a new file is owner-only");
    remove(log_name);
    remove("sweep2.log");
  }

  // findlast
  {
    gxString a("hello world"), b("aab"), c("abc");
    gxString e;
    check(a.FindLast("") == -1 && e.FindLast("x") == -1 && c.FindLast("abcd") == -1,
	  "findlast: empty pattern, empty string and long pattern return -1");
    check(a.FindLast("o") == 8 && a.FindLast("world") == 7 && b.FindLast("ab") == 2 &&
	  c.FindLast("a") == 1 && c.FindLast("x") == -1, "findlast: positions as in 6.2");
    gxString d("abaab");
    check(d.FindLast("aab") == 3, "findlast: a failed partial match does not hide the last match");
  }

  // membuf
  {
    MemoryBuffer empty, a("abc", 3);
    MemoryBuffer b = empty;  // Copy of an empty buffer
    a = empty;               // Assign an empty buffer (memmove from null)
    check(a.length() == 0 && b.length() == 0, "membuf: copy and assign of an empty buffer");
    a.Load(0, 0); a.Cat(0, 0); a += empty;
    check(a.length() == 0, "membuf: load and append of nothing");
    MemoryBlock blk; blk.Load(0, 0);
    check(blk.length() == 0, "membuf: MemoryBlock load of nothing");

    MemoryBuffer c("ab", 2);  // Exact size: no slack after the data
    check(c.Find((void *)"bc") == __MEMBUF_NO_MATCH__ &&
	  c.Find("bc", 2) == __MEMBUF_NO_MATCH__, "membuf: a partial match at the end is not a match");
    check(c.Find((void *)"b") == 1 && c.Find("b", 1) == 1 && c.Find("ab", 2) == 0 &&
	  c.Find((void *)"ab", 1) == __MEMBUF_NO_MATCH__, "membuf: Find() positions");
    check(c.Find((void *)"a", 100) == __MEMBUF_NO_MATCH__ && c.Find("a", 1, 100) == __MEMBUF_NO_MATCH__ &&
	  c.Find("abc", 3) == __MEMBUF_NO_MATCH__, "membuf: offset or pattern past the end");
    MemoryBuffer f("aab", 3);
    check(f.Find((void *)"ab") == 1 && f.Find("ab", 2) == 1, "membuf: match after a failed partial match");

    MemoryBuffer d("xy", 2);
    __MEMBUF_INT__ n = d.InsertAt(10, "1234", 4); // Position past the end
    check(n == 4 && d.length() == 6 && memcmp(d.m_buf(), "xy1234", 6) == 0,
	  "membuf: InsertAt() past the end appends");
    n = d.DeleteAt(2, 0xFFFFFFFF); // position + bytes wraps
    check(n == 4 && d.length() == 2 && memcmp(d.m_buf(), "xy", 2) == 0,
	  "membuf: DeleteAt() with a size that wraps");
    MemoryBuffer e(8);
    check(e.MemSet('z', 4, 0xFFFFFFFE) == 0 && e.MemSet('z', 4, 4) == 1, "membuf: MemSet() with a size that wraps is refused");
    MemoryBuffer g("ab", 2);
    check(g[100] == 0 && g[1] == 'b', "membuf: operator[] past the end of a full buffer");
  }

  // gxstring
  {
    gxString a("hello world"), e, z("");
    check(a.Mid(6, 0xFFFFFFF0) == "world" && a.Mid(0, 5) == "hello" && a.Mid(6, 0) == "world" &&
	  a.Left(0xFFFFFFFF) == "hello world" && a.Left(5) == "hello" &&
	  a.Right(0xFFFFFFFF) == "hello world" && a.Right(5) == "world" &&
	  a.Mid(100, 3) == "d", "gxstring: Mid/Left/Right clamp counts past the end");
    check(e.Mid(3, 4).length() == 0 && e.Left(2).length() == 0 && e.Right(2).length() == 0 &&
	  z.Mid(1, 0xFFFFFFFF).length() == 0, "gxstring: substrings of an empty string");
    gxString b("ab");
    b.InsertAt(100, "XYZ");
    check(b == "abXYZ" && b.length() == 5, "gxstring: InsertAt() past the end appends");
    gxString c("abcdef");
    c.DeleteAt(2, 0xFFFFFFFF);
    check(c == "ab" && c.length() == 2, "gxstring: DeleteAt() with a count that wraps");
    gxString d("aaaa");
    check(d.ReplaceChar('a', 'b', 2) == 2 && d == "aabb" && d.ReplaceChar('a', 'c', 10) == 0,
	  "gxstring: ReplaceChar() with an offset stays in the string");
    gxString f("");
    f.SetLength(10);
    check(f.length() == 10 && f.dlength() >= 10, "gxstring: SetLength() on an empty string resizes it");
    gxString g("ab%4");
    g.ConvertEscapeSeq('%', 80); // Octets past the end of the string
    check(g.length() == 3 && g[2] == 4, "gxstring: ConvertEscapeSeq() at the end of the string");
    gxString h("x%41y%");
    check(h.ConvertEscapeSeq('%', 81) >= 1, "gxstring: ConvertEscapeSeq() with 81 octets");
    gxString k("%41%42");
    check(k.ConvertEscapeSeq('%', 2) == 2 && k == "AB", "gxstring: ConvertEscapeSeq() as before");
    gxString m;
    m << 1e300;
    check(m.length() > 300 && m[0] == '1', "gxstring: operator<< of 1e300");
    gxString q;
    q << 2.5;
    check(q == "2.5", "gxstring: operator<< of 2.5");
  }

  // strutil
  {
    char *ab = new char[2]; ab[0] = 'a'; ab[1] = 'b'; // Not null-terminated
    check(FindMatch(ab, "bc", 0, 2, 2) == -1 && FindMatch(ab, "b", 0, 2, 1) == 1 &&
	  IFindMatch(ab, "BC", 0, 2, 2) == -1 && IFindMatch(ab, "AB", 0, 2, 2) == 0,
	  "strutil: a partial match at the end of the source is not a match");
    delete[] ab;
    check(FindMatch("abab", "ab", 1, 0, 0) == 2 && FindMatch("aab", "ab", 0, 0, 0) == 1 &&
	  FindMatch("abc", "", 0, 0, 0) == -1 && IFindMatch("xAaB", "ab", 0, 0, 0) == 2, "strutil: FindMatch() positions as before");
#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
    StringToLLWORD("99999999999999999999999");
    StringToLLWORD("-99999999999999999999999");
    check(StringToLLWORD("-42") == -42 && StringToLLWORD("123") == 123,
	  "strutil: StringToLLWORD() with too many digits (UBSan) and as before");
    char nb[100];
    check(strcmp(LLWORDToString(nb, (__ULLWORD__)(__LLWORD__)-5, 10), "-5") == 0 &&
	  strcmp(LLWORDToString(nb, 255, 16), "FF") == 0, "strutil: LLWORDToString() of a negative value");
    check(strcmp(ULLWORDToString(nb, 5, 1), "5") == 0 && strcmp(ULLWORDToString(nb, 12, 0), "12") == 0,
	  "strutil: a bad radix gives base 10");
#endif
  }

  // strutil: StringANCompare
  {
    check(StringANCompare("1", "12", 1) < 0 && StringANCompare("12", "1", 1) > 0 &&
	  StringANCompare("a2", "a10", 0) < 0 && StringANCompare("abc", "abc", 1) == 0,
	  "strutil: StringANCompare() with digits to the end of a string");
  }

  // base64
  {
    const char *plain[] = { "", "f", "fo", "foo", "foob", "fooba", "foobar",
			    "\xc3\xa9", "\xff\xfe\xfd\x80", 0 };
    const char *coded[] = { "", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=",
			    "Zm9vYmFy", "w6k=", "//79gA==", 0 };
    int ok = 1;
    for(int i = 0; plain[i]; i++) {
      char enc[64], dec[64];
      memset(enc, 0x55, sizeof(enc));
      gxsBase64Encode(plain[i], enc, (int)strlen(plain[i]));
      if(strcmp(enc, coded[i]) != 0) {
	printf("     base64: \"%s\" encoded as \"%s\", expected \"%s\"\n", plain[i], enc, coded[i]);
	ok = 0;
      }
      gxsBase64Decode(coded[i], dec);
      if(strcmp(dec, plain[i]) != 0) ok = 0;
    }
    check(ok, "base64: RFC 4648 vectors and bytes over 0x7f encode and decode");
    unsigned char all[256];
    for(int i = 0; i < 256; i++) all[i] = (unsigned char)(255 - i);
    char enc[400], dec[300];
    gxsBase64Encode((const char *)all, enc, 255); // Without a 0 byte to stop the decode
    gxsBase64Decode(enc, dec);
    check(memcmp(dec, all, 255) == 0, "base64: every byte value round trips");
  }

#if defined (__USE_GX_SSL_EXTENSIONS__) && !defined (__USE_GX_SCHANNEL__)
  // rsa_key
  {
    const char *key_name = "sweep_key.pem";
    remove(key_name);
    gxSSL ssl;
    int rv = ssl.MakeRSAPrivateKey(key_name, 2048);
    check(rv == 0, "rsa_key: key written");
    int o = owner_only(key_name);
    check(o != 0, "rsa_key: the private key file is mode 0600");
    remove(key_name);
  }
#endif

  if(failures) {
    printf("%d security sweep tests failed\n", failures);
    return 1;
  }
  printf("All security sweep tests passed\n");
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
