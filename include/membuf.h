// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: membuf.h
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

The MemoryBuffer class is used to create and manipulate resizable
memory buffers. The MemoryBuffer class guarantees that each object is 
unique by storing a unique copy of each buffer with each object. This 
ensures that MemoryBuffer objects can be safely copy constructed, 
assigned, resized, and deleted by multiple threads. NOTE: Multiple 
threads accessing shared memory segments must be handled by the 
application.

Changes:
==============================================================
10/05/2001: Added the MemoryBuffer::MemSet() function used to 
fill a memory buffer starting at the specified offset with a 
specified number of bytes.

02/05/2002: Added the clean_buf argument to the MemoryBuffer::Clear()
function. If the clean_buf variable is true the contents of the buffer 
will be cleared, otherwise the logical length will be set to zero and 
the contents of the buffer will not cleared.

06/04/2002: Added the __MEMBUF_INT__ type definition and the
__MEMBUF_NO_MATCH__ constant used to eliminate signed and 
unsigned comparison warnings.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_MEMBUF_HPP__
#define __GX_MEMBUF_HPP__

#include "gxdlcode.h"

#include <string.h>

typedef unsigned __MEMBUF_INT__; // Buffer size for 32-bit compilers
const __MEMBUF_INT__ __MEMBUF_NO_MATCH__ = 0xFFFFFFFF; 

// 09/14/2005: Added null buffer class used to reserve address space 
// for null memory buffers
class GXDLCODE_API MemoryBufferNULLPtr
{
public:
  MemoryBufferNULLPtr() { }
  ~MemoryBufferNULLPtr() { }

public: // Global null memory buffer pointers
  static unsigned char MemoryBufferNUllChar;
  static unsigned char MemoryBufferNUllBuf[1];
};

