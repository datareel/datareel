// ------------------------------- //
// -------- start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: ustring.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/29/1996
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
07/10/2001: Added __AIX__ conditional directive for the IBM xlC 
C and C++ Compilers. This compiler does not define the 
strcasecmp() function.

09/12/2001: Modified the UStringB::SetString() function to 
ensure that the "s" pointer is initialized before calling 
the gxstrlen() function.

09/12/2001: Modified the UStringB::InsertAt() function to check 
the byte size to ensure that caller specified a number of bytes 
greater then zero.

11/15/2001: Modified the overloaded + const chat *s operator
to append the null terminated "s" string to the end of the
UStringB object.

02/27/2002: The logical length in the UString(unsigned nchars) 
constructor is now set to zero when this constructor is called.

06/05/2002: Added Unicode versions of all UStringB functions
included with release 4.1.

06/05/2002: Moved all UString functions to the UStringB class
and added the UStringw and gxwchar_t class used to operate on
Unicode strings.

09/04/2002: All versions of the CaseICmp() functions are no 
longer using compiler dependent string case insensitive 
functions.

09/10/2002: Added global null pointers used internally to 
return null character and null strings.

09/19/2002: Modified the UString::UString(unsigned nchars) and
UStringw::UStringw(unsigned nchars) constructors not to set the
logical length of the string object. The logical length will be
set when a string is assigned or copied into the string object.

10/13/2002: Added additional copy constructors, assignment operators, 
and overloaded += operators to support the wxString class.

06/04/2003: Modified all versions of the UStringB::SHN() function to
pad the output stream if the output width is greater then 1.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "ustring.h"
#include "strutil.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

// Initialize the global null character and string pointers
char UStringNULLPtr::UStringNUllChar = '\0';
char UStringNULLPtr::UStringNUllStr[1] = { '\0' };
wchar_t UStringNULLPtr::UStringwNUllChar = '\0';
wchar_t UStringNULLPtr::UStringwNUllStr[1] = { '\0' };

UStringB::~UStringB()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  delete_sptr();
}

int UStringB::set_string(const void *s, unsigned bytes, 
			 gxStringType st, unsigned cs)
// Set the string value for this object. This function will try to
// re-use the current memory segment allocated for this string before
// re-allocating memory for the string. Returns true if successful or
// false if an error occurs. NOTE: The UStringB class guarantees that
// each object is unique by storing a unique copy of the string with
// each object. This ensures that UStringB objects can be safely copy
// constructed, assigned, resized, and deleted by multiple threads.
// Multiple threads accessing shared memory segments must be handled
// by the application. NOTE: The number of bytes must be calculated
// according to the strings bit alignment, which will vary based on the
// string type set in the derived class. ASCII string types will have a
// byte size of 1 multiplied by the number non-null characters in the 
// string. Unicode string types will have a byte size of the native
// Unicode character size  multiplied by the number non-null characters 
// in the string. The number of bytes passed to this function should not 
// include a null terminator, which will be accounted for and allocated 
// by this function.
{
  // Calculate the length of this string if no bytes size is specified
  if(bytes == 0) {
    if(s) { 
      // Ensure that the "s" pointer is initialized before
      // calling the string length function.
      if(st == gxASCII_STRING) {
	bytes = (cs * gxstrlen((char *)s));
      }
      else {
	bytes = (cs * gxstrlen((wchar_t *)s));
      }
    }
    else {
      return 0; // A null pointer was passed to this function
    }
  }

  // Calculate the number of characters and adjust the byte size
  unsigned nchars = bytes/cs;
  if(bytes % cs) s_length++;
  bytes = nchars * cs;

  if(sptr) { // Memory was previously allocated for this object
    if(d_length >= bytes) { // Try to reuse this space
      s_length = nchars;
      memmove(sptr, s, bytes);
      null_terminate(st); // Null terminate the string
      return 1;
    }
    else { // Must resize the string
      delete_sptr(); // Delete the previous copy and re-allocate memory
    }
  }
  
  // Allocate memory for the string plus a null-terminator
  if(bytes < cs) bytes = 0; // Assume this is a null string

  void *nsptr = allocate(bytes, st, cs, nchars);
  if(!nsptr) return 0; // Allocation failed 
  memmove(nsptr, s, bytes); // Create a unique copy of this string 
  sptr = nsptr;
  d_length = bytes;
  s_length = nchars;
  null_terminate(st); // Null terminate the string
  return 1;
}

unsigned UStringB::ReplaceAt(unsigned c_position, const char *s, 
			     unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(!is_unicode()) { // String type set by the derived class
    return replace_at(c_position, s, nchars, gxASCII_STRING, 1);
  }
  else { // This is a Unicode string so convert the ASCII string and replace 
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    unsigned cs = sizeof(wchar_t);
    unsigned bytes = replace_at((c_position*cs), nsptr, (nchars*cs), 
				gxUNICODE_STRING, cs);
    delete[] nsptr;
    return bytes;
  }
}

unsigned UStringB::ReplaceAt(unsigned c_position, const wchar_t *s, 
			     unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(!is_unicode()) {
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    unsigned bytes = replace_at(c_position, nsptr, nchars, gxASCII_STRING, 1);
    delete[] nsptr;
    return bytes;
  }
  else {
    unsigned cs = sizeof(wchar_t);
    return replace_at((c_position*cs), s, (nchars*cs), gxUNICODE_STRING, cs);
  }
}

unsigned UStringB::ReplaceAt(unsigned c_position, const UStringB &s, 
			     unsigned nchars)
{
  if(!s.sptr) return 0; // Check for null pointers
  if((nchars == 0) || (nchars > s.s_length)) nchars = s.s_length;
  if(!is_unicode()) {
    if(!s.is_unicode()) {
      return replace_at(c_position, s.sptr, nchars, gxASCII_STRING, 1);
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, nchars);
      if(!nsptr) return 0;
      unsigned rv =  replace_at(c_position, nsptr, nchars, gxASCII_STRING, 1);
      delete[] nsptr;
      return rv;
    }
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, nchars);
      if(!nsptr) return 0;
      unsigned rv = replace_at((c_position*cs), nsptr, (nchars*cs), 
				gxUNICODE_STRING, cs);
      delete[] nsptr;
      return rv;
    }
    else {
      return replace_at((c_position*cs), s.sptr, (nchars*cs), 
			gxUNICODE_STRING, cs);
    }
  }
}

unsigned UStringB::InsertAt(unsigned c_position, const char *s, 
			    unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(!is_unicode()) { // String type set by the derived class
    return insert_at(c_position, s, nchars, gxASCII_STRING, 1);
  }
  else { // This is a Unicode string so convert the ASCII string and replace 
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    unsigned cs = sizeof(wchar_t);
    unsigned bytes = insert_at((c_position*cs), nsptr, (nchars*cs), 
			       gxUNICODE_STRING, cs);
    delete[] nsptr;
    return bytes;
  }
}

unsigned UStringB::InsertAt(unsigned c_position, const wchar_t *s, 
			    unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(!is_unicode()) {
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    unsigned bytes = insert_at(c_position, nsptr, nchars, gxASCII_STRING, 1);
    delete[] nsptr;
    return bytes;
  }
  else {
    unsigned cs = sizeof(wchar_t);
    return insert_at((c_position*cs), s, (nchars*cs), gxUNICODE_STRING, cs);
  }
}

unsigned UStringB::InsertAt(unsigned c_position, const UStringB &s, 
			    unsigned nchars)
{
  if(!s.sptr) return 0; // Check for null pointers and strings
  if((nchars == 0) || (nchars > s.s_length)) nchars = s.s_length;
  if(!is_unicode()) {
    if(!s.is_unicode()) {
      return insert_at(c_position, s.sptr, nchars, gxASCII_STRING, 1);
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, nchars);
      if(!nsptr) return 0;
      unsigned rv = insert_at(c_position, nsptr, nchars, gxASCII_STRING, 1);

      delete[] nsptr;
      return rv;
    }
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, nchars);
      if(!nsptr) return 0;
      unsigned rv = insert_at((c_position*cs), nsptr, (nchars*cs), 
			      gxUNICODE_STRING, cs);
      delete[] nsptr;
      return rv;
    }
    else {
      return insert_at((c_position*cs), s.sptr, (nchars*cs), 
		       gxUNICODE_STRING, cs);
    }
  }
}

int UStringB::SetString(const UStringB &s, unsigned nchars)
{
  if(!s.sptr) return 0; // Check for null pointers
  if((nchars == 0) || (nchars > s.s_length)) nchars = s.s_length;
  if(!is_unicode()) {
    if(!s.is_unicode()) {
      return set_string(s.sptr, nchars, gxASCII_STRING, 1);
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, nchars);
      if(!nsptr) return 0;
      int rv = set_string(nsptr, nchars, gxASCII_STRING, 1);
      delete[] nsptr;
      return rv;
    }
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, nchars);
      if(!nsptr) return 0;
      int rv = set_string(nsptr, (nchars*cs), gxUNICODE_STRING, cs);
      delete[] nsptr;
      return rv;
    }
    else {
      return set_string(s.sptr, (nchars*cs), gxUNICODE_STRING, cs);
    }
  }
}

int UStringB::SetString(const char *s, unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) {
    return set_string(s, nchars, gxASCII_STRING, 1);
  }
  else { // Convert this ASCII string literal to a Unicode string
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    unsigned cs = sizeof(wchar_t);
    int rv = set_string(nsptr, (nchars*cs), gxUNICODE_STRING, cs);
    delete[] nsptr;
    return rv;
  }
}

int UStringB::SetString(const wchar_t *s, unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) { // Convert this Unicode string literal to ASCII
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    int rv = set_string(nsptr, nchars, gxASCII_STRING, 1);
    delete[] nsptr;
    return rv;
  }
  else { 
    unsigned cs = sizeof(wchar_t);
    return set_string(s, (nchars*cs), gxUNICODE_STRING, sizeof(wchar_t));
  }
}

unsigned UStringB::Cat(const char *s, unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) {
    return insert_at(s_length, s, nchars, gxASCII_STRING, 1);
  }
  else {
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    unsigned cs = sizeof(wchar_t);
    unsigned bytes = insert_at((s_length*cs), nsptr, (nchars*cs), 
			       gxUNICODE_STRING, cs);
    delete[] nsptr;
    return bytes;
    
  }
}

unsigned UStringB::Cat(const wchar_t *s, unsigned nchars)
{
  if(!s) return 0; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) {
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    unsigned bytes = insert_at(s_length, nsptr, nchars, gxASCII_STRING, 1);
    delete[] nsptr;
    return bytes;
  }
  else {
    unsigned cs = sizeof(wchar_t);
    return insert_at((s_length*cs), s, (nchars*cs), gxUNICODE_STRING, cs);
  }
}

unsigned UStringB::Cat(const UStringB &s, unsigned nchars)
{
  if(!s.sptr) return 0; // Check for null pointers
  if((nchars == 0) || (nchars > s.s_length)) nchars = s.s_length;
  if(!is_unicode()) {
    if(!s.is_unicode()) {
      return insert_at(s_length, s.sptr, nchars, gxASCII_STRING, 1);
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, nchars);
      if(!nsptr) return 0;
      unsigned rv = insert_at(s_length, nsptr, nchars, gxASCII_STRING, 1);
      delete[] nsptr;
      return rv;
    }
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, nchars);
      if(!nsptr) return 0;
      unsigned rv = insert_at((s_length*cs), nsptr, (nchars*cs), 
			      gxUNICODE_STRING, cs);
      delete[] nsptr;
      return rv;
    }
    else {
      return insert_at((s_length*cs), s.sptr, (nchars*cs), 
		       gxUNICODE_STRING, cs);
    }
  }
}

void UStringB::Clear(int clear_all)
// Clear this string without freeing its memory segment.
{
  Reset(); // Reset all the format specifiers.
  s_length = 0; // Reset the string length
  if(!sptr) return;

  if(clear_all) { // Zero out the memory for the entire string
    memset(sptr, 0, d_length); 
  }
  else { // Null the first character and return
    if(!is_unicode()) { // Null the string
      ((char *)sptr)[s_length] = 0;
    }
    else {
      ((wchar_t *)sptr)[s_length] = (wchar_t)0;
    }    
  }
}

void UStringB::delete_sptr()
// Internal function used to free heap space memory  
// previously allocated for this string.
{
  // Cast sptr to unsigned char * type so that the delete 
  // operator will reference a type other than void *.
  if(sptr) delete (unsigned char *)sptr;

  // Null the pointer and reset the length variables
  sptr = 0;
  s_length = d_length = 0;
}

void *UStringB::allocate(unsigned &bytes, gxStringType st, unsigned cs,
			 unsigned &nchars) 
