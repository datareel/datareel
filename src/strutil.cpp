// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: strutil.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 12/16/1997 
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

The string utilities are a collection of data structures and 
standalone functions used to manipulate and parse null terminated 
ASCII and Unicode character strings.

Changes:
==============================================================
07/10/2001: Added __AIX__ conditional directive for the IBM xlC 
C and C++ Compilers. This compiler does not define the 
strcasecmp() function.

09/28/2001: Added a string length parameter to the FindMatch()
and IFindMatch() functions used to set the length of the string
variable when the string is not null-terminated.

06/05/2002: Added standalone functions needed to work with both
ASCII and Unicode strings
 
06/05/2002: Added conversion functions used with ASCII and
Unicode string literals.

09/04/2002: Added Unicode versions of all strutil functions
included with release 4.1.

09/04/2002: All versions of the CaseICmp() functions are no 
longer using compiler dependent string case insensitive 
functions.

09/04/2002: All versions of the StringCat() functions now use 
null pointers as the default arguments and will return a null
string if no arguments are passed to the function from the 
caller.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "strutil.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

gxwchar::gxwchar(unsigned long wc)
{
  UnPackBits(wc);
}

gxwchar::gxwchar(const gxwchar& wc)
{
  memmove((void *)byte, (const void *)wc.byte, 4);
}

gxwchar& gxwchar::operator=(const gxwchar& wc)
{
  if(this != &wc) { // Prevent self assignment 
    memmove((void *)byte, (const void *)wc.byte, 4);
  }
  return *this;
}

gxwchar& gxwchar::operator=(const unsigned long wc)
{
  UnPackBits(wc);
  return *this;
}

gxwchar::operator unsigned long() const
{
  return PackBits();
}

unsigned long gxwchar::PackBits() const
{
  unsigned long a, b, c, d;
  
  a = (unsigned long)byte[0];
  b = (unsigned long)byte[1];
  c = (unsigned long)byte[2];
  d = (unsigned long)byte[3];

  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;

  return a + b + c + d;
}

void gxwchar::UnPackBits(unsigned long wc)
{
  byte[0] = (unsigned char)(wc & 0xFF);
  byte[1] = (unsigned char)((wc & 0xFF00)>>8);
  byte[2] = (unsigned char)((wc & 0xFF0000)>>16);
  byte[3] = (unsigned char)((wc & 0xFF000000)>>24);
}

GXDLCODE_API gxwchar_t *gxstrcpy(gxwchar_t *to, const gxwchar_t *from)
// Cross platform gxwchar_t string function used to copy a null terminated 
// gxwchar_t string. Redundantly returns a copy of the string. NOTE: This 
// function assumes that the caller has previously allocated enough memory 
// for the destination string plus room for the string's null terminator. 
{
  unsigned cs = sizeof(gxwchar_t);
  unsigned nchars = gxstrlen(from);

  // Cast to unsigned char * type so that the pointer will be
  // incremented one byte at a time.
  memmove((unsigned char *)to, (unsigned char *)from, (nchars*cs));
  to[nchars] = (gxwchar_t)0; // Null terminate the string
  return to;
}

GXDLCODE_API unsigned gxstrlen(const gxwchar_t *s)
// Cross platform gxwchar_t string function used calculate the string length 
// of a null terminated gxwchar_t string. NOTE: The string length of a string 
// refers to the number of characters in the string and not the number of 
// bytes the string occupies in memory. The number of bytes occupied by a 
// gxwchar_t string can be calculated by multiplying the string length by 
// sizeof(gxwchar_t).
{
  gxwchar_t *p = (gxwchar_t *)s;
  unsigned i = 0;
  while(p[i++] != (gxwchar_t)0) ; 
  return i - 1; // Return the length minus the null terminator
}

GXDLCODE_API gxwchar_t *gxstrcat(gxwchar_t *des, const gxwchar_t *src)
// Cross compiler gxwchar_t string function used to append a null terminated 
// gxwchar_t string. Redundantly returns a copy of the string. NOTE: This 
// function assumes that the caller has previously allocated enough memory 
// for the destination string plus room for the string's null terminator. 
{
  unsigned cs = sizeof(gxwchar_t);
  unsigned nchars_d = gxstrlen(des);
  unsigned nchars_s = gxstrlen(src);

  // Cast to unsigned char * type so that the pointer will be
  // incremented one byte at a time.
  memmove((unsigned char *)des+(nchars_d*cs), (unsigned char *)src, 
	  (nchars_s*cs));
  des[nchars_d+nchars_s] = (gxwchar_t)0; // Null terminate the string
  return des;
}

