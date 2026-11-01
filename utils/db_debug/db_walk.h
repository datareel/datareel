// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: db_walk.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/01/2026
// Date Last Modified: 11/01/2026
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

11/01/2026: Block walk used by db_debug's find, dump and walk
commands, shared with the tests. FindBlock() added the stored
block length to the address with no checks, so a damaged length
of 0 looped forever and a huge one jumped past real blocks. A
block now counts as good only if its header tests good, its 
length is at least one block header and it ends at or before the
end of file, the same checks gxDatabaseDebugManager::Rebuild()
uses. Otherwise the search restarts one byte further on, so every
step moves forward and the walk always ends.
*/
// ----------------------------------------------------------- // 
#ifndef __GX_DB_WALK_HPP__
#define __GX_DB_WALK_HPP__

#include "gxdlcode.h"

#include "dbugmgr.h"

namespace datareel {

enum gxdWalkResult {
  gxdWALK_GOOD,  // Good block at addr; next is the following block
  gxdWALK_BAD,   // Bad block at addr; next is the next candidate or 0
  gxdWALK_ERROR  // The block header could not be read
};

// Examine the block at addr. blk receives its header. next is set to
// the next address to examine, always greater than addr, or 0 when no
// block is left.
inline gxdWalkResult gxdWalkStep(gxDatabaseDebugManager *f, FAU_t addr,
				 FAU_t eof, gxBlockHeader &blk, FAU_t &next)
{
  next = (FAU_t)0;
  if((FAU_t)(addr + sizeof(gxBlockHeader)) > eof) return gxdWALK_ERROR;
  if(f->Read(&blk, sizeof(gxBlockHeader), addr) != gxDBASE_NO_ERROR) {
    return gxdWALK_ERROR;
  }
  if(f->TestBlockHeader(blk) &&
     (FAU_t)blk.block_length >= (FAU_t)f->BlockHeaderSize() &&
     (FAU_t)(addr + (FAU_t)blk.block_length) <= eof) {
    next = addr + (FAU_t)blk.block_length;
    if(next >= eof) next = (FAU_t)0; // Last block
    return gxdWALK_GOOD;
  }
  next = f->BlockSearch(addr + 1); // Search for the next good block
  return gxdWALK_BAD;
}

// Walk every block in the file. Returns false if a header could not
// be read. good and bad receive the block counts.
inline bool gxdWalkBlocks(gxDatabaseDebugManager *f, FAU_t &good, 
			  FAU_t &bad)
{
  good = bad = (FAU_t)0;
  FAU_t eof = f->FileSize(f->DatabaseName());
  FAU_t addr = f->BlockSearch((FAU_t)0);
  gxBlockHeader blk;
  while(addr > (FAU_t)0 && addr < eof) {
    FAU_t next;
    gxdWalkResult r = gxdWalkStep(f, addr, eof, blk, next);
    if(r == gxdWALK_ERROR) return false;
    if(r == gxdWALK_GOOD) good++; else bad++;
    if(next != (FAU_t)0 && next <= addr) return false; // Cannot happen
    addr = next;
  }
  return true;
}

} // namespace datareel

#endif // __GX_DB_WALK_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
