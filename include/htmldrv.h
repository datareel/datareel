// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: htmldrv.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/09/1999 
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

The HyperTextDrv and HyperText class is used to create HTML
documents. The HyperTextDrv class is a base class that uses
the C++ ostream library to write HTML tags and text to a
specified stream. The HyperText class is used to write
HTML document templates to a specified stream,

Changes:
==============================================================
03/11/2002: Users now have the option to use the ANSI Standard 
C++ library by defining the __USE_ANSI_CPP__ preprocessor directive. 
By default the old iostream library will be used for all iostream 
operations.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_HTMLDRV_HPP__
#define __GX_HTMLDRV_HPP__

#include "gxdlcode.h"

// This directive must be defined to use the HTML print driver
#if defined (__USE_HTM_PRINTING__)

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <fstream>
#else // Use the old iostream library by default
#include <fstream.h>
#endif // __USE_ANSI_CPP__

// Constants
const int DefaultPrecision = 2; // Default precision for floating points

// Define some common HTML colors
const int NumHTMLColors = 16;
enum htmCOLORS { 
  htmBLACK       = 0x000000,
  htmDARKBLUE    = 0x000080,
  htmBLUE        = 0x0000ff,
  htmGREEN       = 0x008000,
  htmTEAL        = 0x008080,
  htmBRIGHTGREEN = 0x00ff00,
  htmTURQUOISE   = 0x00ffff,
  htmDARKRED     = 0x800000,
  htmVIOLET      = 0x800080,
  htmDARKYELLOW  = 0x808000,
  htmDARKGRAY    = 0x808080,
  htmGRAY        = 0xC0C0C0,
  htmRED         = 0xff0000,
  htmPINK        = 0xff00ff,
  htmYELLOW      = 0xffff00,
  htmWHITE       = 0xffffff
};

// Define some common HTML fonts
const int NumHTMLFonts = 4;
enum htmFONTS {
  htmARIAL,       // Arial
  htmARIALBLACK,  // Arial Black
  htmARIALNARROW, // Arial Narrow
  htmCOURIER      // Courier New
};

// Functions used to print characters with special meaning 
inline GXSTD::ostream& lt(GXSTD::ostream &s)   { return s << "<";  }
inline GXSTD::ostream& gt(GXSTD::ostream &s)   { return s << ">";  }
inline GXSTD::ostream& amp(GXSTD::ostream &s)  { return s << "&";  }
inline GXSTD::ostream& quot(GXSTD::ostream &s) { return s << "\""; }

// Functions used to print special characters
inline GXSTD::ostream& nbsp(GXSTD::ostream &s) { return s << "&nbsp;"; }
inline GXSTD::ostream& hyphen(GXSTD::ostream &s) { return s << "&shy;"; }
inline GXSTD::ostream& copyright(GXSTD::ostream &s) { return s << "&copy;"; } 
inline GXSTD::ostream& registered(GXSTD::ostream &s) { return s << "&reg;"; }

// Functions used to create HMTL tags
inline GXSTD::ostream& stag(GXSTD::ostream &s) { return s << "<";  } // Start 
inline GXSTD::ostream& etag(GXSTD::ostream &s) { return s << "</"; } // End tag
inline GXSTD::ostream& ctag(GXSTD::ostream &s) { return s << ">";  } // Close  

// HTML document formatting functions
inline GXSTD::ostream& anchor(GXSTD::ostream &s) { return s << "<A>"; }
inline GXSTD::ostream& eanchor(GXSTD::ostream &s) { return s << "</A>"; }
inline GXSTD::ostream& comment(GXSTD::ostream &s) { return s << "<!-- "; }
inline GXSTD::ostream& ecomment(GXSTD::ostream &s) { return s << " -->"; }
inline GXSTD::ostream& body(GXSTD::ostream &s) { return s << "<BODY>"; }
inline GXSTD::ostream& ebody(GXSTD::ostream &s) { return s << "</BODY>"; }
inline GXSTD::ostream& br(GXSTD::ostream &s) { return s << "<BR>"; }
inline GXSTD::ostream& head(GXSTD::ostream &s) { return s << "<HEAD>"; }
inline GXSTD::ostream& ehead(GXSTD::ostream &s) { return s << "</HEAD>"; }
inline GXSTD::ostream& html(GXSTD::ostream &s) { return s << "<HTML>"; }
inline GXSTD::ostream& ehtml(GXSTD::ostream &s) { return s << "</HTML>"; }
inline GXSTD::ostream& hr(GXSTD::ostream &s) { return s << "<HR>"; }
inline GXSTD::ostream& par(GXSTD::ostream &s) { return s << "<P>"; }     
inline GXSTD::ostream& epar(GXSTD::ostream &s) { return s << "</P>"; }   
inline GXSTD::ostream& pre(GXSTD::ostream &s) { return s << "<PRE>"; }   
inline GXSTD::ostream& epre(GXSTD::ostream &s) { return s << "</PRE>"; } 
inline GXSTD::ostream& title(GXSTD::ostream &s) { return s << "<TITLE>"; }
inline GXSTD::ostream& etitle(GXSTD::ostream &s) { return s << "</TITLE>"; }

