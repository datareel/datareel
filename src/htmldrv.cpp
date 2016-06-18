// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: htmldrv.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/09/1999 
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
  
The HyperTextDrv and HyperText class is used to create HTML
documents. The HyperTextDrv class is a base class that uses
the C++ ostream library to write HTML tags and text to a
specified stream. The HyperText class is used to write
HTML document templates to a specified stream,
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

// This directive must be defined to use the HTML print driver
#if defined (__USE_HTM_PRINTING__)

#include <string.h>
#include <stdio.h>
#include "htmldrv.h"
#include "systime.h"

// Define program constants
const char *DefaultTitle = "HTML Document";

const char *htmColors[NumHTMLColors] = {
  "\"#000000\"", // htmBLACK       
  "\"#000080\"", // htmDARKBLUE    
  "\"#0000ff\"", // htmBLUE        
  "\"#008000\"", // htmGREEN       
  "\"#008080\"", // htmTEAL        
  "\"#00ff00\"", // htmBRIGHTGREEN 
  "\"#00ffff\"", // htmTURQUOISE   
  "\"#800000\"", // htmDARKRED     
  "\"#800080\"", // htmVIOLET      
  "\"#808000\"", // htmDARKYELLOW  
  "\"#808080\"", // htmDARKGRAY    
  "\"#C0C0C0\"", // htmGRAY        
  "\"#ff0000\"", // htmRED         
  "\"#ff00ff\"", // htmPINK        
  "\"#ffff00\"", // htmYELLOW      
  "\"#ffffff\""  // htmWHITE       
};

const char *htmFonts[NumHTMLFonts] = {
  "Arial",        // htmARIAL,
  "Arial Black",  // htmARIALBLACK
  "Arial Narrow", // htmARIALNARROW
  "Courier New"   // htmCOURIER,
};

HyperTextDrv::HyperTextDrv(GXSTD::ostream &s)
{
  stream = &s;
  dec_precision = DefaultPrecision;
  non_breaking_sp = 0; // Do not use non-breaking spaces by default
}

HyperTextDrv::~HyperTextDrv()
{
  // Destructor provided for virtuality
  // PC-lint 09/14/2005: Ignore the warning about the stream member
  // not being freed. The derived class is responsible for releasing 
  // any memory allocated for the stream pointer.
}

void HyperTextDrv::WriteString(const char *s)
// Write a single line of text to the stream, filtering all
// the characters that have special meaning in HTML documents.
{
 char *p = (char *)s;
 unsigned len = strlen(s);

 while(len--) {
   WriteChar((const unsigned char)*p);
   p++;
 }
}

void HyperTextDrv::WriteChar(const unsigned char c) const
// Write a single character to the stream, filtering all the
// characters that have special meaning in HTML documents.
{
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
      int ex_set = c;
      if(ex_set >= 127) { // Fitler the extended ASCII character set
	*(stream) << "&#" << ex_set << ";";
	break;
      }
      *(stream) << c;
      break;
  }
}

void HyperTextDrv::Write(char c)
{
  WriteChar((const unsigned char)c);
}

void HyperTextDrv::Write(const char c) const
{
  WriteChar((const unsigned char)c);
}

void HyperTextDrv::Write(unsigned char c)
{
  WriteChar((const unsigned char)c);
}

void HyperTextDrv::Write(const unsigned char c) const
{
  WriteChar(c);
}

void HyperTextDrv::Write(char *s)
{
  WriteString((const char *)s);
}

void HyperTextDrv::Write(const char *s)
{
  WriteString(s);
}

void HyperTextDrv::Write(unsigned char *s)
{
  WriteString((const char *)s);
}

void HyperTextDrv::Write(const unsigned char *s)
{
  WriteString((const char *)s);
}

void HyperTextDrv::Write(const long val) const
{
  *(stream) << val;
}

void HyperTextDrv::Write(long val)
{
  *(stream) << val;
}

void HyperTextDrv::Write(const unsigned long val) const
{
  *(stream) << val;
}

void HyperTextDrv::Write(unsigned long val)
{
  *(stream) << val;
}

void HyperTextDrv::Write(const int val) const
{
  *(stream) << val;
}

void HyperTextDrv::Write(int val)
{
  *(stream) << val;
}

void HyperTextDrv::Write(const unsigned int val) const
{
  *(stream) << val;
}

void HyperTextDrv::Write(unsigned int val)
{
  *(stream) << val;
}