// Internal function used to allocate heap space memory for a
// specified number of bytes. Returns null if memory cannot
// be allocated for the buffer. NOTE: The calling function must
// adjust the string's dimensioned length if the byte size is
// changed during allocation. The number of characters used
// by this string will be passed back in the "nchars" variable.
// This version of the allocate function will account for a
// single null terminator in addition to the number of bytes
// specified.
{
  // Not allocating memory for any buffer characters
  if(bytes < cs) bytes = 0; // Assume this is a null string

  // Calculate the buffer size and adjust the byte size
  nchars = bytes/cs;
  if(bytes % cs) nchars++; // Allocate extra character in place of remainder 
  bytes = nchars * cs;

  if(st == gxASCII_STRING) {
    // Allocate memory for the string plus a null terminator
    char *aptr = new char[nchars+1];
    if(!aptr) return (void *)0;  // Allocation failed
    memset(aptr, 0, (bytes+cs)); // Zero out the memory
    return (void *)aptr;
  }
  else { // This is a Unicode string type
    // Allocate memory for the string plus a null terminator
    wchar_t *uptr = new wchar_t[nchars+1];
    if(!uptr) return (void *)0;  // Allocation failed
    memset(uptr, 0, (bytes+cs)); // Zero out the memory
    return (void *)uptr;
  }
}

void UStringB::null_terminate(gxStringType st) 
// Internal function used to null terminate a ASCII or Unicode string.
{
  // PC-lint: 09/14/2005 Possible use of null pointer
  if(!sptr) return;

  if(st == gxASCII_STRING) {
    ((char *)sptr)[s_length] = 0;
  }
  else {
    ((wchar_t *)sptr)[s_length] = 0;
  }
}

unsigned UStringB::insert_at(unsigned position, const void *s, unsigned bytes,
			     gxStringType st, unsigned cs)
// Insert a specified number of bytes at the specified byte position, keeping
// the current string intact. Returns the number of bytes inserted or
// zero if an error occurs.
{
  // Check the byte size to ensure that caller
  // specified a number of bytes greater then zero
  // or at least one character size.
  if(bytes < cs) return 0;

  // PC-lint: 09/14/2005 Possible use of null pointer
  if(!s) return 0;

  // Record the current string length in bytes
  unsigned old_length = s_length * cs; 

  // Ensure that there are enough bytes to hold the insertion
  if(!resize_buffer((bytes+(s_length*cs)), 1, st, cs)) return 0;

  // PC-lint: 09/14/2005 Possible use of null pointer
  if(!sptr) return 0;

  if(position < old_length) { // Move the data in the middle of the string

    // Cast sptr to unsigned char * type so that the pointer will be
    // incremented one byte at a time.
    memmove((unsigned char *)sptr+position+bytes, 
	    (unsigned char *)sptr+position, old_length-position);
  }
  
  if(position > (s_length * cs)) position = s_length; // Stay in bounds

  // Cast sptr to unsigned char * type so that the pointer will be
  // incremented one byte at a time.
  memmove((unsigned char *)sptr+position, s, bytes);

  null_terminate(st); // Ensure null termination
  return bytes;
}

unsigned UStringB::replace_at(unsigned position, const void *s, unsigned bytes,
			      gxStringType st, unsigned cs)
// Replace a specified number of bytes at the specified byte position. Returns
// the number of bytes replaced of zero if an error occurs.
{
  // Check the byte size to ensure that caller
  // specified a number of bytes greater then zero
  // or at least one character size.
  if(bytes < cs) return 0;

  // Calculate the number characters being replaced
  unsigned nchars = bytes/cs;
  if(bytes % cs) nchars++;

  // PC-lint 09/15/2005: nchars not accessed, so following line was added for
  // unicode characters. 
  bytes = nchars * cs; // Adjust the byte size

  if(position > (s_length*cs)) position = (s_length*cs); // Stay in bounds
  unsigned end = ((s_length*cs)-position);
  if(bytes > end) { // There are not enough bytes to hold the replacement
    unsigned needed = (bytes-end)/cs;
    if((bytes-end) % cs) needed++;
    if(!resize(s_length + needed)) return 0;
  }

  // PC-lint: 09/14/2005 Possible use of null pointer
  if(!sptr) return 0;

  // Cast sptr to unsigned char * type so that the pointer will be
  // incremented one byte at a time.
  memmove((unsigned char *)sptr+position, s , bytes);

  null_terminate(st); // Ensure null termination
  return bytes;
}

int UStringB::resize(unsigned nchars, int keep)
// Resize the logical length of the buffer. If the
// "keep" variable is true the old data will be
// copied into the new space. By default the old
// data will not be deleted. Returns true if
// successful or false if an error occurs.
{
  if(!is_unicode()) {
    return resize_buffer(nchars, keep, gxASCII_STRING, 1);
  }
  else {
    return resize_buffer((nchars*sizeof(wchar_t)),
			 keep, gxUNICODE_STRING, sizeof(wchar_t));
  }
}

int UStringB::resize_buffer(unsigned bytes, int keep, 
			    gxStringType st, unsigned cs)
// Resize the dimensioned length of the buffer. If the
// "keep" variable is true the old data will be
// copied into the new space. Returns true if
// successful or false if an error occurs.
{
  if(bytes < cs) bytes = cs;

  // Calculate the number of characters needed
  unsigned nchars = bytes/cs; 
  if(bytes % cs) s_length++;
  bytes = nchars * cs; // Adjust the byte size

  // No additional memory has to be re-allocated
  if(d_length >= bytes) {
    s_length = nchars;
    if(!keep) { // Remove the old data
      // PC-lint: 09/14/2005 Possible use of null pointer
      if(!sptr) return 0;
      memset(sptr, 0, (d_length+cs)); 
    }
    else { // Keep the old data and resize the string
      null_terminate(st);
    }
    return 1;
  }
  
  void *nsptr = allocate(bytes, st, cs, nchars);
  if(!nsptr) return 0; // Allocation failed 

  if((keep == 1) && (sptr != 0)) { // Copy old data into the new memory segment
    if(nchars < s_length) s_length = nchars;
    memmove(nsptr, sptr, (s_length*cs));
    null_terminate(st); // Null terminate the string
  }

  delete_sptr();     // Free the previously allocated memory
  sptr = nsptr;      // Point to new string
  s_length = nchars; // Record the new character length
  d_length = bytes;  // Record new allocated length
  return 1;
}

unsigned UStringB::DeleteAt(unsigned c_position, unsigned nchars) 
// Deletes a specified number of characters starting at the
// specified character position. Returns the total number 
// of characters deleted or zero if an error occurs.
{
  if(!sptr) return 0; // Check for null pointers
  if(!is_unicode()) {
    return delete_at(c_position, nchars, gxASCII_STRING, 1);
  }
  else {
    unsigned cs =  sizeof(wchar_t);
    return delete_at((c_position*cs), (nchars*cs), gxUNICODE_STRING, cs);
  }
}

unsigned UStringB::delete_at(unsigned position, unsigned bytes, 
			     gxStringType st, unsigned cs) 
// Deletes a specified number of characters starting at the
// specified byte position in the string. Returns the total 
// number of bytes deleted or zero if an error occurs.
{
  if(bytes < cs) return 0; // Not deleting any characters from the string
  // Calculate the character position and the number of characters
  unsigned c_position = 0;
  unsigned nchars = 0;
  if(st == gxASCII_STRING) {
    c_position = position;
    nchars = bytes;
  }
  else {
    if(position >= cs) { // Delete starting at the second char position 
      c_position = position/cs;
      if(position % cs) c_position++;
    }
    nchars = bytes/cs;
    if(bytes % cs) bytes++;
  }
  
  unsigned end;
  if(c_position < s_length) {
    end = c_position + nchars;
    if(end > s_length) end = s_length;
    nchars = end - c_position;

    // Cast sptr to unsigned char * type so that the pointer will be
    // incremented one byte at a time.
    // PC-lint: 09/14/2005 Possible use of null pointer
    if(!sptr) return 0;
    memmove(((unsigned char *)sptr+(c_position*cs)), 
	    ((unsigned char *)sptr+(end*cs)), ((s_length*cs)-(end*cs)));

    s_length -= nchars;
    null_terminate(st);
  }
  else {
    nchars = 0;
  }

  // Return the number of bytes deleted
  return nchars * cs;
}

int UStringB::Find(const char *s, unsigned c_offset) const
// Returns -1 if the pattern is not found.
{
  if(!s) return -1; // Check for null pointers
  unsigned nchars = gxstrlen(s);
  return Find(s, nchars, c_offset);
}

int UStringB::Find(const char *s, unsigned nchars, unsigned c_offset) const
{
  if((!s) || !(sptr)) return -1; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) {
      return gxfind_pattern(sptr, s_length, s, nchars, 
			    c_offset, gxASCII_STRING, 1);
  }
  else {
    unsigned cs = sizeof(wchar_t);
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    int rv = gxfind_pattern(sptr, (s_length*cs), nsptr, (nchars*cs), 
			    (c_offset*cs), gxUNICODE_STRING, cs);
    delete[] nsptr;
    return rv;
  }
}

int UStringB::Find(const wchar_t *s, unsigned c_offset) const
{
  if(!s) return -1; // Check for null pointers
  unsigned nchars = gxstrlen(s);
  return Find(s, nchars, c_offset);
}

int UStringB::Find(const wchar_t *s, unsigned nchars, unsigned c_offset) const
{
  if((!s) || !(sptr)) return -1; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) {
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    int rv = gxfind_pattern(sptr, s_length, nsptr, nchars, 
			    c_offset, gxASCII_STRING, 1);
    delete[] nsptr;
    return rv;
  }
  else { 
    unsigned cs = sizeof(wchar_t);
    return gxfind_pattern(sptr, (s_length*cs), s, (nchars*cs), 
			  (c_offset*cs), gxUNICODE_STRING, cs);
  }
}

int UStringB::Find(const UStringB &s, unsigned c_offset) const
{
  return Find(s, s.s_length, c_offset);
}

int UStringB::Find(const UStringB &s, unsigned nchars, unsigned c_offset) const
{
  if((!s.sptr) || !(sptr)) return -1; // Check for null pointers
  if((nchars == 0) || (nchars > s.s_length)) nchars = s.s_length;
  if(!is_unicode()) {
    if(!s.is_unicode()) {
      return gxfind_pattern(sptr, s_length, s.sptr, nchars, 
			    c_offset, gxASCII_STRING, 1);
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, nchars);
      if(!nsptr) return 0;
      int rv = gxfind_pattern(sptr, s_length, nsptr, nchars, 
			      c_offset, gxASCII_STRING, 1);
      delete[] nsptr;
      return rv;
    }
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, nchars);
      if(!nsptr) return 0;
      int rv = gxfind_pattern(sptr, (s_length*cs), nsptr, (nchars*cs), 
			      (c_offset*cs), gxUNICODE_STRING, cs);
      delete[] nsptr;
      return rv;
    }
    else {
      return gxfind_pattern(sptr, (s_length*cs), s.sptr, (nchars*cs), 
			    (c_offset*cs), gxUNICODE_STRING, cs);
    }
  }
}

int UStringB::IFind(const char *s, unsigned c_offset) const
{
  if(!s) return -1; // Check for null pointers
  unsigned nchars = gxstrlen(s);
  return IFind(s, nchars, c_offset);
}

int UStringB::IFind(const char *s, unsigned nchars, unsigned c_offset) const
{
  if((!s) || !(sptr)) return -1; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) {
      return gxifind_pattern(sptr, s_length, s, nchars, 
			     c_offset, gxASCII_STRING, 1);
  }
  else {
    unsigned cs = sizeof(wchar_t);
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    int rv = gxifind_pattern(sptr, (s_length*cs), nsptr, (nchars*cs), 
			     (c_offset*cs), gxUNICODE_STRING, cs);
    delete[] nsptr;
    return rv;
  }
}

int UStringB::IFind(const wchar_t *s, unsigned c_offset) const
{
  if(!s) return -1; // Check for null pointers
  unsigned nchars = gxstrlen(s);
  return IFind(s, nchars, c_offset);
}

int UStringB::IFind(const wchar_t *s, unsigned nchars, unsigned c_offset) const
{
  if((!s) || !(sptr)) return -1; // Check for null pointers
  if(nchars == 0) nchars = gxstrlen(s);
  if(!is_unicode()) {
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    int rv = gxifind_pattern(sptr, s_length, nsptr, nchars, 
			     c_offset, gxASCII_STRING, 1);
    delete[] nsptr;
    return rv;
  }
  else { 
    unsigned cs = sizeof(wchar_t);
    return gxifind_pattern(sptr, (s_length*cs), s, (nchars*cs), 
			   (c_offset*cs), gxUNICODE_STRING, cs);
  }
}

int UStringB::IFind(const UStringB &s, unsigned c_offset) const
{
  return IFind(s, s.s_length, c_offset);
}

