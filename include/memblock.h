// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: memblock.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

The MemoryBlock data structure is use by the device cache
classes to store data in small blocks of memory with a fixed
length. 
*/
// ----------------------------------------------------------- //   
#ifndef __GX_MEMBLOCK_HPP__
#define __GX_MEMBLOCK_HPP__

#include "gxdlcode.h"

const unsigned MEMORY_BLOCK_SIZE = 1024;

struct GXDLCODE_API MemoryBlock {
  MemoryBlock();
  ~MemoryBlock() { }

  unsigned length() { return l_length; }
  unsigned dlength() { return d_length; }
  char *m_buf() { return (char *)mem_block; } 
  const char *m_buf() const { return (char *)mem_block; } 
  void Load(const void *buf, unsigned bytes);
  int resize(unsigned bytes);
  
  char mem_block[MEMORY_BLOCK_SIZE];
  unsigned d_length;   // Number of bytes allocated for the buffer
  unsigned l_length;   // Logical length of the buffer
};

#endif // __GX_MEMBLOCK_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
