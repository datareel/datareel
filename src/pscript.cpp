// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: pscript.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 12/17/1997  
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description aond Details ------------- // 
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
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

// This directive must be defined to use the postscript print driver
#if defined (__USE_POSTSCRIPT_PRINTING__)

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h> // For ceil() function
#include "pscript.h"

// PostScript fonts
const char *PostScriptFonts[NumberOfFonts] = {
  // Courier fonts
  "Courier",             // COURIER
  "Courier-Bold",        // COURIER_BOLD
  "Courier-Oblique",     // COURIER_OBLIQUE
  "Courier-BoldOblique", // COURIER_BOLD_OBLIQUE

  // Times-Roman fonts
  "Times-Roman",      // TIMES
  "Times-Bold",       // TIMES_BOLD
  "Times-Italic",     // TIMES_ITALIC
  "Times-BoldItalic", // TIMES_BOLD_ITALIC

  // Helvetica fonts
  "Helvetica",             // HELV
  "Helvetica-Bold",        // HELV_BOLD
  "Helvetica-Oblique",     // HELV_OBLIUQE
  "Helvetica-BoldOblique", // HELV_BOLD_OBLIQUE

  // Symbol font
  "Symbol" // SYMBOL                     
};

// PostScript command strings defined in the postscript prologue
const char *BACKSPACE  = "B";
const char *ENDPAGE    = "EP";
const char *LINETO     = "L";
const char *MOVETO     = "M";
const char *NEWPATH    = "NP";
const char *SHOW       = "S";
const char *STARTPAGE  = "SP";
const char *STROKE     = "ST";
const char *TAB        = "T";

PostScriptDrv::PostScriptDrv()
{
  ncopies = 1;          // Print at least one copy of this document
  landscape = 0;        // Use portrait mode by default
  use_header = 0;       // Do not use headers and trailers by default
  use_lr_margin = 0;    // Do not use left and right margins by default  
  use_tb_margin = 0;    // Do not use top and bottom margins by default
  document_name = 0;    // Name of this document
  date_string = 0;      // Time and date this document was printed
  draw_header_line = 0; // Do not draw line between header and document 

  tabstop = DEFAULT_TAB_SIZE;
  page_count = 0;
  SetFont(COURIER, 10);
  DocumentSetup();
  row = start_y;
  line_count = lines_per_page;
  SetHeaderFont(COURIER_BOLD, 15);
}

PostScriptDrv::PostScriptDrv(const PostScriptDrv &ob)
{
  ncopies = ob.ncopies;        
  landscape = ob.landscape;      
  lines_per_page = ob.lines_per_page;
  columns = ob.columns;
  tabstop = ob.tabstop;
  row = ob.row;
  start_x = ob.start_x;
  start_y = ob.start_y;
  page_count = ob.page_count;
  font_size = ob.font_size;
  page_width = ob.page_width; 
  page_height = ob.page_height;
  chars_per_inch = ob.chars_per_inch;
  char_width = ob.char_width;
  use_lr_margin = ob.use_lr_margin;
  use_tb_margin = ob.use_tb_margin;
  use_header = ob.use_header;
  draw_header_line = ob.draw_header_line;
  line_count = ob.line_count;

  // PC-lint 04/26/2005: Warning, direct pointer copy of member
  text_font = ob.text_font;
  header_font = ob.header_font;
  header_font_size = ob.header_font_size;
  document_name = ob.document_name;
  date_string = ob.date_string;
}
  
PostScriptDrv &PostScriptDrv::operator=(const PostScriptDrv &ob)
{
  // PC-lint 04/26/2005: Check for self assignment
  if(this != &ob) {
    ncopies = ob.ncopies;        
    landscape = ob.landscape;      
    lines_per_page = ob.lines_per_page;
    columns = ob.columns;
    tabstop = ob.tabstop;
    row = ob.row;
    start_x = ob.start_x;
    start_y = ob.start_y;
    page_count = ob.page_count;
    font_size = ob.font_size;
    page_width = ob.page_width; 
    page_height = ob.page_height;
    chars_per_inch = ob.chars_per_inch;
    char_width = ob.char_width;
    use_lr_margin = ob.use_lr_margin;
    use_tb_margin = ob.use_tb_margin;
    use_header = ob.use_header;
    draw_header_line = ob.draw_header_line;
    line_count = ob.line_count;

    // PC-lint 04/26/2005: Warning, direct pointer copy of member
    text_font = ob.text_font;
    header_font = ob.header_font;
    header_font_size = ob.header_font_size;
    document_name = ob.document_name;
    date_string = ob.date_string;
  }
  return *this;
}