int UStringB::IFind(const UStringB &s, unsigned nchars, 
		    unsigned c_offset) const
{
  if((!s.sptr) || !(sptr)) return -1; // Check for null pointers
  if((nchars == 0) || (nchars > s.s_length)) nchars = s.s_length;
  if(!is_unicode()) {
    if(!s.is_unicode()) {
      return gxifind_pattern(sptr, s_length, s.sptr, nchars, 
			     c_offset, gxASCII_STRING, 1);
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, nchars);
      if(!nsptr) return 0;
      int rv = gxifind_pattern(sptr, s_length, nsptr, nchars, 
			       c_offset, gxASCII_STRING, 1);
      delete[] nsptr;
      return rv;
    }
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, nchars);
      if(!nsptr) return 0;
      int rv = gxifind_pattern(sptr, (s_length*cs), nsptr, (nchars*cs), 
			       (c_offset*cs), gxUNICODE_STRING, cs);
      delete[] nsptr;
      return rv;
    }
    else {
      return gxifind_pattern(sptr, (s_length*cs), s.sptr, (nchars*cs), 
			     (c_offset*cs), gxUNICODE_STRING, cs);
    }
  }
}

int UStringB::Atoi()
{
  if((sptr == 0) || (s_length == 0)) return (int)0;
  if(!is_unicode()) {
    // PC-lint 09/15/2005: atoi() function is considered dangerous
#if defined (__PCLINT_CHECK__)
    return 0;
#else
    return atoi((char *)sptr);
#endif
  }
  else {

#if defined (__HAS_UNICODE__) && defined (__WINCE__) 
    return _wtoi((wchar_t *)sptr);
#elif defined (__HAS_UNICODE__) && defined (__MSVC__)
    return _wtoi((wchar_t *)sptr);
#else
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    if(!nsptr) return 0;
    // PC-lint 09/15/2005: atoi() function is considered dangerous
#if defined (__PCLINT_CHECK__)
    unsigned rv = 0;
#else
    unsigned rv = atol(nsptr);
#endif
    delete[] nsptr;
    return rv;
#endif // __HAS_UNICODE__
  }
}

long UStringB::Atol()
{
  if((sptr == 0) || (s_length == 0)) return 0L;
  if(!is_unicode()) {
    // PC-lint 09/15/2005: atol() function is considered dangerous
#if defined (__PCLINT_CHECK__)
    return 0;
#else 
    return atol((char *)sptr);
#endif
  }
  else {
#if defined (__HAS_UNICODE__) && defined (__WINCE__) 
    return _wtol((wchar_t *)sptr);
#elif defined (__HAS_UNICODE__) && defined (__MSVC__)
    return _wtol((wchar_t *)sptr);
#else
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    if(!nsptr) return 0;
    // PC-lint 09/15/2005: atol() function is considered dangerous
#if defined (__PCLINT_CHECK__)
    unsigned rv = 0;
#else
    unsigned rv = atol(nsptr);
#endif
    delete[] nsptr;
    return rv;
#endif // __HAS_UNICODE__
  }
}

unsigned long UStringB::Atou()
{
  if((sptr == 0) || (s_length == 0)) return (unsigned long)0;
  if(!is_unicode()) {
    return StringToULWORD((const char *)sptr);
  }
  else {
    return StringToULWORD((const wchar_t *)sptr);
  }
}

double UStringB::Atof()
{
  if((sptr == 0) || (s_length == 0)) return (double)0;
  if(!is_unicode()) {
    // PC-lint 09/15/2005: atof() function is considered dangerous
#if defined (__PCLINT_CHECK__)
    return 0;
#else
    return atof((char *)sptr);
#endif
  }
  else {
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    if(!nsptr) return 0;

    // PC-lint 09/15/2005: atof() function is considered dangerous
#if defined (__PCLINT_CHECK__)
    double rv = 0;
#else
    double rv = atof(nsptr);
#endif

    delete[] nsptr;
    return rv;
  }
}

int UStringB::ToUpper()
// Change all characters in the string to upper case.
// Returns true if successful.
{
  if((sptr == 0) || (s_length == 0)) return 0;
  unsigned nchars = s_length;

  if(!is_unicode()) {
    char *start = (char *)sptr;
    
    while(nchars--) {
      *start = (char)toupper(*start);
      start++;
    }
    return 1;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;
    
    while(nchars--) {
#if defined (__HAS_UNICODE__)
      *start = towupper(*start);
#else
      *start = toupper(*start);
#endif // __HAS_UNICODE__
      start++;
    }
    return 1;
  }
}

int UStringB::ToLower()
// Change all characters in the string to lower case.
// Returns true if successful.
{
  if((sptr == 0) || (s_length == 0)) return 0;
  unsigned nchars = s_length;

  if(!is_unicode()) {
    char *start = (char *)sptr;
    
    while(nchars--) {
      *start = (char)tolower(*start);
      start++;
    }
    return 1;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;
    
    while(nchars--) {
#if defined (__HAS_UNICODE__)
      *start = towlower(*start);
#else
      *start = tolower(*start);
#endif // __HAS_UNICODE__
      start++;
    }
    return 1;
  }
}

int UStringB::find_last(const void *s) const
// Returns index of the last occurrence of string s.   
// Returns -1 if the pattern is not found.
{
  // PC-lint: 09/14/2005 Possible use of null pointer
  if(!s) return -1;
  if(!sptr) return -1;

  if(!is_unicode()) {
    unsigned i;
    char *end = (char *)sptr + s_length;
    char *pattern = (char *)s; 
    unsigned nchars = gxstrlen(pattern);
    pattern += (nchars - 1);
    unsigned j = 1;    
    for(i = s_length; i != 0; i--) {
      if(*end == 0) end--; // Skip over the null terminator
      if(*end == *pattern) {
	if(j == nchars) return (int)i; // pattern was found
	j++;
	pattern--;
      }
      else {
	pattern = (char *)s;
	pattern += (nchars - 1);
	j = 1;
      }
      end--;
    }
    return -1; // No match was found
  }
  else {
    unsigned i;
    wchar_t *end = (wchar_t *)sptr + s_length;
    wchar_t *pattern = (wchar_t *)s; 
    unsigned nchars = gxstrlen(pattern);
    pattern += (nchars - 1);
    unsigned j = 1;
    for(i = s_length; i != 0; i--) {
      if(*end == 0) end--; // Skip over the null terminator
      if(*end == *pattern) {
	if(j == nchars) return (int)i; // pattern was found
	j++;
	pattern--;
      }
      else {
	pattern = (wchar_t *)s;
	pattern += (nchars - 1);
	j = 1;
      }
      end--;
    }
    return -1; // No match was found
  }
}

int UStringB::FindLast(const char *s) const
{
  if((!s) || !(sptr)) return -1; // Check for null pointers
  if(!is_unicode()) {
    return find_last(s);
  }
  else {
    unsigned nchars = gxstrlen(s);
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    int rv = find_last(nsptr);
    delete[] nsptr;
    return rv;
  }
}

int UStringB::FindLast(const wchar_t *s) const
{
  if((!s) || !(sptr)) return -1; // Check for null pointers
  if(!is_unicode()) {
    unsigned nchars = gxstrlen(s);
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    int rv = find_last(nsptr);
    delete[] nsptr;
    return rv;
  }
  else { 
    return find_last(s);
  }
}

int UStringB::FindLast(const UStringB &s) const
{
  // Check for null pointers and strings
  if((!s.sptr) || !(sptr)) return -1; // Check for null pointers
  if(!is_unicode()) {
    if(!s.is_unicode()) {
      return find_last(s.sptr);
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, s.s_length);
      if(!nsptr) return 0;
      int rv = find_last(nsptr);
      delete[] nsptr;
      return rv;
    }
  }
  else {
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, s.s_length);
      if(!nsptr) return 0;
      int rv = find_last(nsptr);
      delete[] nsptr;
      return rv;
    }
    else {
      return find_last(s.sptr);
    }
  }
}

unsigned UStringB::ReplaceChar(const char c, const char replacement,
			       unsigned offset)
// Replace the specified character. Returns the number of characters
// replaced in the string.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  if(!is_unicode()) {
    unsigned num_replaced = 0;
    unsigned i;
    char *start = (char *)sptr + offset;

    for(i = 0; i < s_length; i++) {
      if(start[i] == c) {
	start[i] = replacement;
	num_replaced++;
      }
    }
    return num_replaced;
  }
  else {
#if defined (__PCLINT_CHECK__)
    return 0;
#else
    wchar_t wc = (wchar_t)c;
    wchar_t wreplacement = (wchar_t)replacement;
    return ReplaceChar(wc, wreplacement, offset);
#endif
  }
}

unsigned UStringB::ReplaceChar(const wchar_t c, const wchar_t replacement,
			       unsigned offset)
// Replace the specified character. Returns the number of characters
// replaced in the string.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  if(!is_unicode()) {
    char ac = (char)c;
    char areplacement = (char)replacement;
    return ReplaceChar(ac, areplacement, offset);
  }
  else {
    unsigned num_replaced = 0;
    unsigned i;
    wchar_t *start = (wchar_t *)sptr + offset;

    for(i = 0; i < s_length; i++) {
      if(start[i] == c) {
	start[i] = replacement;
	num_replaced++;
      }
    }
    return num_replaced;
  }
}

unsigned UStringB::ReplaceString(const char *s, 
				 const char *replacement,
				 unsigned offset)
{
  unsigned num_replaced = 0;
  while(1) {
    offset = Find(s, offset);
    if ((int)offset == -1) break;
    DeleteAt(offset, gxstrlen(s));
    InsertAt(offset, replacement, gxstrlen(replacement));
    num_replaced++;
    offset += gxstrlen(replacement);
  }
  return num_replaced;
}

unsigned UStringB::ReplaceString(const wchar_t *s, 
				 const wchar_t *replacement,
				 unsigned offset)
{
  unsigned num_replaced = 0;
  while(1) {
    offset = Find(s, offset);
    if ((int)offset == -1) break;
    DeleteAt(offset, gxstrlen(s));
    InsertAt(offset, replacement, gxstrlen(replacement));
    num_replaced++;
    offset += gxstrlen(replacement);
  }
  return num_replaced;
}

unsigned UStringB::ReplaceString(const UStringB &s, 
				 const UStringB &replacement,
				 unsigned offset)
{
  unsigned num_replaced = 0;
  while(1) {
    offset = Find(s, offset);
    if ((int)offset == -1) break;
    DeleteAt(offset, s.s_length);
    InsertAt(offset, replacement, replacement.s_length);
    num_replaced++;
    offset += replacement.s_length;
  }
  return num_replaced;
}

unsigned UStringB::FilterString(const char *s, unsigned offset)
// Filter the specified string from the object. Returns
// the number of strings filtered from the string.
{
  unsigned num_replaced = 0;
  while(1) {
    offset = Find(s, offset);
    if((int)offset == -1) break;
    DeleteAt(offset, gxstrlen(s));
    num_replaced++;
    offset++;
  }
  return num_replaced;
}

unsigned UStringB::FilterString(const wchar_t *s, unsigned offset)
// Filter the specified string from the object. Returns
// the number of strings filtered from the string.
{
  unsigned num_replaced = 0;
  while(1) {
    offset = Find(s, offset);
    if((int)offset == -1) break;
    DeleteAt(offset, gxstrlen(s));
    num_replaced++;
    offset++;
  }
  return num_replaced;
}

unsigned UStringB::FilterString(const UStringB &s, unsigned offset)
// Filter the specified string from the object. Returns
// the number of strings filtered from the string.
{
  unsigned num_replaced = 0;
  while(1) {
    offset = Find(s, offset);
    if ((int)offset == -1) break;
    DeleteAt(offset, s.s_length);
    num_replaced++;
    offset++;
  }
  return num_replaced;
}

unsigned UStringB::FilterChar(const char c, unsigned offset)
// Filter the specified character from the object. Returns
// the number of characters filtered from the string.
{
  if(!is_unicode()) {
    char s[2]; // Define a two char array (char_type + null terminator)
    s[0] = c; // Character to be filtered
    s[1] = 0; // Null-terminate the string so the gxstrlen() function 
              // will work correctly.
    return FilterString((const char *)s, offset);
  }
  else {
#if defined (__PCLINT_CHECK__)
    return 0;
#else
    // Define a two char array (char_type + null terminator)
    wchar_t ws_char[2]; 
    ws_char[0] = (wchar_t)c; // Character to be filtered
    ws_char[1] = 0; // Null-terminate the string so the gxstrlen() function 
                    // will work correctly.
    return FilterString((const wchar_t *)ws_char, offset);
#endif
  }
}

unsigned UStringB::FilterChar(const wchar_t c, unsigned offset)
// Filter the specified character from the object. Returns
// the number of characters filtered from the string.
{
  if(!is_unicode()) {
    char s[2]; // Define a two char array (char_type + null terminator)
    s[0] = (char)c; // Character to be filtered
    s[1] = 0; // Null-terminate the string so the gxstrlen() function 
              // will work correctly.
    return FilterString((const char *)s, offset);
  }
  else {
#if defined (__PCLINT_CHECK__)
    return 0;
#else
    // Define a two char array (char_type + null terminator)
    wchar_t ws_char[2]; 
    ws_char[0] = c; // Character to be filtered
    ws_char[1] = 0; // Null-terminate the string so the gxstrlen() function 
               // will work correctly.
    return FilterString((const wchar_t *)ws_char, offset);
#endif
  }
}

