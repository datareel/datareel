// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxshtml.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 05/03/2016
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

The gxsHTML class is a base class used to parse html documents.

Changes:
==============================================================
06/02/2002: Corrected incorrect assignment in two if statements 
in the gxsHTML::CollectHTMLTags() function.

09/30/2002: Modified all versions of the gxsHTML::CollectHTMLTags() 
functions to account for tags using the ; character inside < > tags.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <ctype.h>
#include "gxshtml.h"
#include "gxstring.h"

#if defined (__wxWIN201__) || (__wxWIN291__)
#include "wx2incs.h"  // Include files for wxWindows version 2.X
#endif

#if defined (__wxWIN302__)
#include "wx2incs.h"  // Include files for wxWindows version 3.X
#endif

#ifdef __BCC32__
#pragma warn -8071
#pragma warn -8072
#pragma warn -8080
#endif

// The following array of HTML tags is a combination of HTML
// 2.0, 3.0, 3.2, 4.0 tags supported by Netscape's Navigator
// web browser, Microsoft's Internet Explorer web browser, 
// and standards defined by the World Wide Web Consortium.
const char *gxs_SUPPROTED_TAGS[gxsMAX_SUPPORTED_TAGS] = {
  "gxs_invalid_tag", // Invalid tag specified
  "gxs_unknown_tag", // Unknown tag specified
  "gxs_special_tag", // Unknown special tags starting with an 
                     // ampersand ending in a semicolon &xxxx;

  // Tags and format specifiers with special meaning
  "!--",         // Comment
  "&lt;",        // Less than sign "<" 
  "&gt;",        // Greater then sign ">" 
  "&amp;",       // Ampersand "&" 
  "&nbsp;",      // Non-breaking space 
  "&quot;",      // Quotation mark
  "&#",          // Extended ASCII character set
  
  // HTML tag strings
  "A",           // Anchor
  "ABBREV",      // Abbreviation
  "ACRONYM",     // Acronym
  "ADDRESS",     // Address
  "APPLET",      // Java Applet
  "AREA",        // Area
  "AU",          // Author
  "AUTHOR",      // Author
  "B",           // Bold
  "BANNER",      // Banner
  "BASE",        // Base
  "BASEFONT",    // Base Font
  "BDO",         // Bi-Directional Override
  "BGSOUND",     // Background Sound
  "BIG",         // Big Text
  "BLINK",       // Blink
  "BLOCKQUOTE",  // Block Quote
  "BQ",          // Block Quote
  "BODY",        // Body
  "BR",          // Line Break
  "BUG",         // Bug tag
  "CAPTION",     // Caption
  "CENTER",      // Center
  "CITE",        // Citation
  "CODE",        // Code
  "COL",         // Table Column
  "COLGROUP",    // Table Column Group
  "COMMENTS",    // Comments
  "CREDIT",      // Credit
  "DEL",         // Deleted Text
  "DFN",         // Definition
  "DIR",         // Directory List
  "DIV",         // Division
  "DL",          // Definition List
  "DT",          // Definition Term
  "DD",          // Definition Definition
  "EM",          // Emphasized
  "EMBED",       // Embed
  "FIELDSET",    // Fieldset
  "FIG",         // Figure
  "FN",          // Footnote
  "FONT",        // Font
  "FORM",        // Form
  "FRAME",       // Frame
  "FRAMESET",    // Frame Set
  "H1",          // Heading 1
  "H2",          // Heading 2
  "H3",          // Heading 3
  "H4",          // Heading 4
  "H5",          // Heading 5
  "H6",          // Heading 6
  "HEAD",        // Head
  "HR",          // Horizontal Rule
  "HTML",        // HTML
  "I",           // Italic
  "IFRAME",      // Frame - Floating
  "ILAYER",      // ILayer
  "IMG",         // Inline Image
  "INPUT",       // Form Input
  "INS",         // Inserted Text
  "ISINDEX",     // Is Index
  "KBD",         // Keyboard
  "LANG",        // Language
  "LAYER",       // Layer
  "LEGEND",      // Legend
  "LH",          // List Heading
  "LI",          // List Item
  "LINK",        // Link
  "LISTING",     // Listing
  "MAP",         // Map
  "MARQUEE",     // Marquee
  "MATH",        // Math
  "MENU",        // Menu List
  "META",        // Meta
  "MULTICOL",    // Multi Column Text
  "NOBR",        // No Break
  "NOEMBED",     // No Embed
  "NOFRAMES",    // No Frames
  "NOLAYER",     // No Layer
  "NOSCRIPT",    // No Script
  "NOTE",        // Note
  "OBJECT",      // Object
  "OPTGROUP",    // Option Group
  "OPTION",      // Option
  "OL",          // Ordered List
  "OVERLAY",     // Overlay
  "P",           // Paragraph
  "PARAM",       // Parameters
  "PERSON",      // Person
  "PLAINTEXT",   // Plain Text
  "PRE",         // Preformatted Text
  "Q",           // Quote
  "RANGE",       // Range
  "RT",          // Ruby Text
  "RUBY",        // Ruby
  "S",           // Strikethrough
  "SAMP",        // Sample
  "SCRIPT",      // Script
  "SELECT",      // Form Select
  "SMALL",       // Small Text
  "SPACER",      // White Space
  "SPAN",        // Span
  "SPOT",        // Spot
  "STRIKE",      // Strikethrough
  "STRONG",      // Strong
  "STYLE",       // Style
  "SUB",         // Subscript
  "SUP",         // Superscript
  "TAB",         // Horizontal Tab
  "TABLE",       // Table
  "TBODY",       // Table Body
  "TD",          // Table Data
  "TEXTAREA",    // Form Text Area
  "TEXTFLOW",    // Java Applet Textflow
  "TFOOT",       // Table Footer
  "TH",          // Table Header
  "THEAD",       // Table Head
  "TITLE",       // Title
  "TR",          // Table Row
  "TT",          // Teletype
  "U",           // Underlined
  "UL",          // Unordered List
  "VAR",         // Variable
  "WBR",         // Word Break
  "XML",         // XML
  "XMP"          // Example
};

gxsHTMLTagInfo::gxsHTMLTagInfo() 
{
  Clear();
}

gxsHTMLTagInfo::~gxsHTMLTagInfo() 
{ 

}

GXDLCODE_API int operator==(const gxsHTMLTagInfo &a, const gxsHTMLTagInfo &b)
// Overloaded == operator added to work with gxs linked list class.
{
  if(a.start_tag != b.start_tag) return 0; 
  if(a.end_tag != b.end_tag) return 0;
  if(a.tag_length != b.tag_length) return 0;
  if(a.tag_id != b.tag_id) return 0;
  if(a.tag != b.tag) return 0;
  if(a.attr != b.attr) return 0;
  if(a.tag_info != b.tag_info) return 0;
  if(a.start_instruction != b.start_instruction) return 0;
  if(a.end_instruction != b.end_instruction) return 0; 
  if(a.has_attributes != b.has_attributes) return 0;

  return 1;
}

void gxsHTMLTagInfo::Copy(const gxsHTMLTagInfo &ob)
// Function used during copying and assignment.
{
  start_tag = ob.start_tag; 
  end_tag = ob.end_tag;
  tag_length = ob.tag_length;
  tag_id = ob.tag_id;
  tag = ob.tag;
  attr = ob.attr;
  tag_info = ob.tag_info;
  start_instruction = ob.start_instruction;
  end_instruction = ob.end_instruction; 
  has_attributes = ob.has_attributes;
}

void gxsHTMLTagInfo::Clear()
{
  start_tag = end_tag = -1;
  tag_length = 0;
  tag_id = 0; 
  start_instruction = end_instruction = 0; 
  has_attributes = 0;
  tag_info.Clear();
  tag.Clear();
  attr.Clear();
}

gxsScriptInfo::gxsScriptInfo()
{
  Clear();
}

gxsScriptInfo::~gxsScriptInfo() 
{ 

}

GXDLCODE_API int operator==(const gxsScriptInfo &a, const gxsScriptInfo &b)
// Overloaded == operator added to work with gxs linked list class.
{
  if(a.script_code != b.script_code) return 0;
  if(a.script_start != b.script_start) return 0;
  if(a.script_end != b.script_end) return 0;
  if(a.script_length != b.script_length) return 0;
  return 1;
}

void gxsScriptInfo::Copy(const gxsScriptInfo &ob)
// Function used during copying and assignment.
{
  script_code = ob.script_code;
  script_start = ob. script_start;
  script_end = ob.script_end;
  script_length = ob.script_length;
}

void gxsScriptInfo::Clear()
{
  script_code.Clear();
  script_start = (FAU_t)0;
  script_end = (FAU_t)0;
  script_length = 0;
}

gxsStyleInfo::gxsStyleInfo()
{
  Clear();
}

gxsStyleInfo::~gxsStyleInfo() 
{ 

}

