// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_parsers.cpp
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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

6.3 libFuzzer targets for the other 6.x-only readers and parsers that
take file or user input (the 6.x security sweep; DataReel 7 removed
this code):

  FUZZ_HTML    gxsHTML: parse an HTML page from memory, tag attributes
  FUZZ_XML     gxsXML: load an XML file, write it back, decode/encode
               strings, attribute parsing
  FUZZ_RSS     gxRSSfeed: write RSS 1 and 2 feeds from untrusted text
  FUZZ_RDBDEF  gxRDBMS table and record definitions, def files
  FUZZ_RDBREC  gxrdDatabaseRecord::LoadDiskRecord() from a damaged
               record, then field reads, compares and keys
  FUZZ_DFILE   DiskFileB::df_GetLine() with small buffers and delimiters
  FUZZ_MEMBUF  MemoryBuffer operations driven by the input
  FUZZ_MISC    CDate, gxIP32, gxMAC48 and vendor files, gxINT64,
               ParseCVSLine(), ParseStrings(), FindMatch() wildcards

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
#include "membuf.h"
#include "dfileb.h"
#include "gxshtml.h"
#include "gxsxml.h"
#include "gxsrss.h"
#include "gxrdbhdr.h"
#include "gxrdbdef.h"
#include "cdate.h"
#include "gxip32.h"
#include "gxmac48.h"

static void write_file(const char *fname, const uint8_t *data, size_t size)
{
  FILE *fp = fopen(fname, "wb");
  if(!fp) return;
  fwrite(data, 1, size, fp);
  fclose(fp);
}

#if defined (FUZZ_HTML)
static void fuzz_html(const uint8_t *data, size_t size)
{
  MemoryBuffer mb(data, (unsigned)size);
  gxsHTML html;
  html.LoadMemoryBuffer(mb);
  gxList<gxsHTMLTagInfo> *tl = html.GetTagList();
  gxListNode<gxsHTMLTagInfo> *p = tl->GetHead();
  int n = 0;
  while(p && n++ < 1000) {
    gxsHTMLTagInfo t(p->data);
    html.ParseHTMLTagInfo(t);
    html.GetTagID(t.tag);
    p = p->next;
  }
}
#endif

#if defined (FUZZ_XML)
static void fuzz_xml(const uint8_t *data, size_t size)
{
  write_file("fz.xml", data, size);
  gxsXML xml;
  if(xml.LoadXMLFile("fz.xml")) {
    gxString tree, xs;
    xml.WriteTree(tree);
    xml.WriteXS(xs);
  }
  std::string s((const char *)data, size);
  s = std::string(s.c_str());
  gxString in(s.c_str()), out, back;
  xml.DecodeString(in, out);
  xml.EncodeString(out, back);
  gxString value, name, attrib;
  xml.GetXMLAttribute(in, "name", value);
  xml.CleanTagName(in, name);
  xml.HasAttributes(in);
  xml.CleanAttributes(in, attrib);
}
#endif

#if defined (FUZZ_RSS)
static void fuzz_rss(const uint8_t *data, size_t size)
{
  std::string s((const char *)data, size);
  s = std::string(s.c_str());
  gxRSSfeed feed;
  feed.channel.title = s.c_str();
  feed.channel.description = s.c_str();
  feed.channel.link = s.c_str();
  feed.channel.pubdate = s.c_str();
  feed.channel.image.url = s.c_str();
  feed.channel.textinput.name = s.c_str();
  gxRSSitem item;
  item.title = s.c_str();
  item.description = s.c_str();
  item.link = s.c_str();
  item.author = s.c_str();
  item.guid = s.c_str();
  feed.channel.items.Add(item);
  gxString out;
  feed.rss_version = "2.0";
  feed.WriteFeed(out);
  feed.rss_version = "1.0";
  feed.WriteFeed(out);
  gxString enc;
  feed.encode_xhtml(s.c_str(), enc);
}
#endif