unsigned UStringB::TrimLeadingSpaces()
// Filter all leading spaces from a string.
// Returns the number of characters filtered.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  unsigned i, num_l_spaces = 0;

  if(!is_unicode()) {
    char *start = (char *)sptr;

    for(i = 0; i < s_length; i++) {
      if(!isspace(start[i])) break;
      num_l_spaces++;
    }
    if(num_l_spaces > 0) DeleteAt(0, num_l_spaces);
    return num_l_spaces;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;

    for(i = 0; i < s_length; i++) {
#if defined (__HAS_UNICODE__)
      if(!iswspace(start[i])) break;
#else
      if(!isspace(start[i])) break;
#endif // __HAS_UNICODE__
      num_l_spaces++;
    }
    if(num_l_spaces > 0) DeleteAt(0, num_l_spaces);
    return num_l_spaces;
  }
}

unsigned UStringB::TrimTrailingSpaces()
// Filter all Trailing spaces from a string.
// Returns the number of characters filtered.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  if(!is_unicode()) {
    char *start = (char *)sptr;
    unsigned i, num_t_spaces = 0;
  
    for(i = (s_length - 1); i != 0; i--) {
      if(!isspace(start[i])) break;
      num_t_spaces++;
    }
    if(num_t_spaces > 0) DeleteAt((s_length - num_t_spaces), num_t_spaces);
    return num_t_spaces;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;
    unsigned i, num_t_spaces = 0;
  
    for(i = (s_length - 1); i != 0; i--) {
#if defined (__HAS_UNICODE__)
      if(!iswspace(start[i])) break;
#else
      if(!isspace(start[i])) break;
#endif // __HAS_UNICODE__
      num_t_spaces++;
    }
    if(num_t_spaces > 0) DeleteAt((s_length - num_t_spaces), num_t_spaces);
    return num_t_spaces;
  }
}

int UStringB::DeleteAfter(const char *s)
// Delete everything after the specified string.
// Returns true if successful.
{
  unsigned nchars = s_length;
  int offset = Find(s);
  if(offset == -1) return 0;
  offset += gxstrlen(s); // Keep the delimiting string
  DeleteAt(offset, (nchars - offset));
  return 1;
}

int UStringB::DeleteAfter(const wchar_t *s)
// Delete everything after the specified string.
// Returns true if successful.
{
  unsigned nchars = s_length;
  int offset = Find(s);
  if(offset == -1) return 0;
  offset += gxstrlen(s); // Keep the delimiting string
  DeleteAt(offset, (nchars - offset));
  return 1;
}

int UStringB::DeleteAfter(const UStringB &s)
// Delete everything after the specified string.
// Returns true if successful.
{
  unsigned nchars = s_length;
  int offset = Find(s);
  if(offset == -1) return 0;
  offset += s.s_length; // Keep the delimiting string
  DeleteAt(offset, (nchars - offset));
  return 1;

}

int UStringB::DeleteBefore(const char *s)
// Delete everything before the specified string.
// Returns true if successful.
{
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(0, offset);
  return 1;
}

int UStringB::DeleteBefore(const wchar_t *s)
// Delete everything before the specified string.
// Returns true if successful.
{
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(0, offset);
  return 1;
}

int UStringB::DeleteBefore(const UStringB &s)
// Delete everything before the specified string.
// Returns true if successful.
{
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(0, offset);
  return 1;
}

int UStringB::DeleteAfterIncluding(const char *s)
// Delete everything after the specified string
// including the string. Returns true if successful.
{
  unsigned nchars = s_length;
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(offset, (nchars - offset));
  return 1;
}

int UStringB::DeleteAfterIncluding(const wchar_t *s)
// Delete everything after the specified string
// including the string. Returns true if successful.
{
  unsigned nchars = s_length;
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(offset, (nchars - offset));
  return 1;
}

int UStringB::DeleteAfterIncluding(const UStringB &s)
// Delete everything after the specified string
// including the string. Returns true if successful.
{
  unsigned nchars = s_length;
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(offset, (nchars - offset));
  return 1;
}

int UStringB::DeleteBeforeIncluding(const char *s)
// Delete everything before the specified string
// including the string. Returns true if successful.
{
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(0, offset+gxstrlen(s));
  return 1;
}

int UStringB::DeleteBeforeIncluding(const wchar_t *s)
// Delete everything before the specified string
// including the string. Returns true if successful.
{
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(0, offset+gxstrlen(s));
  return 1;
}

int UStringB::DeleteBeforeIncluding(const UStringB &s)
// Delete everything before the specified string
// including the string. Returns true if successful.
{
  int offset = Find(s);
  if(offset == -1) return 0;
  DeleteAt(0, offset+s.s_length);
  return 1;
}

int UStringB::DeleteAfterLast(const char *s)
// Deletes everything after the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  unsigned nchars = s_length;
  
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) {
    index += gxstrlen(s); // Keep the specified string
    DeleteAt(index, (nchars - index));
  }
  return 1;
}

int UStringB::DeleteAfterLast(const wchar_t *s)
// Deletes everything after the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  unsigned nchars = s_length;
  
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) {
    index += gxstrlen(s); // Keep the specified string
    DeleteAt(index, (nchars - index));
  }
  return 1;
}

int UStringB::DeleteAfterLast(const UStringB &s)
// Deletes everything after the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  unsigned nchars = s_length;
  
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) {
    index += s.s_length; // Keep the specified string
    DeleteAt(index, (nchars - index));
  }
  return 1;
}

int UStringB::DeleteAfterLastIncluding(const char *s)
// Deletes everything after the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  unsigned nchars = s_length;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(index, (nchars - index)); 
  return 1;
}

int UStringB::DeleteAfterLastIncluding(const wchar_t *s)
// Deletes everything after the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  unsigned nchars = s_length;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(index, (nchars - index)); 
  return 1;

}

int UStringB::DeleteAfterLastIncluding(const UStringB &s)
// Deletes everything after the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  unsigned nchars = s_length;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(index, (nchars - index)); 
  return 1;
}

int UStringB::DeleteBeforeLast(const char *s)
// Deletes everything before the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(0, index);
  return 1;
}

int UStringB::DeleteBeforeLast(const wchar_t *s)
// Deletes everything before the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(0, index);
  return 1;
}

int UStringB::DeleteBeforeLast(const UStringB &s)
// Deletes everything before the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(0, index);
  return 1;
}

int UStringB::DeleteBeforeLastIncluding(const char *s)
// Deletes everything before the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(0, (index + gxstrlen(s)));
  return 1;
}

int UStringB::DeleteBeforeLastIncluding(const wchar_t *s)
// Deletes everything before the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(0, (index + gxstrlen(s)));
  return 1;
}

int UStringB::DeleteBeforeLastIncluding(const UStringB &s)
// Deletes everything before the last occurrence
// of the specified string. Return true if the
// any characters were deleted.
{
  int offset = 0;
  int index = 0;
  while(1) { 
    offset = Find(s, offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }
  if(index > 0) DeleteAt(0, (index + s.s_length));
  return 1;

}

int UStringB::ConvertEscapeSeq(char esc, int num_octets)
// Function used to convert hex escape sequences within a string 
// to printable ASCII characters. This function assumes that 
// the number of octets following the escape character is hex 
// values that can be converted to an ASCII value. Returns the 
// number of escape sequences converted to printable characters.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  if(!is_unicode()) { 
    unsigned num_esc = 0;
    char s[2]; s[0] = esc; s[1] = 0;
    const int sbuf_len = 81;
    if(num_octets > sbuf_len) num_octets = sbuf_len-1;
    if(num_octets <= 0) num_octets = 2;
    char sbuf[sbuf_len];
    int offset = 0;
    char *start = (char *)sptr;
    int hex_digits;
    
    while(1) {
      offset = Find(s, offset);
      if(offset == -1) break;
      if(start[offset+1] == esc) {
	// This is an escaped escaped sequence, so
	// leave one escape character in the string.
	DeleteAt(offset, 1); // Remove the first esc
	offset++;
	continue;
      }
      DeleteAt(offset, 1); // Remove the esc from this string
      memmove(sbuf, (start+offset), num_octets);
      sbuf[num_octets] = 0;
      sscanf(sbuf, "%x", &hex_digits);
      DeleteAt(offset, num_octets); // Remove the octets from this string
      sbuf[0] = (char)hex_digits;
      InsertAt(offset, sbuf, 1); // Insert the ASCII character
      num_esc++;
      offset++;
    }
    return num_esc;
  }
  else {
#if defined (__PCLINT_CHECK__)
    return 0;
#else 
    return ConvertEscapeSeq((wchar_t)esc, num_octets);
#endif
  }
}

int UStringB::ConvertEscapeSeq(wchar_t esc, int num_octets)
// Function used to convert hex escape sequences within a string 
// to printable ASCII characters. This function assumes that 
// the number of octets following the escape character is hex 
// values that can be converted to an ASCII value. Returns the 
// number of escape sequences converted to printable characters.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  if(!is_unicode()) {
    return ConvertEscapeSeq((char)esc, num_octets);
  }
  else {
    unsigned num_esc = 0;
    const unsigned cs = sizeof(wchar_t);
    const int max_chars = 81;
    const int min_chars = 2;
    const int sbuf_len = max_chars * cs;
    wchar_t *start = (wchar_t *)sptr;
    wchar_t s[2]; s[0] = esc; s[1] = 0;
    
    // Do not exceed the bound of the sbuf array
    if(int(num_octets * cs) > sbuf_len) num_octets = max_chars-1;
    if(num_octets <= 0) num_octets = min_chars;
    wchar_t sbuf[sbuf_len];
    int offset = 0;
    int hex_digits;
    while(1) {
      offset = Find(s, offset);
      if(offset == -1) break;
      if(start[offset+1] == esc) {
	// This is an escaped escaped sequence, so
	// leave one escape character in the string.
	DeleteAt(offset, 1); // Remove the first esc
	offset++;
	continue;
      }
      DeleteAt(offset, 1); // Remove the esc from this string
      memmove(sbuf, (start+offset), (num_octets * cs));
      sbuf[num_octets] = 0;
#if defined (__HAS_UNICODE__) && defined (__WIN32__)
      wchar_t wf[3]; char f0 = '%'; char f1 = 'x';
      // PC-lint 09/15/2005: Ignore type cast warnings
      wf[0] = (wchar_t)f0; wf[1] = (wchar_t)f1; wf[2] = (wchar_t)0;
      swscanf(sbuf, wf, &hex_digits);
#else
      char *asbuf = Unicode2ASCII(sbuf, num_octets);
      sscanf(asbuf, "%x", &hex_digits);
      wchar_t *wsbuf = ASCII2Unicode(asbuf, gxstrlen(asbuf));
      gxstrcpy(sbuf, wsbuf);
      delete[] asbuf;
      delete[] wsbuf;
#endif // __HAS_UNICODE__
      
      DeleteAt(offset, num_octets); // Remove the octets from this string
      sbuf[0] = (wchar_t)hex_digits;
      InsertAt(offset, sbuf, 1); // Insert the Unicode character
      num_esc++;
      offset++;
    }
    return num_esc;
  }
}

unsigned UStringB::SetLength()
// Function used to set or reset the logical length of this string
// based on the string length returned by the string length function.
// Redundantly returns the logical length of this string.
{
  if(!sptr) { // Check for null pointers
    s_length = 0;
    d_length = 0;
    return 0; 
  }
  if(!is_unicode()) {
    return s_length = gxstrlen((char *)sptr); 
  }
  else {
    return s_length = gxstrlen((wchar_t *)sptr); 
  }
}

unsigned UStringB::SetLength(unsigned nchars)
// Function used to change the logical length of this string.
// If the dimensioned is exceeded the string will be resized.
// Redundantly returns the logical length of this string.
{
  unsigned cs; gxStringType st;
  if(!is_unicode()) {
    cs = sizeof(char);
    st = gxASCII_STRING;
  }
  else {
    cs = sizeof(wchar_t);
    st = gxUNICODE_STRING;
  }
  if((nchars * cs) > (d_length-1)) {
    if(!this->resize(nchars+1)) {
      nchars = (d_length-1)/cs;
      if((d_length-1) % cs) nchars++;
    }
  }
  if(!sptr) { // Check for null pointers after resize
    s_length = 0;
    d_length = 0;
    return 0; 
  }
  s_length = nchars;  // Set the logical length 
  null_terminate(st); // Ensure null termination
  return s_length; 
}

void UStringB::ResetGlobalNull()
// Function used to reset the global null pointers 
// in the event that they have been modified and
// are no longer contain a null character.
{
  UStringNULLPtr::UStringNUllChar = '\0';
  UStringNULLPtr::UStringNUllStr[0] = '\0';
  UStringNULLPtr::UStringwNUllChar = '\0';
  UStringNULLPtr::UStringwNUllStr[0] = '\0';
}