void PostScriptDrv::SetFont(PSFonts font, double size)
{
  switch(font){
    case COURIER:              
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case COURIER_BOLD:         
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case COURIER_OBLIQUE:      
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case COURIER_BOLD_OBLIQUE: 
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case TIMES:             
      font_size = size;
      char_width = 0.57 * font_size; // Width of each character
      break;
      
    case TIMES_BOLD:        
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case TIMES_ITALIC:      
      font_size = size;
      char_width = 0.55 * font_size; // Width of each character
      break;
      
    case TIMES_BOLD_ITALIC: 
      font_size = size;
      char_width = 0.58 * font_size; // Width of each character
      break;
      
    case HELV:             
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case HELV_BOLD:        
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case HELV_OBLIQUE:     
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case HELV_BOLD_OBLIQUE:
      font_size = size;
      char_width = 0.6 * font_size; // Width of each character
      break;
      
    case SYMBOL:
      font_size = size;
      char_width = 0.57 * font_size; // Width of each character
      break;

    default: // COURIER              
      font_size = DEFAULT_POINT_SIZE;
      char_width = 0.6 * font_size; // Width of each character
      break;
  }

  // Set font name
  text_font = (char *)PostScriptFonts[font];

  // Calculate the number of characters per inch
  chars_per_inch = PIXELS_PER_INCH / char_width; 
}

void PostScriptDrv::SetHeaderFont(PSFonts font, double size)
{
  header_font_size = size;
  header_font = (char *)PostScriptFonts[font];
}

void PostScriptDrv::SetPaperSize(PSPaperSizes size)
// Calculates the x and y positions for landscape and portrait modes
// for different paper sizes. There are 72 points per inch, with the
// origin (0,0) in the lower left corner of the page. By default,
// distances are specified in PostScript points (1/72 of an inch). 
 
{
  switch(size) {
    case LETTER_SIZE: // 8.5 X 11 inches 
      page_width = int(8.5 * PIXELS_PER_INCH); // 612 points per inch 
      page_height = int(11 * PIXELS_PER_INCH); // 729 points per inch 
      break;

    case LEGAL_SIZE: // 8.5 x 14 inches 
      page_width = int(8.5 * PIXELS_PER_INCH); // 612 points per inch 
      page_height = int(14 * PIXELS_PER_INCH); // 1008 points per inch 
      break;
      
    case TABLOID_SIZE: // 11 x 17 inches  
      page_width = int(11 * PIXELS_PER_INCH);  // 792 points per inch 
      page_height = int(17 * PIXELS_PER_INCH); // 1224 points per inch 
      break;

    case A3_SIZE: // 297mm x 420mm (11.70" X 16.55")
      page_width = int(11.70 * PIXELS_PER_INCH);  // 842.4 points per inch 
      page_height = int(16.55 * PIXELS_PER_INCH); // 1191.6 points per inch 
      break;

    case A4_SIZE: // 210mm x 297mm (8.27" X 11.70") 
      page_width = int(8.27 * PIXELS_PER_INCH);   // 595.44 points per inch 
      page_height = int(11.70 * PIXELS_PER_INCH); // 842.4 points per inch 
      break;
      
    default: // Default value is letter size: 8.5 X 11 inches 
      page_width = int(8.5 * PIXELS_PER_INCH); // 612 points per inch 
      page_height = int(11 * PIXELS_PER_INCH); // 729 points per inch 
      break;
  }
}