GXDLCODE_API int gxstrcmp(const gxwchar_t *a, const gxwchar_t *b)
// Cross compiler gxwchar_t string function used to compare two null 
// terminated gxwchar_t strings. Returns -1 if a < b, 0 if a == b, 
// and 1 if a > b
{
  unsigned cs = sizeof(gxwchar_t);
  unsigned nchars_a = gxstrlen(a);
  unsigned nchars_b = gxstrlen(b);
  return gxbyte_compare(a, (nchars_a*cs), b, (nchars_b*cs));
}

GXDLCODE_API wchar_t *gxstrcpy(wchar_t *to, const wchar_t *from)
// Cross compiler Unicode string function used to copy a null terminated 
// Unicode string. Redundantly returns a copy of the string. NOTE: This 
// function assumes that the caller has previously allocated enough memory 
// for the destination string plus room for the string's null terminator. 
{
#if defined (__HAS_UNICODE__)
  return wcscpy(to, from);
#else // Non-compiler specific Unicode string copy function
  unsigned cs = sizeof(wchar_t);
  unsigned nchars = gxstrlen(from);

  // Cast to unsigned char * type so that the pointer will be
  // incremented one byte at a time.
  memmove((unsigned char *)to, (unsigned char *)from, (nchars*cs));
  to[nchars] = (wchar_t)0; // Null terminate the string
  return to;
#endif // __HAS_UNICODE__
}

GXDLCODE_API wchar_t *gxstrcat(wchar_t *des, const wchar_t *src)
// Cross compiler Unicode string function used to append a null terminated 
// Unicode string. Redundantly returns a copy of the string. NOTE: This 
// function assumes that the caller has previously allocated enough memory 
// for the destination string plus room for the string's null terminator. 
{
#if defined (__HAS_UNICODE__)
  return wcscat(des, src);
#else // Non-compiler specific Unicode string cat function
  unsigned cs = sizeof(wchar_t);
  unsigned nchars_d = gxstrlen(des);
  unsigned nchars_s = gxstrlen(src);

  // Cast to unsigned char * type so that the pointer will be
  // incremented one byte at a time.
  memmove((unsigned char *)des+(nchars_d*cs), (unsigned char *)src, 
	  (nchars_s*cs));
  des[nchars_d+nchars_s] = (wchar_t)0; // Null terminate the string
  return des;
#endif // __HAS_UNICODE__
}

GXDLCODE_API unsigned gxstrlen(const wchar_t *s)
// Cross compiler Unicode string function used calculate the string length 
// of a null terminated Unicode string. NOTE: The string length of a string 
// refers to the number of characters in the string and not the number of 
// bytes the string occupies in memory. The number of bytes occupied by a 
// Unicode string can be calculated by multiplying the string length by 
// sizeof(wchar_t).
{
#if defined (__HAS_UNICODE__)
  return wcslen(s);
#else // Non-compiler specific Unicode string function
  wchar_t *p = (wchar_t *)s;
  unsigned i = 0;
  while(p[i++] != (wchar_t)0) ; 
  return i - 1; // Return the length minus the null terminator
#endif // __HAS_UNICODE__
}

GXDLCODE_API int gxstrcmp(const wchar_t *a, const wchar_t *b)
// Cross compiler Unicode string function used to compare two null terminated 
// Unicode strings. Returns -1 if a < b, 0 if a == b, and 1 if a > b
{
#if defined (__HAS_UNICODE__)
  return wcscmp(a, b);
#else // Non-compiler specific Unicode string function
  unsigned cs = sizeof(wchar_t);
  unsigned nchars_a = gxstrlen(a);
  unsigned nchars_b = gxstrlen(b);
  return gxbyte_compare(a, (nchars_a*cs), b, (nchars_b*cs));
#endif // __HAS_UNICODE__
}

GXDLCODE_API int gxbyte_compare(const void *a, unsigned a_bytes, 
				const void *b, unsigned b_bytes)
// Function used to compare two memory buffers byte by byte.
// Returns -1 if a < b, 0 if a == b, and 1 if a > b
{
  unsigned sn = (a_bytes < b_bytes) ? a_bytes : b_bytes;
  char *ap = (char *)a;
  char *bp = (char *)b;
  for(unsigned i = 0; i < sn; i++) {
    if(*ap < *bp) return -1;
    if(*ap++ > *bp++) return 1;
  }
  if(a_bytes == b_bytes) return 0;
  if(a_bytes < b_bytes) return -1;
  return 1;
}

GXDLCODE_API int gxbyte_icompare(const void *a, unsigned a_bytes, 
				 const void *b, unsigned b_bytes,
				 gxStringType st)