#if defined (FUZZ_RDBDEF)
static void fuzz_rdbdef(const uint8_t *data, size_t size)
{
  std::string s((const char *)data, size);
  s = std::string(s.c_str());
  gxString def(s.c_str());
  {
    gxrdDatabaseTable t;
    if(t.CreateTable(def) == gxDBASE_NO_ERROR) {
      gxString rdef = t.table_header.TableDef();
      gxrdDatabaseRecord r;
      if(r.CreateRecord(t.record_def) == gxDBASE_NO_ERROR) {
	MemoryBuffer disk;
	r.CreateDiskRecord(disk);
	r.LoadDiskRecord(disk);
      }
    }
  }
  {
    gxrdDatabaseRecord r;
    if(r.CreateRecord(def) == gxDBASE_NO_ERROR) {
      MemoryBuffer disk;
      r.CreateDiskRecord(disk);
      r.LoadDiskRecord(disk);
      gxrdDatabaseRecord r2(r);
      r2.Compare(r, (__ULWORD__)-1, 1, 0); r2.Compare(r, 0, 0, 1, 1);
      for(__ULWORD__ i = 0; i < r.NumFields(); i++) {
	__ULWORD__ bytes;
	r.GetField(i, bytes);
	r.FieldHasNull(i);
	gxrdKeyType_t k = r.CreateKey(i, (int)(i & 1));
	(void)k;
      }
    }
  }
  write_file("fz.def", data, size);
  gxString tdef, kdef, fdef, fkdef, udef, gdef, err;
  gxrdReadDefFile("fz.def", "fuzz", tdef, kdef, fdef, fkdef, udef, gdef, err);
}
#endif

#if defined (FUZZ_RDBREC)
static void fuzz_rdbrec(const uint8_t *data, size_t size)
{
  static const char *defs[4] = {
    "TABLE,CHAR[16],INT32,FLOAT,VCHAR[8]",
    "TABLE,INT16,INT64,BOOL,DATE,CURRENCY",
    "TABLE,BINARY[4],BLOB,CHAR",
    "TABLE,CHAR[2],CHAR[2],CHAR[2],CHAR[2],CHAR[2],CHAR[2],CHAR[2],CHAR[2]"
  };
  if(size < 1) return;
  gxString def(defs[data[0] % 4]);
  gxrdDatabaseRecord r;
  if(r.CreateRecord(def) != gxDBASE_NO_ERROR) return;
  // A valid header for this table, followed by the input
  MemoryBuffer disk;
  r.CreateDiskRecord(disk);
  gxrdRecordHeader rh;
  unsigned hs = (unsigned)rh.SizeOf();
  MemoryBuffer in;
  if(data[0] & 0x80) {
    in.Load(data + 1, (unsigned)(size - 1)); // Damaged header too
  }
  else {
    in.Load(disk.m_buf(), (hs < disk.length()) ? hs : disk.length());
    in.Cat(data + 1, (unsigned)(size - 1));
  }
  if(r.LoadDiskRecord(in) != gxDBASE_NO_ERROR) return;
  gxrdDatabaseRecord r2;
  r2.CreateRecord(def);
  r.Compare(r2, (__ULWORD__)-1, 0, 1); r.Compare(r2, 1, 1, 0, 1);
  r2.Compare(r, (__ULWORD__)-1, 1, 0); r2.Compare(r, 0, 0, 1, 1);
  for(__ULWORD__ i = 0; i < r.NumFields(); i++) {
    __ULWORD__ bytes;
    r.GetField(i, bytes);
    r.FieldHasNull(i);
    r.FieldLength(i);
    __ULWORD__ off;
    r.FieldDiskOffset(off, i);
    gxrdKeyType_t k = r.CreateKey(i, (int)(i & 1));
    (void)k;
  }
  MemoryBuffer out;
  r.CreateDiskRecord(out);
}
#endif

#if defined (FUZZ_DFILE)
static void fuzz_dfile(const uint8_t *data, size_t size)
{
  if(size < 2) return;
  unsigned bytes = 1 + data[0] % 64;
  char delim = (char)data[1];
  write_file("fz.txt", data + 2, size - 2);
  DiskFileB f;
  if(f.df_Open("fz.txt", DiskFileB::df_READONLY) != DiskFileB::df_NO_ERROR) return;
  char *buf = new char[bytes];
  int n = 0;
  while(!f.df_EOF() && n++ < 10000) {
    if(f.df_GetLine(buf, bytes, delim, data[0] & 0x80) != DiskFileB::df_NO_ERROR) break;
    size_t l = strlen(buf); // Must be null-terminated inside the buffer
    (void)l;
  }
  delete[] buf;
  f.df_Close();
}
#endif

