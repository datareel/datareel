// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: dbasekey.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/22/2000 
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

The DatabaseKeyB base class is an abstract base class used define
database key types and the methods by which database keys are
sorted.  

Changes:
==============================================================
11/05/2001: Added single comparison method to DatabaseKeyB class. 
To use a single compare instead of the greater then and equal to 
operators define the __USE_SINGLE_COMPARE__ preprocessor directive 
when compiling. NOTE: If a dual comparison is used the derived class 
must override the pure virtual greater than and equal to operators 
declared in the DatabaseKeyB base class. If the a single comparison
method is used the derived class must override the pure virtual 
DatabaseKeyB::CompareKey() function with return values of: -1 less 
than, 0 equal to, 1 greater than.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_KEY_HPP__
#define __GX_DATABASE_KEY_HPP__

#include "gxdlcode.h"

#include "keytypes.h"

class GXDLCODE_API DatabaseKeyB
{
public:
  DatabaseKeyB(void *kptr);
  virtual ~DatabaseKeyB();

protected:
  DatabaseKeyB(const DatabaseKeyB &ob) {
    db_key = ob.db_key;
    right_child = ob.right_child;
  }
  DatabaseKeyB& operator=(const DatabaseKeyB &ob) { 
    db_key = ob.db_key;
    right_child = ob.right_child;
    return *this; 
  }

public:
  size_t SizeOfDatabaseKey() { return KeySize() + sizeof(right_child); }
    
public: // Derived class interface
  virtual size_t KeySize() = 0;
#ifndef __USE_SINGLE_COMPARE__ // Use dual compare by default
  virtual int operator==(const DatabaseKeyB& key) const = 0;
  virtual int operator>(const DatabaseKeyB& key) const = 0;
#else // Use single compare 
  virtual int CompareKey(const DatabaseKeyB& key) const = 0;
#endif

public: // Persistent data members
  FAU right_child; // File pointer to the right child
  void *db_key;    // Memory Pointer to the database entry key
  // PC-lint 05/24/2002: The derived class must free memory
  // if heap space is used for the db_key pointer.
};

#endif // __GX_DATABASE_KEY_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