// Returns -1 if a < b, 0 if a == b, and 1 if a > b without
// regard for the case of any ASCII letters.
{
  if(st == gxASCII_STRING) {
    unsigned sn = (a_bytes < b_bytes) ? a_bytes : b_bytes;
    char *ap = (char *)a;
    char *bp = (char *)b;
    for(unsigned i = 0; i < sn; i++) {
      if(tolower(*ap) < tolower(*bp)) return -1;
      if(tolower(*ap++) > tolower(*bp++)) return 1;
    }
    if(a_bytes == b_bytes) return 0;
    if(a_bytes < b_bytes) return -1;
    return 1;
  }
  else {
    unsigned cs = sizeof(wchar_t);
    unsigned a_nchars = a_bytes/cs;
    if(a_bytes % cs) a_nchars++;
    unsigned b_nchars = b_bytes/cs;
    if(b_bytes % cs) b_nchars++;
    unsigned sn = (a_nchars < b_nchars) ? a_nchars : b_nchars;
    wchar_t *ap = (wchar_t *)a;
    wchar_t *bp = (wchar_t *)b;
    for(unsigned i = 0; i < sn; i++) {
#if defined (__HAS_UNICODE__)
      if(towlower(ap[i]) < towlower(bp[i])) return -1;
      if(towlower(ap[i]) > towlower(bp[i])) return 1;
#else
      if(tolower(ap[i]) < tolower(bp[i])) return -1;
      if(tolower(ap[i]) > tolower(bp[i])) return 1;
#endif // __HAS_UNICODE__
    }
    if(a_bytes == b_bytes) return 0;
    if(a_bytes < b_bytes) return -1;
    return 1;
  }
}

GXDLCODE_API int gxfind_pattern(const void *src, unsigned s_bytes, 
				const void *p, unsigned p_bytes, 
				unsigned offset, gxStringType st, unsigned cs)
// Returns index of first occurrence of pattern buffer *s   
// Returns -1 if the pattern is not found
{
  if(st == gxASCII_STRING) {
    char *start = (char *)src + offset; // start of string data
    char *next = start; // next string element
    char *pattern = (char *)p; // next pattern element
    unsigned i = offset, j = 1; // String and pattern indexes
    
    while(i < s_bytes && j <= p_bytes) {
      if (*next == *pattern) { // Matching character
	if(j == p_bytes) return (int)i; // Entire match was found
	next++; pattern++; j++;
      }
      else { // Try next spot in string
	i++;
	start++;
	next = start;
	pattern = (char *)p; 
	j = 1;
      }
    }
  }
  else {
    // Calculate the string and pattern indexes
    unsigned i = offset/cs;
    if(offset % cs) i++;
    unsigned nchars_p = p_bytes/cs;
    if(p_bytes % cs) nchars_p++;
    unsigned nchars_s = s_bytes/cs;
    if(s_bytes % cs) nchars_s++;

    unsigned j = 1; 
    wchar_t *start = (wchar_t *)src + i; // start of string data
    wchar_t *next = start; // next string element
    wchar_t *pattern = (wchar_t *)p; // next pattern element
    
    while(i < nchars_s && j <= nchars_p) {
      if(*next == *pattern) { // Matching character
	if(j == nchars_p) return (int)i; // Entire match was found
	next++; pattern++; j++;
      }
      else { // Try next spot in string
	i++;
	start++;
	next = start;
	pattern = (wchar_t *)p; 
	j = 1;
      }
    }
  }

  return (int)-1; // No match was found
}

GXDLCODE_API int gxifind_pattern(const void *src, unsigned s_bytes, 
				 const void *p, unsigned p_bytes, 
				 unsigned offset, gxStringType st, unsigned cs)