void PostScriptDrv::DocumentSetup(double LR_margin, double TB_margin, int man)
// Calculates the x and y positions, lines per page and columns for
// landscape and portrait modes. The left/right and top/bottom margin
// offsets will not be used if left/right or top/bottom margins are
// not enabled. If man if true the line per page will be set to max
// lines per page for UNIX man pages produced by nroff. nroff is used
// to format man pages under most variants of UNIX.
{
  start_x = (int)PRINTABLE_OFFSET_X;
  
   // If using page headers set the Top, Bottom, Left and Right margins
  if(use_header) {
    if(landscape && use_lr_margin == 0)
      use_tb_margin = 1;
    else
      use_lr_margin = use_tb_margin = 1;
    
    LR_margin = TB_margin = HEADER_OFFSET;
  }
  
  if(landscape) {
    start_y = int(page_width - (PRINTABLE_OFFSET_Y + font_size));
    
    // Account for the printable areas at the top and bottom of the page
    lines_per_page = (int)ceil((start_y - PRINTABLE_OFFSET_Y) / font_size);

    if(man == 1 && lines_per_page > UNIX_MAN_PAGE_LINES) 
      lines_per_page = UNIX_MAN_PAGE_LINES;
    
    if(use_lr_margin) { // Calculate the left and right margins offsets
      double right_margin = LR_margin * 2;
      double new_page_width = page_height - (right_margin * PIXELS_PER_INCH);
      columns = (int)ceil(new_page_width / char_width);
      start_x = int(LR_margin * PIXELS_PER_INCH);
    }
    else {
      double width = page_height - (PRINTABLE_OFFSET_Y * 2);
      columns = (int)ceil((width / char_width) - 1);
    }

    if(use_tb_margin) { // Calculate the top and bottom margins
      int nlines = (int)ceil((TB_margin * PIXELS_PER_INCH) / font_size);
      start_y = int(page_width - (nlines * font_size));
      double bottom_margin = page_width - ((TB_margin *2 ) * PIXELS_PER_INCH);
      lines_per_page = (int)ceil(bottom_margin / font_size);
      if(man == 1 && lines_per_page > UNIX_MAN_PAGE_LINES)
	lines_per_page = UNIX_MAN_PAGE_LINES;
    }
  }
  else { 
    start_y = int(page_height - (PRINTABLE_OFFSET_Y + font_size));
    lines_per_page = (int)ceil(start_y / font_size) - 1;

    if(man == 1 && lines_per_page > UNIX_MAN_PAGE_LINES)
      lines_per_page = UNIX_MAN_PAGE_LINES;

    if(use_lr_margin) { // Calculate the left and right margins offsets
      double right_margin = LR_margin * 2;
      double new_page_width = page_width - (right_margin * PIXELS_PER_INCH);
      columns = (int)ceil(new_page_width / char_width);
      start_x = int(LR_margin * PIXELS_PER_INCH);
    }
    else {
      double width = page_width - (PRINTABLE_OFFSET_X * 2);
      columns = (int)ceil((width / char_width) - 1);
    }
    
    if(use_tb_margin) { // Calculate the top and bottom margins
      int nlines = (int)ceil((TB_margin * PIXELS_PER_INCH) / font_size);
      start_y = int(page_height - (nlines * font_size));
      double bottom_margin = page_height - ((TB_margin * 2) * PIXELS_PER_INCH);
      lines_per_page = (int)ceil(bottom_margin / font_size);
      if(man == 1 && lines_per_page > UNIX_MAN_PAGE_LINES)
	lines_per_page = UNIX_MAN_PAGE_LINES;
    }
  }
}

void PostScriptDrv::Prologue(GXSTD::ostream &stream, char *pn, char *hn,
			     char *cd, char *un)
// Generates the PostScript header and includes the prologue.
// Arguments are: pn - Program Name, hn - Hostname,
// cd - Creation Date, un - Username.
{
  stream << "%!PS-Adobe-2.0" << "\n";
  if(pn) {
    stream << "%%Produced By: " <<  pn;
    if(hn) stream << " on " << hn << "\n"; else  stream << "\n";
  }

  if(cd) stream << "%%Creation Date: " << cd << "\n";
  if(un) stream << "%%For: " << un << "\n";
  if(text_font) stream << "%%DocumentFont: " << text_font << "\n";
  stream << "%%Pages: (atend)" << "\n";
  stream << "%%EndComments" << "\n";

  stream << "% PostScript Prologue for ASCII to PostScript converter" << "\n";
  stream << "%%BeginProlog" << "\n";
  
  // PostScript command strings 
  stream << "/B {NW 0 rmoveto}bind def" << "\n";
  stream << "/EP {SV restore /#copies exch def showpage}bind def" << "\n";
  stream << "/L /lineto load def" << "\n";
  stream << "/M /moveto load def" << "\n";
  stream << "/NP /newpath load def" << "\n";
  stream << "/S /show load def" << "\n";
  stream << "/SP {/SV save def findfont exch scalefont setfont ( )" << "\n";
  stream << "  stringwidth pop dup /W exch def neg /NW exch def}bind def"
	 << "\n";
  stream << "/ST /stroke load def" << "\n";
  stream << "/T {W mul 0 rmoveto}bind def" << "\n";

  // PostScript procedures 
  stream << "/drawLine { " << "\n";
  stream << "gsave " << LINE_WIDTH
	 << " setlinewidth 0 setgray moveto 0 rlineto stroke grestore"
	 << "\n";
  stream << "} bind def" << "\n";
  
  stream << "/drawThickLine { " << "\n";
  stream << "gsave " << THICK_LINE_WIDTH
	 << " setlinewidth 0 setgray moveto 0 rlineto stroke grestore" 
	 << "\n";
  stream << "} bind def" << "\n";

  stream << "%%EndProlog" << "\n";
}

