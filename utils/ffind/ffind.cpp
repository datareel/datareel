// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: ffind.cpp
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

Text search and replace: finds, replaces and inserts strings in text
files, line by line, or replaces a whole section (a multi-line text).

Changes:
==============================================================
11/17/2026: DataReel 7.4: back from DataReel 6.x (removed in 7.0),
rewritten on the C++ standard library with the 6.x switches, --
options and exit codes (the number of matches, replacements or
insertions). A file name with * or ? is expanded here too, so the
wildcards work on Windows as they do in a POSIX shell. Fixed from
6.x: --SurroundBy, --TerminateWith and the --Trim options changed the
strings again for every file (they are applied once), and the -I
message gave the two strings the wrong way round. A file is replaced
through a temporary file and a rename, never truncated in place.
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <algorithm>
#include "strutil.h"

using namespace datareel;
namespace fs = std::filesystem;

static const char *ProgramName = "ffind";
static const char *VersionString = "4000.102";

struct Opts {
  bool find = false, find_all = false, check_case = true, cat = false;
  bool replace = false, replace_line = false, insert = false, append_lines = true;
  bool replace_once = false, no_escape = false, section = false;
  bool surround = false, terminate = false;
  std::string to_find, to_replace, to_insert, surround_str, terminate_str, trim_leading, trim_trailing;
  bool fixed = false;
};
static Opts O;
static unsigned long num_matches = 0, num_replaced = 0, num_processed = 0;

static void version() { printf("\n%s program version %s (DataReel %s)\n", ProgramName, VersionString, DATAREEL_VERSION_STRING); }
static void help()
{
  version();
  printf("Usage: %s [switches] iofile.txt\n", ProgramName);
  printf("Switches:\n");
  printf("          -a = Do not append lines connected by a backslash\n");
  printf("          -c = Concatenate file to stdout.\n");
  printf("          -f = Find first occurrence of specified string: -f\"string\"\n");
  printf("          -F = Find all occurrences of specified string: -F\"string\"\n");
  printf("          -i = Perform case insensitive compare.\n\n");
  printf("          -R -W = Replace all occurrences of a string in a file.\n");
  printf("          Usage: -R\"string\" -W\"replacement\"\n\n");
  printf("          -L -W = Replace entire line after the specified string.\n");
  printf("          Usage: -L\"string\" -W\"replacement\"\n\n");
  printf("          -I -W = Insert a line after the specified string.\n");
  printf("          Usage: -I\"string\" -W\"insert\"\n\n");
  printf("          --infile=\"file.txt\"     The -W text from a file\n");
  printf("          --ReplaceSection=\"f\"    Replace the text in file f (several lines) with -W or --infile\n");
  printf("          --ReplaceOnce           Only the first match\n");
  printf("          --SurroundBy=Quotes|SingleQuotes|text   Put the -W text between these\n");
  printf("          --TerminateWith=\"text\"  Append text to the -W text\n");
  printf("          --TrimLeading=\"chars\" / --TrimTrailing=\"chars\"\n");
  printf("          --NoEscapeSeq           \\n \\r \\t and %%20 are not converted\n");
  printf("          --version / --help\n\n");
}

static void escapes(std::string &s)
{
  if(O.no_escape) return;
  replace_all(s, "\\n", "\n"); replace_all(s, "\\r", "\r"); replace_all(s, "\\t", "\t"); replace_all(s, "%20", " ");
}

