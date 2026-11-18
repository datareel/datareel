// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
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

6.3.1 tests for the PostScript and HTML print drivers (11/14/2026),
built into the library when __USE_POSTSCRIPT_PRINTING__ and
__USE_HTM_PRINTING__ are defined (the default in env/linux.env and the
CMake build). Run it under AddressSanitizer for the over-reads.

  ps_cut      ProcessText() truncating a long line: the copy overwrote
              the terminator and PrintLine() read past the buffer.
  ps_filter   ProcessText() nroff filter on a long line: read up to 4
              bytes past the end of the string.
  ps_wrap     ConvertTextFile() wrapping a long line: the same lost
              terminator.
  ps_maxlen   PrintLine(s, max_len, ofstream).
  html_color  HyperText color values out of the table range.

Returns 0 if every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iterator>
#include "pscript.h"
#include "htmldrv.h"

#if defined (__USE_POSTSCRIPT_PRINTING__) && defined (__USE_HTM_PRINTING__)

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  fflush(stdout);
  if(!ok) failures++;
}

static int count_char(const std::string &s, char c)
// Count c inside the PostScript strings "(...)" only, not in the
// operator and font names around them
{
  int n = 0, in = 0;
  for(size_t i = 0; i < s.size(); i++) {
    if(!in && s[i] == '(') in = 1;
    else if(in && s[i] == '\\') i++;
    else if(in && s[i] == ')') in = 0;
    else if(in && s[i] == c) n++;
  }
  return n;
}

int main()
{
  PostScriptDrv ps;
  ps.DocumentSetup();
  int cols = ps.Columns();
  check(cols > 10 && cols < 1000, "ps: DocumentSetup() column count");

  // ps_cut: a heap line of exactly cols+50 'a' characters
  {
    std::string line(cols + 50, 'a');
    char *in = new char[line.size() + 1];
    memcpy(in, line.c_str(), line.size() + 1);
    std::ostringstream out;
    int rv = ps.ProcessText(in, out, 1, 0);
    check(rv == 1 && count_char(out.str(), 'a') == cols,
	  "ps_cut: a long line is cut to the column count");
    delete[] in;
  }

  // ps_filter: the nroff filter on a long line ending in a repeated pair
  {
    std::string line(cols + 3, 'b');
    line += "xyxyx";
    char *in = new char[line.size() + 1];
    memcpy(in, line.c_str(), line.size() + 1);
    std::ostringstream out;
    ps.ProcessText(in, out, 1, 1);
    check(1, "ps_filter: nroff filter at the end of a long line (ASan)");
    delete[] in;
  }

  // ps_wrap: ConvertTextFile() wraps a long line
  {
    const char *fn = "print_sec.txt";
    FILE *fp = fopen(fn, "wb");
    std::string line(cols * 2 + 5, 'c');
    fputs(line.c_str(), fp); fputs("\n", fp);
    fclose(fp);
    DiskFileB f;
    int ok = f.df_Open(fn, DiskFileB::df_READONLY) == DiskFileB::df_NO_ERROR;
    std::ostringstream out;
    int rv = ok ? ps.ConvertTextFile(f, out, 1, 0) : 0;
    f.df_Close();
    remove(fn);
    int expect = (int)line.size();
    if(expect > MAX_LINE - 1) expect = MAX_LINE - 1; // df_GetLine() limit
    check(rv == 1 && count_char(out.str(), 'c') == expect,
	  "ps_wrap: a wrapped line prints each character once");
  }

  // ps_maxlen: PrintLine() with a length limit
  {
    const char *fn = "print_sec.ps";
    std::ofstream os(fn);
    char in[64];
    memset(in, 'd', 63); in[63] = 0;
    int rv = ps.PrintLine(in, 10, os);
    os.close();
    std::ifstream is(fn);
    std::string all((std::istreambuf_iterator<char>(is)),
		    std::istreambuf_iterator<char>());
    is.close();
    remove(fn);
    check(rv == 1 && count_char(all, 'd') == 10, "ps_maxlen: PrintLine() cuts to max_len");
  }

  // html_color
  {
    std::ostringstream out;
    HyperText ht(out);
    ht.StartBody((htmCOLORS)99);
    ht.GenTable(1, 4, 75, 1, (htmCOLORS)-5);
    ht.GetSystemTime(0);
    check(out.str().find("BGCOLOR=\"#000000\"") != std::string::npos &&
	  out.str().find("BORDERCOLOR=\"#000000\"") != std::string::npos,
	  "html_color: colors out of range use the first color");
  }

  if(failures) printf("%d check(s) FAILED\n", failures);
  else printf("All checks passed\n");
  return failures ? 1 : 0;
}
#else
int main()
{
  printf("SKIP: the library was built without the print drivers\n");
  return 0;
}
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