GXDLCODE_API int operator==(const gxsStyleInfo &a, const gxsStyleInfo &b)
// Overloaded == operator added to work with gxs linked list class.
{
  if(a.style_code != b.style_code) return 0;
  if(a.style_start != b.style_start) return 0;
  if(a.style_end != b.style_end) return 0;
  if(a.style_length != b.style_length) return 0;
  return 1;
}

void gxsStyleInfo::Copy(const gxsStyleInfo &ob)
// Function used during copying and assignment.
{
  style_code = ob.style_code;
  style_start = ob. style_start;
  style_end = ob.style_end;
  style_length = ob.style_length;
}

void gxsStyleInfo::Clear()
{
  style_code.Clear();
  style_start = (FAU_t)0;
  style_end = (FAU_t)0;
  style_length = 0;
}

gxsHTML::gxsHTML() 
{ 
  num_tags = 0;
  num_processed = 0;
  list_ptr = 0;
  num_scripts = 0;
  num_scripts_processed = 0;
  num_style_sheets = 0;
  num_style_sheets_processed = 0;
  ClearTagList();

  // 11/30/2006: Enable script processing by default
  process_scripts = 1; 

  // 04/24/2007: Enable embedded style sheet processing by default
  process_style_sheets = 1; 
}

gxsHTML::~gxsHTML() 
{ 
  // PC-lint 09/08/2005: Function may throw exception in destructor
  df_Close();
  ClearTagList();
}

void gxsHTML::ClearTagList()
// Clears the tag list.
{
  tag_list.ClearList();
  script_list.ClearList();
  style_sheet_list.ClearList();
}

char *gxsHTML::GetTag(int tag_id)
// Returns a null terminated string 
// based on the value of the tag id 
// number.
{
  if(tag_id > gxsMAX_SUPPORTED_TAGS) tag_id = gxsHTML::gxs_invalid_tag;
  return (char *)gxs_SUPPROTED_TAGS[tag_id];
}

const char *gxsHTML::GetTag(int tag_id) const
// Returns a null terminated string 
// based on the value of the tag id 
// number.
{
  if(tag_id > gxsMAX_SUPPORTED_TAGS) tag_id = gxsHTML::gxs_invalid_tag;
  return (char *)gxs_SUPPROTED_TAGS[tag_id];
}

int gxsHTML::GetTagID(const gxString &tag)
// Returns an numerical value that represents the specified tag.
{
  int i; // Index of supported tags

  for(i = 0; i < gxsMAX_SUPPORTED_TAGS; i++) {
    if(CaseICmp(tag, gxs_SUPPROTED_TAGS[i]) == 0)
      return i; // Corresponds to the supported tags enumeration
  }
  return gxsHTML::gxs_unknown_tag;
}

int gxsHTML::LoadHTMLFile(const char *fname)
// Open the specified HTML file and collect the tags in a 
// doubly linked list. Returns a DiskFileB error code if 
// an error occurs.
{
  // Open the specified file
  int rv = df_Open(fname);
  if(rv != DiskFileB::df_NO_ERROR) return rv;

  // Obtain the file location of all the tags in the document
  rv = CollectHTMLTags();
  if(rv != DiskFileB::df_NO_ERROR) return rv;

  // Read the tags
  rv = ProcessHTMLTags();
  if(rv != DiskFileB::df_NO_ERROR) return rv;

  return  DiskFileB::df_NO_ERROR;
}

void gxsHTML::CloseFile()
// Close the open HTML file after a load operation.
{
  df_Close();
}

int gxsHTML::CollectHTMLTags()
// Collect all the HTML tags in an previously opened file.
// Returns a DiskFileB error code if an error occurs.
{
  int rv, i;
  num_tags = 0;
  int found_open_tag = 0;
  int looking_for_tag_end = 0;
  char prev_tag = 0;
  int start_of_comment = 0;
  // int end_of_comment = 0;
  FAU_t curr_pos = (FAU_t)0;
  err_string.Clear();
  char curr_open_tag;
  gxString sbuf;
  gxString tag_buf; // Temp buffer used to process scripts and style sheets

  tag_list.ClearList(); // Clear the tag list
  script_list.ClearList(); // Clear the script list
  style_sheet_list.ClearList(); // Clear the style sheet list
  rv = df_Rewind(); // Rewind the open html file
  if(rv != DiskFileB::df_NO_ERROR) return rv;

  gxsHTMLTagInfo tag;
  gxsScriptInfo script;
  gxsStyleInfo style_sheet;

  // 04/25/2007: Added to skip script and CSS content
  int in_script = 0;
  int in_style_sheet = 0;

  char c;
  while(!df_EOF()) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    rv = df_Get(c);
    if(rv != DiskFileB::df_NO_ERROR) return rv;

    switch(c) {
      case '<' : case '&' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) {
	  // 04/25/2007: Skip embedded scripts and CSS
	  tag_buf.Clear();
	  tag_buf << c;
	  break;
	}

	if(start_of_comment) {
	  // Ignore all tags until the end of the comment tag
	  break;
	}

	// 04/25/2007: Account for nested tags for example:
	// &amp; tag nested inside <a href=""> tags
	if(found_open_tag) break;

	if(c == '<') {
	  prev_tag = '<'; 
	}
	else {
	  prev_tag = '&';
	}

	if(!looking_for_tag_end) { 
	  found_open_tag = 1;
	  looking_for_tag_end = 1;
	  tag.start_tag = df_Tell();
	  if(tag.start_tag > (FAU_t)0) tag.start_tag--;
	  curr_open_tag = c;

	  // 11/29/2006: Clear the tag buffer
	  tag_buf.Clear();

	  // 04/25/2007: Record the open tag so the complete tag 
	  // can be parsed.
	  tag_buf << c;
	}
	break;

	// 07/30/2006: Code added to parse comments with
	// nested tags.
      case '!' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;
	if(start_of_comment) break;
	if(prev_tag == '<') prev_tag = '!';
	break;

      case '-' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;
	if(prev_tag == '!') { // Found the start of a comment tag
	  rv = df_Get(c);
	  if(rv != DiskFileB::df_NO_ERROR) return rv;
	  if(c == '-') {
	    start_of_comment = 1;  
	    prev_tag = 0;
	  }
	}
	if(start_of_comment) {
	  curr_pos = df_Tell();
	  rv = df_Get(c);
	  if(rv != DiskFileB::df_NO_ERROR) return rv;
	  if(c == '-') rv = df_Get(c);
	  if(rv != DiskFileB::df_NO_ERROR) return rv;
	  while(c == ' ') df_Get(c); // Eat spaces
	  while(c == '-') df_Get(c); // 07/24/2007: Eat extra dashes
	  if(c == '>') { // Found the end of the comment tag
	    start_of_comment = 0;  
	    prev_tag = 0;
	    tag.end_tag = df_Tell();
	    num_tags++;
      
	    // Calculate the length of the tag including the angle brackets
	    tag.tag_length = unsigned(tag.end_tag - tag.start_tag);
      
	    tag_list.Add(tag); // Add the tag to the list
	    tag.Clear();
	    found_open_tag = 0;
	    looking_for_tag_end = 0;
	  }
	}
	break;

      case '#' : // Look for extended ASCII characters denoted by: "&#xxx;"
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;
	// 06/02/2002: Corrected BCC32 incorrect assignment warning
	if((prev_tag == '&') && (tag.start_tag == (df_Tell() - (FAU_t)1))) {
	  if(found_open_tag) { // Look for the end of a tag
	    // Get the next three numeric characters following a "&#" tag
	    // plus the terminating semicolon or space.
	    for(i = 0; i < 4; i++) { 
	      df_Get(c);
	      if((c == ';') || (c == ' ')) break;
	    }
	    tag.start_tag--; // Account for the ampersand
	    tag.end_tag = df_Tell();
	    num_tags++;
	    
	    // Calculate the length of the tag
	    tag.tag_length = unsigned(tag.end_tag - tag.start_tag);
	    
	    tag_list.Add(tag); // Add the tag to the list
	    tag.Clear();
	    found_open_tag = 0;
	    looking_for_tag_end = 0;
	  }
	} 
	break;

      case '>' : case  ';' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) { 
	  // Allow the open tag to close
	  if(!found_open_tag) break; 
	}

	if(start_of_comment) break;

	// 09/30/2002: Account for tags using the ; char inside < > tags
	if((c == ';') && (prev_tag != '&')) break;

	if(c == '>') {
	  prev_tag = '>'; 
	}
	else {
	  prev_tag = ';';
	}
	if(found_open_tag) { // Look for the end of a tag
	  tag.end_tag = df_Tell();
	  num_tags++;
      
	  // Calculate the length of the tag including the angle brackets
	  tag.tag_length = unsigned(tag.end_tag - tag.start_tag);
      
	  tag_list.Add(tag); // Add the tag to the list
	  tag.Clear();
	  found_open_tag = 0;
	  looking_for_tag_end = 0;
	}
	break;

      case ' ': case '\n': case '\r': 
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;

	// Look for chars that might be confused with a tag
	if((found_open_tag) && (looking_for_tag_end)) {
	  if(curr_open_tag == '&') {
	    // The & character might be used in text without proper formatting
	    found_open_tag = 0;
	    looking_for_tag_end = 0;
	    prev_tag = 0;
	  }
	}
	break;

      default:
	// 11/29/2006: Added to capture the current tag for any
	// additional processing.
	tag_buf << c;
	tag_buf.ToUpper();

	// 11/29/2006: Added to record script code file positions
	// 04/25/2007: Modifed to catch all variations of the script tag
	if((tag_buf == "<SCRIPT") || (tag_buf == "< SCRIPT")) {
	  script.script_start = df_Tell();
	  in_script = 1;
	}
	// 04/25/2007: Modifed to catch all variations of the script tag
	if((tag_buf == "</SCRIPT") || (tag_buf == "< /SCRIPT") || 
	   (tag_buf == "< / SCRIPT") || (tag_buf == "</ SCRIPT")){
	  if(tag_buf == "</SCRIPT") sbuf = "/SCRIPT";
	  if(tag_buf == "< /SCRIPT") sbuf = " /SCRIPT";
	  if(tag_buf == "< / SCRIPT") sbuf = " / SCRIPT";
	  if(tag_buf == "</ SCRIPT") sbuf = "/ SCRIPT";
	  script.script_end = df_Tell();
	  script.script_length = script.script_end - script.script_start;
	  script.script_length -= sbuf.length() + 1; // Remove </script tag
	  script_list.Add(script);
	  num_scripts++;
	  script.Clear();
	  in_script = 0;
	}

	// 04/24/2007: Added to record embedded style sheet file positions
	// 04/25/2007: Modifed to catch all variations of the style tag
	if((tag_buf == "<STYLE") || (tag_buf == "< STYLE")) {
	  style_sheet.style_start = df_Tell();
	  in_style_sheet = 1;
	}
	// 04/25/2007: Modifed to catch all variations of the style tag
	if((tag_buf == "</STYLE") || (tag_buf == "< /STYLE") || 
	   (tag_buf == "< / STYLE") || (tag_buf == "</ STYLE")){
	  if(tag_buf == "</STYLE") sbuf = "/STYLE";
	  if(tag_buf == "< /STYLE") sbuf = " /STYLE";
	  if(tag_buf == "< / STYLE") sbuf = " / STYLE";
	  if(tag_buf == "</ STYLE") sbuf = "/ STYLE";
	  style_sheet.style_end = df_Tell();
	  style_sheet.style_length = style_sheet.style_end - style_sheet.style_start;
	  style_sheet.style_length -= sbuf.length() + 1; // Remove </style tag
	  style_sheet_list.Add(style_sheet);
	  num_style_sheets++;
	  style_sheet.Clear();
	  in_style_sheet = 0;
	}
	break;
    }
  }

  // 09/11/2006: Added to report parsing errors
  if(start_of_comment) {
    err_string << clear << "Document missing closing comment tag." 
	       << "Tag start " << tag.start_tag;
  }
  if(found_open_tag) {
    if(looking_for_tag_end) {
      err_string << clear << "Document missing closing " << prev_tag 
		 << " tag. Tag start " << tag.start_tag;
    }
  }

  // 04/25/2007: Added to report parsing errors in scripts and CSS
  if(in_script) {
    err_string << clear << "Document missing closing script tag.";
  }
  if(in_style_sheet) {
    err_string << clear << "Document missing closing style tag.";
  }

  return DiskFileB::df_NO_ERROR;
}

