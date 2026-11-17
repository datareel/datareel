// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: repl6_records.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/17/2026
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

DataReel 6.4 replication example: the records repl6_writer writes and
repl6_check reads back. 11/17/2026: one record is one object of the
"readings" database of a gxv7ObjRepl node (the DataReel 7.x gxObjRepl);
the 7.x gxrepl, which copied a 6.x gxDatabase folder, went from
DataReel 7.4 beta 5.
*/
// ----------------------------------------------------------- //
#ifndef __REPL6_RECORDS_H__
#define __REPL6_RECORDS_H__

#include <string.h>
#include "gxstring.h"
#include "gxcrc32.h"
#include "membuf.h"

// One station reading, a plain 6.x record. The CRC covers the bytes
// before it.
struct Reading {
  char station[16];
  long long seq;
  double value;
  unsigned int day;
  unsigned int crc;
};

static const char *REPL6_DB = "readings";

static inline void make_reading(Reading &r, long long seq, int version)
{
  memset(&r, 0, sizeof(r));
  gxString s;
  s << "KST" << (int)(seq % 97);
  strncpy(r.station, s.c_str(), sizeof(r.station) - 1);
  r.seq = seq;
  r.value = (double)seq * 0.25 + version;
  r.day = 20261117 + (unsigned)(seq % 3);
  r.crc = (unsigned int)calcCRC32((const char *)&r, (unsigned)((char *)&r.crc - (char *)&r));
}

static inline int reading_ok(const MemoryBuffer &b)
{
  if(b.length() != sizeof(Reading)) return 0;
  Reading r;
  memcpy(&r, b.m_buf(), sizeof(r));
  return r.crc == (unsigned int)calcCRC32((const char *)&r, (unsigned)((char *)&r.crc - (char *)&r));
}

#endif // __REPL6_RECORDS_H__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