void PostScriptDrv::drawLine(GXSTD::ostream &stream, int points,
				       int xpos, int ypos)
{
  stream << points << " " << xpos << " " << ypos << " drawLine"
	 << "\n";
}

void PostScriptDrv::drawThickLine(GXSTD::ostream &stream, int points,
				  int xpos, int ypos)
{
  stream << points << " " << xpos << " " << ypos << " drawThickLine"
	 << "\n";
}

int PostScriptDrv::ProcessText(char *in, GXSTD::ostream &stream, int cut, 
			       int filter)
// Process a line of text before printing it. If cut is true the line
// will be truncated to match the number of columns for this line. If
// filter is true each line will be filtered for escape sequences and
// non-printable characters before the line is truncated. Returns true 
// if successful.
{
  int len = strlen(in);

  // Truncate the line if it exceeds the maximum number of columns
  if(len >= columns && filter == 1) { 
    int word_count = 0;

    // Filter escape seqences and non-printable chars before truncating line
    // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
    for(int pos = 0; pos < (len-1); pos++) { 
      if(in[pos] == ' ') word_count = 0; else word_count++;

      // Filter bold sequences produced by nroff
      // nroff is used to format man pages under most variants of UNIX
      char c = in[pos++];
      if(c == in[pos+1] && in[pos] == in[pos+2] &&
	 c == in[pos+3] &&  in[pos] == in[pos+4] &&
	 c == in[pos+5]) {
	int bold_offset = (word_count * 7) - word_count;
	len -= bold_offset; 
	word_count = 0;
      }
      
      // Filter underline sequences produced by nroff 
      // nroff is used to format man pages under most variants of UNIX
      if(in[pos] == '\b') len--;
    } 
  }

  // Truncate if necessary after filtering line 
  if(len >= columns && cut == 1) { 
    char *buf = new char[columns];
    // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
    buf[columns-1] = '\0';
    memmove(buf, in, columns);
    stream << start_x << " "  << row << " " << MOVETO << "\n";
    int rv = PrintLine(buf, stream);
    delete[] buf;
    return rv;
  }
  else {
    stream << start_x << " "  << row << " " << MOVETO << "\n";
    return PrintLine(in, stream);
  }
}