int gxsHTML::ProcessHTMLTags()
// Read and process all the tags in a open file. 
// Returns a DiskFileB error code if an error occurs.
{
  num_processed = 0;

  int rv = df_Rewind(); // Start at the beginning of the file
  if(rv != DiskFileB::df_NO_ERROR) return rv;

  MemoryBuffer mbuf;
  list_ptr = tag_list.GetHead();

  while((list_ptr) && (!df_EOF())) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    mbuf.Clear(); // Clear the buffer before each read
    num_processed++;
    mbuf.resize(list_ptr->data.tag_length);

    // Read the tag starting at the opening angle bracket
    FAU_t pos = list_ptr->data.start_tag;
    rv = df_Read((char *)mbuf.m_buf(), mbuf.length(), pos);
    if(rv != DiskFileB::df_NO_ERROR) return rv;

    // Store the tag and indentify it
    mbuf += '\0'; // Ensure null termination
    list_ptr->data.tag_info = (char *)mbuf.m_buf();

    // Parse the tag type and attributes
    ParseHTMLTagInfo(list_ptr->data);

    // Excute the derived class version of the tag handler
    HandleHTMLTag(list_ptr->data.tag_id);
    
    list_ptr = list_ptr->next;  
  }

  if(process_scripts) {
    rv = ProcessScripts();
    if(rv != DiskFileB::df_NO_ERROR) return rv;
  }

  if(process_style_sheets) {
    rv = ProcessStyleSheets();
    if(rv != DiskFileB::df_NO_ERROR) return rv;
  }

  return DiskFileB::df_NO_ERROR;
}

int gxsHTML::ProcessScripts()
// Read and process any script code in a open file. 
// Returns a DiskFileB error code if an error occurs.
{
  num_scripts_processed = 0;

  int rv = df_Rewind(); // Start at the beginning of the file
  if(rv != DiskFileB::df_NO_ERROR) return rv;

  MemoryBuffer mbuf;
  script_ptr = script_list.GetHead();

  while((script_ptr) && (!df_EOF())) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    mbuf.Clear(); // Clear the buffer before each read
    num_scripts_processed++;
    mbuf.resize(script_ptr->data.script_length);

    // Read the script starting at the opening angle bracket
    FAU_t pos = script_ptr->data.script_start;
    rv = df_Read((char *)mbuf.m_buf(), mbuf.length(), pos);
    if(rv != DiskFileB::df_NO_ERROR) return rv;

    // Store the tag and indentify it
    mbuf += '\0'; // Ensure null termination
    script_ptr->data.script_code = (char *)mbuf.m_buf();

    // Excute the derived class version of the script handler
    Handle_Script(script_ptr->data);
    
    script_ptr = script_ptr->next;  
  }

  return DiskFileB::df_NO_ERROR;
}

int gxsHTML::ProcessStyleSheets()
// Read and process any embedded style sheet code in a open file. 
// Returns a DiskFileB error code if an error occurs.
{
  num_style_sheets_processed = 0;

  int rv = df_Rewind(); // Start at the beginning of the file
  if(rv != DiskFileB::df_NO_ERROR) return rv;

  MemoryBuffer mbuf;
  style_sheet_ptr = style_sheet_list.GetHead();

  while((style_sheet_ptr) && (!df_EOF())) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    mbuf.Clear(); // Clear the buffer before each read
    num_style_sheets_processed++;
    mbuf.resize(style_sheet_ptr->data.style_length);

    // Read the style sheet starting at the opening angle bracket
    FAU_t pos = style_sheet_ptr->data.style_start;
    rv = df_Read((char *)mbuf.m_buf(), mbuf.length(), pos);
    if(rv != DiskFileB::df_NO_ERROR) return rv;

    // Store the tag and indentify it
    mbuf += '\0'; // Ensure null termination
    style_sheet_ptr->data.style_code = (char *)mbuf.m_buf();

    // Excute the derived class version of the style_sheet handler
    Handle_StyleSheet(style_sheet_ptr->data);
    
    style_sheet_ptr = style_sheet_ptr->next;  
  }

  return DiskFileB::df_NO_ERROR;
}