void *UStringB::SetSPrt(void *s, unsigned nchars, unsigned bytes)
// Function used to bind this object to another null terminated 
// string. The caller must set the logical length of this string 
// with the option to set the dimensioned length is there are 
// characters past the string's null terminator. Redundantly 
// returns a pointer to the string passed to this function.
// NOTE: The memory held by the current pointer will be freed 
// unless the UStringB::Release() function is called prior to 
// calling this function. The Release() function should be called 
// only if other entities are bound to this object's string. 
{
  unsigned cs;
  if(!is_unicode()) {
    cs = sizeof(char);
  }
  else {
    cs = sizeof(wchar_t);
  }
  delete_sptr(); // Free the memory this object is holding

  if(s) { // Check for null pointers
    sptr = s;
    s_length = nchars;
    d_length = nchars * cs;
    if(bytes > 0) d_length = bytes;
  }
  return sptr;
}

UStringB &UStringB::operator+=(const char c)
{
  Cat((const char *)&c, 1);
  return *this;
}

UStringB &UStringB::operator+=(const wchar_t c)
{
  Cat((const wchar_t *)&c, 1);
  return *this;
}

UStringB &UStringB::operator<<(UStringB & (*_f)(UStringB&))
{
  // PC-lint 09/15/2005: Ignore PC-lint type warning
  (*_f)(*(this));
  return *(this);
}

UStringB &UStringB::operator<<(const char c)
{
  if(output_width > 1) {
    int i = output_width - 1;
    while(i--) *this += output_fill;
  }
  return *this += c;
}

UStringB& UStringB::operator<<(const long val)
{
  char sbuf[81];
  if(output_hex == 1) { // Base 16
    // Using type cast to int type to eliminate
    // warning: int format, long int arg (arg 3) when compiled
    // under Linux gcc.
    sprintf(sbuf, "%X", (int)val);
  }
  else if(output_hex == 2) { // Base 8
    sprintf(sbuf, "%o", (int)val);
  }
  else if(output_hex == 3) { // Shorthand notation
    SHN((unsigned long)val);
    return *this;
  }
  else if(output_hex == 4) { // Whole number comma notation
    WholeNumber((unsigned long)val);
    return *this;
  }
  else { // Default to base 10
    sprintf(sbuf, "%d", (int)val);
  }
  if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
    int i = output_width - strlen(sbuf);
    while(i--) *this += output_fill;
  }
  return *this += sbuf;
}

UStringB& UStringB::operator<<(const unsigned long val)
{
  char sbuf[81];
  if(output_hex == 1) { // Base 16
    // Using type cast to int type to eliminate
    // warning: int format, long int arg (arg 3) when compiled
    // under Linux gcc.
    sprintf(sbuf, "%X", (unsigned int)val);
  }
  else if(output_hex == 2) { // Base 8
    sprintf(sbuf, "%o", (int)val);
  }
  else if(output_hex == 3) { // Shorthand notation
    SHN(val);
    return *this;
  }
  else if(output_hex == 4) { // Whole number comma notation
    WholeNumber(val);
    return *this;
  }
  else { // Default to base 10
    sprintf(sbuf, "%u", (unsigned int)val);
  }
  if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
    int i = output_width - strlen(sbuf);
    while(i--) *this += output_fill;
  }
  return *this += sbuf;
}

UStringB& UStringB::operator<<(const double val)
{
  char sbuf[81]; char fbuf[81];
  sprintf(fbuf, "%%.%df", (int)output_pre);
  sprintf(sbuf, (const char *)fbuf, val);
  if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
    int i = output_width - strlen(sbuf);
    while(i--) *this += output_fill;
  }
  return *this += sbuf;
}

UStringB& UStringB::operator>>(char *s)
{
  if(!s) return *this; // Check for null pointers
  if(!sptr) {
    gxstrcpy(s, UStringNULLPtr::UStringNUllStr); 
    return *this;
  }

  if(!is_unicode()) {
    gxstrcpy(s, (char *)sptr); // Assumes enough memory is allocated for *s
  }
  else {
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    if(!nsptr) return *this;
    gxstrcpy(s, nsptr); // Assumes enough memory is allocated for *s
    delete[] nsptr;
  }
  return *this;
}

UStringB& UStringB::operator>>(char &c)
{
  if(!sptr) {
    c = UStringNULLPtr::UStringNUllChar; 
    return *this; // Check for null pointers
  }
  if(!is_unicode()) {
    char *asptr = (char *)sptr;
    c = asptr[0];
  }
  else {
    char *wsptr = (char *)sptr;
    c = (char)wsptr[0];
  }
  return *this;
}

UStringB& UStringB::operator>>(long &val)
{
  val = this->Atol();
  return *this;
}

UStringB& UStringB::operator>>(unsigned long &val)
{
  val = (unsigned long)this->Atou();
  return *this;
}

UStringB& UStringB::operator>>(int &val)
{
  val = this->Atoi();
  return *this;
}

UStringB& UStringB::operator>>(unsigned int &val)
{
  val = (unsigned int)this->Atou();
  return *this;
}

UStringB& UStringB::operator>>(unsigned short &val)
{
  val = (unsigned short)this->Atoi();
  return *this;
}

UStringB& UStringB::operator>>(short &val)
{
  val = (short)this->Atoi();
  return *this;
}

UStringB& UStringB::operator>>(float &val)
{
  val = (float)this->Atof();
  return *this;
}

UStringB& UStringB::operator>>(double &val)
{
  val = (double)this->Atof();
  return *this;
}

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
UStringB& UStringB::operator<<(const __LLWORD__ val)
{
  char sbuf[81];
  if(output_hex == 1) { // Base 16
    LLWORDToString(sbuf, val, 16);
  }
  else if(output_hex == 2) { // Base 8
    LLWORDToString(sbuf, val, 8);
  }
  else if(output_hex == 3) { // Shorthand notation
    SHN((__ULLWORD__)val);
    return *this;
  }
  else if(output_hex == 4) { // Whole number comma notation
    WholeNumber((__ULLWORD__)val);
    return *this;
  }
  else { // Default to base 10
    LLWORDToString(sbuf, val, 10);
  }
  if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
    int i = output_width - strlen(sbuf);
    while(i--) *this += output_fill;
  }
  return *this += sbuf;
}

UStringB& UStringB::operator<<(const __ULLWORD__ val)
{
  char sbuf[81];
  if(output_hex == 1) { // Base 16
    ULLWORDToString(sbuf, val, 16);
  }
  else if(output_hex == 2) { // Base 8
    ULLWORDToString(sbuf, val, 8);
  }
  else if(output_hex == 3) { // Shorthand notation
    SHN(val);
    return *this;
  }
  else if(output_hex == 4) { // Whole number comma notation
    WholeNumber(val);
    return *this;
  }
  else { // Default to base 10
    ULLWORDToString(sbuf, val, 10);
  }
  if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
    int i = output_width - strlen(sbuf);
    while(i--) *this += output_fill;
  }
  return *this += sbuf;
}

__LLWORD__ UStringB::Atoi64()
{
  // Check for null pointers
  if((sptr == 0) || (s_length == 0)) return (__LLWORD__)0; 
  if(!is_unicode()) {
    return StringToLLWORD((char *)sptr);
  }
  else {
    return StringToLLWORD((wchar_t *)sptr);
  }
}

__ULLWORD__ UStringB::Atou64()
{
  // Check for null pointers
  if((sptr == 0) || (s_length == 0)) return (__ULLWORD__)0; 
  if(!is_unicode()) {
    return StringToULLWORD((char *)sptr);
  }
  else {
    return StringToULLWORD((wchar_t *)sptr);
  }
}

UStringB& UStringB::operator>>(__LLWORD__ &val)
{
  val = this->Atoi64();
  return *this;
}

UStringB& UStringB::operator>>(__ULLWORD__ &val)
{
  val = (__ULLWORD__)this->Atou64();
  return *this;
}

void UStringB::WholeNumber(__ULLWORD__ bytes)
// Public member function used to insert a comma 
// separated whole number into a string.
{
  char sbuf[81];
  ULLWORDToString(sbuf, bytes, 10);
  unsigned start_len = length();
  *this += sbuf;
  unsigned stop_len = length();
  unsigned slen = stop_len - start_len;
  stop_len -= slen;
  unsigned offset = length();
  unsigned num_commas = 0;
  if(slen > 3) num_commas = slen/3;
  for(unsigned i = 0; i < num_commas; i ++) {
    offset -= 3;
    if(offset == stop_len) break;
    InsertAt(offset, ",", 1);
  }
}

void UStringB::SHN(__ULLWORD__ bytes)
{
  double shn_n; // Shorthand notation number
  char shn_c[2] = { '\0', '\0' }; // Shorthand notation nomenclature
  char sbuf[81]; char hbuf[81]; char rbuf[81];
  unsigned long lval;
  int i;
  __ULLWORD__ llval, rlval;
  __ULLWORD__ x1G = (1000*1000)*1000;
  if(bytes > 1000) { // More than 1K bytes
    if(bytes < (1000*1000)) { // Less than 1M
      lval = (unsigned long)bytes;
      shn_n = (double)lval/double(1000);
      shn_c[0] = 'K';
      sprintf(sbuf, "%.2f", shn_n);
    }
    else if(bytes < ((1000*1000)*1000)) { // Less than 1G 
      lval = (unsigned long)bytes;
      shn_n = (double)lval/double(1000*1000);
      shn_c[0] = 'M';
      sprintf(sbuf, "%.2f", shn_n);
    }
    else if(bytes < (1000*x1G)) { // Less than 1T 
      llval = bytes/((1000*1000)*1000);
      rlval = bytes%((1000*1000)*1000);
      ULLWORDToString(hbuf, llval, 10);
      ULLWORDToString(rbuf, rlval, 10);
      rbuf[3] = 0;
      sprintf(sbuf, "%s.%s", hbuf, rbuf);
      shn_c[0] = 'G';
    }
    else { // 1T or greater
      llval = bytes/(1000*x1G);
      rlval = bytes%(1000*x1G);
      ULLWORDToString(hbuf, llval, 10);
      ULLWORDToString(rbuf, rlval, 10);
      rbuf[3] = 0;
      sprintf(sbuf, "%s.%s", hbuf, rbuf);
      shn_c[0] = 'T';
    }
  }
  else {
    sprintf(sbuf, "%u", (unsigned int)bytes);
    if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
      i = output_width - strlen(sbuf);
      while(i--) *this += output_fill;
    }
    *(this) += sbuf;
    return;
  }

  i = strlen(sbuf);
  while(i--) {
    if((sbuf[i] == '0') || (sbuf[i] == '.')) {
      // Remove all trailing zeros and trailing dots
      sbuf[i] = 0;
    }
    else {
      // Break on the first non zero or non dot
      break;
    }
  }
  if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
    i = (output_width-1) - strlen(sbuf);
    while(i--) *this += output_fill;
  }

  *(this) += sbuf; 
  *(this) += shn_c;
}
#endif

int UStringB::StringCompare(const char *s, int check_case) const
// Function used to perform a string compare. If the "check_case"
// variable is false this function will ignore the upper and lower
// case letters. Returns -1 if this->sptr < s, 0 if this->sptr < s, 
// and 1 if this->sptr > s.
{
  // Comparisons for null pointers and null strings
  if((sptr == 0) || (s_length == 0)) {
    if(!s) {
      return 0; // Both pointers are null
    }
    if(gxstrlen(s) == 0) return 0; // Both strings are null
    return -1; // this->sptr is null
  }
  if(!s) {
    if((sptr == 0) || (s_length == 0)) return 0; // Both pointers are null
    return 1; // this->sptr is not null
  }

  if(!is_unicode()) {
    if(check_case) {
      return gxbyte_compare(sptr, s_length, s, gxstrlen(s));
    }
    else {
      return gxbyte_icompare(sptr, s_length, s, gxstrlen(s),
			     gxASCII_STRING);
    }
  }
  else {
    unsigned nchars = gxstrlen(s);
    unsigned cs = sizeof(wchar_t);
    wchar_t *nsptr = ASCII2Unicode(s, nchars);
    if(!nsptr) return 0;
    int rv;
    if(check_case) {
      rv = gxbyte_compare(sptr, (s_length*cs), nsptr, (nchars*cs));
    }
    else {
      rv = gxbyte_icompare(sptr, (s_length*cs), nsptr, (nchars*cs),
			   gxUNICODE_STRING);
    }
    delete[] nsptr;
    return rv;
  }
}