int PostScriptDrv::PrintLine(char *in, GXSTD::ostream &stream)
// Print a line of PostScript text, escaping characters when
// necessary and handling tabs.
{
  char bufout[BUFOUT];
  char *p, *q, *savep;
  int currentp, instr, tabc, tabto, i, ncols;
  int colskip = 0;
  int seen_sep = 0;
  
  currentp = instr = tabto = 0;
  char *last = bufout + MAX_LINE - 20; // Subtract error factor 

  q = bufout;
  *q = '\0';
  for(p = in; *p != '\0'; p++) {
    switch (*p) {
      case SEP_CHAR: // Column separator character 
	// This assumes that the input buffer contains the entire line,
	// otherwise the column count will be offset.
	if (!seen_sep) { // Discern number of columns 
	  seen_sep = 1;
	  ncols = 2; // There are at least two columns
	  savep = p++;
	  while (*p != '\0') {
	    if (*p++ == SEP_CHAR)
	      ncols++;
	  }
	  p = savep;
	  colskip = columns / ncols;
	}
	if(instr) {
	  sprintf(q, ")%s ", SHOW);
	  q += strlen(q);
	  instr = 0;
	}
	// PC-lint 04/26/2005: colskip may not have been initialized
	if(colskip > 0) tabto += (colskip - currentp);
	currentp = 0;
	break;

      case '\t': // Tab
	// Count the number of tabs that immediately follow this one
	tabc = 0;
	while (*(p + 1) == '\t') {
	  p++;
	  tabc++;
	}
	if (currentp > 0) { // Not beginning of line 
	  i = tabstop - (currentp % tabstop) + tabc * tabstop;
	  if (instr) {
	    (void) sprintf(q, ")%s ", SHOW);
	    q += strlen(q);
	    instr = 0;
	  }
	}
	else
	  i = (tabc + 1) * tabstop;
	tabto += i;
	currentp += i;
	break;

      case '\b': // Back Space
	*q = '\0';
	stream << bufout << ")" << SHOW << "\n";
	// backspacing over tabs doesn't work
	if (tabto != 0) return 0; // Attempt to backspace over a tab
	p++;
	for (i = 1; *p == '\b'; p++)
	  i++;
	if (currentp - i < 0) return 0; // Too many backspaces
	if (!instr) return 0; // Bad Back Spacing

	// Handle consecutive backspace sequences produced by nroff 
	// nroff is used to format man pages under most variants of UNIX
	if (i == 1)  
	  sprintf(bufout, "%s (", BACKSPACE);
	else
	  sprintf(bufout, "-%d %s (", i, TAB);
	currentp -= i;
	q = bufout + strlen(bufout);
	p--;
	break;

      case '\f': // Form Feed
	tabto = 0; // Optimizes 
	*q = '\0';
	if(instr)
	  stream << bufout << ")" << SHOW << "\n";
	else
	  stream << bufout << "\n";
	EndPage(stream);
	StartPage(page_count + 1, stream);
	row = start_y;
	stream << start_x << " " << row << " " << MOVETO << "\n";
	q = bufout;
	currentp = 0;
	instr = 0;
	break;

      case '\r': // Hard Return
	tabto = 0; // optimizes 
	if (instr) {
	  *q = '\0';
	  stream << bufout << ")" << SHOW << "\n";
	  instr = 0;
	  q = bufout;
	}
	stream << start_x << " " << row << " " << MOVETO << "\n";
	currentp = 0;
	break;

      case '\\': case '(': case ')':
	if (!instr) {
	  if (tabto) {
	    sprintf(q, "%d %s ", tabto, TAB);
	    q += strlen(q);
	    tabto = 0;
	  }
	  *q++ = '(';
	  instr = 1;
	}
	*q++ = '\\';
	*q++ = *p;
	currentp++;
	break;

      default:
	// PostScript files can contain only the printable subset
	// of the ASCII character set (plus the newline marker).
	// PC-lint 04/26/2005: Warning, suspicious cast
	if (!isascii((char)*p) || !isprint((char)*p)) return 0; // Bad character in input
	if (!instr) {
	  if (tabto) {
	    sprintf(q, "%d %s ", tabto, TAB);
	    q += strlen(q);
	    tabto = 0;
	  }
	  *q++ = '(';
	  instr = 1;
	}
	*q++ = *p;
	currentp++;
	break;
    }

    if (q >= last) {
      *q = '\0';
      if (instr)
	stream << bufout << ")" << SHOW << "\n";
      else
	stream << bufout << "\n";
      q = bufout;
      instr = 0;
    }
  }

  if (instr) {
    sprintf(q, ")%s", SHOW);
    q += strlen(q);
  }
  else
    *q = '\0';

  if(q >= last) return 0; // Output buffer overflow
  if(bufout[0] != '\0') stream << bufout << "\n";
  return 1;
}

void PostScriptDrv::EndPage(GXSTD::ostream &stream)
// Terminate the page and indicate the start of the next
{
  if (page_count == MAXPAGES) return; // pagelimit reached;
  stream << ncopies << " " << ENDPAGE << "\n";
  page_count++;
}

void PostScriptDrv::ChangeFont(GXSTD::ostream &stream, PSFonts font, 
			       double size)
// Change the font before printing
{
  stream << size << " /" <<  PostScriptFonts[font] << " ";
  stream << "findfont " << size << " scalefont setfont" << "\n"; 
}