// Returns index of first occurrence of pattern buffer *s starting at the
// specified offset using a case insensitive compare. Returns -1
// if the pattern is not found.
{
  if(st == gxASCII_STRING) {
    char *start = (char *)src + offset; // start of string data
    char *next = start;            // next string element
    char *pattern = (char *)p; // next pattern element
    unsigned i = offset, j = 1;    // String and pattern indexes
  
    while(i < s_bytes && j <= p_bytes) {
      if (tolower(*next) == tolower(*pattern)) {
	if(j == p_bytes) return (int)i; // Entire match was found
	next++; pattern++; j++;
      }
      else { // Try next spot in string
	i++;
	start++;
	next = start;
	pattern = (char *)p; 
	j = 1;
      }
    }
  }
  else {
    // Calculate the string and pattern indexes
    unsigned i = offset/cs;
    if(offset % cs) i++;
    unsigned nchars_p = p_bytes/cs;
    if(p_bytes % cs) nchars_p++;
    unsigned nchars_s = s_bytes/cs;
    if(s_bytes % cs) nchars_s++;

    unsigned j = 1; 
    wchar_t *start = (wchar_t *)src + i; // start of string data
    wchar_t *next = start; // next string element
    wchar_t *pattern = (wchar_t *)p; // next pattern element

    while(i < nchars_s && j <= nchars_p) {
#if defined (__HAS_UNICODE__)
      if(towlower(*next) == towlower(*pattern)) {
#else
      if(tolower(*next) == tolower(*pattern)) {
#endif // __HAS_UNICODE__
	if(j == nchars_p) return (int)i; // Entire match was found
	next++; pattern++; j++;
      }
      else { // Try next spot in string
	i++;
	start++;
	next = start;
	pattern = (wchar_t *)p; 
	j = 1;
      }
    }
  }
  return (int)-1; // No match was found
}

GXDLCODE_API wchar_t *ASCII2Unicode(const char *s, unsigned nchars,
				    wchar_t *nsptr)
{
  if(!s) return 0;
  if(nchars == 0) nchars = gxstrlen(s);
  unsigned cs = sizeof(wchar_t);
  if(!nsptr) {
    nsptr = new wchar_t[nchars+1];
    if(!nsptr) return 0;
  }
  memset(nsptr, 0, ((nchars*cs)+cs));
  for(unsigned i = 0; i < nchars; i++) {
    // PC-lint 04/26/2005: Warning suspicious cast
    nsptr[i] = (wchar_t)s[i];
  }
  return nsptr;
}

GXDLCODE_API char *Unicode2ASCII(const wchar_t *s, unsigned nchars, 
				 char *nsptr)
{
  if(!s) return 0;
  if(nchars == 0) nchars = gxstrlen(s);
  if(!nsptr) {
    nsptr = new char[nchars+1];
    if(!nsptr) return 0;
  }
  memset(nsptr, 0, (nchars+1));
  for(unsigned i = 0; i < nchars; i++) nsptr[i] = (char)s[i];
  return nsptr;
}

GXDLCODE_API wchar_t *gxwchar_t2Unicode(const gxwchar_t *s, unsigned nchars,
					wchar_t *nsptr)
{
  if(!s) return 0;
  if(nchars == 0) nchars = gxstrlen(s);
  unsigned cs = sizeof(wchar_t);
  if(!nsptr) {
    nsptr = new wchar_t[nchars+1];
    if(!nsptr) return 0;
  }
  memset(nsptr, 0, ((nchars*cs)+cs));
  for(unsigned i = 0; i < nchars; i++) 
    nsptr[i] = wchar_t((unsigned long)s[i]);
  return nsptr;
}

GXDLCODE_API char *gxwchar_t2ASCII(const gxwchar_t *s, unsigned nchars,
				   char *nsptr)
{
  if(!s) return 0;
  if(nchars == 0) nchars = gxstrlen(s);
  if(!nsptr) {
    nsptr = new char[nchars+1];
    if(!nsptr) return 0;
  }
  memset(nsptr, 0, (nchars+1));
  for(unsigned i = 0; i < nchars; i++)  
    nsptr[i] = char((unsigned long)s[i]);
  return nsptr;
}

GXDLCODE_API gxwchar_t *ASCII2gxwchar_t(const char *s, unsigned nchars,
					gxwchar_t *nsptr)
{
  if(!s) return 0;
  if(nchars == 0) nchars = gxstrlen(s);
  unsigned cs = sizeof(gxwchar_t);
  if(!nsptr) {
    nsptr = new gxwchar_t[nchars+1];
    if(!nsptr) return 0;
  }
  memset(nsptr, 0, ((nchars*cs)+cs));
  for(unsigned i = 0; i < nchars; i++) {
    // PC-lint 04/26/2005: Warning suspicious cast
    nsptr[i] = gxwchar_t((unsigned long)s[i]);
  }
  return nsptr;
}

GXDLCODE_API gxwchar_t *Unicode2gxwchar_t(const wchar_t *s, unsigned nchars,
					  gxwchar_t *nsptr)
{
  if(!s) return 0;
  if(nchars == 0) nchars = gxstrlen(s);
  unsigned cs = sizeof(gxwchar_t);
  if(!nsptr) {
    nsptr = new gxwchar_t[nchars+1];
    if(!nsptr) return 0;
  }
  memset(nsptr, 0, ((nchars*cs)+cs));
  for(unsigned i = 0; i < nchars; i++) 
    nsptr[i] = gxwchar_t((unsigned long)s[i]);
  return nsptr;
}

GXDLCODE_API char *StringCat(const char *s1, const char *s2, const char *s3)
{
  int len = 1; // Reserve one byte for the null terminator
  if(s1) len += gxstrlen(s1);
  if(s2) len += gxstrlen(s2);
  if(s3) len += gxstrlen(s3);
  char *comp = new char[len];
  comp[len-1] = '\0'; // Null terminate the buffer
  if(s1) gxstrcpy(comp, s1);
  if(s2) gxstrcat(comp, s2);
  if(s3) gxstrcat(comp, s3);
  return comp;
}

GXDLCODE_API wchar_t *StringCat(const wchar_t *s1, const wchar_t *s2,
				const wchar_t *s3) 
{
  int len = 1; // Reserve one byte for the null terminator
  if(s1) len += gxstrlen(s1);
  if(s2) len += gxstrlen(s2);
  if(s3) len += gxstrlen(s3);
  wchar_t *comp = new wchar_t[len];
  comp[len-1] = '\0'; // Null terminate the buffer
  if(s1) gxstrcpy(comp, s1);
  if(s2) gxstrcat(comp, s2);
  if(s3) gxstrcat(comp, s3);
  return comp;
}

GXDLCODE_API int parse(const wchar_t *str, 
		       wchar_t words[MAXWORDS][MAXWORDLENGTH],
		       int *numwords, const wchar_t sepchar)
// General purpose ASCII string parser
{
  wchar_t *s = (wchar_t *)str;
  int i = 0;
  wchar_t newword[MAXWORDLENGTH];
  *numwords = 0;
  const char space = ' ';
  const wchar_t wspace = (wchar_t)space;
  const wchar_t wnull = (wchar_t)0;

  while (1) {
    if (*s == wnull) return 0;
    if (*s != wspace) break; 
    s++;
  }
  
  while(1) {
    // Check to see if there is room for another word in the array
    if(*numwords == MAXWORDS) return 1;

    i = 0;
    // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
    while (i < (MAXWORDLENGTH-1)) {
      if(*s == 0 || *s == sepchar) break;
      newword[i] = *s;
      s++;
      i++;
    }
    newword[i] = wnull; // Ensure null termination
    gxstrcpy(words[*numwords], newword);  // Install word into the array 
    (*numwords)++;
    
    if(*s == wnull) break;
    
    s++;
    if(*s == wnull) break;
  }
  return 0;
}

GXDLCODE_API int parse(const char *str, char words[MAXWORDS][MAXWORDLENGTH],
		       int *numwords, const char sepchar)
// General purpose ASCII string parser
{
  char *s = (char *)str;
  int i = 0;
  char newword[MAXWORDLENGTH];
  *numwords = 0;

  while (1) {
    if (*s == '\0') return 0;
    if (*s != ' ') break;
    s++;
  }
  
  while(1) {
    // Check to see if there is room for another word in the array
    if(*numwords == MAXWORDS) return 1;

    i = 0;
    // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
    while (i < (MAXWORDLENGTH-1)) {
      if(*s == 0 || *s == sepchar) break;
      newword[i] = *s;
      s++;
      i++;
    }
    newword[i] = 0; // Ensure null termination
    strcpy (words[*numwords], newword);  // Install word into the array 
    (*numwords)++;
    
    if(*s == 0) break;
    
    s++;
    if(*s == 0) break;
  }
  return 0;
}

GXDLCODE_API int CaseICmp(const wchar_t *a, const wchar_t *b)
// Compare two strings without regard to the case of the letters
{
  unsigned cs = sizeof(wchar_t);
  unsigned nchars_a = gxstrlen(a);
  unsigned nchars_b = gxstrlen(b);
  return gxbyte_icompare(a, (nchars_a*cs), b, (nchars_b*cs), gxUNICODE_STRING);
}

GXDLCODE_API int CaseICmp(const char *a, const char *b)
// Compare two strings without regard to the case of the letters
{
  unsigned nchars_a = gxstrlen(a);
  unsigned nchars_b = gxstrlen(b);
  return gxbyte_icompare(a, nchars_a, b, nchars_b, gxASCII_STRING);
}

GXDLCODE_API int FindMatch(const char *src, const char *p, 
			   unsigned offset, unsigned src_len, unsigned p_len)
// Finds pattern "p" in string "src" starting at specified offset.
// Returns index of first occurrence of matching pattern or -1
// if no match is found. If the "src_len" or "p_len" variables 
// equal zero it's assumed that that "src" and/or "p" are null-
// terminated string(s).
{
  if(src_len == 0) src_len = gxstrlen(src);
  if(p_len == 0) p_len = gxstrlen(p);
  return gxfind_pattern(src, src_len, p, p_len, offset, gxASCII_STRING, 1);
}

GXDLCODE_API int FindMatch(const wchar_t *src, const wchar_t *p, 
			   unsigned offset, unsigned src_len, unsigned p_len)
// Finds pattern "p" in string "src" starting at specified offset.
// Returns index of first occurrence of matching pattern or -1
// if no match is found. If the "src_len" or "p_len" variables 
// equal zero it's assumed that that "src" and/or "p" are null-
// terminated string(s).
{
  if(src_len == 0) src_len = gxstrlen(src);
  if(p_len == 0) p_len = gxstrlen(p);
  return gxfind_pattern(src, src_len, p, p_len, offset, 
			gxUNICODE_STRING, sizeof(wchar_t));
}

GXDLCODE_API int IFindMatch(const char *src, const char *p, 
			   unsigned offset, unsigned src_len, unsigned p_len)
// Finds pattern "p" in string "src" starting at specified offset
// using a case insensitive compare. Returns index of first occurrence 
// of matching pattern or -1 if no match is found. If the "src_len" or 
// "p_len" variables equal zero it's assumed that that "src" and/or "p" 
// are null-terminated string(s).
{
  if(src_len == 0) src_len = gxstrlen(src);
  if(p_len == 0) p_len = gxstrlen(p);
  return gxifind_pattern(src, src_len, p, p_len, offset, gxASCII_STRING, 1);
}

GXDLCODE_API int IFindMatch(const wchar_t *src, const wchar_t *p, 
			   unsigned offset, unsigned src_len, unsigned p_len)
// Finds pattern "p" in string "src" starting at specified offset
// using a case insensitive compare. Returns index of first occurrence 
// of matching pattern or -1 if no match is found. If the "src_len" or 
// "p_len" variables equal zero it's assumed that that "src" and/or "p" 
// are null-terminated string(s).
{
  if(src_len == 0) src_len = gxstrlen(src);
  if(p_len == 0) p_len = gxstrlen(p);
  return gxifind_pattern(src, src_len, p, p_len, offset, 
			 gxUNICODE_STRING, sizeof(wchar_t));
}

GXDLCODE_API int test_for_null(const char *s, unsigned nchars)
// General-purpose function used to test for null a terminator.
// Returns true if a null terminator is found or false if no
// null terminator is found.
{
  char *p = (char *)s;
  for(unsigned i = 0; i < nchars; i++) {
    if(p[i] == 0) return 1;
  }
  return 0;
}

GXDLCODE_API int test_for_null(const gxwchar_t *s, unsigned nchars)
// General-purpose function used to test for null a terminator.
// Returns true if a null terminator is found or false if no
// null terminator is found.
{
  gxwchar_t *p = (gxwchar_t *)s;
  for(unsigned i = 0; i < nchars; i++) {
    if(p[i] == (gxwchar_t)0) return 1;
  }
  return 0;
}

GXDLCODE_API int StringANCompare(const char *a, const char *b, int check_case)
// String compare function used to compare strings with alphanumeric content. 
// If the "check_case" variable is false a case insensitive compare will be 
// preformed on the non-numeric part of string 1 and 2. Returns 0 if the 
// strings are equal, 1 if string 1 is greater than string 2, or -1 if 
// string 1 is less than string 2. 
{
  unsigned long i;
  unsigned long a_bytes = gxstrlen(a); 
  unsigned long b_bytes = gxstrlen(b);
  unsigned long sn = (a_bytes < b_bytes) ? a_bytes : b_bytes;
  char *ap = (char *)a;
  char *bp = (char *)b;
  unsigned long digit_pos1 = 0;
  unsigned long digit_pos2 = 0;
  char *dbuf1 = new char[sn+1]; // Digit buffer 1, collecting digits
  if(!dbuf1) return -1;
  char *dbuf2 = new char[sn+1]; // Digit buffer 2, collecting digits
  if(!dbuf2) {
    // PC-lint 04/26/2005: Prevent memory leak
    delete[] dbuf1;
    return -1;
  }
  memset(dbuf1, 0, (sn+1));
  memset(dbuf2, 0, (sn+1));
  unsigned long ibuf1, ibuf2;

  for(i = 0; i < sn; i++) {
    if((isdigit(*ap)) && (isdigit(*bp))) {
      if(a_bytes == b_bytes) {
	if((!isdigit(*ap+1)) &&	(!isdigit(*bp+1))) {
	  if(*ap < *bp) {
	    // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	    // but dbuf is not a user defined type so there is no destructor
	    // to call for the array members.
	    delete[] dbuf1;
	    delete[] dbuf2;
	    return -1;
	  }
	  if(*ap > *bp) {
	    // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	    // but dbuf is not a user defined type so there is no destructor
	    // to call for the array members.
	    delete[] dbuf1;
	    delete[] dbuf2;
	    return 1;
	  }
	}
      }
      dbuf1[digit_pos1++] = *ap++;
      dbuf2[digit_pos2++] = *bp++;
    }
    else {
      if((dbuf1[0] != 0) && (dbuf2[0] != 0)) {
	if((isdigit(*(ap)))) {
	  dbuf1[digit_pos1] = *((ap));
	}
	if((isdigit(*(bp)))) {
	  dbuf2[digit_pos2] = *((bp));
	}
	ibuf1 = StringToULWORD(dbuf1);
	ibuf2 = StringToULWORD(dbuf2);
	if(ibuf1 < ibuf2) {
	  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	  // but dbuf is not a user defined type so there is no destructor
	  // to call for the array members.
	  delete[] dbuf1;
	  delete[] dbuf2;
	  return -1;
	}
	if(ibuf1 > ibuf2) {
	  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	  // but dbuf is not a user defined type so there is no destructor
	  // to call for the array members.
	  delete[] dbuf1;
	  delete[] dbuf2;
	  return 1;
	}
	memset(dbuf1, 0, (sn+1));
	memset(dbuf2, 0, (sn+1));
	digit_pos1 = digit_pos2 = 0;
      }
      if(check_case) {
	if(*ap < *bp) {
	  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	  // but dbuf is not a user defined type so there is no destructor
	  // to call for the array members.
	  delete[] dbuf1;
	  delete[] dbuf2;
	  return -1;
	}
	if(*ap++ > *bp++) {
	  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	  // but dbuf is not a user defined type so there is no destructor
	  // to call for the array members.
	  delete[] dbuf1;
	  delete[] dbuf2;
	  return 1;
	}
      }
      else {
	if(tolower(*ap) < tolower(*bp)) {
	  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	  // but dbuf is not a user defined type so there is no destructor
	  // to call for the array members.
	  delete[] dbuf1;
	  delete[] dbuf2;
	  return -1;
	}
	if(tolower(*ap++) > tolower(*bp++)) {
	  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
	  // but dbuf is not a user defined type so there is no destructor
	  // to call for the array members.
	  delete[] dbuf1;
	  delete[] dbuf2;
	  return 1;
	}
      }
    }
  }

  // Check for any remaining digits at the end of the string
  if((dbuf1[0] != 0) && (dbuf2[0] != 0)) {
    if(a_bytes > b_bytes) {
      if(isdigit(*(ap))) {
	dbuf1[digit_pos1] = *(ap);
      }
    }
    if(a_bytes < b_bytes) {
      if(isdigit(*(bp))) {
	dbuf2[digit_pos2] = *(bp);
      }
    }
    ibuf1 = StringToULWORD(dbuf1);
    ibuf2 = StringToULWORD(dbuf2);
    if(ibuf1 < ibuf2) {
      // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
      // but dbuf is not a user defined type so there is no destructor
      // to call for the array members.
      delete[] dbuf1;
      delete[] dbuf2;
      return -1;
    }
    if(ibuf1 > ibuf2) {
      // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
      // but dbuf is not a user defined type so there is no destructor
      // to call for the array members.
      delete[] dbuf1;
      delete[] dbuf2;
      return 1;
    }
  }

  // Tested all digits and letters so return the final result
  // based on the string lengths.
  if(a_bytes < b_bytes) {
    // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
    // but dbuf is not a user defined type so there is no destructor
    // to call for the array members.
    delete[] dbuf1;
    delete[] dbuf2;
    return -1;
  }
  if(a_bytes == b_bytes) {
    // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
    // but dbuf is not a user defined type so there is no destructor
    // to call for the array members.
    delete[] dbuf1;
    delete[] dbuf2;
    return 0;
  }
  
  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
  // but dbuf is not a user defined type so there is no destructor
  // to call for the array members.
  delete[] dbuf1;
  delete[] dbuf2;
  return 1;
}

GXDLCODE_API unsigned long StringToULWORD(const char *str)
{
  char *s = (char *)str;

  while(*s == ' ') ++s;

  int c = (int)(unsigned char)*s++;
  if(c == '-' || c == '+') c = (int)(unsigned char)*s++;
  
  unsigned long value = (unsigned long)0;

  while(isdigit(c)) {
    value = 10 * value + (c - '0'); 
    c = (int)(unsigned char)*s++; 
  }
  return value;  
}

GXDLCODE_API unsigned long StringToULWORD(const wchar_t *str)
{
  unsigned long i = (unsigned long)0;
  char *asbuf = Unicode2ASCII(str);
  if(!asbuf) return i;
  i = StringToULWORD(asbuf);
  delete asbuf;
  return i;
}

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
GXDLCODE_API wchar_t *ULLWORDToString(wchar_t *sbuf, __ULLWORD__ val, 
				      int radix)
// Returns a null terminated string representing the specified
// 64-bit value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  char asbuf[255];
  ULLWORDToString(asbuf, val, radix);
  wchar_t *wsbuf = ASCII2Unicode(asbuf);
  gxstrcpy(sbuf, wsbuf);
  delete wsbuf;
  return sbuf;
}

