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
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxconfig.h"
#include "logfile.h"
#include "dfileb.h"
#include "gxs_b64.h"
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