int UStringB::StringCompare(const wchar_t *s, int check_case) const
// Function used to perform a string compare. If the "check_case"
// variable is false this function will ignore the upper and lower
// case letters. Returns -1 if this->sptr < s, 0 if this->sptr < s, 
// and 1 if this->sptr > s.
{
  // Comparisons for null pointers and null strings
  if((sptr == 0) || (s_length == 0)) {
    if(!s) {
      return 0; // Both pointers are null
    }
    if(gxstrlen(s) == 0) return 0; // Both strings are null
    return -1; // this->sptr is null
  }
  if(!s) {
    if((sptr == 0) || (s_length == 0)) return 0; // Both pointers are null
    return 1; // this->sptr is not null
  }

  if(!is_unicode()) {
    unsigned nchars = gxstrlen(s);
    char *nsptr = Unicode2ASCII(s, nchars);
    if(!nsptr) return 0;
    int rv;
    if(check_case) {
      rv = gxbyte_compare(sptr, s_length, nsptr, nchars);
    }
    else {
      rv = gxbyte_icompare(sptr, s_length, nsptr, nchars,
			   gxASCII_STRING);
    }
    delete[] nsptr;
    return rv;
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(check_case) {
      return gxbyte_compare(sptr, (s_length*cs), s, (gxstrlen(s)*cs));
    }
    else {
      return gxbyte_icompare(sptr, (s_length*cs), s, (gxstrlen(s)*cs),
			     gxUNICODE_STRING);
    }
  }
}

int UStringB::StringCompare(const UStringB &s, int check_case) const
// Function used to perform a string compare. If the "check_case"
// variable is false this function will ignore the upper and lower
// case letters. Returns -1 if this->sptr < s, 0 if this->sptr < s, 
// and 1 if this->sptr > s.
{
  // Comparisons for pointers and null strings
  if((sptr == 0) || (s_length == 0)) {
    if((s.sptr == 0) || (s.s_length == 0)) return 0; // Both strings are null
    return -1; // this->sptr is null
  }
  if((s.sptr == 0) || (s.s_length == 0)) {
    if((sptr == 0) || (s_length == 0)) return 0; // Both strings are null
    return 1; // this->sptr is not null
  }

  if(!is_unicode()) {
    if(!s.is_unicode()) {
      if(check_case) {
	return gxbyte_compare(sptr, s_length, s.sptr, s.s_length);
      }
      else {
	return gxbyte_icompare(sptr, s_length, s.sptr, s.s_length,
			       gxASCII_STRING);
      }
    }
    else {
      char *nsptr = Unicode2ASCII((wchar_t *)s.sptr, s.s_length);
      if(!nsptr) return 0;
      int rv;
      if(check_case) {
	rv = gxbyte_compare(sptr, s_length, nsptr, s.s_length);
      }
      else {
	rv = gxbyte_icompare(sptr, s_length, nsptr, s.s_length,
			     gxASCII_STRING);
      }
      delete[] nsptr;
      return rv;
    }
  }
  else {
    unsigned cs = sizeof(wchar_t);
    if(!s.is_unicode()) {
      wchar_t *nsptr = ASCII2Unicode((char *)s.sptr, s.s_length);
      if(!nsptr) return 0;
      int rv;
      if(check_case) {
	rv = gxbyte_compare(sptr, (s_length*cs), nsptr, (s.s_length*cs));
      }
      else {
	rv = gxbyte_icompare(sptr, (s_length*cs), nsptr, (s.s_length*cs),
			     gxUNICODE_STRING);
      }
      delete[] nsptr;
      return rv;
    }
    else {
      if(check_case) {
	return gxbyte_compare(sptr, (s_length*cs), s.sptr, (s.s_length*cs));
      }
      else {
	return gxbyte_icompare(sptr, (s_length*cs), s.sptr, (s.s_length*cs),
			       gxUNICODE_STRING);
      }
    }
  }
}

char *UStringB::c_str(char *sbuf)
// Returns a null terminated string representing this object's
// ASCII value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  // Check for null pointers
  if(!sbuf) return UStringNULLPtr::UStringNUllStr; 
  if(!sptr) return gxstrcpy(sbuf, UStringNULLPtr::UStringNUllStr); 
  if(!is_unicode()) {
    gxstrcpy(sbuf, (char *)sptr);
    return sbuf;
  }
  else {
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    gxstrcpy(sbuf, nsptr);
    delete nsptr;
    return sbuf;
  }
}

const char *UStringB::c_str(char *sbuf) const
// Returns a null terminated string representing this object's
// ASCII value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly eturned by
// this function.
{
  // Check for null pointers
  if(!sbuf) return UStringNULLPtr::UStringNUllStr; 
  if(!sptr) return gxstrcpy(sbuf, UStringNULLPtr::UStringNUllStr); 
  if(!is_unicode()) {
    gxstrcpy(sbuf, (char *)sptr);
    return sbuf;
  }
  else {
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    gxstrcpy(sbuf, nsptr);
    delete nsptr;
    return sbuf;
  }
}

wchar_t *UStringB::c_wstr(wchar_t *sbuf)
// Returns a null terminated string representing this object's
// Unicode value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  // Check for null pointers
  if(!sbuf) return UStringNULLPtr::UStringwNUllStr; 
  if(!sptr) return gxstrcpy(sbuf, UStringNULLPtr::UStringwNUllStr); 
  if(!is_unicode()) {
    wchar_t *nsptr = ASCII2Unicode((char *)sptr, s_length);
    gxstrcpy(sbuf, nsptr);
    delete nsptr;
    return sbuf;
  }
  else {
    gxstrcpy(sbuf, (wchar_t *)sptr);
    return sbuf;
  }
}

const wchar_t *UStringB::c_wstr(wchar_t *sbuf) const
// Returns a null terminated string representing this object's
// Unicode value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  // Check for null pointers
  if(!sbuf) return UStringNULLPtr::UStringwNUllStr; 
  if(!sptr) return gxstrcpy(sbuf, UStringNULLPtr::UStringwNUllStr); 
  if(!is_unicode()) {
    wchar_t *nsptr = ASCII2Unicode((char *)sptr, s_length);
    gxstrcpy(sbuf, nsptr);
    delete nsptr;
    return sbuf;
  }
  else {
    gxstrcpy(sbuf, (wchar_t *)sptr);
    return sbuf;
  }
}

UString::UString(unsigned nchars)
// Class constructor used to allocate a specified
// number of charaters for a string without setting
// the string value inside this object. NOTE: This 
// function will account for a single null terminator 
// in addition to the number of characters specified.
{
  reset_all(); // Reset all the string class members
  sptr = new char[nchars+1];
  if(sptr) {
    memset(sptr, 0, (nchars+1)); // Zero out the memory
    s_length = 0; // No string has been set at this point
    d_length = nchars;
  }
}

UString::UString(const gxwchar_t *s, unsigned nchars) 
{
  reset_all();
  if(s) { // Check for null pointers
    if(nchars == 0) nchars = gxstrlen(s);
    char *nsptr = gxwchar_t2ASCII(s, nchars);
    SetSPrt(nsptr, nchars, nchars);
  }
}

UString &UString::operator=(const gxwchar_t *s)
{
  Clear(); 
  if(s) { // Check for null pointers
    unsigned nchars = gxstrlen(s);
    char *nsptr = gxwchar_t2ASCII(s, nchars);
    SetSPrt(nsptr, nchars, nchars);
  }
  return *this; 
}

UString &UString::operator=(const char *s) 
{ 
  Clear(); 
  SetString(s);
  return *this; 
}  

UString &UString::operator=(const wchar_t *s) 
{ 
  Clear(); 
  SetString(s);
  return *this; 
}
  
UString &UString::operator=(const UString &s) 
{ 
  if(this != &s) { // Prevent self assignment 
   Clear(); 
   output_hex = s.output_hex ;   
   output_pre = s.output_pre;   
   output_width = s.output_width;
   output_fill = s.output_fill;
   SetString((char *)s.sptr, s.s_length);
  }
  return *this; 
}  

int UString::is_unicode() const
// ASCII string type derived class interface.
{
  return 0; // This is not a unicode type
}

UString *UString::strdup()
// Returns a duplicate string object or a null value if an error
// occurs.
{
  return (UString *)new UString((char*)sptr);
}

UString *UString::strdup() const
// Returns a duplicate string object or a null value if an error
// occurs.
{
  return (UString *)new UString((char*)sptr);
}

char &UString::operator[](unsigned i)
{
  // Check for null pointers
  if(!sptr) return UStringNULLPtr::UStringNUllChar;
  if(i >= s_length) i = s_length; // If not in bounds truncate to s_length
  char *p = (char *)sptr;
  return p[i];
}

char &UString::operator[](unsigned i) const
{
  // Check for null pointers
  if(!sptr) return UStringNULLPtr::UStringNUllChar;
  if(i >= s_length) i = s_length; // If not in bounds truncate to s_length
  char *p = (char *)sptr;
  return p[i];
}

char *UString::c_str() 
{ 
  // Check for null pointers
  if(!sptr) return UStringNULLPtr::UStringNUllStr;
  return (char *)sptr; 
}

const char *UString::c_str() const 
{ 
  // Check for null pointers
  if(!sptr) return UStringNULLPtr::UStringNUllStr;
  return (const char *)sptr; 
} 

wchar_t *UStringw::c_wstr() 
{ 
  if(!sptr) return UStringNULLPtr::UStringwNUllStr;
  return (wchar_t *)sptr; 
}
 
const wchar_t *UStringw::c_wstr() const 
{ 
  if(!sptr) return UStringNULLPtr::UStringwNUllStr;
  return (const wchar_t *)sptr; 
} 

UStringw::UStringw(unsigned nchars)
// Class constructor used to allocate a specified
// number of charaters for a string without setting
// the string value inside this object. NOTE: This 
// function will account for a single null terminator 
// in addition to the number of characters specified.
{
  reset_all(); // Reset all the string class members
  unsigned cs = sizeof(wchar_t);
  unsigned bytes = nchars * cs;
  sptr = new wchar_t[nchars+1];
  if(sptr) {
    memset(sptr, 0, (bytes+cs)); // Zero out the memory
    s_length = 0; // No string has been set at this point
    d_length = bytes;
  }
}

UStringw &UStringw::operator=(const char *s) 
{ 
  Clear(); 
  SetString(s);
  return *this; 
}  

UStringw &UStringw::operator=(const wchar_t *s) 
{ 
  Clear(); 
  SetString(s);
  return *this; 
}
  
UStringw::UStringw(const gxwchar_t *s, unsigned nchars) 
{
  reset_all();
  if(s) { // Check for null pointers
    if(nchars == 0) nchars = gxstrlen(s);
    wchar_t *nsptr = gxwchar_t2Unicode(s, nchars);
    SetSPrt(nsptr, nchars, (nchars*sizeof(wchar_t)));
  }
}

UStringw &UStringw::operator=(const gxwchar_t *s)
{
  Clear(); 
  if(s) { // Check for null pointers
    unsigned nchars = gxstrlen(s);
    wchar_t *nsptr = gxwchar_t2Unicode(s, nchars);
    SetSPrt(nsptr, nchars, (nchars*sizeof(wchar_t)));
  }
  return *this; 
}

UStringw &UStringw::operator=(const UStringw &s) 
{ 
  if(this != &s) { // Prevent self assignment 
    Clear(); 
    output_hex = s.output_hex ;   
    output_pre = s.output_pre;   
    output_width = s.output_width;
    output_fill = s.output_fill;
    SetString((wchar_t *)s.sptr, s.s_length);
  }
  return *this; 
}  

UStringw *UStringw::strdup()
// Returns a duplicate string object or a null value if an error
// occurs.
{
  return (UStringw *)new UStringw((wchar_t*)sptr);
}

UStringw *UStringw::strdup() const
// Returns a duplicate string object or a null value if an error
// occurs.
{
  return (UStringw *)new UStringw((wchar_t*)sptr);
}

wchar_t &UStringw::operator[](unsigned i)
{
  // Check for null pointers
  if(!sptr) return UStringNULLPtr::UStringwNUllChar;
  if(i >= s_length) i = s_length; // If not in bounds truncate to s_length
  wchar_t *p = (wchar_t *)sptr;
  return p[i];
}

wchar_t &UStringw::operator[](unsigned i) const
{
  // Check for null pointers
  if(!sptr) return UStringNULLPtr::UStringwNUllChar;
  if(i >= s_length) i = s_length; // If not in bounds truncate to s_length
  wchar_t *p = (wchar_t *)sptr;
  return p[i];
}

int UStringw::is_unicode() const
// Unicode string type derived class interface.
{
  return 1; // This is a Unicode type
}

GXDLCODE_API int CaseICmp(const UString &s1, const UString &s2)
{
  return s1.StringICompare(s2);
}

GXDLCODE_API int CaseICmp(const UString &s1, const char *s)
{
  return s1.StringICompare(s);
}

GXDLCODE_API int CaseICmp(const UString &s1, const wchar_t *s)
{
  return s1.StringICompare(s);
}

GXDLCODE_API int CaseICmp(const char *s, const UString &s2)
{
  return s2.StringICompare(s);
}

GXDLCODE_API int CaseICmp(const wchar_t *s, const UString &s2)
{
  return s2.StringICompare(s);
}