void PostScriptDrv::StartPage(int n, GXSTD::ostream &stream)
// Start a new page
{
  stream << "%%Page: ? " << n << "\n"; 

  if(use_header) // Set the header font
    stream << header_font_size << " /" << header_font << " ";
  else
    stream << font_size << " /" << text_font << " ";
  
  if(landscape) {
    stream << STARTPAGE << " " << page_width << " 0 translate " << "\n";
    stream << "90 rotate" << "\n";
  }
  else
    stream << STARTPAGE << "\n";

  if(use_header) { // Print the header info and reset the font
    int hpoints = int(HEADER_OFFSET * PIXELS_PER_INCH);
    int header_char_width = int(header_font_size * .6);
    int line_width, width, len, line_points, ypos, xpos = start_x; 
    int hoffset = hpoints / 2; // Offsets the header from text
    
    if(landscape) {
      // Leave some room for three hole punch margin at top of page
      ypos = page_width - (hpoints - 20);

      if(use_lr_margin == 0)
	line_points = int(page_height - (PRINTABLE_OFFSET_X * 2));
      else
	line_points = page_height - (hpoints * 2);

      width = page_height;
    }
    else {
      ypos = page_height - hoffset;
      line_points = page_width - (hpoints * 2);
      width = page_width;
    }

    if(draw_header_line) { // Draw the top and bottom lines
      drawThickLine(stream, line_points, xpos, ypos);
      drawThickLine(stream, line_points, xpos, hoffset);
      line_width = THICK_LINE_WIDTH * 2; // Offset the line from header text
    }
    else
      line_width = 0;
    
    ypos += line_width;
    if(date_string != 0 && document_name != 0) {
      // Left justify the document_name
      stream << xpos << " " << ypos << " " << MOVETO << "\n";
      stream << "(" << document_name << ")" << " " << SHOW << "\n";

      // Right justify the date string
      len = strlen(date_string) * header_char_width;
      if(use_lr_margin == 0)
	xpos = int((width - PRINTABLE_OFFSET_X) - len);
      else
	xpos = (line_points + hpoints) - len;
      stream << xpos << " " << ypos << " " << MOVETO << "\n";
      stream << "(" << date_string << ")" << " " << SHOW << "\n";
    }
    else { // Center the doucuments name
      if(document_name == 0) document_name = "PostScript Document";
      len = strlen(document_name) * header_char_width;
      xpos = (width / 2) - (len / 2);
      stream << xpos << " " << ypos << " " << MOVETO << "\n";
      stream << "(" << document_name << ")" << " " << SHOW << "\n";
      xpos = start_x;
    }

    // Draw the page numbers
    char page_number_string[255];
    sprintf(page_number_string, "PAGE %d", n);
    len = strlen(page_number_string) * header_char_width;
    xpos = (width / 2) - (len / 2);
    ypos = int(hoffset - (header_font_size + line_width));
    stream << xpos << " " << ypos << " " << MOVETO << "\n";
    stream << "(" << page_number_string << ")" << " " << SHOW << "\n";
    
    // Reset the text font
    stream << font_size << " /" << text_font << " " << STARTPAGE << "\n";
  }
}

int PostScriptDrv::ConvertTextFile(DiskFileB &infile, 
				   GXSTD::ostream &stream,
				   int wrap, int cut)
// Convert the ASCII document to postscript. Returns 1 if successful.
// If wrap is true the line will wrap around the page if the number
// of columns are exceeded. If cut is true the line will be truncated
// to match the number of columns.
{
  if(!infile) return 0; 
  char bufin[BUFIN];

  // Read in text file line by line until EOF
  while(!infile.df_EOF()) {
    StartPage(page_count + 1, stream);
    row = start_y;
    line_count = lines_per_page;
    // Process the next page 
    for (int lineno = 0; lineno < line_count; lineno++) {
      if(infile.df_GetLine(bufin, MAX_LINE) != DiskFileB::df_NO_ERROR) {
	break;
      }
      if (bufin[0] == '\f') break;
      if (bufin[0] != '\0') {
	int len = strlen(bufin);
	
	if(wrap == 1 && len >= columns) { // Wrap text lines 
	  int i = 0, j = 0, x = 0;
	  char *temp_buf = new char[columns];
	  // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
	  temp_buf[columns-1] = '\0';
    
	  while(len >= columns) { // Loop until length is less then columns
	    len = len - columns;
	    for(j = 0; j < columns; j++, i++) temp_buf[j] = bufin[i];
	    stream << start_x << " "  << row << " " << MOVETO << "\n";
	    if(PrintLine(temp_buf, stream) == 0) {
	      // PC-lint 04/26/2005: Prevent memory leak
	      delete[] temp_buf;
	      return 0;
	    }
	    for(x = 0; x < columns; x++) temp_buf[x] = '\0';
	    row -= (int)font_size; // Update the y position after wrap

	    if(row < font_size) { // Start a new page
	      EndPage(stream);
	      StartPage(page_count + 1, stream);
	      row = start_y;
	      line_count = lines_per_page;
	      lineno = 0;
	    }
	    line_count--;
	    if(len < columns) break;
	  }

	  if(len > 0) { // Print the remaining characters
	    for(j = 0; j < len; j++, i++) temp_buf[j] = bufin[i];
	    stream << start_x << " "  << row << " " << MOVETO << "\n";
	    if(PrintLine(temp_buf, stream) == 0) {
	      // PC-lint 04/26/2005: Prevent memory leak
	      delete[] temp_buf;
	      return 0;
	    }
	  }
	  // PC-lint 04/26/2005: Prevent memory leak
	  delete[] temp_buf;
	}
	else { 
	  if(ProcessText(bufin, stream, cut) == 0) return 0;
	}
	for(int i = 0; i < BUFIN; i++) bufin[i] = 0;  // Clear the buffer
      }
      row -= (int)font_size;
    }
    EndPage(stream);
  } 
  return 1;
}