#if defined (FUZZ_MEMBUF)
static void fuzz_membuf(const uint8_t *data, size_t size)
{
  MemoryBuffer a, b("seed", 4);
  size_t i = 0;
  while(i + 3 <= size) {
    unsigned op = data[i] % 12, x = data[i+1], y = data[i+2];
    i += 3;
    unsigned n = (y < size - i) ? y : (unsigned)(size - i);
    const uint8_t *p = data + i;
    switch(op) {
      case 0: a.Cat(p, n); i += n; break;
      case 1: a.InsertAt(x, p, n); i += n; break;
      case 2: a.ReplaceAt(x, p, n); i += n; break;
      case 3: a.DeleteAt(x, y); break;
      case 4: a.Find(p, n, x); i += n; break;
      case 5: { std::string s((const char *)p, n); a.Find((void *)s.c_str(), x); i += n; break; }
      case 6: a.MemSet((char)x, y, x); break;
      case 7: a.resize(x * 4 + y, y & 1); break;
      case 8: b = a; a = b; a += b; break;
      case 9: a.Load(p, n); i += n; break;
      case 10: a.Clear(y & 1); a.FreeBytes(); break;
      case 11: { volatile unsigned char c = a[x]; (void)c; BufferCompare(a, b); break; }
    }
    if(a.length() > 65536) a.Destroy();
  }
}
#endif

#if defined (FUZZ_MISC)
static void fuzz_misc(const uint8_t *data, size_t size)
{
  if(size < 1) return;
  std::string s((const char *)data + 1, size - 1);
  s = std::string(s.c_str());
  const char *cs = s.c_str();
  char sbuf[1024];
  switch(data[0] % 8) {
    case 0: {
      CDate d(cs);
      CDate e(cs, '-');
      char *p = d.c_str(); delete[] p;
      p = e.month_c_str(); delete[] p;
      break;
    }
    case 1: {
      gxIP32 ip(cs);
      ip.c_str(sbuf);
      break;
    }
    case 2: {
      gxMAC48 mac(cs);
      mac.c_str(sbuf);
      mac.GetVendorCode(sbuf);
      gxMAC48 mac2(cs, '-', 4);
      mac2.c_str(sbuf, '.', 4);
      break;
    }
    case 3: {
      write_file("fz.vendors", data + 1, size - 1);
      char name[1024], comment[1024];
      FindVendor("fz.vendors", "00:00:0C", name, comment);
      gxMAC48 mac("00:00:0c:01:02:03");
      FindVendor("fz.vendors", mac, name, comment);
      break;
    }
    case 4: {
      __LLWORD__ v = StringToLLWORD(cs);
      LLWORDToString(sbuf, (__ULLWORD__)v, 2 + (int)(v & 31));
      ULLWORDToString(sbuf, StringToULLWORD(cs), 10);
      break;
    }
    case 5: {
      unsigned n = 0;
      UString *a = ParseCVSLine(UString(cs), n, 1, 1);
      delete[] a;
      a = ParseCVSLine(UString(cs), n, 0, 0);
      delete[] a;
      break;
    }
    case 6: {
      size_t cut = s.find('\x01');
      std::string s1 = s.substr(0, cut), s2 = (cut == std::string::npos) ? "," : s.substr(cut + 1);
      unsigned n = 0;
      UString *a = ParseStrings(UString(s1.c_str()), UString(s2.c_str()), n, 1, 1);
      delete[] a;
      break;
    }
    case 7: {
      size_t cut = s.find('\x01');
      std::string s1 = s.substr(0, cut), s2 = (cut == std::string::npos) ? "*" : s.substr(cut + 1);
      FindMatch(s1.c_str(), s2.c_str(), 0);
      FindMatch(s1.c_str(), s2.c_str(), 1);
      break;
    }
  }
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
#if defined (FUZZ_HTML)
  fuzz_html(data, size);
#elif defined (FUZZ_XML)
  fuzz_xml(data, size);
#elif defined (FUZZ_RSS)
  fuzz_rss(data, size);
#elif defined (FUZZ_RDBDEF)
  fuzz_rdbdef(data, size);
#elif defined (FUZZ_RDBREC)
  fuzz_rdbrec(data, size);
#elif defined (FUZZ_DFILE)
  fuzz_dfile(data, size);
#elif defined (FUZZ_MEMBUF)
  fuzz_membuf(data, size);
#elif defined (FUZZ_MISC)
  fuzz_misc(data, size);
#else
#error Define one of the FUZZ_ targets listed at the top of this file
#endif
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