void HyperTextDrv::Write(double val)
{
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

void HyperTextDrv::Write(const double val) const
{
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

void HyperTextDrv::Write(float val)
{
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

void HyperTextDrv::Write(const float val) const
{
  stream->setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  stream->precision(dec_precision);
  *(stream) << val;
}

GXSTD::ostream& HyperTextDrv::operator<<(GXSTD::ostream & \
					 (*_f)(GXSTD::ostream&))
{
  (*_f)(*(stream));
  return *(stream);
}

HyperTextDrv& HyperTextDrv::operator<<(char *s)
{
  Write(s);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(const char *s)
{
  Write(s);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(unsigned char *s)
{
  Write(s);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(const unsigned char *s)
{
  Write(s);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(char c)
{
  Write(c);
  return *this;
}

const HyperTextDrv& HyperTextDrv::operator<<(const char c) const
{
  Write(c);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(unsigned char c)
{
  Write(c);
  return *this;
}

const HyperTextDrv& HyperTextDrv::operator<<(const unsigned char c) const
{
  Write(c);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(long val)
{
  Write(val);
  return *this;
}

const HyperTextDrv& HyperTextDrv::operator<<(const long val) const
{
  Write(val);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(unsigned long val)
{
  Write(val);
  return *this;
}

const HyperTextDrv& HyperTextDrv::operator<<(const unsigned long val) const
{
  Write(val);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(int val) 
{
  Write(val);
  return *this;
}

const HyperTextDrv& HyperTextDrv::operator<<(const int val) const
{
  Write(val);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(unsigned int val)
{
  Write(val);
  return *this;
}

const HyperTextDrv& HyperTextDrv::operator<<(const unsigned int val) const
{
  Write(val);
  return *this;
}
 
const HyperTextDrv& HyperTextDrv::operator<<(const float val) const
{
  Write(val);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(float val)
{
  Write(val);
  return *this;
}

const HyperTextDrv& HyperTextDrv::operator<<(const double val) const
{
  Write(val);
  return *this;
}

HyperTextDrv& HyperTextDrv::operator<<(double val)
{
  Write(val);
  return *this;
}

void HyperText::Prologue(const char *doc_title)
{
  *(stream) << html << "\n";
  *(stream) << head << "\n";
  if(doc_title)
    *(stream) << title << ' ' << doc_title << ' ' << etitle << "\n";
  else
    *(stream) << title << ' ' << DefaultTitle << ' ' << etitle << "\n";
  *(stream) << ehead << "\n";
}

void HyperText::StartBody(const char *parameters)
{
  if(parameters == 0)
    *(stream) << body;
  else
    BODY(parameters);

  *(stream) << "\n";
}

void HyperText::StartBody(htmCOLORS color)
{
  *(stream) << lt << "BODY BGCOLOR=" << htmColors[color] << gt << "\n";
  *(stream) << "\n";
}


void HyperText::Epilogue()
{
  *(stream) << "\n";
  *(stream) << ebody << "\n";
  *(stream) << ehtml << "\n";
}

void HyperText::DocHeader(const char *doc_title)
{
  if(doc_title)
    *(stream) << h1 << center << doc_title << ecenter << eh1 << "\n";
  else
    *(stream) << h1 << center << DefaultTitle << ecenter << eh1 << "\n";

  *(stream) << hr << "\n";
  *(stream) << "\n";
}

void HyperText::DocTrailer()
{
  *(stream) << "\n" << hr << "\n";
  PAR("ALIGN=\"CENTER\"");
  *(stream) << center << "\n";
  TABLE("BORDER CELLSPACING=1 BORDERCOLOR=\"#000000\"");
  *(stream) << tr;
  TD("VALIGN=\"MIDDLE\"");
  *(stream) << "\n";
  PAR("ALIGN=\"CENTER\"");
  *(stream) << "End Of Document";
  *(stream) << etd << "\n";
  *(stream) << etr << etable << ecenter << epar << "\n";
}

void HyperText::GenTable(int cell_spacing, int cell_padding, int width,
			 int border, htmCOLORS bordercolor)
{
  *(stream) << otable << "BORDER=" << border <<  ' ' << "CELLSPACING=";
  *(stream) << cell_spacing << ' ' << "BORDERCOLOR=" << htmColors[bordercolor];
  *(stream) << ' ' << "CELLPADDING=" << cell_padding << ' ';
  *(stream)  << "WIDTH=\"%" << width << "\"" << ctag;
  *(stream) << "\n";
}

void HyperText::StartTableRow()
{
  *(stream) << tr << "\n"; 
}

void HyperText::TableHeader(char *valign, int colspan, int rowspan, int width)
{
  *(stream) << lt << "TH ALIGN=" << valign << ' ' << "COLSPAN=" << colspan;
  *(stream) << ' ' << "ROWSPAN=" << rowspan << ' ';
  *(stream) << "WIDTH=\"%" << width << "\"" << gt;
}

void HyperText::TableData(char *valign, int colspan, int rowspan, int width)
{
  *(stream) << lt << "TD ALIGN=" << valign << ' ' << "COLSPAN=" << colspan;
  *(stream) << ' ' << "ROWSPAN=" << rowspan << ' ';
  *(stream) << "WIDTH=\"%" << width << "\"" << gt;
}

void HyperText::GetSystemTime(char *s, int full_month_name)
// Function used to create a custom time string. This function
// assumes that enough memory to hold the date/time string has
// been allocated for the "s" buffer. If "s" is a null pointer
// heap memory will be allocated for the string.
{
  if(!s) { // This is a null pointer
    s = new char[255]; // Allocate memory for the string buffer
    if(!s) return;
  } 
  SysTime date;
  strcpy(s, date.GetSystemDateTime(full_month_name));
}

#endif // __GX_HTMLDRV_HPP__ 
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