void gxsHTML::ParseHTMLTagInfo(gxsHTMLTagInfo &t)
// Parse the specific tag infomation based on the
// string contained in the gxsHTMLTagInfo::tag_info
// member.
{
  // Replace all carriage returns and line feed with spaces
  // if the tag occupies one or more lines in the file.
  t.tag_info.ReplaceChar('\r', ' ');
  t.tag_info.ReplaceChar('\n', ' ');

  gxString sbuf(t.tag_info);

  // Remove the opening and closing angle brackets
  int offset = sbuf.Find("<");
  if(offset != -1) sbuf.DeleteAt(offset, 1);
  offset = sbuf.Find(">");
  if(offset != -1) sbuf.DeleteAt(offset, 1);

  // Filter all leading and trailing spaces
  sbuf.TrimLeadingSpaces();
  sbuf.TrimTrailingSpaces();

  gxString tag(sbuf); gxString attr(sbuf);

  // Check to see if this the start or the end of a tag instruction
  if(tag[0] == '/') {
    tag.DeleteAt(0, 1); // Remove the foward slash
    t.start_instruction = 0;
    t.end_instruction = 1;
  }
  else {
    t.start_instruction = 1;
    t.end_instruction = 0;
  }

  // Check to see if this is comment
  offset = tag.Find(gxs_SUPPROTED_TAGS[gxs_comment_tag]);
  if(offset != -1) { // This is comment string
    t.has_attributes = 0;
    t.tag_id = gxs_comment_tag;
    t.tag = gxs_SUPPROTED_TAGS[gxs_comment_tag];
    return;
  }

  // Check to see if this a special meaning tag
  //  08/23/2006: Do not get confused with & query characters 
  if((tag.IFind("HREF") == -1) && (tag.IFind("CODE") == -1) &&
     (tag.IFind("SRC") == -1) && (tag.IFind("LOWSRC") == -1) &&
     (tag.IFind("DATA") == -1) && (tag.IFind("VALUE") == -1) &&
     (tag.IFind("PLUGINSPAGE") == -1)) {
    
    offset = tag.Find("&");
    if(offset != -1) {
      t.has_attributes = 0;
      t.tag = tag;

      // This is an unknown special meaning tag
      offset = tag.Find(";");
      if(offset != -1) {
	t.tag_id = gxs_special_tag;
      }
      
      // Identify the most common special meaning tags
      offset = tag.Find(gxs_SUPPROTED_TAGS[gxs_less_then]);
      if(offset != -1) {
	t.tag_id = gxs_less_then;
	t.tag = gxs_SUPPROTED_TAGS[gxs_less_then];
      }
      offset = tag.Find(gxs_SUPPROTED_TAGS[gxs_greater_then]);
      if(offset != -1) {
	t.tag_id = gxs_greater_then;
	t.tag = gxs_SUPPROTED_TAGS[gxs_greater_then];
      }
      offset = tag.Find(gxs_SUPPROTED_TAGS[gxs_ampersand]);
      if(offset != -1) {
	t.tag_id = gxs_ampersand;
	t.tag = gxs_SUPPROTED_TAGS[gxs_ampersand];
      }
      offset = tag.Find(gxs_SUPPROTED_TAGS[gxs_nb_space]);
      if(offset != -1) {
	t.tag_id =  gxs_nb_space;
	t.tag = gxs_SUPPROTED_TAGS[gxs_nb_space];
      }
      offset = tag.Find(gxs_SUPPROTED_TAGS[gxs_quote]);
      if(offset != -1) {
	t.tag_id = gxs_quote;
	t.tag = gxs_SUPPROTED_TAGS[gxs_quote];
      }
      
      // Identify extended ASCII character tags
      offset = tag.Find(gxs_SUPPROTED_TAGS[gxs_ex_acsii_set]);
      if(offset != -1) {
	t.tag_id = gxs_ex_acsii_set;
	t.tag = tag;
      }
      return;
    }
  }

  // Separate the tag and its attributes
  offset = tag.Find(" "); 
  if(offset == -1) { // This tag has no attributes
    t.tag = tag;
    t.tag_id = GetTagID(t.tag); // Identify the tag
    t.has_attributes = 0;

    // Covert the tag to upper case so that it matches
    // the strings in the supported tags array. 
    tag.ToUpper(); 
    return;
  }
  else {
    // Remove everything after the first space
    tag.DeleteAt(offset, (tag.length() - offset));
    t.tag = tag;
    t.tag_id = GetTagID(t.tag); // Identify the tag

    // Covert the tag to upper case so that it matches
    // the strings in the supported tags array. 
    tag.ToUpper(); 
  }

  // Look for the tag's attribute
  offset = attr.Find(" ");
  if(offset != -1) {
    // Remove everything before the first space
    attr.DeleteAt(0, offset+1);
    t.attr = attr;

    // Trim any spaces that may have been inserted over linefeeds
    t.attr.TrimLeadingSpaces();
    t.has_attributes = 1;
  }
}

void gxsHTML::Copy(const gxsHTML &ob)
// Member function used during copying and assignment.
{
  tag_list.ClearList();
  gxListNode<gxsHTMLTagInfo> *ptr = ob.tag_list.GetHead();
  while(ptr) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    tag_list.Add(ptr->data);
    ptr = ptr->next;
  }
  num_tags = ob.num_tags;
  num_processed = ob.num_processed;
  list_ptr = ob.list_ptr;
  err_string = ob.err_string;

  // 11/29/2006: Added to process scripts in HTML files
  num_scripts = ob.num_scripts;
  num_scripts_processed = ob.num_scripts_processed;
  process_scripts = ob.process_scripts;
  script_list.ClearList();
  gxListNode<gxsScriptInfo> *sptr = ob.script_list.GetHead();
  while(sptr) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    script_list.Add(sptr->data);
    sptr = sptr->next;
  }

  // 04/24/2007: Added to process embedded style sheets
  num_style_sheets = ob.num_style_sheets;
  num_style_sheets_processed = ob.num_style_sheets_processed;
  process_style_sheets = ob.process_style_sheets;
  style_sheet_list.ClearList();
  gxListNode<gxsStyleInfo> *cssptr = ob.style_sheet_list.GetHead();
  while(cssptr) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    style_sheet_list.Add(cssptr->data);
    cssptr = cssptr->next;
  }
}

// Derived class interface used to process tags
void gxsHTML::Handle_INVALID_Tag()
{
  // Override to handle INVALID tags
}

void gxsHTML::Handle_UNKNOWN_Tag()
{
  // Override to handle UNKNOWN tags
}

void gxsHTML::Handle_UNKNOWN_SPECIAL_Tag()
{
  // Override to handle unknown special tags starting 
  // with an ampersand ending in a semicolon &xxxx;
}

void gxsHTML::Handle_COMMENT_Tag()
{
  // Override to handle COMMENT tags
}


void gxsHTML::Handle_LESS_THEN_Tag()
{
  // Override to handle a less than sign "&lt;" 
}

void gxsHTML::Handle_GREATER_THEN_Tag() 
{
  // Override to handle a greater than sign "&gt;"
}

void gxsHTML::Handle_AMPERSAND_Tag()
{
  // Override to handle an ampersand "&amp;" 
}

void gxsHTML::Handle_NB_SPACE_Tag()
{
  // Override to handle a non-breaking space "&nbsp;"
}

void gxsHTML::Handle_QUOTE_Tag() 
{
  // Override to handle a quotation mark "&quot;" 
}

void gxsHTML::Handle_EX_ASCII_Tag()
{
  // Override to handle the extended ASCII character set "&#"
}

void gxsHTML::Handle_A_Tag()
{
  // Override to handle ANCHOR tags
}

void gxsHTML::Handle_ABBREV_Tag()
{
  // Override to handle ABBREVIATION tags
}

void gxsHTML::Handle_ACRONYM_Tag()
{
  // Override to handle ACRONYM tags
}

void gxsHTML::Handle_ADDRESS_Tag()
{
  // Override to handle ADDRESS tags
}

void gxsHTML::Handle_APPLET_Tag()
{
  // Override to handle JAVA APPLET tag
}

void gxsHTML::Handle_AREA_Tag()
{
  // Override to handle AREA tags
}

void gxsHTML::Handle_AU_Tag()
{
  // Override to handle AUTHOR tags
}

void gxsHTML::Handle_AUTHOR_Tag()
{
  // Override to handle AUTHOR tags
}

void gxsHTML::Handle_B_Tag()
{
  // Override to handle BOLD tags
}

void gxsHTML::Handle_BANNER_Tag()
{
  // Override to handle BANNER tags
}

void gxsHTML::Handle_BASE_Tag()
{
  // Override to handle BASE tags
}

void gxsHTML::Handle_BASEFONT_Tag()
{
  // Override to handle BASE FONT
}

void gxsHTML::Handle_BDO_Tag()
{
  // Override to handle BDO tags
}

void gxsHTML::Handle_BGSOUND_Tag()
{
  // Override to handle BACKGROUND SOUND tags
}

void gxsHTML::Handle_BIG_Tag()
{
  // Override to handle BIG text
}

void gxsHTML::Handle_BLINK_Tag()
{
  // Override to handle BLINK tags
}

void gxsHTML::Handle_BLOCKQUOTE_Tag()
{
  // Override to handle BLOCK QUOTE tags
}

void gxsHTML::Handle_BQ_Tag()
{
  // Override to handle BLOCK QUOTE tags
}

void gxsHTML::Handle_BODY_Tag()
{
  // Override to handle BODY tags
}

void gxsHTML::Handle_BR_Tag()
{
  // Override to handle LINE BREAK tags
}

void gxsHTML::Handle_BUG_Tag()
{
  // Override to handle BUG tags
}

void gxsHTML::Handle_CAPTION_Tag()
{
  // Override to handle CAPTION tags
}

void gxsHTML::Handle_CENTER_Tag()
{
  // Override to handle CENTER tags
}

void gxsHTML::Handle_CITE_Tag()
{
  // Override to handle CITATION tags
}

void gxsHTML::Handle_CODE_Tag()
{
  // Override to handle CODE tags
}

void gxsHTML::Handle_COL_Tag()
{
  // Override to handle TABLE Cols tags
}

void gxsHTML::Handle_COLGROUP_Tag()
{
  // Override to handle TABLE Cols tags
}

void gxsHTML::Handle_COMMENTS_Tag()
{
  // Override to handle COMMENTS tags
}

void gxsHTML::Handle_CREDIT_Tag()
{
  // Override to handle CREDIT tags
}

void gxsHTML::Handle_DEL_Tag()
{
  // Override to handle DELETED text tags
}

void gxsHTML::Handle_DFN_Tag()
{
  // Override to handle DEFINITION tags
}

