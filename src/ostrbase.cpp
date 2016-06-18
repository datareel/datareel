// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: ostrbase.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995 
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program description and details ------------- // 
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
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

// This directive must be defined to use the OutputStreamB class
#if defined (__USE_CPP_IOSTREAM__)

#include "ostrbase.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

OutputStreamB::OutputStreamB()
{
  dec_precision = ostrDefaultPrecision;
  filter_mode = ostrNO_FILTERING;
  non_breaking_sp = 0; // Do not use non-breaking spaces by default
  stream = 0;
}

OutputStreamB::OutputStreamB(GXSTD::ostream &s)
{
  stream = &s;
  dec_precision = ostrDefaultPrecision;
  filter_mode = ostrNO_FILTERING;
  non_breaking_sp = 0; // Do not use non-breaking spaces by default
}

OutputStreamB::~OutputStreamB()
{
  // Destructor provided for virtuality
  // PC-lint 09/14/2005: Ignore the warning about the stream member
  // not being freed. The derived class is responsible for releasing 
  // any memory allocated for the stream pointer.
}

void OutputStreamB::WriteString(const char *s)
// Write a single line of text to the stream.
{
  if(!stream) return;
  
  char *p = (char *)s;
  unsigned len = strlen(s);
  
  while(len--) {
    WriteChar((const unsigned char)*p);
    p++;
  }
}

void OutputStreamB::HTMLFilter(const unsigned char c) const
{
  if(!stream) return;
  int ex_set;
  
  switch(c) {
    case '<' : // Less than sign
      *(stream) << "&lt;"; 
      break;

    case '>': // Greater then sign
      *(stream) << "&gt;";  
      break;

    case '&' : // Ampersand
      *(stream) << "&amp;"; 
      break;

    case ' ' : // Treat spaces as breaking or non-breaking
      if(non_breaking_sp)
	*(stream) << "&nbsp;";
      else
	*(stream) << c;
      break;

    default:  
      ex_set = c;
      if(ex_set >= 127) { // Fitler the extended ASCII character set
	*(stream) << "&#" << ex_set << ";";
	break;
      }
      *(stream) << c;
      break;
  }
}

void OutputStreamB::WriteChar(const unsigned char c) const
// Write a single character to the stream, filtering all the
// characters that have special meaning.
{
  if(!stream) return;
  switch(filter_mode) {
    case ostrNO_FILTERING:  
      *(stream) << c;
      break;

    case ostrUPPERCASE: 
      *(stream) << toupper(c);
      break;

    case ostrLOWERCASE: 
      *(stream) << tolower(c);  
      break;

    case ostrFILTER_EX_ASCII: {
      int ex_set = c;
      if(ex_set >= 127) { // Filter the extended ASCII character set
	break;
      }
      *(stream) << c;
      break;
    }
    
    case ostrEAT_SPACES: 
      if(c == ' ') break;
      *(stream) << c;
      break;

    case ostrHTLM_FILTER:
      HTMLFilter(c);
      break;

    default:  
      *(stream) << c;
      break;
  }
}

void OutputStreamB::Write(char c)
{
  WriteChar((const unsigned char)c);
}

void OutputStreamB::Write(const char c) const
{
  WriteChar((const unsigned char)c);
}

void OutputStreamB::Write(unsigned char c)
{
  WriteChar((const unsigned char)c);
}

void OutputStreamB::Write(const unsigned char c) const
{
  WriteChar(c);
}

void OutputStreamB::Write(char *s)
{
  WriteString((const char *)s);
}

void OutputStreamB::Write(const char *s)
{
  WriteString(s);
}

void OutputStreamB::Write(unsigned char *s)
{
  WriteString((const char *)s);
}

void OutputStreamB::Write(const unsigned char *s)
{
  WriteString((const char *)s);
}

void OutputStreamB::Write(const long val) const
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(long val)
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(const unsigned long val) const
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(unsigned long val)
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(const int val) const
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(int val)
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(const unsigned int val) const
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(unsigned int val)
{
  if(!stream) return;
  *(stream) << val;
}

void OutputStreamB::Write(double val)
{
  if(!stream) return;
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

void OutputStreamB::Write(const double val) const
{
  if(!stream) return;
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

void OutputStreamB::Write(float val)
{
  if(!stream) return;
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

void OutputStreamB::Write(const float val) const
{
  if(!stream) return;
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

GXSTD::ostream& OutputStreamB::operator<<(GXSTD::ostream & \
					  (*_f)(GXSTD::ostream&))
{
  // PC-lint 09/14/2005: Likely use of null pointer. CAUTION: The 
  // manipulator functions can only be used if the stream member
  // has been properly initialized by the OutputStreamB contructor.
  if(!stream) return *(stream); 
  (*_f)(*(stream));
  return *(stream);
}

OutputStreamB& OutputStreamB::operator<<(char *s)
{
  Write(s);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(const char *s)
{
  Write(s);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(unsigned char *s)
{
  Write(s);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(const unsigned char *s)
{
  Write(s);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(char c)
{
  Write(c);
  return *this;
}

const OutputStreamB& OutputStreamB::operator<<(const char c) const
{
  Write(c);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(unsigned char c)
{
  Write(c);
  return *this;
}

const OutputStreamB& OutputStreamB::operator<<(const unsigned char c) const
{
  Write(c);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(long val)
{
  Write(val);
  return *this;
}

const OutputStreamB& OutputStreamB::operator<<(const long val) const
{
  Write(val);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(unsigned long val)
{
  Write(val);
  return *this;
}

const OutputStreamB& OutputStreamB::operator<<(const unsigned long val) const
{
  Write(val);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(int val) 
{
  Write(val);
  return *this;
}

const OutputStreamB& OutputStreamB::operator<<(const int val) const
{
  Write(val);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(unsigned int val)
{
  Write(val);
  return *this;
}

const OutputStreamB& OutputStreamB::operator<<(const unsigned int val) const
{
  Write(val);
  return *this;
}
 
const OutputStreamB& OutputStreamB::operator<<(const float val) const
{
  Write(val);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(float val)
{
  Write(val);
  return *this;
}

const OutputStreamB& OutputStreamB::operator<<(const double val) const
{
  Write(val);
  return *this;
}

OutputStreamB& OutputStreamB::operator<<(double val)
{
  Write(val);
  return *this;
}

#endif // __USE_CPP_IOSTREAM__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