static bool read_file(const std::string &fn, std::string &out)
{
  FILE *f = fopen(fn.c_str(), "rb");
  if(!f) return false;
  out.clear();
  char b[8192]; size_t n;
  while((n = fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
  bool ok = !ferror(f);
  fclose(f);
  return ok;
}

// the whole text to FN: a temporary file next to it, then a rename (the original is never left half written)
static bool write_file(const std::string &fn, const std::string &text)
{
  std::string tmp = fn + ".ffind.tmp";
  FILE *f = fopen(tmp.c_str(), "wb");
  if(!f) return false;
  bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
  if(fclose(f) != 0) ok = false;
  std::error_code ec;
  if(ok) { fs::rename(tmp, fn, ec); if(ec) { fs::remove(fn, ec); fs::rename(tmp, fn, ec); } ok = !ec; }
  if(!ok) fs::remove(tmp, ec);
  return ok;
}

static size_t find_in(const std::string &s, const std::string &p, size_t from)
{
  if(p.empty()) return std::string::npos;
  return O.check_case ? s.find(p, from) : ifind(s, p, from);
}

static void fix_strings()
{
  if(O.fixed) return;
  O.fixed = true;
  for(char c : O.trim_leading) {
    for(std::string *s : {&O.to_find, &O.to_replace, &O.to_insert}) { size_t k = s->find_first_not_of(c); s->erase(0, k == std::string::npos ? s->size() : k); }
  }
  for(char c : O.trim_trailing) {
    for(std::string *s : {&O.to_find, &O.to_replace, &O.to_insert}) { size_t k = s->find_last_not_of(c); s->erase(k == std::string::npos ? 0 : k + 1); }
  }
  if(O.surround) O.to_insert = O.surround_str + O.to_insert + O.surround_str;
  if(O.terminate) O.to_insert += O.terminate_str;
}

static const char *eol()
{
#if defined (_WIN32)
  return "\r\n";
#else
  return "\n";
#endif
}

static bool process_text_file(const std::string &fn)
{
  fix_strings();
  std::string text;
  if(!read_file(fn, text)) { fprintf(stderr, "\nCannot open file: %s\nExiting...\n\n", fn.c_str()); return false; }
  std::vector<std::string> lines;
  std::string out;
  std::string appended;
  unsigned long line_number = 0;
  size_t p = 0;
  bool changed = false;
  while(p < text.size()) {
    size_t e = text.find('\n', p);
    std::string line = text.substr(p, e == std::string::npos ? std::string::npos : e - p);
    p = e == std::string::npos ? text.size() : e + 1;
    if(!line.empty() && line.back() == '\r') line.pop_back();
    line_number++;
    if(O.append_lines && !line.empty() && line.back() == '\\') { appended += line; appended += '\n'; continue; }
    std::string data = appended + line;
    appended.clear();
    if(O.cat) printf("%s\n", data.c_str());
    if(O.find && !O.to_find.empty()) {
      if(O.find_all) {
	for(size_t off = 0; (off = find_in(data, O.to_find, off)) != std::string::npos; off++) { num_matches++; num_processed++; }
      }
      else if(find_in(data, O.to_find, 0) != std::string::npos) {
	num_matches++; num_processed++;
	printf("\nFile name: %s String: %s\nFound match on line: #%lu\n", fn.c_str(), O.to_find.c_str(), line_number);
      }
    }
    if(O.replace) {
      for(size_t off = 0; !(O.replace_once && num_replaced >= 1);) {
	off = find_in(data, O.to_replace, off);
	if(off == std::string::npos) break;
	data.replace(off, O.to_replace.size(), O.to_insert);
	num_replaced++; num_processed++; changed = true;
	off += O.to_insert.size();
      }
    }
    else if(O.replace_line || O.insert) {
      if(!(O.replace_once && num_replaced >= 1)) {
	size_t off = find_in(data, O.to_replace, 0);
	if(off != std::string::npos) {
	  off += O.to_replace.size();
	  if(O.replace_line) data.erase(off);
	  data.insert(off, O.to_insert);
	  num_replaced++; num_processed++; changed = true;
	}
      }
    }
    lines.push_back(data);
  }
  if(!appended.empty()) lines.push_back(appended);           // a backslash on the last line
  if(changed) {
    for(auto &l : lines) {
#if defined (_WIN32)
      replace_all(l, "\n", "\r\n");
#endif
      out += l; out += eol();
    }
    if(!write_file(fn, out)) { fprintf(stderr, "\nCannot write to: %s\nExiting...\n\n", fn.c_str()); exit(1); }
  }
  return true;
}

static bool replace_by_section(const std::string &fn)
{
  fprintf(stderr, "\nReplacing section in file %s\n", fn.c_str());
  if(O.to_insert.empty()) { fprintf(stderr, "\nError you must supply an input string for section replace\nExiting...\n\n"); exit(1); }
  if(O.to_replace.empty()) { fprintf(stderr, "\nError you must supply a string to replace\nExiting...\n\n"); exit(1); }
  fix_strings();
  std::string text;
  if(!read_file(fn, text)) { fprintf(stderr, "\nError reading from: %s\nNo sections were replaced\n\n", fn.c_str()); return false; }
  replace_all(text, "\r\n", "\n");
  for(size_t off = 0; !(O.replace_once && num_replaced >= 1);) {
    off = find_in(text, O.to_replace, off);
    if(off == std::string::npos) break;
    text.replace(off, O.to_replace.size(), O.to_insert);
    num_replaced++; num_processed++;
    off += O.to_insert.size();
  }
  if(O.cat) fwrite(text.data(), 1, text.size(), stdout);
  if(num_replaced > 0) {
#if defined (_WIN32)
    replace_all(text, "\n", "\r\n");
#endif
    if(!write_file(fn, text)) { fprintf(stderr, "\nCannot write to: %s\nNo sections were replaced\n\n", fn.c_str()); return false; }
  }
  if(num_replaced == 0) fprintf(stderr, "No sections were found to replace\n\n");
  else fprintf(stderr, "Replaced %lu section%s.\n\n", num_replaced, num_replaced == 1 ? "" : "s");
  return true;
}

// -- options: false to end the program
static bool dash_option(const std::string &a, int &rc)
{
  std::string opt = a, val;
  size_t eq = a.find('=');
  bool has_val = eq != std::string::npos;
  if(has_val) { opt = a.substr(0, eq); val = a.substr(eq + 1); }
  auto need = [&](const char *what) { if(!has_val || val.empty()) { fprintf(stderr, "\nError you must supply %s with the --%s argument\nExiting...\n\n", what, opt.c_str()); rc = 1; return false; } return true; };
  if(opt == "infile") {
    if(!need("a file name")) return false;
    if(!read_file(val, O.to_insert)) { fprintf(stderr, "\nError opening infile %s\nExiting...\n\n", val.c_str()); rc = 1; return false; }
    replace_all(O.to_insert, "\r\n", "\n"); escapes(O.to_insert);
  }
  else if(opt == "SurroundBy") {
    O.surround = true;
    O.surround_str = !has_val || val == "Quotes" ? "\"" : val == "SingleQuotes" ? "'" : val;
  }
  else if(opt == "TerminateWith") { if(!need("a string")) return false; O.terminate = true; O.terminate_str = val; escapes(O.terminate_str); }
  else if(opt == "TrimLeading") { if(!need("a string")) return false; O.trim_leading += val; escapes(O.trim_leading); }
  else if(opt == "TrimTrailing") { if(!need("a string")) return false; O.trim_trailing += val; escapes(O.trim_trailing); }
  else if(opt == "ReplaceSection") {
    O.section = true;
    if(has_val && !val.empty()) {
      if(!read_file(val, O.to_replace)) { fprintf(stderr, "\nError opening replace section file %s\nExiting...\n\n", val.c_str()); rc = 1; return false; }
      replace_all(O.to_replace, "\r\n", "\n"); escapes(O.to_replace);
    }
  }
  else if(opt == "version") { version(); rc = 0; return false; }
  else if(opt == "help") { help(); rc = 0; return false; }
  else if(opt == "ReplaceOnce") O.replace_once = true;
  else if(opt == "NoEscapeSeq") O.no_escape = true;
  else { fprintf(stderr, "\nInvalid option and -- argument\n--%s\nExiting...\n\n", a.c_str()); rc = 1; return false; }
  return true;
}

static bool process_switch(const char *arg, int &rc)
{
  char sw = arg[1];
  const char *v = arg + 2;
  auto need = [&](const char *usage) { if(!*v) { fprintf(stderr, "\nYou must enter a string after the -%c switch.\nUsage: %s\n\n", sw, usage); rc = 1; return false; } return true; };
  switch(sw) {
    case '?': help(); rc = 0; return false;
    case 'a': O.append_lines = false; break;
    case 'c': O.cat = true; break;
    case 'i': O.check_case = false; break;
    case 'f': case 'F':
      if(!need(sw == 'f' ? "-f\"string\"" : "-F\"string\"")) return false;
      O.find = true; O.find_all = sw == 'F'; O.replace = O.replace_line = O.insert = false;
      O.to_find = v; escapes(O.to_find); break;
    case 'R': case 'L': case 'I':
      if(!need(sw == 'R' ? "-R\"string\" -W\"replacement\"" : sw == 'L' ? "-L\"string\" -W\"replacement\"" : "-I\"string\" -W\"insert\"")) return false;
      O.to_replace = v; escapes(O.to_replace);
      O.replace = sw == 'R'; O.replace_line = sw == 'L'; O.insert = sw == 'I'; O.find = O.find_all = false;
      break;
    case 'W':
      if(!need("-R\"string\" -W\"replacement\"")) return false;
      if(O.to_replace.empty() && !O.section) { fprintf(stderr, "\nThe -W switch must follow -R, -L or -I.\nUsage: -R\"string\" -W\"replacement\"\n\n"); rc = 1; return false; }
      O.to_insert = v; escapes(O.to_insert); break;
    case '-':
      if(!arg[2]) { fprintf(stderr, "\nYou must enter an option and -- argument.\nExample usage: %s --infile=\"textfile.txt\"\n\n", ProgramName); rc = 1; return false; }
      return dash_option(arg + 2, rc);
    default: fprintf(stderr, "\nUnknown switch %s\nExiting...\n\n", arg); rc = 1; return false;
  }
  return true;
}

// a name with * or ?: the files it matches in its folder (sorted), as a POSIX shell would give them
static std::vector<std::string> expand(const std::string &a)
{
  if(a.find_first_of("*?") == std::string::npos) return {a};
  fs::path p(a);
  fs::path dir = p.has_parent_path() ? p.parent_path() : fs::path(".");
  std::string pat = p.filename().string();
  std::function<bool(size_t, size_t, const std::string &)> match = [&](size_t i, size_t j, const std::string &n) -> bool {
    if(i == pat.size()) return j == n.size();
    if(pat[i] == '*') { for(size_t k = j; k <= n.size(); k++) if(match(i + 1, k, n)) return true; return false; }
    if(j == n.size()) return false;
    if(pat[i] == '?' || pat[i] == n[j]) return match(i + 1, j + 1, n);
    return false;
  };
  std::vector<std::string> out;
  std::error_code ec;
  for(auto &e : fs::directory_iterator(dir, ec)) {
    std::string n = e.path().filename().string();
    if(e.is_regular_file(ec) && match(0, 0, n)) out.push_back(p.has_parent_path() ? (dir / n).string() : n);
  }
  std::sort(out.begin(), out.end());
  return out;
}

int main(int argc, char *argv[])
{
  if(argc < 2) { help(); return 0; }
  int rc = 0;
  unsigned long num_files = 0;
  for(int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if(!arg[0]) continue;
    if(arg[0] == '-') { if(!process_switch(arg, rc)) return rc; continue; }
    for(const std::string &fn : expand(arg)) {
      std::error_code ec;
      if(fs::is_directory(fn, ec)) continue;                 // folder names are skipped
      if(!fs::exists(fn, ec)) { fprintf(stderr, "\nCannot open file: %s\n", fn.c_str()); continue; }
      num_files++;
      num_processed = 0;
      if(O.section) { if(!replace_by_section(fn)) return 1; continue; }
      if((O.replace || O.replace_line || O.insert) && O.to_insert.empty()) {
	fprintf(stderr, "\nThe -%c option must be used with the -W option.\n\n", O.replace ? 'R' : O.replace_line ? 'L' : 'I');
	return 1;
      }
      if(O.replace) printf("\nOpening file: %s\nReplacing: %s\nWith: %s\n", fn.c_str(), O.to_replace.c_str(), O.to_insert.c_str());
      if(O.replace_line) printf("\nOpening file: %s\nReplacing line after: %s\nWith: %s\n", fn.c_str(), O.to_replace.c_str(), O.to_insert.c_str());
      if(O.insert) printf("\nOpening file: %s\nInserting: %s\nAfter: %s\n", fn.c_str(), O.to_insert.c_str(), O.to_replace.c_str());
      if(!process_text_file(fn)) return 1;
      if(O.replace || O.replace_line) printf("Replaced %lu occurrence(s).\n\n", num_processed);
      if(O.insert) printf("Inserted %lu line(s).\n\n", num_processed);
      if(O.find && O.find_all && num_processed > 0) {
	printf("\nFile name: %s String: %s\n", fn.c_str(), O.to_find.c_str());
	if(num_processed > 1) printf("Found %lu matches\n", num_processed); else printf("Found 1 match\n");
      }
    }
  }
  if(num_files == 0) { fprintf(stderr, "\nYou must enter a file name.\nExiting...\n\n"); return 1; }
  // the exit code is the count, as in 6.x (an exit status holds 0-255)
  unsigned long n = (O.replace || O.replace_line || O.insert || O.section) ? num_replaced : O.find ? num_matches : 0;
  return (int)(n > 255 ? 255 : n);
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