void gxsHTML::Handle_DIR_Tag()
{
  // Override to handle DIRECTORY list tags
}

void gxsHTML::Handle_DIV_Tag()
{
  // Override to handle DIVISION tags
}

void gxsHTML::Handle_DL_Tag()
{
  // Override to handle DEFINITION list tags
}

void gxsHTML::Handle_DT_Tag()
{
  // Override to handle DEFINITION term tags
}

void gxsHTML::Handle_DD_Tag()
{
  // Override to handle DEFINITION tags
}

void gxsHTML::Handle_EM_Tag()
{
  // Override to handle EMPHASIZED tags
}

void gxsHTML::Handle_EMBED_Tag()
{
  // Override to handle EMBED tags
}

void gxsHTML::Handle_FIELDSET_Tag()
{
  // Override to handle FIELDSET tags
}

void gxsHTML::Handle_FIG_Tag()
{
  // Override to handle FIGURE tags
}

void gxsHTML::Handle_FN_Tag()
{
  // Override to handle FOOTNOTE tags
}

void gxsHTML::Handle_FONT_Tag()
{
  // Override to handle FONT tags
}

void gxsHTML::Handle_FORM_Tag()
{
  // Override to handle FORM tags
}

void gxsHTML::Handle_FRAME_Tag()
{
  // Override to handle FRAME tags
}

void gxsHTML::Handle_FRAMESET_Tag()
{
  // Override to handle FRAME sets
}

void gxsHTML::Handle_H1_Tag()
{
  // Override to handle HEADING 1 tags
}

void gxsHTML::Handle_H2_Tag()
{
  // Override to handle HEADING 2 tags
}

void gxsHTML::Handle_H3_Tag()
{
  // Override to handle HEADING 3 tags
}

void gxsHTML::Handle_H4_Tag()
{
  // Override to handle HEADING 4 tags
}

void gxsHTML::Handle_H5_Tag()
{
  // Override to handle HEADING 5 tags
}

void gxsHTML::Handle_H6_Tag()
{
  // Override to handle HEADING 6 tags
}

void gxsHTML::Handle_HEAD_Tag()
{
  // Override to handle HEAD tags
}

void gxsHTML::Handle_HR_Tag()
{
  // Override to handle HORIZONTAL rules
}

void gxsHTML::Handle_HTML_Tag()
{
  // Override to handle HTML tags
}

void gxsHTML::Handle_I_Tag()
{
  // Override to handle ITALIC tags
}

void gxsHTML::Handle_IFRAME_Tag()
{
  // Override to handle FRAME - Floating tag
}

void gxsHTML::Handle_ILAYER_Tag()
{
  // Override to handle ILAYER tags
}

void gxsHTML::Handle_IMG_Tag()
{
  // Override to handle INLINE images
}

void gxsHTML::Handle_INPUT_Tag()
{
  // Override to handle FORM input tags
}

void gxsHTML::Handle_INS_Tag()
{
  // Override to handle INSERTED text
}

void gxsHTML::Handle_ISINDEX_Tag()
{
  // Override to handle ISINDEX tag
}

void gxsHTML::Handle_KBD_Tag()
{
  // Override to handle KEYBOARD tags
}

void gxsHTML::Handle_LANG_Tag()
{
  // Override to handle LANGUAGE tags
}

void gxsHTML::Handle_LAYER_Tag()
{
  // Override to handle LAYER tags
}

void gxsHTML::Handle_LEGEND_Tag()
{
  // Override to handle LEGEND tags
}

void gxsHTML::Handle_LH_Tag()
{
  // Override to handle LIST header tags
}

void gxsHTML::Handle_LI_Tag()
{
  // Override to handle LIST item tags
}

void gxsHTML::Handle_LINK_Tag()
{
  // Override to handle LINK tags
}

void gxsHTML::Handle_LISTING_Tag()
{
  // Override to handle LISTING tags
}

void gxsHTML::Handle_MAP_Tag()
{
  // Override to handle MAP tags
}

void gxsHTML::Handle_MARQUEE_Tag()
{
  // Override to handle MARQUEE tags
}

void gxsHTML::Handle_MATH_Tag()
{
  // Override to handle MATH tags
}

void gxsHTML::Handle_MENU_Tag()
{
  // Override to handle MENU list tags
}

void gxsHTML::Handle_META_Tag()
{
  // Override to handle META tags
}

void gxsHTML::Handle_MULTICOL_Tag()
{
  // Override to handle MULTI COLUMN tags
}

void gxsHTML::Handle_NOBR_Tag()
{
  // Override to handle NO BREAK tags
}

void gxsHTML::Handle_NOEMBED_Tag()
{
  // Override to handle NO EMBED tags
}

void gxsHTML::Handle_NOFRAMES_Tag()
{
  // Override to handle NO FRAMES tags
}

void gxsHTML::Handle_NOLAYER_Tag()
{
  // Override to handle NO LAYER tags
}

void gxsHTML::Handle_NOSCRIPT_Tag()
{
  // Override to handle NO SCRIPT tags
}

void gxsHTML::Handle_NOTE_Tag()
{
  // Override to handle NOTE tags
}

void gxsHTML::Handle_OBJECT_Tag()
{
  // Override to handle OBJECT tags
}

void gxsHTML::Handle_OL_Tag()
{
  // Override to handle ORDERED list tags
}

void gxsHTML::Handle_OPTGROUP_Tag()
{
  // Override to handle OPTGROUP tags
}

void gxsHTML::Handle_OPTION_Tag()
{
  // Override to handle OPTION tags
}

void gxsHTML::Handle_OVERLAY_Tag()
{
  // Override to handle OVERLAY tags
}

void gxsHTML::Handle_P_Tag()
{
  // Override to handle PARAGRAPH tags
}

void gxsHTML::Handle_PARAM_Tag()
{
  // Override to handle PARAMETERS tags
}

void gxsHTML::Handle_PERSON_Tag()
{
  // Override to handle PERSON tags
}

void gxsHTML::Handle_PLAINTEXT_Tag()
{
  // Override to handle PLAIN text tags
}

void gxsHTML::Handle_PRE_Tag()
{
  // Override to handle PREFORMATTED text tags
}

void gxsHTML::Handle_Q_Tag()
{
  // Override to handle QUOTE tags
}

void gxsHTML::Handle_RANGE_Tag()
{
  // Override to handle RANGE tags
}

void gxsHTML::Handle_RT_Tag()
{
  // Override to handle RT tags
}

void gxsHTML::Handle_RUBY_Tag()
{
  // Override to handle RUBY tags
}

void gxsHTML::Handle_S_Tag()
{
  // Override to handle S tags
}

void gxsHTML::Handle_SAMP_Tag()
{
  // Override to handle SAMPLE tags
}

void gxsHTML::Handle_SCRIPT_Tag()
{
  // Override to handle SCRIPT tags
}

void gxsHTML::Handle_SELECT_Tag()
{
  // Override to handle FORM SELECT tags
}

void gxsHTML::Handle_SMALL_Tag()
{
  // Override to handle SMALL text tags
}

void gxsHTML::Handle_SPACER_Tag()
{
  // Override to handle WHITE SPACE tags
}

void gxsHTML::Handle_SPAN_Tag()
{
  // Override to handle SPAN tags
}

void gxsHTML::Handle_SPOT_Tag()
{
  // Override to handle SPOT tags
}

void gxsHTML::Handle_STRIKE_Tag()
{
  // Override to handle STRIKETHROUGH tags
}

void gxsHTML::Handle_STRONG_Tag()
{
  // Override to handle STRONG tags
}

void gxsHTML::Handle_STYLE_Tag()
{
  // Override to handle STYLE tags
}

void gxsHTML::Handle_SUB_Tag()
{
  // Override to handle SUBSCRIPT tags
}

void gxsHTML::Handle_SUP_Tag()
{
  // Override to handle SUPERSCRIPT tags
}

void gxsHTML::Handle_TAB_Tag()
{
  // Override to handle HORIZONTAL TABS tags
}

void gxsHTML::Handle_TABLE_Tag()
{
  // Override to handle TABLE tags
}

void gxsHTML::Handle_TBODY_Tag()
{
  // Override to handle TABLE body tags
}

void gxsHTML::Handle_TD_Tag()
{
  // Override to handle TABLE data tags
}

void gxsHTML::Handle_TEXTAREA_Tag()
{
  // Override to handle FORM form tags
}

void gxsHTML::Handle_TEXTFLOW_Tag()
{
  // Override to handle JAVA applet textflow
}

void gxsHTML::Handle_TFOOT_Tag()
{
  // Override to handle TABLE footer tags
}

void gxsHTML::Handle_TH_Tag()
{
  // Override to handle TABLE head
}

void gxsHTML::Handle_THEAD_Tag()
{
  // Override to handle TABLE head tag
}

void gxsHTML::Handle_TITLE_Tag()
{
  // Override to handle TITLE tags
}

void gxsHTML::Handle_TR_Tag()
{
  // Override to handle TABLE row tags 
}

void gxsHTML::Handle_TT_Tag()
{
  // Override to handle TELETYPE tags
}

