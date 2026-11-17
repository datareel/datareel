// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: hexdump.cpp
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

Hexadecimal file dump: shows a binary file in hex (or decimal, or as
a C array), rebuilds the file from a hex dump (-r, checked with a
CRC-32), prints CRC-32 and CRC-16 checksums and converts numbers.

Changes:
==============================================================
11/17/2026: DataReel 7.4: back from DataReel 6.x (removed in 7.0),
rewritten on the C++ standard library. The switches and the dump
formats are the 6.x ones, so a dump made by the 6.x hexdump rebuilds
here and the reverse. The CRC-16 values are the 6.x ones (ARC/XMODEM
polynomial 0xA001 and CCITT/AX.25 0x8408).
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>


using namespace datareel;

static const char *ProgramName = "hexdump";
static const char *HexdumpVersion = "4000.102";   // the 6.x program's line (4000.101), one up

static const int hxBuffSize = 16;   // bytes per line, hex
static const int decBuffSize = 8;   // bytes per line, decimal

struct Opts {
  std::string in_file, out_file;
  bool prompting = false, decimal = false, c_array = false, rebuild = false, strip = false;
  bool crc32 = false, crc16 = false;
  int lines = 8;
};

// ---------- checksums (the 6.x hxCRC tables) ----------
struct Crc {
  unsigned long t32[256]; unsigned short t16[256], tccitt[256];
  Crc() {
    for(int i = 0; i < 256; i++) {
      unsigned long c = (unsigned long)i; for(int n = 0; n < 8; n++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : c >> 1; t32[i] = c;
      unsigned short a = (unsigned short)i; for(int n = 0; n < 8; n++) a = (a & 1) ? (unsigned short)((a >> 1) ^ 0xA001) : (unsigned short)(a >> 1); t16[i] = a;
      unsigned short b = (unsigned short)i; for(int n = 0; n < 8; n++) b = (b & 1) ? (unsigned short)((b >> 1) ^ 0x8408) : (unsigned short)(b >> 1); tccitt[i] = b;
    }
  }
  unsigned long crc32(FILE *f) const {
    unsigned long c = 0xFFFFFFFFUL; int ch; rewind(f);
    while((ch = fgetc(f)) != EOF) c = t32[(c ^ (unsigned)ch) & 0xFF] ^ ((c >> 8) & 0x00FFFFFFUL);
    return (c ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
  }
  // mode 1: ARC/XMODEM/LZH (init 0, no final XOR); mode 0: CCITT/AX.25 (init 0xFFFF, final XOR)
  unsigned short crc16(FILE *f, int mode) const {
    unsigned short c = mode == 1 ? 0 : 0xFFFF; int ch; rewind(f);
    while((ch = fgetc(f)) != EOF) {
      unsigned i = (unsigned)ch & 0xFF;
      c = (unsigned short)(((c >> 8) & 0x00FF) ^ (mode == 1 ? t16[(c ^ i) & 0xFF] : tccitt[(c ^ i) & 0xFF]));
    }
    return mode == 1 ? c : (unsigned short)(c ^ 0xFFFF);
  }
};

static void help()
{
  printf("\nHexadecimal file dump program version %s (DataReel %s)\n", HexdumpVersion, DATAREEL_VERSION_STRING);
  printf("Usage: %s [switches] infile outfile (optional)\n", ProgramName);
  printf("Switches:  -?      = Display this help message.\n");
  printf("           -a      = Display in a C array.\n");
  printf("           -c      = Display a 32-bit CRC checksum value.\n");
  printf("           -C      = Display a 16-bit CRC checksum value.\n");
  printf("           -d      = Display in decimal mode (Default is HEX).\n");
  printf("           -p#     = Prompting after displaying # line: -p10\n");
  printf("           -r      = Rebuild binary file from hex dump text file.\n");
  printf("                     Only works for files created in hex mode.\n");
  printf("           -s      = Strip comments from output.\n\n");
  printf("           -x      = Convert 32-bit HEX number to DEC: -xfefe\n");
  printf("           -X      = Convert signed 32-bit HEX number to DEC.\n");
  printf("           -B      = Convert 32-bit HEX number to binary: -Bfefe\n");
  printf("           -D      = Convert DEC number to HEX: -D23\n\n");
}

static const char *bintab[16] = {
  "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
  "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"
};

// a number switch (-x -X -B -D): printed, and the program ends (1: it was one)
static bool number_switch(char sw, const char *arg)
{
  unsigned long u = strtoul(arg, 0, sw == 'D' ? 10 : 16) & 0xFFFFFFFFUL;
  switch(sw) {
    case 'x': printf("\n0x%lX = %lu\n\n", u, u); return true;
    case 'X': { long s = (long)(int)(unsigned int)u; printf("\n0x%lX = %ld\n\n", u, s); return true; }
    case 'D': printf("\n%lu = 0x%lX\n\n", u, u); return true;
    case 'B': {
      printf("\n0x%lX = ", u);
      for(int k = 7; k >= 0; k--) printf("%s%c", bintab[(u >> (4 * k)) & 0xF], k ? ' ' : '\n');
      printf("\n"); return true;
    }
  }
  return false;
}

static void pause_if(const Opts &o, bool console, int &count, FILE *out)
{
  if(!console || !o.prompting) return;
  if(++count == o.lines) { count = 0; fprintf(out, "\nPress ENTER to continue: "); fflush(out); (void)getchar(); fprintf(out, "\n"); }
}

static int dump(FILE *in, FILE *out, const Opts &o, bool console)
{
  const int per = o.decimal ? decBuffSize : hxBuffSize;
  unsigned char c[hxBuffSize];
  int count = 0;
  long total = 0, len = 0;
  if(o.c_array) { fseek(in, 0, SEEK_END); len = ftell(in); rewind(in); fprintf(out, "unsigned char CArray[%ld] = {\n", len); }
  for(;;) {
    int n = (int)fread(c, 1, (size_t)per, in);
    if(n <= 0) break;
    total += n;
    if(o.c_array) {
      fprintf(out, "  ");
      for(int j = 0; j < n; j++) { fprintf(out, "0x%02X", c[j]); if(total != len || j < n - 1) fprintf(out, ","); }
      for(int j = n; j < per; j++) fprintf(out, "   ");
      fprintf(out, "\n");
      continue;
    }
    for(int j = 0; j < n; j++) fprintf(out, o.decimal ? "%03u " : "%02X ", c[j]);
    for(int j = n; j < per; j++) fprintf(out, o.decimal ? "    " : "   ");
    if(!o.strip) {
      fprintf(out, "\t; ");
      for(int j = 0; j < n; j++) fputc(isgraph(c[j]) ? c[j] : '.', out);
    }
    fprintf(out, "\n");
    pause_if(o, console, count, out);
  }
  if(ferror(in)) { fprintf(stderr, "%s: a read error\n", ProgramName); return 1; }
  if(o.c_array) fprintf(out, "};\n");
  return 0;
}

// the bytes of a hex dump: on each line, the 2-digit hex words before the first tab or ';'
static int rebuild(const Opts &o)
{
  printf("Building new binary file from hex dump text file...\n");
  if(o.in_file.empty()) { printf("\nYou must specify a valid hex dump text file.\n\n"); return 1; }
  std::string ofname = o.out_file.empty() ? "outfile.bin" : o.out_file;
  FILE *in = fopen(o.in_file.c_str(), "rb");
  if(!in) { fprintf(stderr, "Could not open %s\n", o.in_file.c_str()); return 1; }
  FILE *out = fopen(ofname.c_str(), "w+b");
  if(!out) { fprintf(stderr, "Could not create %s\n", ofname.c_str()); fclose(in); return 1; }
  Crc crc;
  unsigned long c32 = 0xFFFFFFFFUL;
  long bytes = 0;
  std::string line;
  int ch;
  bool more = true;
  while(more) {
    line.clear();
    while((ch = fgetc(in)) != EOF && ch != '\n') line += (char)ch;
    if(ch == EOF) more = false;
    size_t cut = line.find_first_of("\t;");
    if(cut != std::string::npos) line.resize(cut);
    size_t i = 0;
    while(i < line.size()) {
      while(i < line.size() && (line[i] == ' ' || line[i] == '\r')) i++;
      size_t j = i;
      while(j < line.size() && line[j] != ' ' && line[j] != '\r') j++;
      if(j - i == 2 && isxdigit((unsigned char)line[i]) && isxdigit((unsigned char)line[i + 1])) {
	unsigned v = (unsigned)strtoul(line.substr(i, 2).c_str(), 0, 16) & 0xFF;
	unsigned char b = (unsigned char)v;
	c32 = crc.t32[(c32 ^ v) & 0xFF] ^ ((c32 >> 8) & 0x00FFFFFFUL);
	if(fwrite(&b, 1, 1, out) != 1) { fprintf(stderr, "Cannot write %s\n", ofname.c_str()); fclose(in); fclose(out); return 1; }
	bytes++;
      }
      i = j;
    }
  }
  c32 = (c32 ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
  fflush(out);
  unsigned long file32 = crc.crc32(out);
  fclose(in); fclose(out);
  if(c32 != file32) {
    printf("Checksum error in %s\nFile fails CRC-32 test.\nCRC of bytes written = 0x%08lX\nFile CRC = 0x%08lX\n\n", ofname.c_str(), c32, file32);
    return 1;
  }
  printf("Finished.\n%ld bytes written to %s\n\n", bytes, ofname.c_str());
  return 0;
}

int main(int argc, char *argv[])
{
  if(argc < 2) { help(); return 0; }
  Opts o;
  for(int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if(a[0] == '-' && a[1]) {
      char sw = a[1];
      switch(sw) {
	case '?': help(); return 0;
	case 'a': o.c_array = true; break;
	case 'c': o.crc32 = true; break;
	case 'C': o.crc16 = true; break;
	case 'p': o.prompting = true; if(atoi(a + 2) > 0) o.lines = atoi(a + 2); break;
	case 'd': o.decimal = true; break;
	case 'r': o.rebuild = true; break;
	case 's': o.strip = true; break;
	case 'x': case 'X': case 'B': case 'D': number_switch(sw, a + 2); return 0;
	default: fprintf(stderr, "\nUnknown switch %s\nExiting...\n\n", a); return 1;
      }
    }
    else if(o.in_file.empty()) o.in_file = a;
    else if(o.out_file.empty()) o.out_file = a;
  }
  if(o.rebuild) return rebuild(o);
  if(o.in_file.empty()) { printf("\nYou must specify a valid input file name.\n\n"); return 1; }
  FILE *in = fopen(o.in_file.c_str(), "rb");
  if(!in) { fprintf(stderr, "\nCannot open file: %s\nExiting...\n\n", o.in_file.c_str()); return 1; }
  if(o.crc32 || o.crc16) {
    Crc crc;
    printf("%s\n", o.in_file.c_str());
    if(o.crc32) printf("CRC-32 = 0x%08lX\n", crc.crc32(in));
    else { printf("CRC-16 = 0x%04X\n", crc.crc16(in, 1)); printf("CCITT CRC-16 = 0x%04X\n", crc.crc16(in, 0)); }
    fclose(in);
    return 0;
  }
  int rc;
  if(o.out_file.empty()) rc = dump(in, stdout, o, true);
  else {
    FILE *out = fopen(o.out_file.c_str(), "w");
    if(!out) { fprintf(stderr, "\nCannot create %s\n\n", o.out_file.c_str()); fclose(in); return 1; }
    fprintf(stderr, "Processing file...\n");
    rc = dump(in, out, o, false);
    if(fclose(out) != 0) rc = 1;
    fprintf(stderr, "Finished.\n");
  }
  fclose(in);
  return rc;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