GXDLCODE_API int CaseICmp(const UStringw &s1, const UStringw &s2)
{
  return s1.StringICompare(s2);
}

GXDLCODE_API int CaseICmp(const UStringw &s1, const char *s)
{
  return s1.StringICompare(s);
}

GXDLCODE_API int CaseICmp(const UStringw &s1, const wchar_t *s)
{
  return s1.StringICompare(s);
}

GXDLCODE_API int CaseICmp(const char *s, const UStringw &s2)
{
  return s2.StringICompare(s);
}

GXDLCODE_API int CaseICmp(const wchar_t *s, const UStringw &s2)
{
  return s2.StringICompare(s);
}

#if defined (__USE_CPP_IOSTREAM__)
GXDLCODE_API GXSTD::ostream &operator<<(GXSTD::ostream &os, const UString &s)
{
  if(!s) return os; // Check for null pointers
  return os.write((char *)s.GetSPtr(), s.length());
}

GXDLCODE_API GXSTD::ostream &operator<<(GXSTD::ostream &os, const UStringw &s)
{
  if(!s) return os; // Check for null pointers
  char *nsptr = Unicode2ASCII((wchar_t *)s.GetSPtr(), s.length());
  os.write(nsptr, s.length());
  delete[] nsptr;
  return os;
}

GXDLCODE_API GXSTD::istream &operator>>(GXSTD::istream &os, UString &s)
{
  const int ibuf_size = 255;
  char *nsptr = new char[ibuf_size];
  if(!nsptr) {
    s.SetString((const char *)"\0", 1);
    return os;
  }
  os >> GXSTD::setw(ibuf_size) >> nsptr;
  s.SetString(nsptr, gxstrlen(nsptr));
  delete[] nsptr;
  return os;
}

GXDLCODE_API GXSTD::istream &operator>>(GXSTD::istream &os, UStringw &s)
{
  const int ibuf_size = 255;
  char *nsptr = new char[ibuf_size];
  if(!nsptr) {
    s.SetString((const char *)"\0", 1);
    return os;
  }
  os >> GXSTD::setw(ibuf_size) >> nsptr;
  s.SetString(nsptr, gxstrlen(nsptr));
  delete[] nsptr;
  return os;
}
#endif // __USE_CPP_IOSTREAM__

GXDLCODE_API int StringCompare(const UString &a, const UString &b) 
{
  return a.StringCompare(b);
}

GXDLCODE_API int StringCompare(const char *s, const UString &b) 
{
  UString a(s);
  return a.StringCompare(b);
}

GXDLCODE_API int StringCompare(const UString &a, const char *s) 
{
  return a.StringCompare(s);
}

GXDLCODE_API int StringCompare(const wchar_t *s, const UString &b) 
{
  UString a(s);
  return a.StringCompare(b);
}

GXDLCODE_API int StringCompare(const UString &a, const wchar_t *s) 
{
  return a.StringCompare(s);
}

GXDLCODE_API UString operator+(const UString &a, const UString &b) 
{
  UString buf(a);
  buf += b;
  return buf;
}

GXDLCODE_API UString operator+(const char *s, const UString &b) 
{
  UString buf(s);
  buf += b; 
  return buf;
}
 
GXDLCODE_API UString operator+(const UString &a, const char *s) 
{
  UString buf(a);
  buf.Cat(s); // Append "s" to the end of the object
  return buf;
}

GXDLCODE_API UString operator+(const wchar_t *s, const UString &b) 
{
  UString buf(s);
  buf += b; 
  return buf;
}

GXDLCODE_API UString operator+(const UString &a, const wchar_t *s) 
{
  UString buf(a);
  buf.Cat(s); // Append "s" to the end of the object
  return buf;
}

