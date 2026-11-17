// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxcsv.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
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

CSV reader and writer (RFC 4180). See gxcsv.h.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <string.h>
#include "gxcsv.h"

namespace datareel {

static const size_t gxCSV_PIECE = 64 * 1024;   // a file is read in pieces of this size

// ---------- reader ----------
gxCSVReader::gxCSVReader(const gxCSVOptions &o) : opt(o) { }
gxCSVReader::~gxCSVReader() { Close(); }

bool gxCSVReader::Open(const std::string &fname)
{
  Close();
  fp = fopen(fname.c_str(), "rb");                           // binary: CR LF is the reader's to handle
  if(!fp) { error = "cannot open " + fname; return false; }
  from_file = true;
  return true;
}

void gxCSVReader::SetText(std::string_view text)
{
  Close();
  buf.assign(text.data(), text.size());
  at_eof = true;
}

void gxCSVReader::Close()
{
  if(fp) { fclose(fp); fp = 0; }
  buf.clear(); pos = 0; from_file = false; at_eof = false; started = false;
  line = 1; row_line = 0; rows = 0; error.clear(); header.clear();
}

bool gxCSVReader::fill()
{
  if(!from_file || at_eof || !fp) return false;
  buf.erase(0, pos); pos = 0;
  size_t have = buf.size();
  buf.resize(have + gxCSV_PIECE);
  size_t n = fread(&buf[have], 1, gxCSV_PIECE, fp);
  buf.resize(have + n);
  if(n == 0) {
    at_eof = true;
    if(ferror(fp)) error = "a read error";
    return false;
  }
  return true;
}

int gxCSVReader::next()
{
  if(pos >= buf.size() && !fill()) return -1;
  return (unsigned char)buf[pos++];
}

int gxCSVReader::peek()
{
  if(pos >= buf.size() && !fill()) return -1;
  return (unsigned char)buf[pos];
}

bool gxCSVReader::ReadRow(std::vector<std::string> &fields)
{
  fields.clear();
  if(!error.empty()) return false;
  if(!started) {
    started = true;
    if(opt.skip_bom) {
      while(buf.size() - pos < 3 && fill()) { }
      if(buf.size() - pos >= 3 && (unsigned char)buf[pos] == 0xEF && (unsigned char)buf[pos + 1] == 0xBB &&
	 (unsigned char)buf[pos + 2] == 0xBF) pos += 3;
    }
  }
  const int d = (unsigned char)opt.delimiter, q = (unsigned char)opt.quote;
  for(;;) {
    if(peek() < 0) return false;                              // the end (or a read error: Error())
    row_line = line;
    std::string f;
    bool quoted = false, in_quotes = false, after_quote = false, any_quoted = false;
    unsigned long quote_line = 0;
    auto push = [&]() {
      if(!quoted && opt.trim_unquoted) {
	size_t a = f.find_first_not_of(" \t"), b = f.find_last_not_of(" \t");
	f = a == std::string::npos ? std::string() : f.substr(a, b - a + 1);
      }
      fields.push_back(f);
      f.clear(); quoted = false; in_quotes = false; after_quote = false;
    };
    for(;;) {
      int c = next();
      if(c < 0) {                                             // the end of the text ends the row
	if(in_quotes && opt.strict) {
	  error = "a quoted field opened on line " + std::to_string(quote_line) + " is never closed";
	  fields.clear(); return false;
	}
	push();
	break;
      }
      if(in_quotes) {
	if(c == q) {
	  if(peek() == q) { next(); f += (char)q; }           // "" is one quote
	  else { in_quotes = false; after_quote = true; }
	}
	else { if(c == '\n') line++; f += (char)c; }
	continue;
      }
      if(c == d) { push(); continue; }
      if(c == '\r') { if(peek() == '\n') next(); line++; push(); break; }
      if(c == '\n') { line++; push(); break; }
      if(c == q && !after_quote && !quoted &&
	 (f.empty() || (opt.trim_unquoted && f.find_first_not_of(" \t") == std::string::npos))) {
	f.clear(); in_quotes = true; quoted = true; any_quoted = true; quote_line = line;
	continue;
      }
      if(after_quote) {
	if(opt.trim_unquoted && (c == ' ' || c == '\t')) continue;   // spaces after the closing quote
	if(opt.strict) {
	  error = "text after a closing quote on line " + std::to_string(line) + ", field " + std::to_string(fields.size() + 1);
	  fields.clear(); return false;
	}
	f += (char)c;                                          // lenient: kept as text
	continue;
      }
      if(c == q && opt.strict) {
	error = "a quote inside an unquoted field on line " + std::to_string(line) + ", field " + std::to_string(fields.size() + 1);
	fields.clear(); return false;
      }
      f += (char)c;
    }
    if(opt.skip_empty_rows && fields.size() == 1 && fields[0].empty() && !any_quoted) {   // an empty line ("" is not)
      fields.clear();                                          // the skipped line leaves nothing in the next row
      if(peek() < 0) return false;
      continue;
    }
    rows++;
    return true;
  }
}

bool gxCSVReader::ReadHeader()
{
  header.clear();
  return ReadRow(header);
}

int gxCSVReader::Column(std::string_view name) const
{
  for(size_t i = 0; i < header.size(); i++) if(header[i] == name) return (int)i;
  return -1;
}

bool gxCSVReader::ParseText(std::string_view text, std::vector<std::vector<std::string>> &out,
			    const gxCSVOptions &o, std::string *why)
{
  out.clear();
  gxCSVReader r(o);
  r.SetText(text);
  std::vector<std::string> row;
  while(r.ReadRow(row)) out.push_back(row);
  if(why) *why = r.ErrorMessage();
  return !r.Error();
}

// ---------- writer ----------
gxCSVWriter::gxCSVWriter(const gxCSVOptions &o) : opt(o) { }
gxCSVWriter::~gxCSVWriter() { Close(); }

bool gxCSVWriter::Open(const std::string &fname, bool append)
{
  Close();
  error.clear();
  fp = fopen(fname.c_str(), append ? "ab" : "wb");
  if(!fp) { error = "cannot open " + fname; return false; }
  return true;
}

void gxCSVWriter::SetOutput(std::string *o)
{
  Close();
  error.clear();
  out = o;
}

std::string gxCSVWriter::FormatField(std::string_view f, const gxCSVOptions &o)
{
  bool need = o.quote_all;
  if(!need) {
    for(char c : f) if(c == o.delimiter || c == o.quote || c == '\r' || c == '\n') { need = true; break; }
    if(!need && !f.empty() && (f.front() == ' ' || f.front() == '\t' || f.back() == ' ' || f.back() == '\t')) need = true;
  }
  if(!need) return std::string(f);
  std::string s;
  s.reserve(f.size() + 2);
  s += o.quote;
  for(char c : f) { if(c == o.quote) s += o.quote; s += c; }
  s += o.quote;
  return s;
}

std::string gxCSVWriter::FormatRow(const std::vector<std::string> &fields, const gxCSVOptions &o)
{
  std::string s;
  for(size_t i = 0; i < fields.size(); i++) {
    if(i) s += o.delimiter;
    s += FormatField(fields[i], o);
  }
  // one empty field alone would read back as an empty line (skipped with skip_empty_rows): written quoted
  if(fields.size() == 1 && fields[0].empty() && !o.quote_all) { s += o.quote; s += o.quote; }
  s += o.crlf ? "\r\n" : "\n";
  return s;
}

bool gxCSVWriter::WriteRow(const std::vector<std::string> &fields)
{
  if(!error.empty()) return false;
  std::string r = FormatRow(fields, opt);
  if(out) { out->append(r); return true; }
  if(!fp) { error = "no file or output to write to"; return false; }
  if(fwrite(r.data(), 1, r.size(), fp) != r.size()) { error = "a write failed"; return false; }
  return true;
}

bool gxCSVWriter::Close()
{
  bool ok = error.empty();
  if(fp) {
    if(fflush(fp) != 0) ok = false;
    if(fclose(fp) != 0) ok = false;
    fp = 0;
    if(!ok && error.empty()) error = "a write failed";
  }
  out = 0;
  return ok;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
