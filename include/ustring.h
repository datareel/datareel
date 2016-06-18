// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: ustring.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/29/1996
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

The user defined string classes are used to construct and manipulate 
null-terminated, resizable ASCII and Unicode character strings. The 
UStringB class is an abstract base class used to define a general-purpose 
set of string functions that operate on both ASCII and Unicode strings. 
The UString class is derived from the UStringB class and sets the string 
type to ASCII. The UStringw class is derived from the UStringB class and 
sets the string type to Unicode. The UStringB class is responsible for 
calling the correct operation based on the string type set in the derived 
class. Using this hierarchy both ASCII and Unicode strings are available 
to the application at the same time and can be used transparently in mixed 
environments. NOTE: The integer type used for the wchar_t type will vary 
depending on the compiler used. The UStringB class automatically accounts 
for all compiler dependent byte alignments. However by nature, Unicode 
strings are not portable between Intel and RISC processors due to the 
difference in byte ordering. In order to account for Unicode strings 
stored in portable database files and shared across network connections 
the gxwchar_t type is supplied to account for different byte ordering. 

Changes:
==============================================================
03/10/2002: The UStringB class now includes a full compliment of 
<< and >> overloads used to insert and extract integers, floating 
points, strings, and characters types. 

03/10/2002: Added the hex, dec, reset, and clear UStringB class 
I/O manipulator functions used to format I/O within an I/O 
statement.

03/10/2002: Added 64-bit integer support to the UStringB class, 
which is enabled by defining the __64_BIT_DATABASE_ENGINE__ 
preprocessor directive.

03/11/2002: By default C++ iostreams are no longer used. In order 
to use the iostream functions built into this class users must 
define __USE_CPP_IOSTREAM__ preprocessor directive. When using C++ 
iostreams users have the option to use the ANSI Standard C++ library 
by defining the __USE_ANSI_CPP__ preprocessor directive. By default 
the old iostream library will be used when the  __USE_CPP_IOSTREAM__ 
directive is defined.

03/24/2002: Added the UStringB::Release() synchronization function. 
The release function is used to signal to the class destructor not to 
delete the string when an object is deleted or no longer visible.

04/07/2002: Added the UStringB::ConvertEscapeSeq() function used 
to convert hex escape sequences within a string to printable ASCII 
characters.

04/08/2002: Added the UStringB::GetSPtr() function used to return
a pointer the objects string pointer.

04/09/2002: Added the UStringB::SetLength(...) functions used to
reset or set the logical length an object's string.

04/09/2002: Added the UStringB::SetSPrt() function used to bind
an object to another string.

05/27/2002: Changed all char *s function arguments to const char *s 
in all functions. This change was made to make the conversion of 
string literals const safe

05/27/2002: Changed the return types from unsigned to int in all 
Find() functions. This change was made to eliminate signed/unsigned 
warnings and errors.

06/05/2002: Added Unicode versions of all UStringB functions
included with release 4.1.

06/05/2002: Moved all UString functions to the UStringB class
and added the UStringw class used to operate on Unicode strings.

09/04/2002: The ustring.h and ustring.cpp modules are now
dependent on the strutil.h include file.

10/10/2002: Changes the default argument for the UString::Clear()
function "clear_all" variable from 1 to 0. This will only null
the first character of the string eliminating the overhead of
clearing the entire memory buffer.

10/13/2002: Added additional copy constructors, assignment operators, 
and overloaded += operators to support the wxString class.

10/13/2002: Removed the following conversion function from the UStringB 
class due to excessive compiler warning under Linux: 
operator const int () const { return ((sptr != 0) && (s_length != 0)); }

10/13/2002: Removed the following conversion function to avoid having
UStringB objects being automatically converted to int types: 
operator int () { return ((sptr != 0) && (s_length != 0)); }

10/13/2002: Added the UStringB::is_not_null() function to replace the
int conversion function. NOTE: The UStringB class no longer has any
conversion functions.

12/02/2002: Added the UString::Left(), UString::Right(), and 
UString::Mid() substring functions.

12/02/2002: Added the UStringw::Left(), UStringw::Right(), and 
UStringw::Mid() substring functions.

05/15/2003: Added the UString::Atou() and UString::Atou64() functions
used to convert strings to unsigned integers.

