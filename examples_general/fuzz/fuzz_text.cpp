// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_text.cpp
// Compiler Used: clang (libFuzzer)
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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

6.3 libFuzzer targets for the 6.x text code that reads file or user
input (the 6.x security sweep; DataReel 7 replaced this code):

  FUZZ_CFG  gxConfig: load a config file, read values, change a value
            (the Encrypted Password Database settings file)
  FUZZ_STR  gxString (UString) searches, edits and conversions, and the
            strutil compares, pattern matches and number parsers
  FUZZ_B64  gxsBase64Decode() and gxsBase64Encode() (fcrypt and the
            Encrypted Password Database store user names with them)

Build with examples_general/fuzz/build_fuzz.sh (clang, ASan, UBSan).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "gxstring.h"
#include "strutil.h"
#include "gxconfig.h"
#include "gxs_b64.h"

#if defined (FUZZ_CFG)
static void fuzz_cfg(const uint8_t *data, size_t size)
{
  FILE *fp = fopen("fz.cfg", "wb");
  if(!fp) return;
  fwrite(data, 1, size, fp);
  fclose(fp);
  gxConfig cfg("fz.cfg");
  char *v = cfg.GetStrValue("Name");
  if(v) { size_t n = strlen(v); (void)n; }
  cfg.GetIntValue("Size");
  cfg.GetFloatValue("Ratio");
  cfg.GetLongValue("Long");
  cfg.ChangeConfigValue("Name", "value");
  cfg.ChangeConfigValue("NewKey", 42);
}
#endif

#if defined (FUZZ_STR)
static void fuzz_str(const uint8_t *data, size_t size)
{
  if(size < 4) return;
  unsigned a = data[0], b = data[1];
  std::string all((const char *)data + 4, size - 4);
  size_t cut = all.find('\x01');
  std::string s1 = all.substr(0, cut), s2 = (cut == std::string::npos) ? "" : all.substr(cut + 1);
  s1 = std::string(s1.c_str()); // As C strings
  s2 = std::string(s2.c_str());
  gxString g(s1.c_str());
  g.Find(s2.c_str());
  g.IFind(s2.c_str(), a);
  g.FindLast(s2.c_str());
  g.Atoi(); g.Atol(); g.Atof(); g.Atou();
  { gxString t = g; t.ReplaceString(s2.c_str(), "xy"); }
  { gxString t = g; t.FilterString(s2.c_str()); }
  { gxString t = g; t.FilterChar((char)b); t.TrimLeadingSpaces(); t.TrimTrailingSpaces(); }
  { gxString t = g; t.DeleteAfter(s2.c_str()); }
  { gxString t = g; t.DeleteBeforeLastIncluding(s2.c_str()); }
  { gxString t = g; t.DeleteAfterLastIncluding(s2.c_str()); }
  { gxString t = g; t.InsertAt(a, s2.c_str()); t.ReplaceAt(b, s2.c_str()); t.DeleteAt(a, b); }
  { gxString t = g; t.ConvertEscapeSeq('%', 1 + (data[2] % 3)); }
  { gxString t = g; t.ToUpper(); t.ToLower(); t.Cat(s2.c_str()); t.Left(a); t.Right(b); t.Mid(a, b); }
  CaseICmp(s1.c_str(), s2.c_str());
  StringANCompare(s1.c_str(), s2.c_str(), data[3] & 1);
  FindMatch(s1.c_str(), s2.c_str(), data[2] & 1);
  IFindMatch(s1.c_str(), s2.c_str(), data[2] & 1);
  StringToULWORD(s1.c_str());
  StringToULLWORD(s1.c_str());
  StringToLLWORD(s1.c_str());
  gxfind_pattern(s1.data(), (unsigned)s1.size(), s2.data(), (unsigned)s2.size(), a, gxASCII_STRING, data[3] & 2);
  char words[MAXWORDS][MAXWORDLENGTH];
  int numwords = 0;
  parse(s1.c_str(), words, &numwords, (char)b);
}
#endif

#if defined (FUZZ_B64)
static void fuzz_b64(const uint8_t *data, size_t size)
{
  std::string s((const char *)data, size);
  std::string c(s.c_str());
  std::string dec(c.size() + 4, '\0');
  gxsBase64Decode(c.c_str(), &dec[0]);
  std::string enc(gxsCalculateBase64Length((int)size) + 4, '\0');
  gxsBase64Encode(s.data(), &enc[0], (int)size);
  std::string back(enc.size() + 4, '\0');
  gxsBase64Decode(enc.c_str(), &back[0]);
  if(memcmp(back.data(), s.data(), size) != 0) __builtin_trap(); // Round trip
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
#if defined (FUZZ_CFG)
  fuzz_cfg(data, size);
#elif defined (FUZZ_STR)
  fuzz_str(data, size);
#elif defined (FUZZ_B64)
  fuzz_b64(data, size);
#else
#error Define FUZZ_CFG, FUZZ_STR or FUZZ_B64
#endif
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
