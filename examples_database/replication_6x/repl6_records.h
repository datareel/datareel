// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: repl6_records.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/15/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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
repl6_check reads. The first object of data.gxd is the header; the
records follow in the order they were written.
*/
// ----------------------------------------------------------- //
#ifndef __REPL6_RECORDS_H__
#define __REPL6_RECORDS_H__

#include <stdio.h>
#include <string.h>
#include "gxcrc32.h"

#define REPL6_MAGIC "REPL6DB1"

struct ReplHeader {
  char magic[8];
  unsigned long count;    // Records written by whole commits
  unsigned long commits;
  unsigned long crc;      // Running CRC-32 of every record, in order
};

struct ReplRecord {
  unsigned long seq;      // 0, 1, 2 ... in write order
  char text[48];
  unsigned long crc;      // CRC-32 of seq and text
};

inline unsigned long RecordSelfCRC(const ReplRecord &r)
{
  char buf[sizeof(r.seq) + sizeof(r.text)];
  memcpy(buf, &r.seq, sizeof(r.seq));
  memcpy(buf + sizeof(r.seq), r.text, sizeof(r.text));
  return calcCRC32(buf, (unsigned)sizeof(buf));
}

inline void MakeRecord(ReplRecord &r, unsigned long seq)
{
  memset(&r, 0, sizeof(r));
  r.seq = seq;
  sprintf(r.text, "record %lu written by a DataReel 6.x program", seq);
  r.crc = RecordSelfCRC(r);
}

// The header's running checksum after one more record
inline unsigned long RecordCRC(unsigned long crc, const ReplRecord &r)
{
  const unsigned char *p = (const unsigned char *)&r;
  for(unsigned i = 0; i < sizeof(r); i++) crc = calcCRC32(p[i], crc);
  return crc;
}

#endif // __REPL6_RECORDS_H__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