06/22/2003: Added the UString::WholeNumber() functions used to 
insert comma separated whole numbers into a string.

07/03/2003: Added the FindMatch() function used to find matching 
string elements separated by a single "*" character placed at the 
beginning, middle, or end of a string.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_USER_DEFINED_STRING_HPP__
#define __GX_USER_DEFINED_STRING_HPP__

#include "gxdlcode.h"
#include "strutil.h"

#if defined (__USE_CPP_IOSTREAM__)
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#include <iomanip>
#elif !defined(__WINCE__) // Use the old iostream library by default
#include <iostream.h>
#include <iomanip.h>
#endif // __USE_ANSI_CPP__
#endif // __USE_CPP_IOSTREAM__

#if defined (__wxWIN201__) || (__wxWIN291__)
// 10/13/2002: Added support for the wxString class
#include "wx2incs.h"
#endif

#if defined (__wxWIN302__)
#include "wx3incs.h"
#endif

// Null string class used to reserve address space for null strings
class GXDLCODE_API UStringNULLPtr
{
public:
  UStringNULLPtr() { }
  ~UStringNULLPtr() { }

public: // Global null character and string pointers
  static char UStringNUllChar;
  static char UStringNUllStr[1];
  static wchar_t UStringwNUllChar;
  static wchar_t UStringwNUllStr[1];
};