// HTML font formatting functions
inline GXSTD::ostream& bold(GXSTD::ostream &s) { return s << "<B>"; } 
inline GXSTD::ostream& ebold(GXSTD::ostream &s) { return s << "</B>"; } 
inline GXSTD::ostream& center(GXSTD::ostream &s) { return s << "<CENTER>"; } 
inline GXSTD::ostream& ecenter(GXSTD::ostream &s) { return s << "</CENTER>"; } 
inline GXSTD::ostream& font(GXSTD::ostream &s) { return s << "<FONT>"; }
inline GXSTD::ostream& efont(GXSTD::ostream &s) { return s << "</FONT>"; }
inline GXSTD::ostream& h1(GXSTD::ostream &s) { return s << "<H1>"; } 
inline GXSTD::ostream& eh1(GXSTD::ostream &s) { return s << "</H1>"; } 
inline GXSTD::ostream& h2(GXSTD::ostream &s) { return s << "<H2>"; } 
inline GXSTD::ostream& eh2(GXSTD::ostream &s) { return s << "</H2>"; } 
inline GXSTD::ostream& h3(GXSTD::ostream &s) { return s << "<H3>"; } 
inline GXSTD::ostream& eh3(GXSTD::ostream &s) { return s << "</H3>"; }
inline GXSTD::ostream& italic(GXSTD::ostream &s) { return s << "<I>"; }
inline GXSTD::ostream& eitalic(GXSTD::ostream &s) { return s << "</I>"; }
inline GXSTD::ostream& underline(GXSTD::ostream &s) { return s << "<U>"; }
inline GXSTD::ostream& eunderline(GXSTD::ostream &s) { return s << "</U>"; }

// HTML table functions
inline GXSTD::ostream& table(GXSTD::ostream &s) { return s << "<TABLE>"; } 
inline GXSTD::ostream& otable(GXSTD::ostream &s) { return s << "<TABLE "; } 
inline GXSTD::ostream& etable(GXSTD::ostream &s) { return s << "</TABLE>"; } 
inline GXSTD::ostream& tr(GXSTD::ostream &s) { return s << "<TR>"; } 
inline GXSTD::ostream& etr(GXSTD::ostream &s) { return s << "</TR>"; } 
inline GXSTD::ostream& th(GXSTD::ostream &s) { return s << "<TH>"; } 
inline GXSTD::ostream& eth(GXSTD::ostream &s) { return s << "</TH>"; } 
inline GXSTD::ostream& td(GXSTD::ostream &s) { return s << "<TD>"; } 
inline GXSTD::ostream& etd(GXSTD::ostream &s) { return s << "</TD>"; } 