void gxsHTML::Handle_U_Tag()
{
  // Override to handle UNDERLINED tags
}

void gxsHTML::Handle_UL_Tag()
{
  // Override to handle UNORDERED list tags
}

void gxsHTML::Handle_VAR_Tag()
{
  // Override to handle VARIABLE tags 
}

void gxsHTML::Handle_WBR_Tag()
{
  // Override to handle WORD BREAK tags
}

void gxsHTML::Handle_XML_Tag()
{
  // Override to handle XML tags
}

void gxsHTML::Handle_XMP_Tag()
{
  // Override to handle EXAMPLE tags
}

void gxsHTML::HandleHTMLTag(int tag_id)
// Routine used to execute the derived class version
// of a specific tag handler.
{
  switch(tag_id) {
    case gxs_invalid_tag :    // Invalid tag specified
      Handle_INVALID_Tag();
      break;
      
    case gxs_unknown_tag :    // Unknown tag specified
      Handle_UNKNOWN_Tag();
      break;

    case gxs_special_tag :    // Unknown special meaning tag
      Handle_UNKNOWN_SPECIAL_Tag();
      break;
      
    case gxs_comment_tag :     // Comments
      Handle_COMMENT_Tag();
      break;
      
    case gxs_less_then :       // Less than sign "&lt;" 
      Handle_LESS_THEN_Tag();
      break;

    case gxs_greater_then :    // Greater then sign "&gt;"
      Handle_GREATER_THEN_Tag();
      break;

    case gxs_ampersand :       // Ampersand "&amp;"
      Handle_AMPERSAND_Tag();
      break;

    case gxs_nb_space :        // Non-breaking space "&nbsp;"
      Handle_NB_SPACE_Tag();
      break;

    case gxs_quote :           // Quotation mark "&quot;"
      Handle_QUOTE_Tag();
      break;

    case gxs_ex_acsii_set :    // Extended ASCII character set
      Handle_EX_ASCII_Tag();
      break;

    case gxs_a_tag :           // anchor
      Handle_A_Tag();
      break;
      
    case gxs_abbrev_tag :      // abbreviation
      Handle_ABBREV_Tag();
      break;
      
    case gxs_acronym_tag :     // acronym
      Handle_ACRONYM_Tag();
      break;
      
    case gxs_address_tag :     // address
      Handle_ADDRESS_Tag();
      break;
      
    case gxs_applet_tag :      // java applet
      Handle_APPLET_Tag();
      break;
      
    case gxs_area_tag :        // area
      Handle_AREA_Tag();
      break;
      
    case gxs_au_tag :          // author
      Handle_AU_Tag();
      break;
      
    case gxs_author_tag :      // author
      Handle_AUTHOR_Tag();
      break;
      
    case gxs_b_tag :           // bold
      Handle_B_Tag();
      break;
      
    case gxs_banner_tag :      // banner
      Handle_BANNER_Tag();
      break;
      
    case gxs_base_tag :        // base
      Handle_BASE_Tag();
      break;
      
    case gxs_basefont_tag :    // base font
      Handle_BASEFONT_Tag();
      break;
      
    case gxs_bdo_tag :         // bdo
      Handle_BDO_Tag();
      break;

    case gxs_bgsound_tag :     // background sound
      Handle_BGSOUND_Tag();
      break;
      
    case gxs_big_tag :         // big text
      Handle_BIG_Tag();
      break;
      
    case gxs_blink_tag :       // blink
      Handle_BLINK_Tag();
      break;
      
    case gxs_blockquote_tag :  // block quote
      Handle_BLOCKQUOTE_Tag();
      break;
      
    case gxs_bq_tag :          // block quote
      Handle_BQ_Tag();
      break;
      
    case gxs_body_tag :        // body
      Handle_BODY_Tag();
      break;
      
    case gxs_br_tag :          // line break
      Handle_BR_Tag();
      break;
      
    case gxs_bug_tag :         // bug 
      Handle_BUG_Tag();
      break;

    case gxs_caption_tag :     // caption
      Handle_CAPTION_Tag();
      break;
      
    case gxs_center_tag :      // center
      Handle_CENTER_Tag();
      break;
      
    case gxs_cite_tag :        // citation
      Handle_CITE_Tag();
      break;
      
    case gxs_code_tag :        // code
      Handle_CODE_Tag();
      break;
      
    case gxs_col_tag :         // table column
      Handle_COL_Tag();
      break;
      
    case gxs_colgroup_tag :    // table column group
      Handle_COLGROUP_Tag();
      break;
      
    case gxs_comments_tag :    // comments tag
      Handle_COMMENTS_Tag();
      break;

    case gxs_credit_tag :      // credit
      Handle_CREDIT_Tag();
      break;
      
    case gxs_del_tag :         // deleted text
      Handle_DEL_Tag();
      break;
      
    case gxs_dfn_tag :         // definition
      Handle_DFN_Tag();
      break;
      
    case gxs_dir_tag :         // directory list
      Handle_DIR_Tag();
      break;
      
    case gxs_div_tag :         // division
      Handle_DIV_Tag();
      break;
      
    case gxs_dl_tag :          // definition list
      Handle_DL_Tag();
      break;
      
    case gxs_dt_tag :          // definition term
      Handle_DT_Tag();
      break;
      
    case gxs_dd_tag :          // definition definition
      Handle_DD_Tag();
      break;
      
    case gxs_em_tag :          // emphasized
      Handle_EM_Tag();
      break;
      
    case gxs_embed_tag :       // embed
      Handle_EMBED_Tag();
      break;
      
    case gxs_fieldset_tag :    // fieldset
      Handle_FIELDSET_Tag();
      break;

    case gxs_fig_tag :         // figure
      Handle_FIG_Tag();
      break;
      
    case gxs_fn_tag :          // footnote
      Handle_FN_Tag();
      break;
      
    case gxs_font_tag :        // font
      Handle_FONT_Tag();
      break;
      
    case gxs_form_tag :        // form
      Handle_FORM_Tag();
      break;
      
    case gxs_frame_tag :       // frame
      Handle_FRAME_Tag();
      break;
      
    case gxs_frameset_tag :    // frame set
      Handle_FRAMESET_Tag();
      break;
      
    case gxs_h1_tag :          // heading 1
      Handle_H1_Tag();
      break;
      
    case gxs_h2_tag :          // heading 2
      Handle_H2_Tag();
      break;
      
    case gxs_h3_tag :          // heading 3
      Handle_H3_Tag();
      break;
      
    case gxs_h4_tag :          // heading 4
      Handle_H4_Tag();
      break;
      
    case gxs_h5_tag :          // heading 5
      Handle_H5_Tag();
      break;
      
    case gxs_h6_tag :          // heading 6
      Handle_H6_Tag();
      break;
      
    case gxs_head_tag :        // head
      Handle_HEAD_Tag();
      break;
      
    case gxs_hr_tag :          // horizontal rule
      Handle_HR_Tag();
      break;
      
    case gxs_html_tag :        // html
      Handle_HTML_Tag();
      break;
      
    case gxs_i_tag :           // italic
      Handle_I_Tag();
      break;
      
    case gxs_iframe_tag :      // frame - floating
      Handle_IFRAME_Tag();
      break;
      
    case gxs_ilayer_tag :      // ilayer
      Handle_ILAYER_Tag();
      break;

    case gxs_img_tag :         // inline image
      Handle_IMG_Tag();
      break;
      
    case gxs_input_tag :       // form input
      Handle_INPUT_Tag();
      break;
      
    case gxs_ins_tag :         // inserted text
      Handle_INS_Tag();
      break;
      
    case gxs_isindex_tag :     // is index
      Handle_ISINDEX_Tag();
      break;
      
    case gxs_kbd_tag :         // keyboard
      Handle_KBD_Tag();
      break;
      
    case gxs_lang_tag :        // language
      Handle_LANG_Tag();
      break;
      
    case gxs_layer_tag :       // layer
      Handle_LAYER_Tag();
      break;

    case gxs_legend_tag :      // legend
      Handle_LEGEND_Tag();
      break;

    case gxs_lh_tag :          // list heading
      Handle_LH_Tag();
      break;
      
    case gxs_li_tag :          // list item
      Handle_LI_Tag();
      break;
      
    case gxs_link_tag :        // link
      Handle_LINK_Tag();
      break;
      
    case gxs_listing_tag :     // listing
      Handle_LISTING_Tag();
      break;
      
    case gxs_map_tag :         // map
      Handle_MAP_Tag();
      break;
      
    case gxs_marquee_tag :     // marquee
      Handle_MARQUEE_Tag();
      break;
      
    case gxs_math_tag :        // math
      Handle_MATH_Tag();
      break;
      
    case gxs_menu_tag :        // menu list
      Handle_MENU_Tag();
      break;
      
    case gxs_meta_tag :        // meta
      Handle_META_Tag();
      break;
      
    case gxs_multicol_tag :    // multi column text
      Handle_MULTICOL_Tag();
      break;
      
    case gxs_nobr_tag :        // no break
      Handle_NOBR_Tag();
      break;

    case gxs_noembed_tag :     // no embed
      Handle_NOEMBED_Tag();
      break;
      
    case gxs_noframes_tag :    // no frames
      Handle_NOFRAMES_Tag();
      break;
      
    case gxs_nolayer_tag :     // no layer
      Handle_NOLAYER_Tag();
      break;

    case gxs_noscript_tag :    // no script
      Handle_NOSCRIPT_Tag();
      break;

    case gxs_note_tag :        // note
      Handle_NOTE_Tag();
      break;
      
    case gxs_object_tag :      // object
      Handle_OBJECT_Tag();
      break;

    case gxs_optgroup_tag :    // option group
      Handle_OPTGROUP_Tag();
      break;

    case gxs_option_tag :      // option
      Handle_OPTION_Tag();
      break;

    case gxs_ol_tag :          // ordered list
      Handle_OL_Tag();
      break;
      
    case gxs_overlay_tag :     // overlay
      Handle_OVERLAY_Tag();
      break;
      
    case gxs_p_tag :           // paragraph
      Handle_P_Tag();
      break;
      
    case gxs_param_tag :       // parameters
      Handle_PARAM_Tag();
      break;
      
    case gxs_person_tag :      // person
      Handle_PERSON_Tag();
      break;
      
    case gxs_plaintext_tag :   // plain text
      Handle_PLAINTEXT_Tag();
      break;
      
    case gxs_pre_tag :         // preformatted text
      Handle_PRE_Tag();
      break;
      
    case gxs_q_tag :           // quote
      Handle_Q_Tag();
      break;
      
    case gxs_range_tag :       // range
      Handle_RANGE_Tag();
      break;
      
    case gxs_rt_tag :          // ruby text
      Handle_RT_Tag();
      break;

    case gxs_ruby_tag :        // ruby
      Handle_RUBY_Tag();
      break;

    case gxs_s_tag :          // strikethrough
      Handle_S_Tag();
      break;

    case gxs_samp_tag :        // sample
      Handle_SAMP_Tag();
      break;
      
    case gxs_script_tag :      // script
      Handle_SCRIPT_Tag();
      break;
      
    case gxs_select_tag :      // form select
      Handle_SELECT_Tag();
      break;
      
    case gxs_small_tag :       // small text
      Handle_SMALL_Tag();
      break;
      
    case gxs_spacer_tag :      // white space
      Handle_SPACER_Tag();
      break;
      
    case gxs_span_tag :        // span
      Handle_SPAN_Tag();
      break;

    case gxs_spot_tag :        // spot
      Handle_SPOT_Tag();
      break;
      
    case gxs_strike_tag :      // strikethrough
      Handle_STRIKE_Tag();
      break;
      
    case gxs_strong_tag :      // strong
      Handle_STRONG_Tag();
      break;

    case gxs_style_tag :       // style
      Handle_STYLE_Tag();
      break;
      
    case gxs_sub_tag :         // subscript
      Handle_SUB_Tag();
      break;
      
    case gxs_sup_tag :         // superscript
      Handle_SUP_Tag();
      break;
      
    case gxs_tab_tag :         // horizontal tab
      Handle_TAB_Tag();
      break;
      
    case gxs_table_tag :       // table
      Handle_TABLE_Tag();
      break;
      
    case gxs_tbody_tag :       // table body
      Handle_TBODY_Tag();
      break;
      
    case gxs_td_tag :          // table data
      Handle_TD_Tag();
      break;
      
    case gxs_textarea_tag :    // form text area
      Handle_TEXTAREA_Tag();
      break;
      
    case gxs_textflow_tag :    // java applet textflow
      Handle_TEXTFLOW_Tag();
      break;
      
    case gxs_tfoot_tag :       // table footer
      Handle_TFOOT_Tag();
      break;
      
    case gxs_th_tag :          // table header
      Handle_TH_Tag();
      break;
      
    case gxs_thead_tag :       // table head
      Handle_THEAD_Tag();
      break;
      
    case gxs_title_tag :       // title
      Handle_TITLE_Tag();
      break;
      
    case gxs_tr_tag :          // table row
      Handle_TR_Tag();
      break;
      
    case gxs_tt_tag :          // teletype
      Handle_TT_Tag();
      break;
      
    case gxs_u_tag :           // underlined
      Handle_U_Tag();
      break;
      
    case gxs_ul_tag :          // unordered list
      Handle_UL_Tag();
      break;
      
    case gxs_var_tag :         // variable
      Handle_VAR_Tag();
      break;
      
    case gxs_wbr_tag :         // word break
      Handle_WBR_Tag();
      break;

    case gxs_xml_tag :         // xml
      Handle_XML_Tag();
      break;

    case gxs_xmp_tag :         // example
      Handle_XMP_Tag();
      break;

    default :
      Handle_INVALID_Tag();
      break;
  }
}