// User defined string base class 
class GXDLCODE_API UStringB 
{ 
public:
  UStringB() { reset_all(); }
  virtual ~UStringB();

private: // Copying and assignment is only allowed in the derived class
  UStringB(const UStringB &s) { }
  void operator=(const UStringB &s) { }

public: // Derived class interface
  virtual int is_unicode() const = 0; // Return true for Unicode classes

public: // String initialization and reset functions
  int SetString(const char *s, unsigned nchars = 0);
  int SetString(const wchar_t *s, unsigned nchars = 0);
  int SetString(const UStringB &s, unsigned nchars = 0);
  void Clear(int clear_all = 0);

public: // Append, Insert, delete, and remove functions
  unsigned DeleteAt(unsigned c_position, unsigned nchars);
  unsigned Cat(const char *s, unsigned nchars = 0);
  unsigned Cat(const wchar_t *s, unsigned nchars = 0);
  unsigned Cat(const UStringB &s, unsigned nchars = 0);
  unsigned ReplaceAt(unsigned c_position, const char *s, unsigned nchars);
  unsigned ReplaceAt(unsigned c_position, const char *s) {
    return ReplaceAt(c_position, s, gxstrlen(s));
  }
  unsigned ReplaceAt(unsigned c_position, const wchar_t *s, unsigned nchars);
  unsigned ReplaceAt(unsigned c_position, const wchar_t *s) {
    return ReplaceAt(c_position, s, gxstrlen(s));
  }
  unsigned ReplaceAt(unsigned c_position, const UStringB &s, unsigned nchars);
  unsigned ReplaceAt(unsigned c_position, const UStringB &s) {
    return ReplaceAt(c_position, s, s.s_length);
  }
  unsigned InsertAt(unsigned c_position, const char *s, unsigned nchars);
  unsigned InsertAt(unsigned c_position, const char *s) {
    return InsertAt(c_position, s, gxstrlen(s));
  }
  unsigned InsertAt(unsigned c_position, const wchar_t *s, unsigned nchars);
  unsigned InsertAt(unsigned c_position, const wchar_t *s) {
    return InsertAt(c_position, s, gxstrlen(s));
  }
  unsigned InsertAt(unsigned c_position, const UStringB &s, unsigned nchars);
  unsigned InsertAt(unsigned c_position, const UStringB &s) {
    return InsertAt(c_position, s, s.s_length);
  }

public: // C String, pointer, and length functions
  char *c_str(char *sbuf);
  const char *c_str(char *sbuf) const;
  wchar_t *c_wstr(wchar_t *sbuf);
  const wchar_t *c_wstr(wchar_t *sbuf) const;
  unsigned length() { return s_length; }
  unsigned length() const { return s_length; }
  unsigned dlength() { return d_length; }
  unsigned dlength() const { return d_length; }
  int is_null() { return ((sptr == 0) || (s_length == 0)); }
  int is_null() const { return ((sptr == 0) || (s_length == 0)); }
  int is_not_null() { return ((sptr != 0) && (s_length != 0)); }
  int is_not_null() const { return ((sptr != 0) && (s_length != 0)); }
  int resize(unsigned nchars , int keep = 1);

public: // String search function
  int Find(const char *s, unsigned c_offset = 0) const; 
  int Find(const char *s, unsigned nchars, unsigned c_offset) const;
  int Find(const wchar_t *s, unsigned c_offset = 0) const; 
  int Find(const wchar_t *s, unsigned nchars, unsigned c_offset) const;
  int Find(const UStringB &s, unsigned c_offset = 0) const ;
  int Find(const UStringB &s, unsigned nchars, unsigned c_offset) const;
  int IFind(const char *s, unsigned c_offset = 0) const;
  int IFind(const char *s, unsigned nchars, unsigned c_offset) const;
  int IFind(const wchar_t *s, unsigned c_offset = 0) const; 
  int IFind(const wchar_t *s, unsigned nchars, unsigned c_offset) const;
  int IFind(const UStringB &s, unsigned c_offset = 0) const;
  int IFind(const UStringB &s, unsigned nchars, unsigned c_offset) const;

public: // Formatting and filtering functions
  int Atoi();
  long Atol();
  unsigned long Atou();
  double Atof();
  int ToUpper();
  int ToLower();
  int FindLast(const char *s) const;
  int FindLast(const wchar_t *s) const;
  int FindLast(const UStringB &s) const;
  unsigned ReplaceChar(const char c, const char replacement,
                       unsigned offset = 0);
  unsigned ReplaceChar(const wchar_t c, const wchar_t replacement,
                       unsigned offset = 0);
  unsigned ReplaceString(const char *s, const char *replacement,
                         unsigned offset = 0);
  unsigned ReplaceString(const wchar_t *s, const wchar_t *replacement,
                         unsigned offset = 0);
  unsigned ReplaceString(const UStringB &s, const UStringB &replacement,
                         unsigned offset = 0);
  unsigned FilterString(const char *s, unsigned offset = 0);
  unsigned FilterString(const wchar_t *s, unsigned offset = 0);
  unsigned FilterString(const UStringB &s, unsigned offset = 0);
  unsigned FilterChar(const char c, unsigned offset = 0);
  unsigned FilterChar(const wchar_t c, unsigned offset = 0);
  unsigned TrimLeadingSpaces();
  unsigned TrimTrailingSpaces();
  unsigned TrimLeading(const char c);
  unsigned TrimLeading(const wchar_t c);
  unsigned TrimTrailing(const char c);
  unsigned TrimTrailing(const wchar_t c);
  int DeleteAfter(const char *s);
  int DeleteAfter(const wchar_t *s);
  int DeleteAfter(const UStringB &s);
  int DeleteBefore(const char *s);
  int DeleteBefore(const wchar_t *s);
  int DeleteBefore(const UStringB &s);
  int DeleteAfterIncluding(const char *s);
  int DeleteAfterIncluding(const wchar_t *s);
  int DeleteAfterIncluding(const UStringB &s);
  int DeleteBeforeIncluding(const char *s);
  int DeleteBeforeIncluding(const wchar_t *s);
  int DeleteBeforeIncluding(const UStringB &s);
  int DeleteAfterLast(const char *s);
  int DeleteAfterLast(const wchar_t *s);
  int DeleteAfterLast(const UStringB &s);
  int DeleteAfterLastIncluding(const char *s);
  int DeleteAfterLastIncluding(const wchar_t *s);
  int DeleteAfterLastIncluding(const UStringB &s);
  int DeleteBeforeLast(const char *s);
  int DeleteBeforeLast(const wchar_t *s);
  int DeleteBeforeLast(const UStringB &s);
  int DeleteBeforeLastIncluding(const char *s);
  int DeleteBeforeLastIncluding(const wchar_t *s);
  int DeleteBeforeLastIncluding(const UStringB &s);
  int ConvertEscapeSeq(char esc, int num_octets = 2);
  int ConvertEscapeSeq(wchar_t esc, int num_octets = 2);

public: // Synchronization functions
  void Release() {
    // The release function is used to release this object's string  
    // without deleting the string or performing any other action. 
    // NOTE: The release function is used to release this sting when  
    // more then one object is referencing this string. After calling   
    // the release function the object previously bound to this string 
    // can safely be deleted without affecting any other objects   
    // bound to this string. This allows the string data to remain in  
    // memory until all threads accessing this list have exited or 
    // released it.
    sptr = 0; s_length = d_length = 0;
  }
  void *GetSPtr() { return sptr; }
  void *GetSPtr() const { return sptr; }
  unsigned SetLength();
  unsigned SetLength(unsigned nchars);
  void *SetSPrt(void *s, unsigned nchars, unsigned bytes = 0);
  void ResetGlobalNull();

public: // Comparision functions
  int StringCompare(const UStringB &s, int check_case = 1) const;
  int StringCompare(const char *s, int check_case = 1) const;
  int StringCompare(const wchar_t *s, int check_case = 1) const;
  int StringICompare(const UStringB &s) const { return StringCompare(s, 0); }
  int StringICompare(const char *s) const { return StringCompare(s, 0); }
  int StringICompare(const wchar_t *s) const { return StringCompare(s, 0); }
    
public: // Overloaded operators
  int operator!() { return ((sptr == 0) || (s_length == 0)); } 
  int operator!() const { return ((sptr == 0) || (s_length == 0)); }
  UStringB &operator+=(const UStringB &s) { Cat(s); return *this; }
  UStringB &operator+=(const char *s) { Cat(s); return *this; }
  UStringB &operator+=(const wchar_t *s) { Cat(s); return *this; }
  UStringB &operator+=(const char c);
  UStringB &operator+=(const wchar_t c);
  UStringB &operator<<(UStringB & (*_f)(UStringB&));
  UStringB &operator<<(const UStringB &s) { return *this += s; }
  UStringB &operator<<(const char *s) { return *this += s; }
  UStringB &operator<<(const wchar_t *s) { return *this += s; }
  UStringB &operator<<(const char c);
  UStringB &operator<<(const long val);
  UStringB &operator<<(const unsigned long val);
  UStringB &operator<<(const int val) { return *this << (long)val; }
  UStringB &operator<<(const unsigned int val) { 
    return *this << (unsigned long)val;
  }
  UStringB &operator<<(const short val) { return *this << (long)val; }
  UStringB &operator<<(const unsigned short val) {
    return *this << (unsigned long)val;
  }
  UStringB &operator<<(const float val) { return *this << (double)val; }
  UStringB &operator<<(const double val);
  UStringB &operator>>(char *s);
  UStringB &operator>>(char &c);
  UStringB &operator>>(long &val);
  UStringB &operator>>(unsigned long &val);
  UStringB &operator>>(int &val);
  UStringB &operator>>(unsigned int &val);
  UStringB &operator>>(short &val);
  UStringB &operator>>(unsigned short &val);
  UStringB &operator>>(float &val);
  UStringB &operator>>(double &val);

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
  __LLWORD__ Atoi64();
  __ULLWORD__ Atou64();
  UStringB &operator<<(const __LLWORD__ val);
  UStringB &operator<<(const __ULLWORD__ val);
  UStringB &operator>>(__LLWORD__ &val);
  UStringB &operator>>(__ULLWORD__ &val);
  void SHN(__ULLWORD__ bytes);
  void WholeNumber(__ULLWORD__ bytes);
#endif

public: // Functions used by UStringB I/O manipulators
  void Dec() { output_hex = (char)0; }
  void Hex() { output_hex = (char)1; }
  void Oct() { output_hex = (char)2; }
  void SHN() { output_hex = (char)3; }
  void WholeNumber() { output_hex = (char)4; }
  void Precision(int i = 1) { output_pre = (char)i; }
  void SetWidth(int i = 0) { output_width = (char )i; }
  void SetFill(char c = ' ') { output_fill = c; }
  void Reset() { 
    output_hex = 0; output_pre = 1; output_width = 0; output_fill = ' ';
  }
  void SHN(unsigned long bytes);
  void WholeNumber(unsigned long bytes);

protected: // Internal string functions
  int set_string(const void *s, unsigned bytes, gxStringType st, unsigned cs); 
  unsigned replace_at(unsigned position, const void *s, unsigned bytes,
		      gxStringType st, unsigned cs);
  unsigned insert_at(unsigned position, const void *s, unsigned bytes,
		     gxStringType st, unsigned cs);
  unsigned delete_at(unsigned position, unsigned bytes, gxStringType st, 
		     unsigned cs);
  int find_last(const void *s) const;
  void *allocate(unsigned &bytes, gxStringType st, unsigned cs, 
		 unsigned &nchars);
  void delete_sptr();
  void null_terminate(gxStringType st);
  int resize_buffer(unsigned bytes, int keep, gxStringType st, unsigned cs);
  void reset_all() {
    sptr = 0; d_length = s_length = 0; Reset();
  }

protected: // Data types common to all string types
  char output_hex;   // 0-decimal, 1-hex, 2-octal, 3-SHN, 4-Whole Number
  char output_pre;   // Floating point precision;
  char output_width; // Minimum string width
  char output_fill;  // Fill character
  unsigned d_length; // Total number of bytes allocated for this object
  unsigned s_length; // String length of this object's null-terminated string

protected:
  void *sptr; // Pointer to this object's null-terminated string

#if defined (__wxWIN201__)
// 10/13/2002: Added support for the wxString class
public:
  UStringB &operator+=(const wxString &s) { Cat(s.c_str()); return *this; }
  UStringB &operator<<(const wxString &s) { return *this += s.c_str(); }
  UStringB &operator>>(wxString &s);
#endif

#if defined (__wxWIN291__)
// 02/17/2011: Added support for the wxString class support for 2.9.1
public:
  UStringB &operator+=(const wxString &s) { Cat((const char *)s.c_str()); return *this; }
  UStringB &operator<<(const wxString &s) { return *this += (const char *)s.c_str(); }
  UStringB &operator>>(wxString &s);
#endif

#if defined (__wxWIN302__)
// 06/17/2016: Added support for the wxString class support for 3.0.2
public:
  UStringB &operator+=(const wxString &s) { Cat((const char *)s.c_str()); return *this; }
  UStringB &operator<<(const wxString &s) { return *this += (const char *)s.c_str(); }
  UStringB &operator>>(wxString &s);
#endif

};

