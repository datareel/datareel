// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdonote.h
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
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
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Object notes (DataReel 7.4, per-object derived apply). While a
gxdJournal has object notes on (gxdJournal::EnableObjectNotes()), each
gxDatabase attached to it records which objects a transaction added,
changed or removed. The commit's ship log record is preceded by an
"objects" record (gxdShipObjects, gxdrepl.h) built from these notes, so
a copy can update the indexes it derives from the objects one object at
a time instead of rebuilding them.

Rules (no guessing):
  Alloc()          an add of the block's object (a deleted block reused
                   in the same transaction: an update)
  Delete/Remove()  a remove
  UnDelete()       an add
  Write()          at an object's first byte: an update of that object;
                   inside an object noted in this transaction: nothing
                   more; anywhere else: the file's notes are incomplete
  Internal writes (file header, block headers, lock headers, checksums,
  a B-tree's header) are not notes.

A file whose notes are incomplete gets "rebuild needed" on the copy.
An object's length is the size of its block's data area.

Changes:
==============================================================
11/14/2026: New in DataReel 7.4.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_OBJECT_NOTES_HPP__
#define __GX_DATABASE_OBJECT_NOTES_HPP__

#include "gxdlcode.h"
#include "gxheader.h"

#include <map>

namespace datareel {

const unsigned char gxdObjectAdd    = 1;
const unsigned char gxdObjectUpdate = 2;
const unsigned char gxdObjectRemove = 3;

struct gxdObjectNote {
  unsigned char op = 0;
  gxUINT32 old_len = 0;
  gxUINT32 new_len = 0;
};

// One file's notes for the open transaction
class GXDLCODE_API gxdObjectNotes
{
public:
  gxdObjectNotes() { }

public:
  void Clear() { complete = 1; notes.clear(); }
  void Incomplete() { complete = 0; }
  void Add(FAU_t addr, gxUINT32 len);
  void Remove(FAU_t addr, gxUINT32 len);
  // A write of "bytes" at "addr": returns 1 if a noted object of this
  // transaction holds it
  int Covers(FAU_t addr, unsigned long long bytes) const;
  void Update(FAU_t addr, gxUINT32 len);

public:
  int complete = 1;
  std::map<FAU_t, gxdObjectNote> notes; // Object (data) address -> note
};

} // namespace datareel

#endif // __GX_DATABASE_OBJECT_NOTES_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
