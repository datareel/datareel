// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: pscript.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 12/17/1997
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

The PostScriptDrv class is used to create postscript documents.
This version prints postscript documents to a file.

Changes:
==============================================================
03/11/2002: Users now have the option to use the ANSI Standard 
C++ library by defining the __USE_ANSI_CPP__ preprocessor directive. 
By default the old iostream library will be used for all iostream 
operations.

03/24/2002: The PostScriptDrv::ConvertTextFile() function now 
uses a DiskFileB type for the input file. This change was made 
to support large files and NTFS file system enhancements.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_PSCRIPT_HPP__
#define __GX_PSCRIPT_HPP__

#include "gxdlcode.h"

// This directive must be defined to use the postscript print driver
#if defined (__USE_POSTSCRIPT_PRINTING__)

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <fstream>
#else // Use the old iostream library by default
#include <fstream.h>
#endif // __USE_ANSI_CPP__

#include "dfileb.h"

// Ignore all BCC32 unused constants warnings

// Setup postscript defaults
const int DEFAULT_TAB_SIZE = 8;        // Default tab size
const double DEFAULT_POINT_SIZE = 10;  // Default point size (10 pitch)
const double DEFAULT_OFFSET = 0.0;     // Default inches to indent each line
const double DEFAULT_LR_MARGIN = 1.0;  // Default left and right margin offset
const double DEFAULT_TB_MARGIN = 1.0;  // Default top and bottom margin offset
const double PRINTABLE_OFFSET_Y = 25;  // Printable area offset
const double PRINTABLE_OFFSET_X = 25;  // Printable area offset
const double HEADER_OFFSET = 1.0;      // Offset for header and trailer text
const int THICK_LINE_WIDTH = 2;        // Line width for header lines
const int LINE_WIDTH = 1;              // Line width for separator lines

// Setup postscript constants
const char SEP_CHAR = '\001';   // Column separator character 
const int MAXPAGES = 10000;     // Maximum number of pages per job 
const int PS_EOF = 0x04;        // PostScript end of file mark
const int MAX_LINES = 160;      // Maximum lines per page for documents 
const int PIXELS_PER_INCH = 72; // PostScript points per inch     

// Max lines for a unix man page produced by nroff
// nroff is used to format man pages under most variants of UNIX
const int UNIX_MAN_PAGE_LINES = 66;   

// Setup input/output buffer sizes for postscript documents
const int MAX_LINE = 256; // No PostScript line can exceed 256 characters
const int BUFIN = 1024;   // Maximum length of an input line 
const int BUFOUT = (BUFIN * 5);

const int NumberOfFonts = 13;      // Number of fonts to support

// (P)ostscript (D)river class
class GXDLCODE_API PostScriptDrv
{
public:
  enum PSFonts {
    // Most PostScript products include software for 13 standard fonts:
    // Courier, Times, Helvetica, and Symbol families.
    
    // Courier fonts
    COURIER,              // Courier
    COURIER_BOLD,         // Courier-Bold
    COURIER_OBLIQUE,      // Courier-Oblique
    COURIER_BOLD_OBLIQUE, // Courier-BoldOblique

    // Times-Roman fonts
    TIMES,             // Times-Roman
    TIMES_BOLD,        // Times-Bold
    TIMES_ITALIC,      // Times-Italic
    TIMES_BOLD_ITALIC, // Times-BoldItalic

    // Helvetica fonts
    HELV,                      // Helvetica
    HELV_BOLD,                 // Helvetica-Bold
    HELV_OBLIQUE,              // Helvetica-Oblique
    HELV_BOLD_OBLIQUE,         // Helvetica-BoldOblique

    // Symbol font
    SYMBOL                     // Symbol
  };

  enum PSPaperSizes {
    // Non-metric Traditional Paper Sizes used in Canada and the
    // United States. Sheet sizes accommodate 1/8" (3 mm) head,
    // foot, and fore edge trim margins (width precedes height).
    // Decimal inches multiplied by 25.4 to convert to approximate mm.
    // N.B. Fractional mm measures must be rounded to the nearest
    // whole number.
    LETTER_SIZE,  // 8.5 x 11 inches 
    LEGAL_SIZE,   // 8.5 x 14 inches 
    TABLOID_SIZE, // 11 x 17 inches  