// Memory buffer class
class GXDLCODE_API MemoryBuffer
{
public:
  MemoryBuffer() { mptr = 0; l_length = d_length = 0; }
  MemoryBuffer(__MEMBUF_INT__ bytes) {
    mptr = 0; l_length = d_length = 0; Alloc(bytes);
  }
  MemoryBuffer(const void *buf, __MEMBUF_INT__ bytes);
  ~MemoryBuffer() { if(mptr) delete mptr; }
  MemoryBuffer(const MemoryBuffer &buf);
  MemoryBuffer &operator=(const MemoryBuffer &buf);

public: // Pointer and length functions
  __MEMBUF_INT__ length() const { return l_length; }  // Returns logical length
  __MEMBUF_INT__ dlength() const { return d_length; } // Returns actual length
  int resize(__MEMBUF_INT__ bytes, int keep = 1);
  unsigned char *m_buf() { return mptr; }
  const unsigned char *m_buf() const { return mptr; } 
  int is_null() { return ((mptr == 0) || (l_length == 0)); }
  int is_null() const { return ((mptr == 0) || (l_length == 0)); }
  int Load(const void *buf, __MEMBUF_INT__ bytes); 
  void Clear(int clean_buf = 1);
  int MemSet(const char c, __MEMBUF_INT__ offset = 0, __MEMBUF_INT__ num = 0);

public: // Append, Insert, delete, and remove functions
  void Cat(const void *buf, __MEMBUF_INT__ bytes) { 
    InsertAt(l_length, buf, bytes); 
  }
  void Cat(unsigned char byte) { Cat((void *)&byte, 1); }
  void Cat(char byte) { Cat((void *)&byte, 1); }
  __MEMBUF_INT__ DeleteAt(__MEMBUF_INT__ position, __MEMBUF_INT__ bytes);
  __MEMBUF_INT__ InsertAt(__MEMBUF_INT__ position, const void *buf, 
			  __MEMBUF_INT__ bytes);
  __MEMBUF_INT__ InsertAt(__MEMBUF_INT__ position, const MemoryBuffer &buf) {
    return InsertAt(position, (const void *)buf.mptr, buf.l_length);
  }
  __MEMBUF_INT__ ReplaceAt(__MEMBUF_INT__ position, const void *buf,
			   __MEMBUF_INT__ bytes);
  __MEMBUF_INT__ ReplaceAt(__MEMBUF_INT__ position, const MemoryBuffer &buf) {
    return ReplaceAt(position, (const void *)buf.mptr, buf.l_length);
  }
  
public: // Search functions
  __MEMBUF_INT__ Find(void *buf, __MEMBUF_INT__ offset = 0); 
  __MEMBUF_INT__ Find(const void *buf, __MEMBUF_INT__ bytes, 
		      __MEMBUF_INT__ offset = 0) const;
  __MEMBUF_INT__ Find(MemoryBuffer &buf, 
		      __MEMBUF_INT__ offset = 0) {
    return Find(buf.mptr, buf.l_length, offset); 
  }
  __MEMBUF_INT__ Find(const MemoryBuffer &buf, 
		      __MEMBUF_INT__ offset = 0) const {
    return Find(buf.mptr, buf.l_length, offset); 
  }
  
public: // Memory allocation/de-allocation routines
  void *Alloc(__MEMBUF_INT__ bytes);
  void *Realloc(__MEMBUF_INT__ bytes, int keep = 1, int reuse = 1);
  void Destroy();
  void *FreeBytes();

public: // Conversion functions
  operator char *() const { return (char *)mptr; }
  operator const char *() const { return (const char*)mptr; }
  operator const int () const { return ((mptr != 0) && (l_length != 0)); }
  operator int () { return ((mptr != 0) && (l_length != 0)); }

public: // Overloaded operators
  int operator!() { return ((mptr == 0) || (l_length == 0)); } 
  int operator!() const { return ((mptr == 0) || (l_length == 0)); }
  unsigned char &operator[](__MEMBUF_INT__ i);
  unsigned char &operator[](__MEMBUF_INT__ i) const;
  void operator+=(const MemoryBuffer &buf) { Cat(buf.mptr, buf.l_length); }
  void operator+=(const unsigned char &byte) { Cat(byte); }
  void operator+=(const char &byte) { Cat(byte); }
  GXDLCODE_API friend MemoryBuffer operator+(const MemoryBuffer &a, 
					     const MemoryBuffer &b);

  GXDLCODE_API friend int operator==(const MemoryBuffer &a, 
				     const MemoryBuffer &b) {
    return BufferCompare(a, b) == 0;
  }

  GXDLCODE_API friend int operator!=(const MemoryBuffer &a, 
				     const MemoryBuffer &b) {
    return BufferCompare(a, b) != 0;
  }

  GXDLCODE_API friend int operator>(const MemoryBuffer &a, 
				    const MemoryBuffer &b) {
    return BufferCompare(a, b) > 0;
  }

  GXDLCODE_API friend int operator>=(const MemoryBuffer &a, 
				     const MemoryBuffer &b) {
    return BufferCompare(a, b) >= 0;
  }
  
  GXDLCODE_API friend int operator<(const MemoryBuffer &a, 
				    const MemoryBuffer &b) {
    return BufferCompare(a, b) < 0;
  }
  
  GXDLCODE_API friend int operator<=(const MemoryBuffer &a, 
				     const MemoryBuffer &b) {
    return BufferCompare(a, b) <= 0;
  }

public: // Friend functions
  // Returns -1 if a < b, 0 if a == b, and 1 if a > b
  GXDLCODE_API friend int BufferCompare(const MemoryBuffer &a, 
					const MemoryBuffer &b);  

private: 
  unsigned char *mptr;     // Pointer to start of buffer
  __MEMBUF_INT__ d_length; // Number of bytes allocated for the buffer
  __MEMBUF_INT__ l_length; // Logical length of the buffer
};

#endif  // __GX_MEMBUF_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