// PC-lint 04/26/2005: Declaration of page_count changed to pc.
// page_count is a member of the PostScriptDrv class.
void PostScriptDrv::Epilogue(GXSTD::ostream &stream, int pc)
{
  stream << "%%Trailer" << "\n";
  stream << "%%Pages: " << pc << "\n";
}


void PostScriptDrv::MediaSetup(GXSTD::ostream &stream, int duplex, 
			       int manualfeed,
			       int use_tray, int tray_number)
// Sets the physical sources and physical destinations for the media
// source and destination to be used in the printer. These settings
// are device dependent. 
{
  stream << "%%BeginSetup" << "\n";

  if(duplex) {
    // Enable duplex printing if a duplex device is attached to the
    // printer. Each pair of consecutive pages will be printed on
    // opposite sides of a single sheet of paper.
    stream << "%%BeginFeature: *Duplex" << "\n";
    stream << "<</Duplex true>>setpagedevice" << "\n";
    stream << "%%EndFeature" << "\n";
  }

  if(manualfeed) {
    // Feed paper from the manual feed position.
    stream << "%%BeginFeature: *ManualFeed" << "\n";
    stream << "<</ManualFeed true>>setpagedevice" << "\n";
    stream << "%%EndFeature" << "\n";
  }

  if(use_tray) {
    // Select a specific paper tray to print from. The number following
    // "MediaPosition" indicates the actual tray. The correspondence
    // between tray numbers and the actual positions is specific to
    // each printer.

    // Example: The HP LaserJet 5Si/5Si MX printer uses the following
    // numbers to represent the actual paper trays:
    // 0 = Tray 2, 1 = Tray 3, 2 = Envelope Feeder, 3 = Tray 1, 4 = Tray 4
    
    stream << "%%BeginFeature: *MediaPosition" << "\n";
    stream << "<</MediaPosition " << tray_number << ">> setpagedevice"
	   << "\n";
    stream << "%%EndFeature" << "\n";
  }
  
  stream << "%%EndSetup" << "\n";
}

void PostScriptDrv::MoveTo(GXSTD::ostream &stream, int x, int y)
{
 stream << x << " "  << y << " " << MOVETO << "\n";
}

int PostScriptDrv::StringLen(char *s, int charWidth)
// Convert string lenght to PostScript points
{
  return strlen(s) * charWidth;
}

int PostScriptDrv::StringLen(const char *s, int charWidth)
// Convert string lenght to PostScript points
{
  return strlen(s) * charWidth;
}

int PostScriptDrv::PrintLine(char *s, unsigned max_len, 
			     GXSTD::ofstream &stream)
{
  if(s == 0 || max_len <= 0) return 0;
  
  if(strlen(s) > max_len) {
    char *buf = new char[max_len];
    // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
    buf[max_len-1] = '\0';
    memmove(buf, s, max_len);
    int rv = PrintLine(buf, stream);
    delete [] buf;
    return rv;
  }
  else
    return PrintLine(s, stream);
}

#endif // __USE_POSTSCRIPT_PRINTING__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