int gxsHTML::LoadMemoryBuffer(const MemoryBuffer &membuf)
{
  int rv = CollectHTMLTags(membuf);
  if(rv) return rv;

  rv = ProcessHTMLTags(membuf);
  if(rv) return rv;

  return 0;
}

int gxsHTML::CollectHTMLTags(const MemoryBuffer &membuf)
{
  int i;
  num_tags = 0;
  num_scripts = 0;
  num_style_sheets = 0;
  int found_open_tag = 0;
  int looking_for_tag_end = 0;
  char prev_tag = 0;
  int start_of_comment = 0;
  // int end_of_comment = 0;
  FAU_t curr_pos = (FAU_t)0;
  err_string.Clear();
  char curr_open_tag;
  gxString sbuf;
  gxString tag_buf; // Temp buffer used to process scripts and embedded CSS
  
  tag_list.ClearList(); // Clear the tag list
  script_list.ClearList(); // Clear the script list
  style_sheet_list.ClearList(); // Clear the style sheet list
  char *start = (char *)membuf.m_buf();
  unsigned len = membuf.length();

  gxsHTMLTagInfo tag;
  gxsScriptInfo script;
  gxsStyleInfo style_sheet;

  // 04/25/2007: Added to skip script and CSS content
  int in_script = 0;
  int in_style_sheet = 0;

  char c; unsigned pos;
  for(pos = 0; pos < len; pos++, start++) {
    c = *start;
    switch(c) {
      case '<' : case '&' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) {
	  // 04/25/2007: Skip embedded scripts and CSS
	  tag_buf.Clear();
	  tag_buf << c;
	  break;
	}

	if(start_of_comment) {
	  // Ignore all tags until the end of the comment tag
	  break;
	}

	// 04/25/2007: Account for nested tags for example:
	// &amp; tag nested inside <a href=""> tags
	if(found_open_tag) break;

	if(c == '<') {
	  prev_tag = '<'; 
	}
	else {
	  prev_tag = '&';
	}

	if(!looking_for_tag_end) { 
	  found_open_tag = 1;
	  looking_for_tag_end = 1;
	  tag.start_tag = pos;
	  if(tag.start_tag > (FAU_t)0) tag.start_tag--;
	  curr_open_tag = c;

	  // 11/29/2006: Clear the tag buffer
	  tag_buf.Clear();

	  // 04/25/2007: Record the open tag so the complete tag 
	  // can be parsed.
	  tag_buf << c;
	}
	break;

	// 07/30/2006: Code added to parse comments with
	// nested tags.
      case '!' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;
	if(start_of_comment) break;
	if(prev_tag == '<') prev_tag = '!';
	break;

      case '-' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;
	if(prev_tag == '!') { // Found the start of a comment tag
	  pos++;
	  if(pos >= len) break;
	  start++;
	  c = *start;
	  if(c == '-') {
	    start_of_comment = 1;  
	    prev_tag = 0;
	  }
	}
	if(start_of_comment) {
	  curr_pos = pos;
	  pos++;
	  if(pos >= len) break;
	  start++;
	  c = *start;

	  if(c == '-') {
	    pos++;
	    if(pos >= len) break;
	    start++;
	    c = *start;
	  }
	  while(c == ' ') {
	    // Eat spaces
	    pos++;
	    if(pos >= len) break;
	    start++;
	    c = *start;
	  }
	  while(c == '-') {
	    // 07/24/2007: Eat extra dashes 
	    pos++;
	    if(pos >= len) break;
	    start++;
	    c = *start;
	  }

	  if(c == '>') { // Found the end of the comment tag
	    start_of_comment = 0;  
	    prev_tag = 0;
	    tag.end_tag = pos;
	    num_tags++;
      
	    // Calculate the length of the tag including the angle brackets
	    tag.tag_length = unsigned(tag.end_tag - tag.start_tag);
      
	    tag_list.Add(tag); // Add the tag to the list
	    tag.Clear();
	    found_open_tag = 0;
	    looking_for_tag_end = 0;
	  }
	}
	break;

      case '#' : // Look for extended ASCII characters denoted by: "&#xxx;"
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;
	// 06/02/2002: Corrected BCC32 incorrect assignment warning
	if((prev_tag == '&') && (tag.start_tag == FAU_t(pos - 1))) {
	  if(found_open_tag) { // Look for the end of a tag
	    // Get the next three numeric characters following a "&#" tag
	    // plus the terminating semicolon or space.
	    if((pos+4) >= len) {
	      pos++;
	      break;
	    }
	    for(i = 0; i < 4; i++, start++, pos++) { 
	      c = *start;
	      if((c == ';') || (c == ' ')) break;
	    }
	    tag.start_tag--; // Account for the ampersand
	    tag.end_tag = pos;
	    num_tags++;
	    
	    // Calculate the length of the tag
	    tag.tag_length = unsigned(tag.end_tag - tag.start_tag);
	    
	    tag_list.Add(tag); // Add the tag to the list
	    tag.Clear();
	    found_open_tag = 0;
	    looking_for_tag_end = 0;
	  }
	} 
	break;

      case '>' : case  ';' :
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) { 
	  // Allow the open tag to close
	  if(!found_open_tag) break; 
	}

	if(start_of_comment) break;

	// 09/30/2002: Account for tags using the ; char inside < > tags
	if((c == ';') && (prev_tag != '&')) break;
	if(c == '>') {
	  prev_tag = '>'; 
	}
	else {
	  prev_tag = ';';
	}
	if(found_open_tag) { // Look for the end of a tag
	  tag.end_tag = pos;
	  num_tags++;
      
	  // Calculate the length of the tag including the angle brackets
	  tag.tag_length = unsigned(tag.end_tag - tag.start_tag);
      
	  tag_list.Add(tag); // Add the tag to the list
	  tag.Clear();
	  found_open_tag = 0;
	  looking_for_tag_end = 0;
	}
	break;

      case ' ': case '\n': case '\r': 
	// 04/25/2007: Skip embedded script and CSS content
	if((in_script) || (in_style_sheet)) break;

	// Look for chars that might be confused with a tag
	if((found_open_tag) && (looking_for_tag_end)) {
	  if(curr_open_tag == '&') {
	    // The & character might be used in text without proper formatting
	    found_open_tag = 0;
	    looking_for_tag_end = 0;
	    prev_tag = 0;
	  }
	}
	break;

      default:
	// 11/29/2006: Added to capture the current tag for any
	// additional processing.
	tag_buf << c;
	tag_buf.ToUpper();

	// 11/29/2006: Added to record script code file positions
	// 04/25/2007: Modifed to catch all variations of the script tag
	if((tag_buf == "<SCRIPT") || (tag_buf == "< SCRIPT")) {
	  script.script_start = pos;
	  in_script = 1;
	}
	// 04/25/2007: Modifed to catch all variations of the script tag
	if((tag_buf == "</SCRIPT") || (tag_buf == "< /SCRIPT") || 
	   (tag_buf == "< / SCRIPT") || (tag_buf == "</ SCRIPT")){
	  if(tag_buf == "</SCRIPT") sbuf = "/SCRIPT";
	  if(tag_buf == "< /SCRIPT") sbuf = " /SCRIPT";
	  if(tag_buf == "< / SCRIPT") sbuf = " / SCRIPT";
	  if(tag_buf == "</ SCRIPT") sbuf = "/ SCRIPT";
	  script.script_end = pos;
	  script.script_length = script.script_end - script.script_start;
	  script.script_length -= sbuf.length() + 1; // Remove </script tag
	  script_list.Add(script);
	  num_scripts++;
	  script.Clear();
	  in_script = 0;
	}

	// 04/24/2007: Added to record style sheet file positions
	// 04/25/2007: Modifed to catch all variations of the style tag
	if((tag_buf == "<STYLE") || (tag_buf == "< STYLE")) {
	  style_sheet.style_start = pos;
	  in_style_sheet = 1;
	}
	// 04/25/2007: Modifed to catch all variations of the style tag
	if((tag_buf == "</STYLE") || (tag_buf == "< /STYLE") || 
	   (tag_buf == "< / STYLE") || (tag_buf == "</ STYLE")){
	  if(tag_buf == "</STYLE") sbuf = "/STYLE";
	  if(tag_buf == "< /STYLE") sbuf = " /STYLE";
	  if(tag_buf == "< / STYLE") sbuf = " / STYLE";
	  if(tag_buf == "</ STYLE") sbuf = "/ STYLE";
	  style_sheet.style_end = pos;
	  style_sheet.style_length = style_sheet.style_end - style_sheet.style_start;
	  style_sheet.style_length -= sbuf.length() + 1; // Remove </style tag
	  style_sheet_list.Add(style_sheet);
	  num_style_sheets++;
	  style_sheet.Clear();
	  in_style_sheet = 0;
	}

	break;
    }
  }
  
  // 09/11/2006: Added to report parsing errors
  if(start_of_comment) {
    err_string << clear << "Document missing closing comment tag." 
	       << "Tag start " << tag.start_tag;
  }
  if(found_open_tag) {
    if(looking_for_tag_end) {
      err_string << clear << "Document missing closing " << prev_tag 
		 << " tag. Tag start " << tag.start_tag;
    }
  }
  
  // 04/25/2007: Added to report parsing errors in scripts and CSS
  if(in_script) {
    err_string << clear << "Document missing closing script tag.";
  }
  if(in_style_sheet) {
    err_string << clear << "Document missing closing style tag.";
  }

  return 0;
}

