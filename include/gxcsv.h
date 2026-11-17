// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxcsv.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
// Date Last Modified: 11/17/2026
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

CSV (comma separated values) reader and writer, RFC 4180: fields
separated by a delimiter (',' by default), a field in double quotes
may hold the delimiter, quotes (doubled: "") and line breaks; rows end
with CR LF or LF. gxCSVReader reads a file in pieces (any size) or
text in memory, one row at a time; gxCSVWriter writes rows, quoting
only the fields that need it (or every field). A spreadsheet's UTF-8
byte order mark at the start of a file is skipped.

Changes:
==============================================================
11/17/2026: New in DataReel 7.4 (from dougg's merge_ss, 2009).
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_CSV_HPP__
#define __GX_CSV_HPP__

#include "gxdlcode.h"

#include <stdio.h>
#include <string>
#include <string_view>
#include <vector>

namespace datareel {

struct GXDLCODE_API gxCSVOptions {
  char delimiter = ',';      // ';' and '\t' are common too
  char quote = '"';
  // reading
  bool strict = false;       // a quote inside an unquoted field, text after a closing quote or a quoted field
			     // never closed is an error (Error()); otherwise it is read as text
  bool trim_unquoted = false;// spaces and tabs around an unquoted field are dropped
  bool skip_bom = true;      // a UTF-8 byte order mark at the start is skipped
  bool skip_empty_rows = false; // a row with nothing on it (an empty line) is not returned
  // writing
  bool crlf = true;          // rows end with CR LF (RFC 4180); false: LF
  bool quote_all = false;    // every field in quotes; otherwise only those with the delimiter, a quote, CR, LF,
			     // or leading or trailing space
};

class GXDLCODE_API gxCSVReader
{
public:
  explicit gxCSVReader(const gxCSVOptions &o = gxCSVOptions());
  ~gxCSVReader();
  gxCSVReader(const gxCSVReader &) = delete;
  gxCSVReader &operator=(const gxCSVReader &) = delete;

public:
  // a file to read (closed by Close() or the destructor); false and Error() if it cannot be opened
  bool Open(const std::string &fname);
  // text in memory instead (copied)
  void SetText(std::string_view text);
  void Close();
  // the next row into FIELDS: true, or false at the end or on an error (Error() then says why). An empty line is a
  // row with one empty field (unless skip_empty_rows)
  bool ReadRow(std::vector<std::string> &fields);
  // the first row as the header: column names for Column()
  bool ReadHeader();
  const std::vector<std::string> &Header() const { return header; }
  // a column's index in the header, or -1
  int Column(std::string_view name) const;
  bool Error() const { return !error.empty(); }
  const std::string &ErrorMessage() const { return error; }
  // the line (1 based) where the last row read started, and the number of rows read
  unsigned long Line() const { return row_line; }
  unsigned long Rows() const { return rows; }
  // every row of TEXT (false and Error() on an error in strict mode)
  static bool ParseText(std::string_view text, std::vector<std::vector<std::string>> &rows,
			const gxCSVOptions &o = gxCSVOptions(), std::string *why = 0);

private:
  int next();                // the next byte, or -1 at the end
  int peek();
  bool fill();

private:
  gxCSVOptions opt;
  FILE *fp = 0;
  std::string buf;           // the text (SetText) or the file's current piece
  size_t pos = 0;
  bool from_file = false, at_eof = false, started = false;
  unsigned long line = 1, row_line = 0, rows = 0;
  std::string error;
  std::vector<std::string> header;
};

class GXDLCODE_API gxCSVWriter
{
public:
  explicit gxCSVWriter(const gxCSVOptions &o = gxCSVOptions());
  ~gxCSVWriter();
  gxCSVWriter(const gxCSVWriter &) = delete;
  gxCSVWriter &operator=(const gxCSVWriter &) = delete;

public:
  // a file to write (APPEND: added to the end); false and Error() if it cannot be opened
  bool Open(const std::string &fname, bool append = false);
  // rows appended to OUT instead (it must outlive the writer)
  void SetOutput(std::string *out);
  bool WriteRow(const std::vector<std::string> &fields);
  // the file flushed and closed: false if a write failed
  bool Close();
  bool Error() const { return !error.empty(); }
  const std::string &ErrorMessage() const { return error; }
  // one field as written (quoted if needed), and one row with its line end
  static std::string FormatField(std::string_view field, const gxCSVOptions &o = gxCSVOptions());
  static std::string FormatRow(const std::vector<std::string> &fields, const gxCSVOptions &o = gxCSVOptions());

private:
  gxCSVOptions opt;
  FILE *fp = 0;
  std::string *out = 0;
  std::string error;
};

} // namespace datareel

#endif // __GX_CSV_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
