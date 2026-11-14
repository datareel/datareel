// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdonote.cpp
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

Object notes of one file for one transaction (gxdonote.h). Several
changes to one object in a transaction become one note:

  add + update      add          update + update   update
  add + remove      (none)       update + remove   remove
  remove + add      update       (a deleted block reused in place)

Anything else (an update of a removed object, a second add) makes the
file's notes incomplete.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include "gxdonote.h"

namespace datareel {

void gxdObjectNotes::Add(FAU_t addr, gxUINT32 len)
{
  if(!complete) return;
  auto it = notes.find(addr);
  if(it == notes.end()) {
    gxdObjectNote n;
    n.op = gxdObjectAdd; n.old_len = 0; n.new_len = len;
    notes[addr] = n;
    return;
  }
  if(it->second.op == gxdObjectRemove) { // Removed, then reused in place
    it->second.op = gxdObjectUpdate;
    it->second.new_len = len;
    return;
  }
  complete = 0;
}

void gxdObjectNotes::Remove(FAU_t addr, gxUINT32 len)
{
  if(!complete) return;
  auto it = notes.find(addr);
  if(it == notes.end()) {
    gxdObjectNote n;
    n.op = gxdObjectRemove; n.old_len = len; n.new_len = 0;
    notes[addr] = n;
    return;
  }
  if(it->second.op == gxdObjectAdd) { notes.erase(it); return; }
  if(it->second.op == gxdObjectUpdate) {
    it->second.op = gxdObjectRemove;
    it->second.new_len = 0;
    return;
  }
  complete = 0;
}

void gxdObjectNotes::Update(FAU_t addr, gxUINT32 len)
{
  if(!complete) return;
  auto it = notes.find(addr);
  if(it == notes.end()) {
    gxdObjectNote n;
    n.op = gxdObjectUpdate; n.old_len = len; n.new_len = len;
    notes[addr] = n;
    return;
  }
  if(it->second.op == gxdObjectRemove) complete = 0;
}

int gxdObjectNotes::Covers(FAU_t addr, unsigned long long bytes) const
{
  if(notes.empty()) return 0;
  auto it = notes.upper_bound(addr);
  if(it == notes.begin()) return 0;
  --it;
  if(it->second.op == gxdObjectRemove) return 0;
  unsigned long long off = (unsigned long long)(addr - it->first);
  return off + bytes <= (unsigned long long)it->second.new_len;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
