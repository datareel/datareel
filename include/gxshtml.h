// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxshtml.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

The gxsHTML class is a base class used to parse html documents. 

Changes:
==============================================================
03/10/2002: The gxsHTML class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used by 
the gxsHTML class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_HTML_HPP__
#define __GX_HTML_HPP__

#include "gxdlcode.h"

#include "gxstring.h"
#include "gxlist.h"
#include "dfileb.h"
#include "membuf.h"

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(disable:4251) // Disable dll-interface template warning
#endif


// Data structure used to store the file position of an html tag,
// the tag itself, its attributes and instructions.
struct GXDLCODE_API gxsHTMLTagInfo
{
  gxsHTMLTagInfo();
  ~gxsHTMLTagInfo();
  gxsHTMLTagInfo(const gxsHTMLTagInfo &ob) { Copy(ob); }
  gxsHTMLTagInfo& operator=(const gxsHTMLTagInfo &ob) { 
    Copy(ob); return *this; 
  }
  void Copy(const gxsHTMLTagInfo &ob);
  void Clear();
  GXDLCODE_API friend int operator==(const gxsHTMLTagInfo &a, 
				     const gxsHTMLTagInfo &b);

  // File information
  FAU_t start_tag;     // This tag's starting position in the file
  FAU_t end_tag;       // This tag's ending position in the file
  unsigned tag_length; // The complete length of this tag "< ---- >"
  
  // Tag information
  int tag_id;        // Numerical value used to identify supported tags
  gxString tag_info; // The complete tag from opening to closing bracket
  gxString tag;      // HTML tag
  gxString attr;     // HTML tag attributes 

  // Tag instructions
  int start_instruction; // True if start of tag instruction "<"
  int end_instruction;   // True if end of a tag instruction "/x>"
  int has_attributes;    // True if this tag has attributes associated with it
};

// 11/29/2006: Data structure added to store any script code 
struct GXDLCODE_API gxsScriptInfo
{
  gxsScriptInfo();
  ~gxsScriptInfo();
  gxsScriptInfo(const gxsScriptInfo &ob) { Copy(ob); }
  gxsScriptInfo& operator=(const gxsScriptInfo &ob) { 
    Copy(ob); return *this; 
  }
  void Copy(const gxsScriptInfo &ob);
  void Clear();
  GXDLCODE_API friend int operator==(const gxsScriptInfo &a, 
				     const gxsScriptInfo &b);

  gxString script_code;   // Script code for this tag
  FAU_t script_start;     // Script code starting position in the file
  FAU_t script_end;       // Script ending position in the file
  unsigned script_length; // Complete length of this script code
};

// 04/24/2007: Data structure added to handle HTML style tags 
// containing Cascading Style Sheets (CSS) inside of an HTML
// document.
struct GXDLCODE_API gxsStyleInfo
{
  gxsStyleInfo();
  ~gxsStyleInfo();
  gxsStyleInfo(const gxsStyleInfo &ob) { Copy(ob); }
  gxsStyleInfo& operator=(const gxsStyleInfo &ob) { 
    Copy(ob); return *this; 
  }
  void Copy(const gxsStyleInfo &ob);
  void Clear();
  GXDLCODE_API friend int operator==(const gxsStyleInfo &a, 
				     const gxsStyleInfo &b);

  gxString style_code;   // Style code for this tag
  FAU_t style_start;     // Style code starting position in the file
  FAU_t style_end;       // Style ending position in the file
  unsigned style_length; // Complete length of this style code
};
 

// The total number of tags defined in the HTML tags and modifiers
// enumeration. The number must match the number of strings defined 
// in the gxs_SUPPROTED_TAGS array.  
const int gxsMAX_SUPPORTED_TAGS = 136;

