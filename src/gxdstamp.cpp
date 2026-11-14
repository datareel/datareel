// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdstamp.cpp
// Compiler Used: GCC
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
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Derived index stamps (gxdstamp.h).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#if !defined (__WIN32__)
#include <fcntl.h>
#include <unistd.h>
#endif
#include "gxdstamp.h"
#include "gxcrc32.h"

namespace datareel {

static const char stamp_magic[8] = { 'G','X','D','S','T','A','M','P' };

static void put32(unsigned char *p, uint32_t v) { memcpy(p, &v, 4); }
static void put64(unsigned char *p, uint64_t v) { memcpy(p, &v, 8); }
static uint32_t get32(const unsigned char *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t get64(const unsigned char *p) { uint64_t v; memcpy(&v, p, 8); return v; }

static void encode(const gxdStampData &d, unsigned char *b)
{
  memset(b, 0, gxdStampSize);
  memcpy(b, stamp_magic, 8);
  put32(b + 8, gxdStampFormat);
  put32(b + 12, d.digest_kind);
  std::string id = d.log_id.substr(0, 16);
  memcpy(b + 16, id.data(), id.size());
  put64(b + 32, d.lsn);
  put64(b + 40, d.records);
  put64(b + 48, d.time_us);
  put32(b + 56, d.flags);
  put32(b + 60, (uint32_t)calcCRC32((const char *)b, 60));
}

static int decode(const unsigned char *b, gxdStampData &d)
{
  d = gxdStampData();
  if(memcmp(b, stamp_magic, 8) != 0) return -1;
  if(get32(b + 60) != (uint32_t)calcCRC32((const char *)b, 60)) return -1;
  if(get32(b + 8) != gxdStampFormat) return -1;
  d.digest_kind = get32(b + 12);
  size_t n = 0;
  while(n < 16 && b[16 + n]) n++;
  d.log_id.assign((const char *)b + 16, n);
  d.lsn = get64(b + 32);
  d.records = get64(b + 40);
  d.time_us = get64(b + 48);
  d.flags = get32(b + 56);
  return 0;
}

int gxdDerivedStamp::IsStamp(const std::string &name)
{
  // "<index>.stamp": something before the suffix (not gxrepl's derived.stamp); no copies (11/14/2026 review:
  // called for every file of every commit)
  static const char ext[] = ".stamp";
  const size_t el = sizeof(ext) - 1;
  size_t k = name.find_last_of('/');
  size_t b = k == std::string::npos ? 0 : k + 1, bl = name.size() - b;
  if(bl <= el) return 0;
  if(name.compare(name.size() - el, el, ext) != 0) return 0;
  if(bl == 13 && name.compare(b, 13, "derived.stamp") == 0) return 0;
  return 1;
}

gxDatabaseError gxdDerivedStamp::Open(const char *path, int create)
{
  Close();
  if(!path || !*path) return gxDBASE_NULL_PTR;
  gxDatabaseError e;
  FILE *t = fopen(path, "rb");
  if(t) { fclose(t); e = f.Open(path); }
  else if(create) {
    e = f.Create(path, (FAU_t)gxdStampSize);
    if(e == gxDBASE_NO_ERROR) {
      unsigned char b[gxdStampSize];
      gxdStampData d;
      encode(d, b);
      e = f.Write(b, (__ULWORD__)gxdStampSize, (FAU_t)f.FileHeaderSize());
      if(e == gxDBASE_NO_ERROR) e = f.Flush();
    }
  }
  else return gxDBASE_FILE_OPEN_ERROR;
  if(e != gxDBASE_NO_ERROR) { f.Close(); return e; }
  if((size_t)f.StaticArea() < gxdStampSize) { f.Close(); return gxDBASE_BAD_REFERENCE; }
  return gxDBASE_NO_ERROR;
}

void gxdDerivedStamp::Close()
{
  if(f.IsOpen()) f.Close();
}

int gxdDerivedStamp::Read(gxdStampData &d)
{
  d = gxdStampData();
  if(!f.IsOpen()) return -1;
  unsigned char b[gxdStampSize];
  if(f.Read(b, (__ULWORD__)gxdStampSize, (FAU_t)f.FileHeaderSize()) != gxDBASE_NO_ERROR) return -1;
  return decode(b, d);
}

gxDatabaseError gxdDerivedStamp::Write(const gxdStampData &d)
{
  if(!f.IsOpen()) return gxDBASE_FILE_NOT_READY;
  unsigned char b[gxdStampSize];
  encode(d, b);
  return f.Write(b, (__ULWORD__)gxdStampSize, (FAU_t)f.FileHeaderSize());
}

int gxdDerivedStamp::ReadFile(const std::string &path, gxdStampData &d)
{
  d = gxdStampData();
  FILE *t = fopen(path.c_str(), "rb");
  if(!t) return 1;
  fclose(t);
  gxDatabase db;
  if(db.Open(path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) return -1;
  int rc = -1;
  if((size_t)db.StaticArea() >= gxdStampSize) {
    unsigned char b[gxdStampSize];
    if(db.Read(b, (__ULWORD__)gxdStampSize, (FAU_t)db.FileHeaderSize()) == gxDBASE_NO_ERROR)
      rc = decode(b, d);
  }
  db.Close();
  return rc;
}

int gxdDerivedStamp::WriteFresh(const std::string &path, const gxdStampData &d)
{
  std::string tmp = path + ".tmp";
  remove(tmp.c_str());
  {
    gxDatabase db;
    if(db.Create(tmp.c_str(), (FAU_t)gxdStampSize) != gxDBASE_NO_ERROR) return -1;
    unsigned char b[gxdStampSize];
    encode(d, b);
    if(db.Write(b, (__ULWORD__)gxdStampSize, (FAU_t)db.FileHeaderSize()) != gxDBASE_NO_ERROR ||
       db.Sync(1) != gxDBASE_NO_ERROR) {
      db.Close(); remove(tmp.c_str()); return -1;
    }
    db.Close();
  }
#if !defined (__WIN32__)
  if(rename(tmp.c_str(), path.c_str()) != 0) { remove(tmp.c_str()); return -1; }
  std::string dir = path.find_last_of('/') == std::string::npos ? "." : path.substr(0, path.find_last_of('/'));
  if(dir.empty()) dir = "/";
  int dfd = open(dir.c_str(), O_RDONLY);
  if(dfd >= 0) { fsync(dfd); close(dfd); }
#else
  remove(path.c_str());
  if(rename(tmp.c_str(), path.c_str()) != 0) { remove(tmp.c_str()); return -1; }
#endif
  return 0;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
