// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: memblock.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

The MemoryBlock data structure is used by the device cache
classes to store data in small blocks of memory.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "memblock.h"
#include "string.h"

MemoryBlock::MemoryBlock()
{
  l_length = 0; // No bytes are being used, set logical length to zero
  d_length = MEMORY_BLOCK_SIZE; // Total number of bytes allocated
  for(unsigned i = 0; i < MEMORY_BLOCK_SIZE; i++) mem_block[i] = 0;
}

void MemoryBlock::Load(const void *buf, unsigned bytes)
// Load data into the memory buffer.
{
  unsigned n = bytes;
  if(n > MEMORY_BLOCK_SIZE) n = MEMORY_BLOCK_SIZE; // Prevent overflow
  l_length = n; // Logical length may be smaller then the fixed block size 
  memmove(mem_block, (char *)buf, n);
}

int MemoryBlock::resize(unsigned bytes)
// Resize the logical length of this block.
// Returns false if the fixed length is exceeded.
{
  if(bytes == l_length) return 1;
  if(bytes < l_length) l_length = bytes;

  // Do not exceed maximum fixed block length
  if(bytes > l_length) {
    if(bytes <= MEMORY_BLOCK_SIZE) l_length = bytes;
    if(bytes > MEMORY_BLOCK_SIZE) { l_length = MEMORY_BLOCK_SIZE; return 0; }
  }
  
  return 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