    // ISO/DIN and JIS Standard Paper Sizes Trim sizes in mm.
    // Width precedes height. Sheet sizes accommodate 3 mm head,
    // foot, and fore edge trim margins. To convert to approximate
    // decimal inches, divide measures by 25.4.
    A3_SIZE, // 297mm x 420mm (11.70" X 16.55")
    A4_SIZE  // 210mm x 297mm (8.27" X 11.70")
  };

public:
  PostScriptDrv();
  ~PostScriptDrv() { }
  PostScriptDrv(const PostScriptDrv &ob);
  PostScriptDrv &operator=(const PostScriptDrv &ob);
  
public: // Document setup functions
  void DocumentSetup(double LR_margin = 0, double TB_margin = 0, int man = 0);
  void SetFont(PSFonts font, double size);
  void SetPaperSize(PSPaperSizes size);
  void UseLRMargin() { use_lr_margin = 1; } // Use left and right margins
  void UseTBMargin() { use_tb_margin = 1; } // Use top and bottom margins
  void NoMargins() { use_lr_margin = use_tb_margin = 0; }
  void LandScapeMode() { landscape = 1; }
  void PortraitMode() { landscape = 0; }
  void Copies(int num) { ncopies = num; }
  void SetTabStop(int num) { tabstop = num; }
  int StringLen(char *s, int charWidth);
  int StringLen(const char *s, int charWidth);

public: // Document parameters
  double CharsPerInch() { return chars_per_inch; }
  double CharWidth() { return char_width; }
  char *TextFont() { return text_font; }
  double FontSize() { return font_size; }
  int PageWidth() { return page_width; }
  int PageHeight() { return page_height; }
  int LinesPerPage() { return lines_per_page; }
  int Columns() { return columns; }
  int StartX() { return start_x; }
  int StartY() { return start_y; }
  int NCopies() { return ncopies; }
  int TabStop() { return tabstop; }
  int UsingLRMargin() { return use_lr_margin; }
  int UsingTBMargin() { return use_tb_margin; }
  int GetMode() { return landscape; }    // Returns true if using landscape 

public: // Document header and trailer information 
  void UseHeader() { use_header = 1; }
  int UsingHeader() { return use_header; }
  char *HeaderFont() { return header_font; }
  double HeaderFontSize() { return header_font_size; }
  void SetHeaderFont(PSFonts font, double size);
  void SetDocumentName(char *s) { document_name = s; }
  void SetDocumentName(const char *s) { document_name = (char *)s; }
  void SetDateString(char *s) { date_string = s; }
  void SetDateString(const char *s) { date_string = (char *)s; }
  void DrawHeaderLine() { draw_header_line = 1; }
  
public: // Postscript printing functions 
  void Epilogue(GXSTD::ostream &stream, int pc);
  void EndPage(GXSTD::ostream &stream);
  void StartPage(int n, GXSTD::ostream &stream);
  void drawLine(GXSTD::ostream &stream, int points, int xpos, int ypos);
  void drawThickLine(GXSTD::ostream &stream, int points, int xpos, int ypos);
  void MoveTo(GXSTD::ostream &stream, int x, int y);
  void ChangeFont(GXSTD::ostream &stream, PSFonts font, double size);
  int ConvertTextFile(DiskFileB &infile, GXSTD::ostream &stream,
		      int wrap = 0, int cut = 1);
  int ProcessText(char *in, GXSTD::ostream &stream, int cut = 1, 
		  int filter = 0);
  int PrintLine(char *in, GXSTD::ostream &stream);
  int PrintLine(char *s, unsigned max_len, GXSTD::ofstream &stream);
  
  // Generates the PostScript header and includes the prologue.
  // Arguments are: pn - Program Name, hn - Hostname,
  // cd - Creation Date and un - Username.
  void Prologue(GXSTD::ostream &stream, char *pn = 0, char *hn = 0,
		char *cd = 0, char *un = 0);
  
  // Sets the physical sources and physical destinations for the media
  // source and destination to be used in the printer. These settings
  // are device dependent.
  void MediaSetup(GXSTD::ostream &stream, int duplex = 0, int manualfeed = 0,
		  int use_tray = 0, int tray_number = 0);

private: // Document Settings
  int ncopies;   // Number of copies
  int landscape; // Use landscape instead of portrait if true
  int tabstop;   // Number of spaces per tab
  
private: // Font dependent members
  double chars_per_inch; // Number of characters per inch
  double char_width;     // Width of each character
  char *text_font;       // Text font for this document
  double font_size;      // Font or point size
  int columns;           // Number of columns
  
private: // Page dependent members
  int page_width;     // Page width for selected paper size
  int page_height;    // Page height for selected paper size
  int use_lr_margin;  // True if using left and right margins
  int use_tb_margin;  // True if using top and bottom margins
  int lines_per_page; // Lines per page   
  int start_x;        // Starting x position 
  int start_y;        // Starting y position 

private: // Page header and trailer information
  int use_header;          // Use page headers and trailers if true
  char *header_font;       // Text font for headers and trailers
  double header_font_size; // Font or point size for page headers
  char *document_name;     // Name of this document
  char *date_string;       // Time and date this document was printed
  int draw_header_line; // Draw line between header and document text if true 

public: // Page processing members 
  int page_count;     // Keeps track of the total number of pages
  int row;            // Keeps track of the number of rows per page
  int line_count;     // Keeps track of the lines per page
};

#endif // __USE_POSTSCRIPT_PRINTING__

#endif // __GX_PSCRIPT_HPP__ 
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