GXDLCODE_API char *ULLWORDToString(char *sbuf, __ULLWORD__ val, int radix)
// Returns a null terminated string representing the specified
// 64-bit value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  
  char cbuf;     
  unsigned value;
  char *s = sbuf;
  char *digit = s; 
  
  do {
    // PC-lint 04/26/2005: Warning signed-unsigned mix with divide
    value = (unsigned)(val % radix);
    val /= radix; 
    
    if(value > 9)
      *s++ = (char)(value - 10 + 'A');
    else
      *s++ = (char)(value + '0'); 
  } while(val > 0);

  *s-- = '\0'; 

  do {
    cbuf = *s;
    *s = *digit;
    *digit = cbuf;   
    --s;
    ++digit;         
  } while(digit < s);

  return sbuf;
}

GXDLCODE_API wchar_t *LLWORDToString(wchar_t *sbuf, __ULLWORD__ val, 
				     int radix)
// Returns a null terminated string representing the specified
// 64-bit value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  char asbuf[255];
  LLWORDToString(asbuf, val, radix);
  wchar_t *wsbuf = ASCII2Unicode(asbuf);
  gxstrcpy(sbuf, wsbuf);
  delete wsbuf;
  return sbuf;
}

GXDLCODE_API char *LLWORDToString(char *sbuf, __ULLWORD__ val, int radix)
// Returns a null terminated string representing the specified
// 64-bit value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  
  char cbuf;     
  unsigned value;
  char *s = sbuf;
  
  if((radix == 10 && (__LLWORD__)val < 0)) {
    *s++ = '-'; 
  }
  
  char *digit = s; 
  
  do {
    // PC-lint 04/26/2005: Warning signed-unsigned mix with divide
    value = (unsigned)(val % radix);
    val /= radix; 
    
    if(value > 9)
      *s++ = (char)(value - 10 + 'A');
    else
      *s++ = (char)(value + '0'); 
  } while(val > 0);

  *s-- = '\0'; 

  do {
    cbuf = *s;
    *s = *digit;
    *digit = cbuf;   
    --s;
    ++digit;         
  } while(digit < s);

  return sbuf;
}

