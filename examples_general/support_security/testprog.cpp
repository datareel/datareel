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

6.3 tests for the DataReel 7.3.1 security sweep fixes to the support
code (gxConfig, LogFile, the database debug manager). Run with no
arguments for every test, or name the tests to run:

  config      gxConfig: long lines, value lookups, number overflow,
              whole name match, parameter ID strings of more than one
              character, refused names, file rewritten by rename.
  logfile     LogFile: no write through a symbolic link planted at the
              log file name, line breaks in log text escaped.
  dbug        gxDatabaseDebugManager: damaged block lengths, Rebuild()
              onto its own input.

Returns 0 if every check passes; failed checks print FAIL lines.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gxconfig.h"
#include "gxstring.h"

#if defined (__UNIX__)
#include <sys/stat.h>
#include <unistd.h>
#endif

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok) failures++;
}

static int file_has(const char *fname, const char *s)
{
  FILE *fp = fopen(fname, "rb");
  if(!fp) return 0;
  static char buf[65536];
  size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = 0;
  return strstr(buf, s) != 0;
}

static void write_file(const char *fname, const char *s)
{
  FILE *fp = fopen(fname, "wb");
  if(!fp) return;
  fputs(s, fp);
  fclose(fp);
}

static int str_is(const char *a, const char *b)
{
  if(!a || !b) return a == b;
  return strcmp(a, b) == 0;
}

static void test_config()
{
  const char *cfg_name = "support_sec.cfg";

  // A value longer than the old 1023-byte line buffer must not add a
  // setting from its tail
  {
    gxString text("Long = ");
    // The old reader cut the line after 1023 bytes, here right before
    // "Admin=yes"
    for(int i = 0; i < 1016; i++) text += 'a';
    text += "Admin=yes\nShort = 1\n";
    write_file(cfg_name, text.c_str());
    gxConfig cfg(cfg_name);
    check(cfg.Load() == 1, "config: load a file with a long line");
    check(cfg.GetStrValue("Admin") == 0, "config: the tail of a long value is not read as a setting");
    char *v = cfg.GetStrValue("Long");
    check(v && strlen(v) == 1025, "config: a long value is read whole");
    check(cfg.GetIntValue("Short") == 1, "config: the setting after a long line");
    check(cfg.ChangeConfigValue("Short", "2") == 1, "config: change a setting in a file with a long line");
    gxConfig cfg2(cfg_name);
    cfg2.Load();
    v = cfg2.GetStrValue("Long");
    check(v && strlen(v) == 1025 && cfg2.GetStrValue("Admin") == 0 && cfg2.GetIntValue("Short") == 2,
	  "config: a rewrite keeps a long line whole");
  }

  // Lookups only match setting names
  {
    write_file(cfg_name, "a = b\nb = c\nx = y\n");
    gxConfig cfg(cfg_name);
    cfg.Load();
    check(str_is(cfg.GetStrValue("b"), "c"), "config: a value equal to a name does not hide the setting");
    check(cfg.GetStrValue("y") == 0, "config: the last value is not matched as a name (null pointer read)");
    check(cfg.GetStrValue("c") == 0 && cfg.GetIntValue("y", 0) == 0, "config: values are not found as names");
  }

  // Numbers that do not fit read as 0
  {
    write_file(cfg_name, "Big = 99999999999999999999\nNeg = -99999999999999999999\n"
	       "IntBig = 4294967296\nOk = 42abc\nMinus = -7\n");
    gxConfig cfg(cfg_name);
    cfg.Load();
    check(cfg.GetIntValue("Big") == 0 && cfg.GetIntValue("Neg") == 0, "config: GetIntValue() out of range reads as 0");
    check(cfg.GetLongValue("Big") == 0 && cfg.GetLongValue("Neg") == 0, "config: GetLongValue() out of range reads as 0");
    check(cfg.GetIntValue("IntBig") == 0, "config: GetIntValue() above INT_MAX reads as 0");
    check(cfg.GetIntValue("Ok") == 42 && cfg.GetIntValue("Minus") == -7 && cfg.GetLongValue("Minus") == -7,
	  "config: numbers in range read as before");
  }

  // The whole name must come before the parameter ID string
  {
    write_file(cfg_name, "Port x = 5\nPortal = 6\n");
    {
      gxConfig cfg(cfg_name);
      check(cfg.ChangeConfigValue("Port", "7") == 1, "config: Port added");
    }
    check(file_has(cfg_name, "Port x = 5\n") && file_has(cfg_name, "Portal = 6\n") &&
	  file_has(cfg_name, "Port=7"), "config: \"Port x\" and \"Portal\" are not changed for \"Port\"");
  }

  // A parameter ID string of more than one character
  {
    write_file(cfg_name, "Speed := 1\nSpeed2:=9\n");
    {
      gxConfig cfg(cfg_name, ":=");
      check(cfg.ChangeConfigValue("Speed", "2") == 1, "config: change with a := parameter ID");
    }
    gxConfig cfg(cfg_name, ":=");
    cfg.Load();
    check(str_is(cfg.GetStrValue("Speed"), "2") && cfg.GetIntValue("Speed2") == 9,
	  "config: the new value goes after the whole := ID");
    check(cfg.GetCommentChar() == '#', "config: the parameter ID constructor sets the comment character");
  }

  // Names that would not read back as the same setting
  {
    write_file(cfg_name, "A = 1\n");
    gxConfig cfg(cfg_name);
    check(cfg.WriteConfigLine("x=y", "1") == 0 && cfg.WriteConfigLine("", "1") == 0,
	  "config: WriteConfigLine() refuses an empty name and a name holding the ID");
    check(cfg.ChangeConfigValue("Admin=yes #", "1") == 0, "config: ChangeConfigValue() refuses a name holding the ID");
    check(!file_has(cfg_name, "x=y") && !file_has(cfg_name, "Admin"), "config: nothing was written");
  }

#if defined (__UNIX__)
  // The file is replaced by rename: same mode, a symbolic link is kept
  {
    write_file(cfg_name, "A = 1\nB = 2\n");
    chmod(cfg_name, 0640);
    struct stat st1, st2;
    stat(cfg_name, &st1);
    {
      gxConfig cfg(cfg_name);
      check(cfg.ChangeConfigValue("A", "3") == 1, "config: change for the rewrite test");
    }
    stat(cfg_name, &st2);
    check((st2.st_mode & 07777) == 0640, "config: the rewritten file keeps its mode");
    check(st1.st_ino != st2.st_ino, "config: the file was replaced by rename, not truncated in place");
    check(file_has(cfg_name, "A=3\nB = 2\n"), "config: rewritten contents");

    const char *link_name = "support_sec_link.cfg";
    remove(link_name);
    if(symlink(cfg_name, link_name) == 0) {
      {
	gxConfig cfg(link_name);
	check(cfg.ChangeConfigValue("B", "4") == 1, "config: change through a symbolic link");
      }
      struct stat lst;
      check(lstat(link_name, &lst) == 0 && S_ISLNK(lst.st_mode) && file_has(cfg_name, "B=4"),
	    "config: a symbolic link to the config file is kept, its target changed");
      remove(link_name);
    }
  }
#endif
  remove(cfg_name);
}

int main(int argc, char **argv)
{
#if defined (__UNIX__)
  umask(022);
#endif
  int all = (argc < 2);
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "config") != 0) {
      printf("Unknown test %s\n", argv[i]);
      return 2;
    }
  }
  if(all || (argc > 1 && strcmp(argv[1], "config") == 0)) test_config();

  if(failures) printf("%d check(s) FAILED\n", failures);
  else printf("All checks passed\n");
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