int gxsHTML::ProcessHTMLTags(const MemoryBuffer &membuf)
// Read and process all the tags in a open file. 
{
  num_processed = 0;
  
  MemoryBuffer mbuf;
  list_ptr = tag_list.GetHead();
  
  char *start = (char *)membuf.m_buf();
  char *tag_pos;
  
  while(list_ptr) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    mbuf.Clear(); // Clear the buffer before each read
    mbuf.resize(list_ptr->data.tag_length);

    // Read the tag starting at the opening angle bracket
    FAU_t pos = list_ptr->data.start_tag;
    tag_pos = start + pos;
    memmove((char *)mbuf.m_buf(), tag_pos, mbuf.length());  
    
    // Store the tag and indentify it
    mbuf += '\0'; // Ensure null termination
    list_ptr->data.tag_info = (char *)mbuf.m_buf();

    // Parse the tag type and attributes
    ParseHTMLTagInfo(list_ptr->data);

    // Excute the derived class version of the tag handler
    HandleHTMLTag(list_ptr->data.tag_id);

    num_processed++;
    list_ptr = list_ptr->next;  
  }

  if(process_scripts) {
    ProcessScripts(membuf);
  }

  if(process_style_sheets) {
    ProcessStyleSheets(membuf);
  }

  return 0;
}

int gxsHTML::ProcessScripts(const MemoryBuffer &membuf)
// Read and process all the tags in a open file. 
{
  num_scripts_processed = 0;
  
  MemoryBuffer mbuf;
  script_ptr = script_list.GetHead();
  
  char *start = (char *)membuf.m_buf();
  char *script_pos;
  
  while(script_ptr) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    mbuf.Clear(); // Clear the buffer before each read
    mbuf.resize(script_ptr->data.script_length);

    // Read the tag starting at the opening angle bracket
    FAU_t pos = script_ptr->data.script_start;
    script_pos = start + pos;
    memmove((char *)mbuf.m_buf(), script_pos, mbuf.length());  
    
    // Store the tag and indentify it
    mbuf += '\0'; // Ensure null termination
    script_ptr->data.script_code = (char *)mbuf.m_buf();

    // Excute the derived class version of the script handler
    Handle_Script(script_ptr->data);

    num_scripts_processed++;
    script_ptr = script_ptr->next;  
  }

  return 0;
}

int gxsHTML::ProcessStyleSheets(const MemoryBuffer &membuf)
// Read and process all the tags in a open file. 
{
  num_style_sheets_processed = 0;
  
  MemoryBuffer mbuf;
  style_sheet_ptr = style_sheet_list.GetHead();
  
  char *start = (char *)membuf.m_buf();
  char *style_sheet_pos;
  
  while(style_sheet_ptr) {
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    mbuf.Clear(); // Clear the buffer before each read
    mbuf.resize(style_sheet_ptr->data.style_length);

    // Read the tag starting at the opening angle bracket
    FAU_t pos = style_sheet_ptr->data.style_start;
    style_sheet_pos = start + pos;
    memmove((char *)mbuf.m_buf(), style_sheet_pos, mbuf.length());  
    
    // Store the tag and indentify it
    mbuf += '\0'; // Ensure null termination
    style_sheet_ptr->data.style_code = (char *)mbuf.m_buf();

    // Excute the derived class version of the style sheet handler
    Handle_StyleSheet(style_sheet_ptr->data);

    num_style_sheets_processed++;
    style_sheet_ptr = style_sheet_ptr->next;  
  }

  return 0;
}

void gxsHTML::Handle_Script(gxsScriptInfo &script)
{
  // Override to handle script code in an HTML file
}


void gxsHTML::Handle_StyleSheet(gxsStyleInfo &style_sheet)
{
  // Override to handle embedded style sheets in an HTML file
}

#ifdef __BCC32__
#pragma warn .8071
#pragma warn .8072
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