GXDLCODE_API int operator==(const UString &a, const UString &b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const char *a, const UString &b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const UString &a, const char *b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const wchar_t *a, const UString &b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const UString &a, const wchar_t *b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const UString &a, const UString &b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const char *a, const UString &b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const UString &a, const char *b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const wchar_t *a, const UString &b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const UString &a, const wchar_t *b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator>(const UString &a, const UString &b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const char *a , const UString &b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const UString &a, const char *b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const wchar_t *a , const UString &b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const UString &a, const wchar_t *b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>=(const UString &a, const UString &b) 
{
  return StringCompare(a, b) >= 0;
}
 
GXDLCODE_API int operator>=(const char *a, const UString &b) 
{
  return StringCompare(a, b) >= 0;
}

GXDLCODE_API int operator>=(const UString &a, const char *b) 
{
  return StringCompare(a, b) >= 0;
}

GXDLCODE_API int operator>=(const wchar_t *a, const UString &b) 
{
  return StringCompare(a, b) >= 0;
}

GXDLCODE_API int operator>=(const UString &a, const wchar_t *b) 
{
  return StringCompare(a, b) >= 0;
}

GXDLCODE_API int operator<(const UString &a, const UString &b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const char *a, const UString &b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const UString &a, const char *b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const wchar_t *a, const UString &b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const UString &a, const wchar_t *b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<=(const UString &a, const UString &b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const char *a, const UString &b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const UString &a, const char *b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const wchar_t *a, const UString &b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const UString &a, const wchar_t *b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int StringCompare(const UStringw &a, const UStringw &b) 
{
  return a.StringCompare(b);
}

GXDLCODE_API int StringCompare(const char *s, const UStringw &b) 
{
  UStringw a(s);
  return a.StringCompare(b);
}

GXDLCODE_API int StringCompare(const UStringw &a, const char *s) 
{
  return a.StringCompare(s);
}

GXDLCODE_API int StringCompare(const wchar_t *s, const UStringw &b) 
{
  UStringw a(s);
  return a.StringCompare(b);
}

GXDLCODE_API int StringCompare(const UStringw &a, const wchar_t *s) 
{
  return a.StringCompare(s);
}

GXDLCODE_API UStringw operator+(const UStringw &a, const UStringw &b) 
{
  UStringw buf(a);
  buf += b;
  return buf;
}

GXDLCODE_API UStringw operator+(const char *s, const UStringw &b) 
{
  UStringw buf(s);
  buf += b; 
  return buf;
}
 
GXDLCODE_API UStringw operator+(const UStringw &a, const char *s) 
{
  UStringw buf(a);
  buf.Cat(s); // Append "s" to the end of the object
  return buf;
}

GXDLCODE_API UStringw operator+(const wchar_t *s, const UStringw &b) 
{
  UStringw buf(s);
  buf += b; 
  return buf;
}

GXDLCODE_API UStringw operator+(const UStringw &a, const wchar_t *s) 
{
  UStringw buf(a);
  buf.Cat(s); // Append "s" to the end of the object
  return buf;
}

GXDLCODE_API int operator==(const UStringw &a, const UStringw &b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const char *a, const UStringw &b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const UStringw &a, const char *b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const wchar_t *a, const UStringw &b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator==(const UStringw &a, const wchar_t *b) 
{
  return StringCompare(a, b) == 0;
}

GXDLCODE_API int operator!=(const UStringw &a, const UStringw &b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const char *a, const UStringw &b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const UStringw &a, const char *b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const wchar_t *a, const UStringw &b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator!=(const UStringw &a, const wchar_t *b) 
{
  return StringCompare(a, b) != 0;
}

GXDLCODE_API int operator>(const UStringw &a, const UStringw &b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const char *a , const UStringw &b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const UStringw &a, const char *b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const wchar_t *a , const UStringw &b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>(const UStringw &a, const wchar_t *b) 
{
  return StringCompare(a, b) > 0;
}

GXDLCODE_API int operator>=(const UStringw &a, const UStringw &b) 
{
  return StringCompare(a, b) >= 0;
}
 
GXDLCODE_API int operator>=(const char *a, const UStringw &b) 
{
  return StringCompare(a, b) >= 0;
}

GXDLCODE_API int operator>=(const UStringw &a, const char *b) 
{
  return StringCompare(a, b) >= 0;
}

GXDLCODE_API int operator>=(const wchar_t *a, const UStringw &b) 
{
  return StringCompare(a, b) >= 0;
}

GXDLCODE_API int operator>=(const UStringw &a, const wchar_t *b) 
{
  return StringCompare(a, b) >= 0; 
}

GXDLCODE_API int operator<(const UStringw &a, const UStringw &b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const char *a, const UStringw &b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const UStringw &a, const char *b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const wchar_t *a, const UStringw &b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<(const UStringw &a, const wchar_t *b) 
{
  return StringCompare(a, b) < 0; 
}

GXDLCODE_API int operator<=(const UStringw &a, const UStringw &b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const char *a, const UStringw &b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const UStringw &a, const char *b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const wchar_t *a, const UStringw &b) 
{
  return StringCompare(a, b) <= 0; 
}

GXDLCODE_API int operator<=(const UStringw &a, const wchar_t *b) 
{
  return StringCompare(a, b) <= 0; 
}

#if defined (__wxWIN201__) 
UString &UString::operator=(const wxString &s) 
{ 
  Clear(); 
  SetString(s.c_str());
  return *this; 
}

UStringw &UStringw::operator=(const wxString &s) 
{ 
  Clear(); 
  SetString(s.c_str());
  return *this; 
}

UStringB& UStringB::operator>>(wxString &s)
{
  if(!sptr) {
    s = UStringNULLPtr::UStringNUllStr; 
    return *this;
  }

  if(!is_unicode()) {
    s = (char *)sptr;
  }
  else {
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    if(!nsptr) return *this;
    s = nsptr;
    delete[] nsptr;
  }
  return *this;
}
#endif

#if defined (__wxWIN291__) 
UString &UString::operator=(const wxString &s) 
{ 
  Clear(); 
  SetString((const char *)s.c_str());
  return *this; 
}

UStringw &UStringw::operator=(const wxString &s) 
{ 
  Clear(); 
  SetString((const char *)s.c_str());
  return *this; 
}

UStringB& UStringB::operator>>(wxString &s)
{
  if(!sptr) {
    s = UStringNULLPtr::UStringNUllStr; 
    return *this;
  }

  if(!is_unicode()) {
    s = (char *)sptr;
  }
  else {
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    if(!nsptr) return *this;
    s = nsptr;
    delete[] nsptr;
  }
  return *this;
}
#endif

#if defined (__wxWIN302__) 
UString &UString::operator=(const wxString &s) 
{ 
  Clear(); 
  SetString((const char *)s.c_str());
  return *this; 
}

UStringw &UStringw::operator=(const wxString &s) 
{ 
  Clear(); 
  SetString((const char *)s.c_str());
  return *this; 
}

UStringB& UStringB::operator>>(wxString &s)
{
  if(!sptr) {
    s = UStringNULLPtr::UStringNUllStr; 
    return *this;
  }

  if(!is_unicode()) {
    s = (char *)sptr;
  }
  else {
    char *nsptr = Unicode2ASCII((wchar_t *)sptr, s_length);
    if(!nsptr) return *this;
    s = nsptr;
    delete[] nsptr;
  }
  return *this;
}
#endif

void UStringB::WholeNumber(unsigned long bytes)
// Public member function used to insert a comma 
// separated whole number into a string.
{
  char sbuf[81];
  sprintf(sbuf, "%u", (unsigned int)bytes);
  unsigned start_len = length();
  *this += sbuf;
  unsigned stop_len = length();
  unsigned slen = stop_len - start_len;
  stop_len -= slen;
  unsigned offset = length();
  unsigned num_commas = 0;
  if(slen > 3) num_commas = slen/3;
  for(unsigned i = 0; i < num_commas; i ++) {
    offset -= 3;
    if(offset == stop_len) break;
    InsertAt(offset, ",", 1);
  }
}

void UStringB::SHN(unsigned long bytes)
{
  double shn_n; // Shorthand notation number
  char shn_c[2] = { '\0', '\0' }; // Shorthand notation nomenclature
  char sbuf[81];  
  int i;
  if(bytes > 1000) { // More than 1K bytes
    if(bytes < (1000*1000)) { // Less than 1M
      shn_n = (double)bytes/double(1000);
      shn_c[0] = 'K';
    }
    else if(bytes < ((1000*1000)*1000)) { // Less than 1G 
      shn_n = (double)bytes/double(1000*1000);
      shn_c[0] = 'M';
    }
    else { // 1G or greater
      shn_n = (double)bytes/double((1000*1000)*1000);
      shn_c[0] = 'G';
    }
  }
  else {
    sprintf(sbuf, "%u", (unsigned int)bytes);
    if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
      i = output_width - strlen(sbuf);
      while(i--) *this += output_fill;
    }
    *(this) += sbuf;
    return;
  }
  sprintf(sbuf, "%.2f", shn_n);

  // Append the string
  i = strlen(sbuf);
  while(i--) {
    if((sbuf[i] == '0') || (sbuf[i] == '.')) {
      // Remove all trailing zeros and trailing dots
      sbuf[i] = 0;
    }
    else {
      // Break on the first non zero or non dot
      break;
    }
  }
  if((output_width > 0) && ((int)strlen(sbuf) < output_width)) {
    i = (output_width-1) - strlen(sbuf);
    while(i--) *this += output_fill;
  }
  *(this) += sbuf; 
  *(this) += shn_c;
}

unsigned UStringB::TrimLeading(const char c) 
// Filter all leading characters from a string.
// Returns the number of characters filtered.

{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  unsigned i, num_l_chars = 0;

  if(!is_unicode()) {
    char *start = (char *)sptr;

    for(i = 0; i < s_length; i++) {
      if(start[i] != c) break;
      num_l_chars++;
    }
    if(num_l_chars > 0) DeleteAt(0, num_l_chars);
    return num_l_chars;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;

    for(i = 0; i < s_length; i++) {
#if defined (__PCLINT_CHECK__)
      if(start[i] != c) break;
#else
      if(start[i] != (wchar_t)c) break;
#endif
      num_l_chars++;
    }
    if(num_l_chars > 0) DeleteAt(0, num_l_chars);
    return num_l_chars;
  }
}

unsigned UStringB::TrimTrailing(const char c)
// Filter all trailing characters from a string.
// Returns the number of characters filtered.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  if(!is_unicode()) {
    char *start = (char *)sptr;
    unsigned i, num_t_chars = 0;
  
    for(i = (s_length - 1); i != 0; i--) {
      if(start[i] != c) break;
      num_t_chars++;
    }
    if(num_t_chars > 0) DeleteAt((s_length - num_t_chars), num_t_chars);
    return num_t_chars;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;
    unsigned i, num_t_chars = 0;
  
    for(i = (s_length - 1); i != 0; i--) {
#if defined (__PCLINT_CHECK__) 
      if(start[i] != c) break;
#else
      if(start[i] != (wchar_t)c) break;
#endif
      num_t_chars++;
    }
    if(num_t_chars > 0) DeleteAt((s_length - num_t_chars), num_t_chars);
    return num_t_chars;
  }
}

unsigned UStringB::TrimLeading(const wchar_t c) 
// Filter all leading characters from a string.
// Returns the number of characters filtered.

{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  unsigned i, num_l_chars = 0;

  if(!is_unicode()) {
    char *start = (char *)sptr;

    for(i = 0; i < s_length; i++) {
      if(start[i] != (char)c) break;
      num_l_chars++;
    }
    if(num_l_chars > 0) DeleteAt(0, num_l_chars);
    return num_l_chars;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;

    for(i = 0; i < s_length; i++) {
      if(start[i] != c) break;
      num_l_chars++;
    }
    if(num_l_chars > 0) DeleteAt(0, num_l_chars);
    return num_l_chars;
  }
}

unsigned UStringB::TrimTrailing(const wchar_t c)
// Filter all trailing characters from a string.
// Returns the number of characters filtered.
{
  if((sptr == 0) || (s_length == 0)) return 0; // Check for null pointers
  if(!is_unicode()) {
    char *start = (char *)sptr;
    unsigned i, num_t_chars = 0;
  
    for(i = (s_length - 1); i != 0; i--) {
      if(start[i] != (char )c) break;
      num_t_chars++;
    }
    if(num_t_chars > 0) DeleteAt((s_length - num_t_chars), num_t_chars);
    return num_t_chars;
  }
  else {
    wchar_t *start = (wchar_t *)sptr;
    unsigned i, num_t_chars = 0;
  
    for(i = (s_length - 1); i != 0; i--) {
      if(start[i] != c) break;
      num_t_chars++;
    }
    if(num_t_chars > 0) DeleteAt((s_length - num_t_chars), num_t_chars);
    return num_t_chars;
  }
}

UString::UString(const UString &s, unsigned c_position, unsigned nchars)
// A constructor used to create a subset of this string.
{
  reset_all();

  // Stay inbounds
  if(c_position > (s.s_length-1)) c_position = s.s_length - 1;
  if((nchars + c_position) > s.s_length) nchars = s.s_length - c_position;

  // Set the substring
  SetString(((char *)s.sptr+c_position), nchars);
}

UString UString::Mid(unsigned c_position, unsigned nchars) const
// Returns substring starting at a specified character position,
// for a specified number of characters.
{
  UString buf(*this, c_position, nchars);
  return buf;
}

UString UString::Left(unsigned nchars) const
// Returns left substring for a specified number of characters
{
  UString buf(*this, 0, nchars);
  return buf;
}

UString UString::Right(unsigned nchars) const
// Returns right substring for a specified number of characters
{
  // Stay inbounds
  if(nchars > s_length) nchars = s_length;
  UString buf(*this, (s_length - nchars), nchars);
  return buf;
}

UStringw::UStringw(const UStringw &s, unsigned c_position, unsigned nchars)
// A constructor used to create a subset of this string.
{
  reset_all();

  // Stay inbounds
  if(c_position > (s.s_length-1)) c_position = s.s_length - 1;
  if((nchars + c_position) > s.s_length) nchars = s.s_length - c_position;

  // Set the substring
  SetString(((wchar_t *)s.sptr+c_position), nchars);
}

UStringw UStringw::Mid(unsigned c_position, unsigned nchars) const
// Returns substring starting at a specified character position,
// for a specified number of characters.
{
  UStringw buf(*this, c_position, nchars);
  return buf;
}

UStringw UStringw::Left(unsigned nchars) const
// Returns left substring for a specified number of characters
{
  UStringw buf(*this, 0, nchars);
  return buf;
}

UStringw UStringw::Right(unsigned nchars) const
// Returns right substring for a specified number of characters
{
  // Stay inbounds
  if(nchars > s_length) nchars = s_length;
  UStringw buf(*this, (s_length - nchars), nchars);
  return buf;
}

GXDLCODE_API int FindMatch(const UString &str, const UString &s, 
			   int compare_case)
// Standalone function used to find matching string elements separated by 
// a single "*" character placed at the beginning, middle, or end of a string. 
// Returns true if a match is found or false if no matching element is found
{
  if(s == "*") return 1; // Everything matches

  UString pattern, p1, p2, s1, s2;
  int rv = 0;
  if((s[0] == '*') && (s[s.length()-1] == '*')) {
    // Find everything in the middle of the string
    pattern = s;
    pattern.FilterChar('*');
    if(compare_case) {
      if(str.Find(pattern) != -1) rv = 1;
    }
    else {
      if(str.IFind(pattern) != -1) rv = 1;
    }
    return rv;
  }

  int offset = s.Find("*");
  if(offset == -1) {
    // No wildcard, so search for the entire string
    if(compare_case) {
      return str == s;
    }
    else { // Do not compare the string case
      s1 = str; s2 = s;
      s1.ToLower(); s2.ToLower();
      return s1 == s2;
    }
  }
  else {
    pattern = s;
    pattern.FilterChar('*');
    if(offset == 0) {
      // Wild card is at the beginning of the string
      s1 = str.Right(pattern.length());
      if(!compare_case) {
	s1.ToLower(); 
	pattern.ToLower();
      }
      return s1 == pattern;
    }
    else if(offset > 0) {
      if(offset == int(s.length()-1)) { 
	// Wildcard is at the end of the string
	s1 = str.Left(pattern.length());
	if(!compare_case) {
	  s1.ToLower(); 
	  pattern.ToLower();
	}
	return s1 == pattern;
      }
      else {
	// Wildcard is somewhere in the middle of the string
	p1 = pattern.Left(offset); 
	p2 = pattern.Right(pattern.length()-offset);
	s1 = str.Left(offset); 
	s2 = str.Right(pattern.length()-offset);
	if(!compare_case) {
	  p1.ToLower(); p2.ToLower();
	  s1.ToLower(); s2.ToLower();
	}
	return ((p1 == s1) && (p2 == s2));
      }
    }
  }
  return 0;
}

GXDLCODE_API int FindMatch(const char *str, const char *s, 
			   int compare_case)
// Standalone function used to find matching string elements separated by 
// a single "*" character placed at the beginning, middle, or end of a string. 
// Returns true if a match is found or false if no matching element is found
{
  UString s1(str); 
  UString s2(s);
  return FindMatch(s1, s2, compare_case); 
}

GXDLCODE_API UString *ParseStrings(const UString &input_str, const UString &delimiter, unsigned &num_arr,
				   int trim_spaces, int trim_quotes)
{
  UString *output_arr = 0;
  num_arr = 0;
  
  UString escaped_str = input_str;
  UString escape_sequence;
  escape_sequence << clear << "\\" << delimiter;
  UString replacement = "<<<ESCAPE SEQUENCE /> /> />";
  escaped_str.ReplaceString(escape_sequence, replacement);

  // We have no delimiter
  if(escaped_str.Find(delimiter) == -1) {
    output_arr = new UString[1];
    num_arr = 1;
    output_arr[0] = escaped_str;
    return output_arr;
  }

  int offset = 0;
  while(offset != -1) {
    offset = escaped_str.Find(delimiter, offset);
    if (offset == -1) break;
    num_arr++;
    offset++;
  }
  num_arr++; // Account for the last delimiter
  output_arr = new UString[num_arr];
  
  offset = 0;
  int start = 0;
  int curr_pos = 0;

  while(offset != -1) {
    offset = escaped_str.Find(delimiter, offset);
    if (offset == -1) break;					
    if(offset > start) {
      UString strbuf = escaped_str.Mid(start, (offset-start));
      if(trim_spaces) {
	strbuf.TrimLeadingSpaces();
	strbuf.TrimTrailingSpaces();
      }
      if(trim_quotes) {
	strbuf.TrimLeading('\'');
	strbuf.TrimTrailing('\'');
	strbuf.TrimLeading('\"');
	strbuf.TrimTrailing('\"');
      }
      strbuf.ReplaceString(replacement, delimiter);
      output_arr[curr_pos] = strbuf; 
    }
    curr_pos++;
    offset += delimiter.length();
    start = offset;
  }

  // Account for the last delimiter
  int left_over = escaped_str.length()-start;
  if(left_over > 0) {
    UString strbuf = escaped_str.Mid(start, left_over);
    if(trim_spaces) {
      strbuf.TrimLeadingSpaces();
      strbuf.TrimTrailingSpaces();
    }
    if(trim_quotes) {
      strbuf.TrimLeading('\'');
      strbuf.TrimTrailing('\'');
      strbuf.TrimLeading('\"');
      strbuf.TrimTrailing('\"');
    }
    strbuf.ReplaceString(replacement, delimiter);
    output_arr[num_arr-1] = strbuf; 
  }
  return output_arr;
}

GXDLCODE_API UStringw *ParseStrings(const UStringw &input_str, const UStringw &delimiter, unsigned &num_arr,
				    int trim_spaces, int trim_quotes)
{
  UStringw *output_arr = 0;
  num_arr = 0;
  wchar_t wsbuf[255];
  
  UStringw escaped_str = input_str;
  UStringw escape_sequence;
  escape_sequence << clear << "\\" << delimiter;
  UStringw replacement(gxLTCHAR("<<<ESCAPE SEQUENCE /> /> />", wsbuf));
  escaped_str.ReplaceString(escape_sequence, replacement);

  // We have no delimiter
  if(escaped_str.Find(delimiter) == -1) {
    output_arr = new UStringw[1];
    num_arr = 1;
    output_arr[0] = escaped_str;
    return output_arr;
  }

  int offset = 0;
  while(offset != -1) {
    offset = escaped_str.Find(delimiter, offset);
    if (offset == -1) break;
    num_arr++;
    offset++;
  }
  num_arr++; // Account for the last delimiter
  output_arr = new UStringw[num_arr];
  
  offset = 0;
  int start = 0;
  int curr_pos = 0;

  while(offset != -1) {
    offset = escaped_str.Find(delimiter, offset);
    if (offset == -1) break;					
    if(offset > start) {
      UStringw strbuf = escaped_str.Mid(start, (offset-start));
      if(trim_spaces) {
	strbuf.TrimLeadingSpaces();
	strbuf.TrimTrailingSpaces();
      }
      if(trim_quotes) {
	strbuf.TrimLeading('\'');
	strbuf.TrimTrailing('\'');
	strbuf.TrimLeading('\"');
	strbuf.TrimTrailing('\"');
      }
      strbuf.ReplaceString(replacement, delimiter);
      output_arr[curr_pos] = strbuf; 
    }
    curr_pos++;
    offset += delimiter.length();
    start = offset;
  }

  // Account for the last delimiter
  int left_over = escaped_str.length()-start;
  if(left_over > 0) {
    UStringw strbuf = escaped_str.Mid(start, left_over);
    if(trim_spaces) {
      strbuf.TrimLeadingSpaces();
      strbuf.TrimTrailingSpaces();
    }
    if(trim_quotes) {
      strbuf.TrimLeading('\'');
      strbuf.TrimTrailing('\'');
      strbuf.TrimLeading('\"');
      strbuf.TrimTrailing('\"');
    }
    strbuf.ReplaceString(replacement, delimiter);
    output_arr[num_arr-1] = strbuf; 
  }
  return output_arr;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