// ASCII string class
class GXDLCODE_API UString : public UStringB
{
public:
  UString() { reset_all(); }
  UString(unsigned nchars);
  ~UString() { }
  UString(const UString &s) {
    reset_all(); SetString((char *)s.sptr, s.s_length);
  }
  UString(const char *s, unsigned nchars = 0) {
    reset_all(); SetString(s, nchars);
  }
  UString(const wchar_t *s, unsigned nchars = 0) {
    reset_all(); SetString(s, nchars);
  }
  UString &operator=(const UString &s);
  UString &operator=(const char *s);
  UString &operator=(const wchar_t *s);

public: // gxwchar_t functions
  UString(const gxwchar_t *s, unsigned nchars = 0);
  UString &operator=(const gxwchar_t *s);

public: // Base class interface
  int is_unicode() const;

public: // C String, pointer, and length functions
  char *c_str();
  const char *c_str() const;
  UString *strdup();
  UString *strdup() const;

public: // Substring functions
  UString(const UString &s, unsigned c_position, unsigned nchars);
  UString Mid(unsigned c_position, unsigned nchars) const; 
  UString Left(unsigned nchars) const;
  UString Right(unsigned nchars) const;

public: // Overloaded operators
  char &operator[](unsigned i);
  char &operator[](unsigned i) const;

public: // Friend functions requiring explicit operands 
  GXDLCODE_API friend int StringCompare(const UString &a, const UString &b);
  GXDLCODE_API friend int StringCompare(const char *s, const UString &b);
  GXDLCODE_API friend int StringCompare(const UString &a, const char *s);
  GXDLCODE_API friend int StringCompare(const wchar_t *s, const UString &b);
  GXDLCODE_API friend int StringCompare(const UString &a, const wchar_t *s);
  GXDLCODE_API friend UString operator+(const UString &a, const UString &b);
  GXDLCODE_API friend UString operator+(const char *s, const UString &b);
  GXDLCODE_API friend UString operator+(const UString &a, const char *s);
  GXDLCODE_API friend UString operator+(const wchar_t *s, const UString &b);
  GXDLCODE_API friend UString operator+(const UString &a, const wchar_t *s);
  GXDLCODE_API friend int operator==(const UString &a, const UString &b);
  GXDLCODE_API friend int operator==(const char *a, const UString &b);
  GXDLCODE_API friend int operator==(const UString &a, const char *b);
  GXDLCODE_API friend int operator==(const wchar_t *a, const UString &b);
  GXDLCODE_API friend int operator==(const UString &a, const wchar_t *b);
  GXDLCODE_API friend int operator!=(const UString &a, const UString &b);
  GXDLCODE_API friend int operator!=(const char *a, const UString &b);
  GXDLCODE_API friend int operator!=(const UString &a, const char *b);
  GXDLCODE_API friend int operator!=(const wchar_t *a, const UString &b);
  GXDLCODE_API friend int operator!=(const UString &a, const wchar_t *b);
  GXDLCODE_API friend int operator>(const UString &a, const UString &b);
  GXDLCODE_API friend int operator>(const char *a , const UString &b);
  GXDLCODE_API friend int operator>(const UString &a, const char *b);
  GXDLCODE_API friend int operator>(const wchar_t *a , const UString &b);
  GXDLCODE_API friend int operator>(const UString &a, const wchar_t *b);
  GXDLCODE_API friend int operator>=(const UString &a, const UString &b);
  GXDLCODE_API friend int operator>=(const char *a, const UString &b);
  GXDLCODE_API friend int operator>=(const UString &a, const char *b);
  GXDLCODE_API friend int operator>=(const wchar_t *a, const UString &b);
  GXDLCODE_API friend int operator>=(const UString &a, const wchar_t *b);
  GXDLCODE_API friend int operator<(const UString &a, const UString &b);
  GXDLCODE_API friend int operator<(const char *a, const UString &b);
  GXDLCODE_API friend int operator<(const UString &a, const char *b);
  GXDLCODE_API friend int operator<(const wchar_t *a, const UString &b);
  GXDLCODE_API friend int operator<(const UString &a, const wchar_t *b);
  GXDLCODE_API friend int operator<=(const UString &a, const UString &b);
  GXDLCODE_API friend int operator<=(const char *a, const UString &b);
  GXDLCODE_API friend int operator<=(const UString &a, const char *b);
  GXDLCODE_API friend int operator<=(const wchar_t *a, const UString &b);
  GXDLCODE_API friend int operator<=(const UString &a, const wchar_t *b);

#if defined (__wxWIN201__)
// 10/13/2002: Added support for the wxString class
public:
  UString(const wxString &s) {
    reset_all(); SetString(s.c_str(), s.length());
  }
  UString &operator=(const wxString &s);
#endif
#if defined (__wxWIN291__)
// 10/13/2002: Added support for the wxString class
public:
  UString(const wxString &s) {
    reset_all(); SetString((const char *)s.c_str(), s.length());
  }
  UString &operator=(const wxString &s);
#endif

#if defined (__wxWIN302__)
// 06/17/2016: Added support for the wxString 3.0.2 
public:
  UString(const wxString &s) {
    reset_all(); SetString((const char *)s.c_str(), s.length());
  }
  UString &operator=(const wxString &s);
#endif

};