class GXDLCODE_API gxsHTML : public DiskFileB
{
public:
  // The following list of HTML tags is a combination of HTML
  // 2.0, 3.0, 3.2, 4.0 tags supported by Netscape's Navigator
  // web browser, Microsoft's Internet Explorer web browser, 
  // and standards defined by the World Wide Web Consortium.
  enum { // HTML tags and modifiers
    gxs_invalid_tag = 0, // Invalid tag specified
    gxs_unknown_tag,     // Unknown tag specified
    gxs_special_tag,     // Unknown special tags starting with an 
                         // ampersand ending in a semicolon &xxxx;

    // Tags and format specifies with special meaning
    gxs_comment_tag,     // comment
    gxs_less_then,       // Less than sign "&lt;" 
    gxs_greater_then,    // Greater then sign "&gt;"
    gxs_ampersand,       // Ampersand "&amp;"
    gxs_nb_space,        // Non-breaking space "&nbsp;"
    gxs_quote,           // Quotation mark "&quot;"
    gxs_ex_acsii_set,    // Extended ASCII character set

    // HTML tag codes
    gxs_a_tag,           // anchor
    gxs_abbrev_tag,      // abbreviation
    gxs_acronym_tag,     // acronym
    gxs_address_tag,     // address
    gxs_applet_tag,      // java applet
    gxs_area_tag,        // area
    gxs_au_tag,          // author
    gxs_author_tag,      // author
    gxs_b_tag,           // bold
    gxs_banner_tag,      // banner
    gxs_base_tag,        // base
    gxs_basefont_tag,    // base font
    gxs_bdo_tag,         // bi-directional override
    gxs_bgsound_tag,     // background sound
    gxs_big_tag,         // big text
    gxs_blink_tag,       // blink
    gxs_blockquote_tag,  // block quote
    gxs_bq_tag,          // block quote
    gxs_body_tag,        // body
    gxs_br_tag,          // line break
    gxs_bug_tag,         // bug tag
    gxs_caption_tag,     // caption
    gxs_center_tag,      // center
    gxs_cite_tag,        // citation
    gxs_code_tag,        // code
    gxs_col_tag,         // table column
    gxs_colgroup_tag,    // table column group
    gxs_comments_tag,    // comments
    gxs_credit_tag,      // credit
    gxs_del_tag,         // deleted text
    gxs_dfn_tag,         // definition
    gxs_dir_tag,         // directory list
    gxs_div_tag,         // division
    gxs_dl_tag,          // definition list
    gxs_dt_tag,          // definition term
    gxs_dd_tag,          // definition definition
    gxs_em_tag,          // emphasized
    gxs_embed_tag,       // embed
    gxs_fieldset_tag,    // fieldset
    gxs_fig_tag,         // figure
    gxs_fn_tag,          // footnote
    gxs_font_tag,        // font
    gxs_form_tag,        // form
    gxs_frame_tag,       // frame
    gxs_frameset_tag,    // frame set
    gxs_h1_tag,          // heading 1
    gxs_h2_tag,          // heading 2
    gxs_h3_tag,          // heading 3
    gxs_h4_tag,          // heading 4
    gxs_h5_tag,          // heading 5
    gxs_h6_tag,          // heading 6
    gxs_head_tag,        // head
    gxs_hr_tag,          // horizontal rule
    gxs_html_tag,        // html
    gxs_i_tag,           // italic
    gxs_iframe_tag,      // frame - floating
    gxs_ilayer_tag,      // ilayer
    gxs_img_tag,         // inline image
    gxs_input_tag,       // form input
    gxs_ins_tag,         // inserted text
    gxs_isindex_tag,     // is index
    gxs_kbd_tag,         // keyboard
    gxs_lang_tag,        // language
    gxs_layer_tag,       // layer
    gxs_legend_tag,      // legend
    gxs_lh_tag,          // list heading
    gxs_li_tag,          // list item
    gxs_link_tag,        // link
    gxs_listing_tag,     // listing
    gxs_map_tag,         // map
    gxs_marquee_tag,     // marquee
    gxs_math_tag,        // math
    gxs_menu_tag,        // menu list
    gxs_meta_tag,        // meta
    gxs_multicol_tag,    // multi column text
    gxs_nobr_tag,        // no break
    gxs_noembed_tag,     // no embed
    gxs_noframes_tag,    // no frames
    gxs_nolayer_tag,     // no layer
    gxs_noscript_tag,    // no script
    gxs_note_tag,        // note
    gxs_object_tag,      // object
    gxs_optgroup_tag,    // opt group
    gxs_option_tag,      // option
    gxs_ol_tag,          // ordered list
    gxs_overlay_tag,     // overlay
    gxs_p_tag,           // paragraph
    gxs_param_tag,       // parameters
    gxs_person_tag,      // person
    gxs_plaintext_tag,   // plain text
    gxs_pre_tag,         // preformatted text
    gxs_q_tag,           // quote
    gxs_range_tag,       // range
    gxs_rt_tag,          // ruby text
    gxs_ruby_tag,        // ruby
    gxs_s_tag,           // strikethrough
    gxs_samp_tag,        // sample
    gxs_script_tag,      // script
    gxs_select_tag,      // form select
    gxs_small_tag,       // small text
    gxs_spacer_tag,      // white space
    gxs_span_tag,        // span
    gxs_spot_tag,        // spot
    gxs_strike_tag,      // strikethrough
    gxs_strong_tag,      // strong
    gxs_style_tag,       // style
    gxs_sub_tag,         // subscript
    gxs_sup_tag,         // superscript
    gxs_tab_tag,         // horizontal tab
    gxs_table_tag,       // table
    gxs_tbody_tag,       // table body
    gxs_td_tag,          // table data
    gxs_textarea_tag,    // form text area
    gxs_textflow_tag,    // java applet textflow
    gxs_tfoot_tag,       // table footer
    gxs_th_tag,          // table header
    gxs_thead_tag,       // table head
    gxs_title_tag,       // title
    gxs_tr_tag,          // table row
    gxs_tt_tag,          // teletype
    gxs_u_tag,           // underlined
    gxs_ul_tag,          // unordered list
    gxs_var_tag,         // variable
    gxs_wbr_tag,         // word break
    gxs_xml_tag,         // xml
    gxs_xmp_tag          // example
  };

public:
  gxsHTML();
  virtual ~gxsHTML();
  gxsHTML(const gxsHTML &ob) { Copy(ob); }
  gxsHTML &operator=(const gxsHTML &ob) { Copy(ob); return *this; }

public:
  int LoadHTMLFile(const char *fname);
  void CloseFile();
  int LoadMemoryBuffer(const MemoryBuffer &membuf);
  unsigned NumTags() { return num_tags; } 
  unsigned NumProcessed() { return num_processed; }
  unsigned NumScripts() { return num_scripts; } 
  unsigned NumScriptsProcessed() { return num_scripts_processed; }
  unsigned NumStyleSheets() { return num_style_sheets; } 
  unsigned NumStyleSheetsProcessed() { return num_style_sheets_processed; }
  void ParseHTMLTagInfo(gxsHTMLTagInfo &t);
  gxList<gxsHTMLTagInfo> *GetTagList() { return &tag_list; }
  gxList<gxsScriptInfo> *GetScriptList() { return &script_list; }
  gxList<gxsStyleInfo> *GetStyleSheetList() { return &style_sheet_list; }
  void ClearTagList();
  char *GetTag(int tag_id);
  const char *GetTag(int tag_id) const;
  int GetTagID(const gxString &tag);
  void HandleHTMLTag(int tag_id);
  
public: // Derived class interface used to process tags
  virtual void Handle_INVALID_Tag();         // Invalid tag handler
  virtual void Handle_UNKNOWN_Tag();         // Unknown tag handler
  virtual void Handle_UNKNOWN_SPECIAL_Tag(); // Unknown special tags 
  virtual void Handle_COMMENT_Tag();         // Comment tag handler

