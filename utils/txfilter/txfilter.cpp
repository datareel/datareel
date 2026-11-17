// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: txfilter.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 11/17/2026
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

Text filter: keeps the printable (or alphabetic, alphanumeric or
numeric) characters of a text file, line by line, and writes it with
UNIX (LF) or DOS (CR LF) line ends, or none.

Changes:
==============================================================
11/17/2026: DataReel 7.4: back from DataReel 6.x (removed in 7.0),
rewritten on the C++ standard library with the 6.x switches. Lines of
any length (6.x cut them at its line buffer). -o replaces the file
through a temporary file and a rename. Bytes 0x80 and up are kept by
-P (UTF-8 text survives; 6.x dropped them).
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <filesystem>
#include "strutil.h"

using namespace datareel;
namespace fs = std::filesystem;

static const char *ProgramName = "txfilter";
static const char *VersionString = "4000.102";

enum Keep { PRINTABLE, ALPHA, ALNUM, NUMERIC };
struct Opts {
  int eol = 1;              // 1 LF (UNIX), 2 CR LF (DOS), 0 none
  bool filter = true, overwrite = false, spaces = false, tabs = false;
  Keep keep = PRINTABLE;
};

static void help()
{
  printf("ASCII Text file filter program version %s (DataReel %s)\n", VersionString, DATAREEL_VERSION_STRING);
  printf("Usage: %s [switches] infile.txt\n", ProgramName);
  printf("Switches:  -?      = Display this help message.\n");
  printf("           -A      = Output alphabetic characters only.\n");
  printf("           -d      = Output text file in DOS CR/LF format.\n");
  printf("           -D      = Do not insert line feeds.\n");
  printf("           -M      = Output alphanumeric characters only.\n");
  printf("           -n      = No text filtering (defaults to filtered).\n");
  printf("           -N      = Output numeric characters only.\n");
  printf("           -o      = Write output to the existing file (defaults to stdout).\n");
  printf("           -P      = Output printable characters only (default).\n");
  printf("           -t      = Filter tabs.\n");
  printf("           -s      = Filter spaces.\n");
  printf("           -u      = Output text file in UNIX format (default).\n");
}

static std::string filter_line(const std::string &line, const Opts &o)
{
  if(!o.filter) return line;
  std::string out;
  out.reserve(line.size());
  for(unsigned char c : line) {
    bool keep_ws = (c == ' ' && !o.spaces) || (c == '\t' && !o.tabs);
    if(c == ' ' || c == '\t') { if(keep_ws) out += (char)c; continue; }
    bool k = false;
    switch(o.keep) {
      case PRINTABLE: k = isgraph(c) || c >= 0x80; break;    // 0x80 and up: UTF-8 (and 8-bit) text is kept
      case ALPHA: k = c < 0x80 && isalpha(c); break;
      case ALNUM: k = c < 0x80 && isalnum(c); break;
      case NUMERIC: k = c < 0x80 && isdigit(c); break;
    }
    if(k) out += (char)c;
  }
  return out;
}

static int convert(const std::string &fn, const Opts &o)
{
  FILE *f = fopen(fn.c_str(), "rb");
  if(!f) return -1;
  std::string text; char b[8192]; size_t n;
  while((n = fread(b, 1, sizeof(b), f)) > 0) text.append(b, n);
  bool rerr = ferror(f) != 0;
  fclose(f);
  if(rerr) { fprintf(stderr, "Error reading %s\n", fn.c_str()); return 1; }
  std::string out;
  size_t p = 0;
  while(p < text.size()) {
    size_t e = text.find('\n', p);
    std::string line = text.substr(p, e == std::string::npos ? std::string::npos : e - p);
    p = e == std::string::npos ? text.size() : e + 1;
    if(!line.empty() && line.back() == '\r') line.pop_back();
    out += filter_line(line, o);
    if(o.eol == 2) out += "\r\n"; else if(o.eol == 1) out += '\n';
  }
  if(!o.overwrite) { fwrite(out.data(), 1, out.size(), stdout); return 0; }
  std::string tmp = fn + ".txfilter.tmp";
  FILE *w = fopen(tmp.c_str(), "wb");
  if(!w) { fprintf(stderr, "Cannot write to: %s\nExiting...\n", fn.c_str()); return 1; }
  bool ok = fwrite(out.data(), 1, out.size(), w) == out.size();
  if(fclose(w) != 0) ok = false;
  std::error_code ec;
  if(ok) { fs::rename(tmp, fn, ec); if(ec) { fs::remove(fn, ec); fs::rename(tmp, fn, ec); } ok = !ec; }
  if(!ok) { fs::remove(tmp, ec); fprintf(stderr, "Cannot write to: %s\nExiting...\n", fn.c_str()); return 1; }
  return 0;
}

int main(int argc, char *argv[])
{
  if(argc < 2) { help(); return 0; }
  Opts o;
  std::vector<std::string> files;
  for(int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if(a[0] == '-' && a[1]) {
      switch(a[1]) {
	case '?': help(); return 0;
	case 'd': o.eol = 2; break;
	case 'D': o.eol = 0; break;
	case 'u': o.eol = 1; break;
	case 'n': o.filter = false; break;
	case 'o': o.overwrite = true; break;
	case 's': o.spaces = true; break;
	case 't': o.tabs = true; break;
	case 'P': o.keep = PRINTABLE; break;
	case 'A': o.keep = ALPHA; break;
	case 'M': o.keep = ALNUM; break;
	case 'N': o.keep = NUMERIC; break;
	default: fprintf(stderr, "Unknown switch %s\nExiting...\n", a); return 1;
      }
    }
    else files.push_back(a);
  }
  int rc = 0, done = 0;
  for(auto &fn : files) {
    std::error_code ec;
    if(fs::is_directory(fn, ec)) continue;
    int r = convert(fn, o);
    if(r < 0) { fprintf(stderr, "Cannot open file: %s\n", fn.c_str()); rc = 1; continue; }
    if(r) rc = 1;
    done++;
  }
  if(!done) { fprintf(stderr, "\nYou must enter a file name.\nExiting...\n\n"); return 1; }
  return rc;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