// Unicode String class
class GXDLCODE_API UStringw : public UStringB
{
public:
  UStringw() { reset_all(); }
  UStringw(unsigned nchars);
  ~UStringw() { }
  UStringw(const UStringw &s) {
    reset_all(); SetString((wchar_t *)s.sptr, s.s_length); 
  }
  UStringw(const char *s, unsigned nchars = 0) {
    reset_all(); SetString(s, nchars);
  }
  UStringw(const wchar_t *s, unsigned nchars = 0) {
    reset_all(); SetString(s, nchars);
  }
  UStringw &operator=(const UStringw &s);
  UStringw &operator=(const char *s);
  UStringw &operator=(const wchar_t *s);

public: // gxwchar_t functions
  UStringw(const gxwchar_t *s, unsigned nchars = 0);
  UStringw &operator=(const gxwchar_t *s);

public: // Base class interface
  int is_unicode() const;

public: // C String, pointer, and length functions
  wchar_t *c_wstr();
  const wchar_t *c_wstr() const;
  UStringw *strdup();
  UStringw *strdup() const;

public: // Substring functions
  UStringw(const UStringw &s, unsigned c_position, unsigned nchars);
  UStringw Mid(unsigned c_position, unsigned nchars) const; 
  UStringw Left(unsigned nchars) const;
  UStringw Right(unsigned nchars) const;

public: // Overloaded operators
  wchar_t &operator[](unsigned i);
  wchar_t &operator[](unsigned i) const;

public: // Friend functions requiring explicit operands 
  GXDLCODE_API friend int StringCompare(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend int StringCompare(const char *s, const UStringw &b);
  GXDLCODE_API friend int StringCompare(const UStringw &a, const char *s);
  GXDLCODE_API friend int StringCompare(const wchar_t *s, const UStringw &b);
  GXDLCODE_API friend int StringCompare(const UStringw &a, const wchar_t *s);
  GXDLCODE_API friend UStringw operator+(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend UStringw operator+(const char *s, const UStringw &b);
  GXDLCODE_API friend UStringw operator+(const UStringw &a, const char *s);
  GXDLCODE_API friend UStringw operator+(const wchar_t *s, const UStringw &b);
  GXDLCODE_API friend UStringw operator+(const UStringw &a, const wchar_t *s);
  GXDLCODE_API friend int operator==(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend int operator==(const char *a, const UStringw &b);
  GXDLCODE_API friend int operator==(const UStringw &a, const char *b);
  GXDLCODE_API friend int operator==(const wchar_t *a, const UStringw &b);
  GXDLCODE_API friend int operator==(const UStringw &a, const wchar_t *b);
  GXDLCODE_API friend int operator!=(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend int operator!=(const char *a, const UStringw &b);
  GXDLCODE_API friend int operator!=(const UStringw &a, const char *b);
  GXDLCODE_API friend int operator!=(const wchar_t *a, const UStringw &b);
  GXDLCODE_API friend int operator!=(const UStringw &a, const wchar_t *b);
  GXDLCODE_API friend int operator>(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend int operator>(const char *a , const UStringw &b);
  GXDLCODE_API friend int operator>(const UStringw &a, const char *b);
  GXDLCODE_API friend int operator>(const wchar_t *a , const UStringw &b);
  GXDLCODE_API friend int operator>(const UStringw &a, const wchar_t *b);
  GXDLCODE_API friend int operator>=(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend int operator>=(const char *a, const UStringw &b);
  GXDLCODE_API friend int operator>=(const UStringw &a, const char *b);
  GXDLCODE_API friend int operator>=(const wchar_t *a, const UStringw &b);
  GXDLCODE_API friend int operator>=(const UStringw &a, const wchar_t *b);
  GXDLCODE_API friend int operator<(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend int operator<(const char *a, const UStringw &b);
  GXDLCODE_API friend int operator<(const UStringw &a, const char *b);
  GXDLCODE_API friend int operator<(const wchar_t *a, const UStringw &b);
  GXDLCODE_API friend int operator<(const UStringw &a, const wchar_t *b);
  GXDLCODE_API friend int operator<=(const UStringw &a, const UStringw &b);
  GXDLCODE_API friend int operator<=(const char *a, const UStringw &b);
  GXDLCODE_API friend int operator<=(const UStringw &a, const char *b);
  GXDLCODE_API friend int operator<=(const wchar_t *a, const UStringw &b);
  GXDLCODE_API friend int operator<=(const UStringw &a, const wchar_t *b);

#if defined (__wxWIN201__)
// 10/13/2002: Added support for the wxString class
public:
  UStringw(const wxString &s) {
    reset_all(); SetString(s.c_str(), s.length());
  }
  UStringw &operator=(const wxString &s);
#endif
#if defined (__wxWIN291__)
// 02/17/2011: Added support for the wxString 2.9.1
public:
  UStringw(const wxString &s) {
    reset_all(); SetString((const char *)s.c_str(), s.length());
  }
  UStringw &operator=(const wxString &s);
#endif

#if defined (__wxWIN302__)
// 06/17/2016: Added support for the wxString 3.0.2
public:
  UStringw(const wxString &s) {
    reset_all(); SetString((const char *)s.c_str(), s.length());
  }
  UStringw &operator=(const wxString &s);
#endif

};

// UStringB class I/O manipulator functions used to format I/O
// within an I/O statement.
inline UStringB& bin(UStringB &s) { return s; }
inline UStringB& text(UStringB &s) { return s; }
inline UStringB& hex(UStringB &s) { s.Hex(); return s; }
inline UStringB& dec(UStringB &s) { s.Dec(); return s; }
inline UStringB& oct(UStringB &s) { s.Oct(); return s; }
inline UStringB& shn(UStringB &s) { s.SHN(); return s; }
inline UStringB& wn(UStringB &s) { s.WholeNumber(); return s; }
inline UStringB& reset(UStringB &s) { s.Reset(); return s; }
inline UStringB& flush(UStringB &s) { return s; }
inline UStringB& clear(UStringB &s) { s.Clear(); return s; }
inline UStringB& resetw(UStringB &s) { s.SetWidth(0); return s; }
inline UStringB& setw1(UStringB &s) { s.SetWidth(1); return s; }
inline UStringB& setw2(UStringB &s) { s.SetWidth(2); return s; }
inline UStringB& setw3(UStringB &s) { s.SetWidth(3); return s; }
inline UStringB& setw4(UStringB &s) { s.SetWidth(4); return s; }
inline UStringB& setw5(UStringB &s) { s.SetWidth(5); return s; }
inline UStringB& setw6(UStringB &s) { s.SetWidth(6); return s; }
inline UStringB& setw7(UStringB &s) { s.SetWidth(7); return s; }
inline UStringB& setw8(UStringB &s) { s.SetWidth(8); return s; }
inline UStringB& setw9(UStringB &s) { s.SetWidth(9); return s; }
inline UStringB& setw10(UStringB &s) { s.SetWidth(10); return s; }
inline UStringB& resetp(UStringB &s) { s.Precision(1); return s; }
inline UStringB& setp1(UStringB &s) { s.Precision(1); return s; }
inline UStringB& setp2(UStringB &s) { s.Precision(2); return s; }
inline UStringB& setp3(UStringB &s) { s.Precision(3); return s; }
inline UStringB& setp4(UStringB &s) { s.Precision(4); return s; }
inline UStringB& setp5(UStringB &s) { s.Precision(5); return s; }
inline UStringB& setp6(UStringB &s) { s.Precision(6); return s; }
inline UStringB& setp7(UStringB &s) { s.Precision(7); return s; }
inline UStringB& setp8(UStringB &s) { s.Precision(8); return s; }
inline UStringB& setp9(UStringB &s) { s.Precision(9); return s; }
inline UStringB& setp10(UStringB &s) { s.Precision(10); return s; }

// General-purpose string routines
GXDLCODE_API int CaseICmp(const UString &s1, const UString &s2);
GXDLCODE_API int CaseICmp(const UString &s1, const char *s);
GXDLCODE_API int CaseICmp(const UString &s1, const wchar_t *s);
GXDLCODE_API int CaseICmp(const char *s, const UString &s2);
GXDLCODE_API int CaseICmp(const wchar_t *s, const UString &s2);
GXDLCODE_API int CaseICmp(const UStringw &s1, const UStringw &s2);
GXDLCODE_API int CaseICmp(const UStringw &s1, const char *s);
GXDLCODE_API int CaseICmp(const UStringw &s1, const wchar_t *s);
GXDLCODE_API int CaseICmp(const char *s, const UStringw &s2);
GXDLCODE_API int CaseICmp(const wchar_t *s, const UStringw &s2);
GXDLCODE_API int FindMatch(const UString &str, const UString &s, 
			   int compare_case = 1);
GXDLCODE_API int FindMatch(const char *str, const char *s, 
			   int compare_case = 1);


// General-purpose string parsers
GXDLCODE_API UString *ParseStrings(const UString &input_str, const UString &delimiter, unsigned &num_arr, 
				   int trim_spaces = 0, int trim_quotes = 0);
GXDLCODE_API UStringw *ParseStrings(const UStringw &input_str, const UStringw &delimiter, unsigned &num_arr, 
				    int trim_spaces = 0, int trim_quotes = 0);


#if defined (__USE_CPP_IOSTREAM__)
GXDLCODE_API GXSTD::ostream &operator<<(GXSTD::ostream &os, const UString &s);
GXDLCODE_API GXSTD::ostream &operator<<(GXSTD::ostream &os, const UStringw &s);
GXDLCODE_API GXSTD::istream &operator>>(GXSTD::istream &os, UString &s);
GXDLCODE_API GXSTD::istream &operator>>(GXSTD::istream &os, UStringw &s);
#endif // __USE_CPP_IOSTREAM__

#endif  // __GX_USER_DEFINED_STRING_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