// HTML driver base class
class GXDLCODE_API HyperTextDrv
{
public:
  HyperTextDrv(GXSTD::ostream &s);
  virtual ~HyperTextDrv();

protected: // Filtered output functions used to write HTML text 
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

public: 
  void precision(int p) { dec_precision = p; } // Floating precision
  void eat_space() { non_breaking_sp = 1; }    // Use non-breaking spaces
  void put_space() { non_breaking_sp = 0; }    // Use breaking spaces
  
public: // Overloaded operators
  GXSTD::ostream &operator<<(GXSTD::ostream & (*_f)(GXSTD::ostream&));
  HyperTextDrv &operator<<(char *s);
  HyperTextDrv &operator<<(const char *s);
  HyperTextDrv &operator<<(unsigned char *s);
  HyperTextDrv &operator<<(const unsigned char *s);
  HyperTextDrv &operator<<(char c);
  const HyperTextDrv &operator<<(const char c) const;
  HyperTextDrv &operator<<(unsigned char c);
  const HyperTextDrv &operator<<(const unsigned char c) const;
  HyperTextDrv &operator<<(long val);
  const HyperTextDrv &operator<<(const long val) const;
  HyperTextDrv &operator<<(unsigned long val);
  const HyperTextDrv &operator<<(const unsigned long val) const;
  HyperTextDrv &operator<<(int val);
  const HyperTextDrv &operator<<(const int val) const;
  HyperTextDrv &operator<<(unsigned int val);
  const HyperTextDrv &operator<<(const unsigned int val) const;
  const HyperTextDrv &operator<<(const float val) const;
  HyperTextDrv &operator<<(float val);
  const HyperTextDrv &operator<<(const double val) const;
  HyperTextDrv &operator<<(double val);
  
public: // HTML document formatting functions
  void ANCHOR(const char *s) { *(stream) << "<A" << s << ">"; }
  void ANCHOR(char *s) { *(stream) << "<A" << s << ">"; }
  void BODY(const char *s) { *(stream) << "<BODY " << s << ">"; }
  void BODY(char *s) { *(stream) << "<BODY " << s << ">"; }
  void COMMENT(const char *s) { *(stream) << "<!-- " << s << " -->"; }
  void COMMENT(char *s) { *(stream) << "<!-- " << s << " -->"; }
  void PAR(const char *s) { *(stream) << "<P " << s << ">"; }
  void PAR(char *s) { *(stream) << "<P " << s << ">"; }
  
public: // HTML font formatting functions
  void BOLD(const char *s) { *(stream) << "<B>" << s << "</B>"; } 
  void BOLD(char *s) { *(stream) << "<B>" << s << "</B>"; } 
  void CENTER(const char *s) { *(stream) << "<CENTER>" << s << "</CENTER>"; } 
  void CENTER(char *s) { *(stream) << "<CENTER>" << s << "</CENTER>"; } 
  void FONT(const char *s) { *(stream) << "<FONT " << s << ">"; }
  void FONT(char *s) { *(stream) << "<FONT " << s << ">"; }
  void H1(const char *s) { *(stream) << "<H1>" << s << "</H1>"; } 
  void H1(char *s) { *(stream) << "<H1>" << s << "</H1>"; } 
  void H2(const char *s) { *(stream) << "<H2>" << s << "</H2>"; } 
  void H2(char *s) { *(stream) << "<H2>" << s << "</H2>"; } 
  void H3(const char *s) { *(stream) << "<H3>" << s << "</H3>"; } 
  void H3(char *s) { *(stream) << "<H3>" << s << "</H3>"; } 
  void ITALIC(const char *s) { *(stream) << "<I>" << s << "</I>"; }
  void ITALIC(char *s) { *(stream) << "<I>" << s << "</I>"; }
  void UNDERLINE(const char *s) { *(stream) << "<U>" << s << "</U>"; }
  void UNDERLINE(char *s) { *(stream) << "<U>" << s << "</U>"; }
  
public: // HTML table functions
  void TABLE(const char *s) { *(stream) << "<TABLE " << s << ">"; } 
  void TABLE(char *s) { *(stream) << "<TABLE " << s << ">"; } 
  void TD(const char *s) { *(stream) << "<TD " << s << ">"; }
  void TD(char *s) { *(stream) << "<TD " << s << ">"; }
  void TH(const char *s) { *(stream) << "<TH " << s << ">"; }
  void TH(char *s) { *(stream) << "<TH " << s << ">"; }
  void TR(const char *s) { *(stream) << "<TR " << s << ">"; }
  void TR(char *s) { *(stream) << "<TR " << s << ">"; }

protected:
  GXSTD::ostream *stream;     // Stream to output HTML data
  int dec_precision;   // Decimal point precision for floating points  
  int non_breaking_sp; // True if using non-breaking spaces
};

class GXDLCODE_API HyperText : public HyperTextDrv
{
public:
  HyperText(GXSTD::ostream &s) : HyperTextDrv(s) { }
  ~HyperText() { }
  
public: // Functions used to create HTML document templates
  void Prologue(const char *doc_title = 0);
  void StartBody(const char *parameters = 0);
  void StartBody(htmCOLORS color = htmWHITE);
  void Epilogue();
  void DocHeader(const char *doc_title = 0);
  void DocTrailer();
  
public: // Table Functions
  void GenTable(int cell_spacing = 1, int cell_padding = 4, int width = 75,
		int border = 1, htmCOLORS bordercolor = htmBLACK); 
  void StartTableRow();
  void EndTableRow() { *(stream) << etr << "\n"; }
  void TableHeader(char *valign="CENTER", int colspan = 1, int rowspan = 1,
		   int width = 10);
  void EndTableHeader() { *(stream) << eth << "\n"; }
  void TableData(char *valign="TOP", int colspan = 1, int rowspan = 1,
		 int width = 10);
  void EndTableData() { *(stream) << etd << "\n"; }
  void EndTable() { *(stream) << etable << "\n"; }
  
public: // Reporting functions
  void GetSystemTime(char *s, int full_month_name = 1);
};

#endif // __USE_HTM_PRINTING__

#endif // __GX_HTMLDRV_HPP__ 
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

