// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: ostrbase.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995 
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

Output stream base class used by derived classes to output filtered
text to a specified ostream object.

Changes:
==============================================================
03/11/2002: Users now have the option to use the ANSI Standard 
C++ library by defining the __USE_ANSI_CPP__ preprocessor directive. 
By default the old iostream library will be used for all iostream 
operations.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_OUTPUT_STREAM_BASE_HPP__
#define __GX_OUTPUT_STREAM_BASE_HPP__

#include "gxdlcode.h"

// This directive must be defined to use the OutputStreamB class
#if defined (__USE_CPP_IOSTREAM__)

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <fstream>
#else // Use the old iostream library by default
#include <fstream.h>
#endif // __USE_ANSI_CPP__

// Constants
const int ostrDefaultPrecision = 2; // Default precision for floating points

// Enumeration used to define character filters when writing strings.
enum ostrFilterMode {
  ostrNO_FILTERING = 0, // Do not filter any characters
  ostrEAT_SPACES,       // Do not print any spaces
  ostrFILTER_EX_ASCII,  // Filter out the extended ASCII character set
  ostrHTLM_FILTER,      // Write text in an HTML format
  ostrLOWERCASE,        // Write all characters in lower case
  ostrUPPERCASE         // Write all characters in upper case
};

// Base class used to write text to an open stream
class GXDLCODE_API OutputStreamB
{
public:
  OutputStreamB();
  OutputStreamB(GXSTD::ostream &s); 
  virtual ~OutputStreamB();

protected: // Filtered output functions used to write text 
  virtual void WriteString(const char *s);
  virtual void WriteChar(const unsigned char c) const;

public: // Functions used to write built-in data types 
  void Write(const char c) const;
  void Write(const unsigned char c) const;
  void Write(char c);
  void Write(unsigned char c);
  void Write(const char *s);
  void Write(char *s);
  void Write(const unsigned char *s);
  void Write(unsigned char *s);
  void Write(const long val) const;
  void Write(long val);
  void Write(const unsigned long val) const;
  void Write(unsigned long val);
  void Write(const int val) const;
  void Write(int val);
  void Write(const unsigned int val) const;
  void Write(unsigned int val);
  void Write(double val); 
  void Write(const double val) const; 
  void Write(float val); 
  void Write(const float val) const; 

public: // Formatting functions 
  void precision(int p) { dec_precision = p; }   // Floating precision
  void FilterMode(ostrFilterMode m) { filter_mode = m; }
  void HTMLFilter(const unsigned char c) const;  // HTML text filter
  void eat_htm_space() { non_breaking_sp = 1; }  // Use non-breaking spaces
  void put_htm_space() { non_breaking_sp = 0; }  // Use breaking spaces
  
public: // Overloaded operators
  GXSTD::ostream &operator<<(GXSTD::ostream & (*_f)(GXSTD::ostream&));
  OutputStreamB &operator<<(char *s);
  OutputStreamB &operator<<(const char *s);
  OutputStreamB &operator<<(unsigned char *s);
  OutputStreamB &operator<<(const unsigned char *s);
  OutputStreamB &operator<<(char c);
  const OutputStreamB &operator<<(const char c) const;
  OutputStreamB &operator<<(unsigned char c);
  const OutputStreamB &operator<<(const unsigned char c) const;
  OutputStreamB &operator<<(long val);
  const OutputStreamB &operator<<(const long val) const;
  OutputStreamB &operator<<(unsigned long val);
  const OutputStreamB &operator<<(const unsigned long val) const;
  OutputStreamB &operator<<(int val);
  const OutputStreamB &operator<<(const int val) const;
  OutputStreamB &operator<<(unsigned int val);
  const OutputStreamB &operator<<(const unsigned int val) const;
  const OutputStreamB &operator<<(const float val) const;
  OutputStreamB &operator<<(float val);
  const OutputStreamB &operator<<(const double val) const;
  OutputStreamB &operator<<(double val);
  
protected:
  GXSTD::ostream *stream;     // ostream object set by the derived class
  int non_breaking_sp;        // True if using non-breaking spaces
  int dec_precision;          // Decimal point precision for floating points  
  ostrFilterMode filter_mode; // Set character filter mode
};

#endif // __USE_CPP_IOSTREAM__

#endif // __GX_OUTPUT_STREAM_BASE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