GXDLCODE_API __LLWORD__ StringToLLWORD(const wchar_t *str)
{
  __LLWORD__ i = (__LLWORD__)0;
  char *asbuf = Unicode2ASCII(str);
  if(!asbuf) return i;
  i = StringToLLWORD(asbuf);
  delete asbuf;
  return i;
}

GXDLCODE_API __LLWORD__ StringToLLWORD(const char *str)
{
  char *s = (char *)str;

  while(*s == ' ') ++s;

  int c = (int)(unsigned char)*s++;
  int sign = c; 
  if(c == '-' || c == '+') c = (int)(unsigned char)*s++;
  
  __LLWORD__ value = (__LLWORD__)0;

  while(isdigit(c)) {
    value = 10 * value + (c - '0'); 
    c = (int)(unsigned char)*s++; 
  }

  if(sign == '-') 
    return -value; 
  else
    return value;  
}

GXDLCODE_API __ULLWORD__ StringToULLWORD(const wchar_t *str)
{
  __ULLWORD__ i = (__ULLWORD__)0;
  char *asbuf = Unicode2ASCII(str);
  if(!asbuf) return i;
  i = StringToULLWORD(asbuf);
  delete asbuf;
  return i;
}

GXDLCODE_API __ULLWORD__ StringToULLWORD(const char *str)
{
  char *s = (char *)str;

  while(*s == ' ') ++s;

  int c = (int)(unsigned char)*s++;
  if(c == '-' || c == '+') c = (int)(unsigned char)*s++;
  
  __ULLWORD__ value = (__ULLWORD__)0;

  while(isdigit(c)) {
    value = 10 * value + (c - '0'); 
    c = (int)(unsigned char)*s++; 
  }

  return (__ULLWORD__)value;  
}
#endif

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
