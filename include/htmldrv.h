// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: htmldrv.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/09/1999 
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

#include <fstream>

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
inline std::ostream& lt(std::ostream &s)   { return s << "<";  }
inline std::ostream& gt(std::ostream &s)   { return s << ">";  }
inline std::ostream& amp(std::ostream &s)  { return s << "&";  }
inline std::ostream& quot(std::ostream &s) { return s << "\""; }

// Functions used to print special characters
inline std::ostream& nbsp(std::ostream &s) { return s << "&nbsp;"; }
inline std::ostream& hyphen(std::ostream &s) { return s << "&shy;"; }
inline std::ostream& copyright(std::ostream &s) { return s << "&copy;"; } 
inline std::ostream& registered(std::ostream &s) { return s << "&reg;"; }

// Functions used to create HMTL tags
inline std::ostream& stag(std::ostream &s) { return s << "<";  } // Start 
inline std::ostream& etag(std::ostream &s) { return s << "</"; } // End tag
inline std::ostream& ctag(std::ostream &s) { return s << ">";  } // Close  

// HTML document formatting functions
inline std::ostream& anchor(std::ostream &s) { return s << "<A>"; }
inline std::ostream& eanchor(std::ostream &s) { return s << "</A>"; }
inline std::ostream& comment(std::ostream &s) { return s << "<!-- "; }
inline std::ostream& ecomment(std::ostream &s) { return s << " -->"; }
inline std::ostream& body(std::ostream &s) { return s << "<BODY>"; }
inline std::ostream& ebody(std::ostream &s) { return s << "</BODY>"; }
inline std::ostream& br(std::ostream &s) { return s << "<BR>"; }
inline std::ostream& head(std::ostream &s) { return s << "<HEAD>"; }
inline std::ostream& ehead(std::ostream &s) { return s << "</HEAD>"; }
inline std::ostream& html(std::ostream &s) { return s << "<HTML>"; }
inline std::ostream& ehtml(std::ostream &s) { return s << "</HTML>"; }
inline std::ostream& hr(std::ostream &s) { return s << "<HR>"; }
inline std::ostream& par(std::ostream &s) { return s << "<P>"; }     
inline std::ostream& epar(std::ostream &s) { return s << "</P>"; }   
inline std::ostream& pre(std::ostream &s) { return s << "<PRE>"; }   
inline std::ostream& epre(std::ostream &s) { return s << "</PRE>"; } 
inline std::ostream& title(std::ostream &s) { return s << "<TITLE>"; }
inline std::ostream& etitle(std::ostream &s) { return s << "</TITLE>"; }

// HTML font formatting functions
inline std::ostream& bold(std::ostream &s) { return s << "<B>"; } 
inline std::ostream& ebold(std::ostream &s) { return s << "</B>"; } 
inline std::ostream& center(std::ostream &s) { return s << "<CENTER>"; } 
inline std::ostream& ecenter(std::ostream &s) { return s << "</CENTER>"; } 
inline std::ostream& font(std::ostream &s) { return s << "<FONT>"; }
inline std::ostream& efont(std::ostream &s) { return s << "</FONT>"; }
inline std::ostream& h1(std::ostream &s) { return s << "<H1>"; } 
inline std::ostream& eh1(std::ostream &s) { return s << "</H1>"; } 
inline std::ostream& h2(std::ostream &s) { return s << "<H2>"; } 
inline std::ostream& eh2(std::ostream &s) { return s << "</H2>"; } 
inline std::ostream& h3(std::ostream &s) { return s << "<H3>"; } 
inline std::ostream& eh3(std::ostream &s) { return s << "</H3>"; }
inline std::ostream& italic(std::ostream &s) { return s << "<I>"; }
inline std::ostream& eitalic(std::ostream &s) { return s << "</I>"; }
inline std::ostream& underline(std::ostream &s) { return s << "<U>"; }
inline std::ostream& eunderline(std::ostream &s) { return s << "</U>"; }

// HTML table functions
inline std::ostream& table(std::ostream &s) { return s << "<TABLE>"; } 
inline std::ostream& otable(std::ostream &s) { return s << "<TABLE "; } 
inline std::ostream& etable(std::ostream &s) { return s << "</TABLE>"; } 
inline std::ostream& tr(std::ostream &s) { return s << "<TR>"; } 
inline std::ostream& etr(std::ostream &s) { return s << "</TR>"; } 
inline std::ostream& th(std::ostream &s) { return s << "<TH>"; } 
inline std::ostream& eth(std::ostream &s) { return s << "</TH>"; } 
inline std::ostream& td(std::ostream &s) { return s << "<TD>"; } 
inline std::ostream& etd(std::ostream &s) { return s << "</TD>"; } 

// HTML driver base class
class GXDLCODE_API HyperTextDrv
{
public:
  HyperTextDrv(std::ostream &s);
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
  std::ostream &operator<<(std::ostream & (*_f)(std::ostream&));
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
  std::ostream *stream;     // Stream to output HTML data
  int dec_precision;   // Decimal point precision for floating points  
  int non_breaking_sp; // True if using non-breaking spaces
};

class GXDLCODE_API HyperText : public HyperTextDrv
{
public:
  HyperText(std::ostream &s) : HyperTextDrv(s) { }
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
  void TableHeader(const char *valign="CENTER", int colspan = 1, int rowspan = 1,
		   int width = 10);
  void EndTableHeader() { *(stream) << eth << "\n"; }
  void TableData(const char *valign="TOP", int colspan = 1, int rowspan = 1,
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