  // Most common special meaning tags
  virtual void Handle_LESS_THEN_Tag();    // Less than sign "&lt;" 
  virtual void Handle_GREATER_THEN_Tag(); // Greater then sign "&gt;"
  virtual void Handle_AMPERSAND_Tag();    // Ampersand "&amp;"
  virtual void Handle_NB_SPACE_Tag();     // Non-breaking space "&nbsp;"
  virtual void Handle_QUOTE_Tag();        // Quotation mark "&quot;" 
  virtual void Handle_EX_ASCII_Tag();     // Extended ASCII character set

  // Known HTML tags
  virtual void Handle_A_Tag();            // Anchor tag handler
  virtual void Handle_ABBREV_Tag();       // Abbreviation tag handler
  virtual void Handle_ACRONYM_Tag();      // Acronym tag handler
  virtual void Handle_ADDRESS_Tag();      // Address tag handler
  virtual void Handle_APPLET_Tag();       // Java Applet tag handler
  virtual void Handle_AREA_Tag();         // Area tag handler
  virtual void Handle_AU_Tag();           // Author tag handler
  virtual void Handle_AUTHOR_Tag();       // Author tag handler
  virtual void Handle_B_Tag();            // Bold tag handler
  virtual void Handle_BANNER_Tag();       // Banner tag handler
  virtual void Handle_BASE_Tag();         // Base tag handler
  virtual void Handle_BASEFONT_Tag();     // Base Font tag handler
  virtual void Handle_BDO_Tag();          // BDO tag handler
  virtual void Handle_BGSOUND_Tag();      // Background Sound tag handler
  virtual void Handle_BIG_Tag();          // Big Text tag handler
  virtual void Handle_BLINK_Tag();        // Blink tag handler
  virtual void Handle_BLOCKQUOTE_Tag();   // Block Quote tag handler
  virtual void Handle_BQ_Tag();           // Block Quote tag handler
  virtual void Handle_BODY_Tag();         // Body tag handler
  virtual void Handle_BR_Tag();           // Line Break tag handler
  virtual void Handle_BUG_Tag();          // Bug tag handler
  virtual void Handle_CAPTION_Tag();      // Caption tag handler
  virtual void Handle_CENTER_Tag();       // Center tag handler
  virtual void Handle_CITE_Tag();         // Citation tag handler
  virtual void Handle_CODE_Tag();         // Code tag handler
  virtual void Handle_COL_Tag();          // Table Column tag handler
  virtual void Handle_COLGROUP_Tag();     // Table Column Group tag handler
  virtual void Handle_COMMENTS_Tag();     // Comments handler
  virtual void Handle_CREDIT_Tag();       // Credit tag handler
  virtual void Handle_DEL_Tag();          // Deleted Text tag handler
  virtual void Handle_DFN_Tag();          // Definition tag handler
  virtual void Handle_DIR_Tag();          // Directory List tag handler
  virtual void Handle_DIV_Tag();          // Division tag handler
  virtual void Handle_DL_Tag();           // Definition List tag handler
  virtual void Handle_DT_Tag();           // Definition Term tag handler
  virtual void Handle_DD_Tag();           // Definition Definition tag handler
  virtual void Handle_EM_Tag();           // Emphasized tag handler
  virtual void Handle_EMBED_Tag();        // Embed tag handler
  virtual void Handle_FIELDSET_Tag();     // Fieldset tag handler
  virtual void Handle_FIG_Tag();          // Figure tag handler
  virtual void Handle_FN_Tag();           // Footnote tag handler
  virtual void Handle_FONT_Tag();         // Font tag handler
  virtual void Handle_FORM_Tag();         // Form tag handler
  virtual void Handle_FRAME_Tag();        // Frame tag handler
  virtual void Handle_FRAMESET_Tag();     // Frame Set tag handler
  virtual void Handle_H1_Tag();           // Heading 1 tag handler
  virtual void Handle_H2_Tag();           // Heading 2 tag handler
  virtual void Handle_H3_Tag();           // Heading 3 tag handler
  virtual void Handle_H4_Tag();           // Heading 4 tag handler
  virtual void Handle_H5_Tag();           // Heading 5 tag handler
  virtual void Handle_H6_Tag();           // Heading 6 tag handler
  virtual void Handle_HEAD_Tag();         // Head tag handler
  virtual void Handle_HR_Tag();           // Horizontal Rule tag handler
  virtual void Handle_HTML_Tag();         // HTML tag handler
  virtual void Handle_I_Tag();            // Italic tag handler
  virtual void Handle_IFRAME_Tag();       // Frame - Floating tag handler
  virtual void Handle_ILAYER_Tag();       // ILayer tag handler
  virtual void Handle_IMG_Tag();          // Inline Image tag handler
  virtual void Handle_INPUT_Tag();        // Form Input tag handler
  virtual void Handle_INS_Tag();          // Inserted Text tag handler
  virtual void Handle_ISINDEX_Tag();      // Is Index tag handler
  virtual void Handle_KBD_Tag();          // Keyboard tag handler
  virtual void Handle_LANG_Tag();         // Language tag handler
  virtual void Handle_LAYER_Tag();        // Layer tag handler
  virtual void Handle_LEGEND_Tag();       // Legend tag handler
  virtual void Handle_LH_Tag();           // List Heading tag handler
  virtual void Handle_LI_Tag();           // List Item tag handler
  virtual void Handle_LINK_Tag();         // Link tag handler
  virtual void Handle_LISTING_Tag();      // Listing tag handler
  virtual void Handle_MAP_Tag();          // Map tag handler
  virtual void Handle_MARQUEE_Tag();      // Marquee tag handler
  virtual void Handle_MATH_Tag();         // Math tag handler
  virtual void Handle_MENU_Tag();         // Menu List tag handler
  virtual void Handle_META_Tag();         // Meta tag handler
  virtual void Handle_MULTICOL_Tag();     // Multi Column Text tag handler
  virtual void Handle_NOBR_Tag();         // No Break tag handler
  virtual void Handle_NOEMBED_Tag();      // No embed tag handler
  virtual void Handle_NOFRAMES_Tag();     // No Frames tag handler
  virtual void Handle_NOLAYER_Tag();      // No layer tag handler
  virtual void Handle_NOSCRIPT_Tag();     // No script tag handler
  virtual void Handle_NOTE_Tag();         // Note tag handler
  virtual void Handle_OBJECT_Tag();       // Object tag handler
  virtual void Handle_OL_Tag();           // Ordered List tag handler
  virtual void Handle_OPTGROUP_Tag();     // Option group tag handler
  virtual void Handle_OPTION_Tag();       // Option tag handler
  virtual void Handle_OVERLAY_Tag();      // Overlay tag handler
  virtual void Handle_P_Tag();            // Paragraph tag handler
  virtual void Handle_PARAM_Tag();        // Parameters tag handler
  virtual void Handle_PERSON_Tag();       // Person tag handler
  virtual void Handle_PLAINTEXT_Tag();    // Plain Text tag handler
  virtual void Handle_PRE_Tag();          // Preformatted Text tag handler
  virtual void Handle_Q_Tag();            // Quote tag handler
  virtual void Handle_RANGE_Tag();        // Range tag handler
  virtual void Handle_RT_Tag();           // Ruby text tag handler
  virtual void Handle_RUBY_Tag();         // Ruby tag handler
  virtual void Handle_S_Tag();            // Strikethrough tag handler
  virtual void Handle_SAMP_Tag();         // Sample tag handler
  virtual void Handle_SCRIPT_Tag();       // Script tag handler
  virtual void Handle_SELECT_Tag();       // Form Select tag handler
  virtual void Handle_SMALL_Tag();        // Small Text tag handler
  virtual void Handle_SPACER_Tag();       // White Space tag handler
  virtual void Handle_SPAN_Tag();         // Span tag handler
  virtual void Handle_SPOT_Tag();         // Spot tag handler
  virtual void Handle_STRIKE_Tag();       // Strikethrough tag handler
  virtual void Handle_STRONG_Tag();       // Strong tag handler
  virtual void Handle_STYLE_Tag();        // Style tag handler
  virtual void Handle_SUB_Tag();          // Subscript tag handler
  virtual void Handle_SUP_Tag();          // Superscript tag handler
  virtual void Handle_TAB_Tag();          // Horizontal Tab tag handler
  virtual void Handle_TABLE_Tag();        // Table tag handler
  virtual void Handle_TBODY_Tag();        // Table Body tag handler
  virtual void Handle_TD_Tag();           // Table Data tag handler
  virtual void Handle_TEXTAREA_Tag();     // Form Text Area tag handler
  virtual void Handle_TEXTFLOW_Tag();     // Java Applet Textflow tag handler
  virtual void Handle_TFOOT_Tag();        // Table Footer tag handler
  virtual void Handle_TH_Tag();           // Table Header tag handler
  virtual void Handle_THEAD_Tag();        // Table Head tag handler
  virtual void Handle_TITLE_Tag();        // Title tag handler
  virtual void Handle_TR_Tag();           // Table Row tag handler
  virtual void Handle_TT_Tag();           // Teletype tag handler
  virtual void Handle_U_Tag();            // Underlined tag handler
  virtual void Handle_UL_Tag();           // Unordered List tag handler
  virtual void Handle_VAR_Tag();          // Variable tag handler
  virtual void Handle_WBR_Tag();          // Word Break tag handler
  virtual void Handle_XML_Tag();          // XML tag handler
  virtual void Handle_XMP_Tag();          // Example tag handler
  
public: // 11/30/2006 Derived class interface used to process scipts
  virtual void Handle_Script(gxsScriptInfo &script);

public: // 04/24/2007 Derived class interface used to embedded style sheets
  virtual void Handle_StyleSheet(gxsStyleInfo &style_sheet);

protected: // Internal processing functions
  void Copy(const gxsHTML &ob);
  int CollectHTMLTags();
  int ProcessHTMLTags();
  int ProcessScripts();
  int ProcessStyleSheets();
  int CollectHTMLTags(const MemoryBuffer &membuf);
  int ProcessHTMLTags(const MemoryBuffer &membuf);
  int ProcessScripts(const MemoryBuffer &membuf);
  int ProcessStyleSheets(const MemoryBuffer &membuf);

protected:
  gxList<gxsHTMLTagInfo> tag_list; // List of HTML tags found in a file
  gxListNode<gxsHTMLTagInfo> *list_ptr; // Current list node
  unsigned num_tags;      // Number of tags collected from an html file
  unsigned num_processed; // Number of valid tags read from the file

public: // 09/11/2006: Added error string to report parsing errors
  gxString err_string;

protected: // 11/29/2006: Data members added to store any script code 
  gxList<gxsScriptInfo> script_list; // List of script code found in a file
  gxListNode<gxsScriptInfo> *script_ptr; // Current script list node
  unsigned num_scripts; // Number of scripts collected from an html file
  unsigned num_scripts_processed; // Number of valid scripts read from the file

public: // 11/30/2006: Added to turn off auto script processing
  int process_scripts;

protected: // 04/24/2007: Data members added to store embedded sytle sheets 
  gxList<gxsStyleInfo> style_sheet_list; // List of style sheet code found in a file
  gxListNode<gxsStyleInfo> *style_sheet_ptr; // Current style sheet list node
  unsigned num_style_sheets; // Number of style sheets collected from an html file
  unsigned num_style_sheets_processed; // Number of valid style sheets read from the file

public: // 04/24/2007: Added to turn off auto style sheet processing
  int process_style_sheets;
};

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(default:4251) // Restore previous warning
#endif

#endif // __GX_HTML_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

