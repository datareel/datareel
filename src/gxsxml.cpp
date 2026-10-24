// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsxml.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

DataReel XML parser.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxsxml.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

const int gxNumXMLEscapedElements = 5;
const char *gxXMLEscapedElements[gxNumXMLEscapedElements] = {
  "&lt;",
  "&gt;",
  "&apos;",
  "&quot;",
  "&amp;",
};

const char *gxXMLEscapedReplacements[gxNumXMLEscapedElements] = {
  "<",
  ">",
  "\'",
  "\"",
  "&",
};

// 10/14/2026: Helper functions added for the bug fixes below.
// Returns nchars characters starting at position. Unlike gxString::Mid()
// it returns an empty string when nchars is 0 (Mid() returns the rest of 
// the string) or the position is past the end of the string.
static gxString gxsXML_mid(const gxString &s, int position, int nchars)
{
  gxString buf;
  if((position < 0) || (nchars <= 0) || (position >= (int)s.length())) {
    return buf;
  }
  buf = s.Mid(position, nchars);
  return buf;
}

// Remove all CDATA sections from a string
static void gxsXML_strip_cdata(gxString &s)
{
  int coffset = s.Find("<![CDATA[");
  while(coffset != -1) {
    int ecoffset = s.Find("]]>", coffset);
    if(ecoffset == -1) break;
    ecoffset += 3;
    s.DeleteAt(coffset, (ecoffset-coffset));
    coffset = s.Find("<![CDATA[", coffset);
  }
}

// Returns true if the tag at offset in s is a start tag for "name".
static int gxsXML_is_start_tag(const gxString &s, int offset, const gxString &name)
{
  if(s[offset] != '<') return 0;
  if(s.Mid(offset+1, name.length()) != name) return 0;
  int i = offset + 1 + name.length();
  if(i >= (int)s.length()) return 0;
  char c = s[i];
  if((c != '>') && (c != '/') && (c != ' ') && (c != '\t') && 
     (c != '\r') && (c != '\n')) return 0;
  // Do not count an empty element tag <name ... />
  int end = s.Find(">", i);
  if((end > 0) && (s[end-1] == '/')) return 0;
  return 1;
}

// Find the end tag "</name>" that matches a start tag of "name", 
// starting the search at offset, which must be just past the start
// tag. Nested elements with the same name and CDATA sections are
// skipped. Returns the offset of the end tag or -1 if not found.
static int gxsXML_find_closing_tag(const gxString &s, const gxString &name, int offset)
{
  gxString end_tag;
  end_tag << "</" << name << ">";
  int level = 0;
  int i = offset;
  while(i < (int)s.length()) {
    i = s.Find("<", i);
    if(i == -1) return -1;
    if(s.Mid(i, 9) == "<![CDATA[") {
      int ecoffset = s.Find("]]>", i);
      if(ecoffset == -1) return -1;
      i = ecoffset + 3;
      continue;
    }
    if(s.Mid(i, end_tag.length()) == end_tag) {
      if(level == 0) return i;
      level--;
    }
    else if(gxsXML_is_start_tag(s, i, name)) {
      level++;
    }
    i++;
  }
  return -1;
}

gxsXMLDocInfo &gxsXMLDocInfo::operator=(const gxsXMLDocInfo &ob) 
{
  if(this != &ob) Copy(ob);
  return *this;
}

void gxsXMLDocInfo::Clear() 
{
  // File name is set by the application
  fname.Clear();

  // Set our default version number
  xml_version_str = "1.0";

  // Set our default encoding
  xml_encoding = "utf-8";

  // The default standalone value for XML documents is NO.
  // A YES value means that no declarations in the external 
  // DTD subset affects the content of the document.
  xml_standalone = "no";

  // This is set only if the XML document has a style sheet.
  xml_style_sheet.Clear();
}

void gxsXMLDocInfo::Copy(const gxsXMLDocInfo &ob) 
{
  fname = ob.fname;
  xml_version_str = ob.xml_version_str;
  xml_encoding = ob.xml_encoding;
  xml_style_sheet = ob.xml_style_sheet;
  xml_standalone = ob.xml_standalone;
}

gxsXMLNode_t::gxsXMLNode_t() 
{ 
  sub_child = 0;
  subelements = last_subelement = 0; 
  has_siblings = 0;
  // 10/14/2026: is_closed was not initialized. The writer reads it.
  is_closed = 0;
  depth = 0;
}

gxsXMLNode_t::~gxsXMLNode_t() 
{ 
  if(subelements) destroy_subelements();
}

gxsXMLNode_t ::gxsXMLNode_t(const gxsXMLNode_t &ob) 
{
  subelements = last_subelement = 0;
  Copy(ob);
}

gxsXMLNode_t &gxsXMLNode_t::operator=(const gxsXMLNode_t &ob) 
{
  if(this != &ob) Copy(ob);
  return *this;
}

void gxsXMLNode_t::Clear()
{
  name.Clear();
  data.Clear();
  attributes.Clear();
  has_siblings = 0;
  is_closed = 0;
  sub_child = 0;
  depth = 0; // 10/14/2026: Added nesting level
  if(subelements) destroy_subelements();
}

int gxsXMLNode_t::HasSubElements()
{ 
  if(subelements) return 1;
  return  0; 
}

int gxsXMLNode_t::HasSiblings()
{ 
  return has_siblings;
}

void gxsXMLNode_t::Copy(const gxsXMLNode_t &ob) 
{
  name = ob.name;
  data = ob.data;
  attributes = ob.attributes;
  has_siblings = ob.has_siblings;
  is_closed = ob.is_closed;
  sub_child = ob.sub_child;
  depth = ob.depth; // 10/14/2026: Added nesting level

  if(subelements) destroy_subelements();
  if(ob.subelements) {
    gxsXMLSubElement *cptr = ob.subelements;
    while(cptr) {
      AddSubElement(*cptr->element);
      cptr = cptr->next;
    }

    // 10/14/2026: The sub_child pointers of the copied sub-elements 
    // still pointed into the other node's list. Point them to the
    // matching sub-elements in this node's list.
    gxsXMLSubElement *nptr = subelements;
    cptr = ob.subelements;
    while(nptr && cptr) {
      if(cptr->element->sub_child) {
	nptr->element->sub_child = 0;
	gxsXMLSubElement *optr = ob.subelements;
	gxsXMLSubElement *mptr = subelements;
	while(optr && mptr) {
	  if(optr == cptr->element->sub_child) {
	    nptr->element->sub_child = mptr;
	    break;
	  }
	  optr = optr->next;
	  mptr = mptr->next;
	}
      }
      nptr = nptr->next;
      cptr = cptr->next;
    }
  }
}

void gxsXMLNode_t::delete_last_subelement()
{
  gxsXMLSubElement *n = last_subelement;
  detach_subelement(n);
  gxsXMLNode_t *e = n->element;
  if(e) delete e;
  delete n;
  n = 0;
  e = 0;
}

void gxsXMLNode_t::destroy_subelements()
{
  while(subelements) {
    delete_last_subelement();
  }
  subelements = last_subelement = 0;
}

void gxsXMLNode_t::delete_subelement(gxsXMLSubElement *n)
{
  detach_subelement(n);
  gxsXMLNode_t *e = n->element;
  if(e) delete e;
  delete n;
  n = 0;
  e = 0;
}

void gxsXMLNode_t::detach_subelement(gxsXMLSubElement *n)
{
  if(subelements == last_subelement) { // There is only one node in the list
    subelements = last_subelement = 0;
  }
  else if(n == subelements) { // This is the head node
    if(n->next) {
      n->next->prev = 0;
      subelements = n->next;
    }
    else {
      // This is the last node starting from the head
      subelements = last_subelement = 0;
    }
  }
  else if(n == last_subelement) { // This is the tail node
    if(n->prev) {
      n->prev->next = 0;
      last_subelement = n->prev;  
    }
    else {
      // This is the last node starting at the tail
      subelements = last_subelement = 0;
    }
  }
  else if((n->next == 0) && (n->prev == 0)) {
    // This node is not linked anywhere in the list
    return;
  }
  else { // We are somewhere in the middle of the list
    n->prev->next = n->next;
    n->next->prev = n->prev;
  }
}

void gxsXMLNode_t::insert_before(gxsXMLSubElement *pos, gxsXMLSubElement *n)
{
  // The list is empty so this will be the first node
  if(!subelements) { 
    last_subelement = subelements = n; // First node in the list
    return;
  }
  
  if(pos == subelements) { // Inserting node at the head of the list
    n->prev = 0; // This node if the new head
    n->next = subelements;
    if(!subelements) {
      subelements = n;
      subelements->prev = n;
    }
    else {
      subelements->prev = n;
      subelements = n;
    }
  }
  else { // Inserting node somewhere in the middle of the list
    n->prev = pos->prev;
    n->next = pos;
    pos->prev->next = n;
    pos->prev = n;
  }
}

void gxsXMLNode_t::insert_after(gxsXMLSubElement *pos, gxsXMLSubElement *n)
{
  // The list is empty so this will be the first node
  if(!subelements) {
    last_subelement = subelements = n; // First node in the list
    return;
  }

  if(pos == last_subelement) { // Inserting node at the tail of the list
    n->next = 0; // This node is the new tail;
    n->prev = last_subelement;
    if(!last_subelement) {
      last_subelement = n;
      last_subelement->next = n;
    }
    else {
      last_subelement->next = n;
      last_subelement = n;
    }
  }
  else { // Inserting node somewhere in the middle of the list
    n->next = pos->next;
    n->prev = pos;
    pos->next->prev = n;
    pos->next = n;
  }
}

gxsXMLSubElement *gxsXMLNode_t::AddSubElement(gxsXMLNode_t &element)
{
  gxsXMLSubElement *n = new gxsXMLSubElement;
  if(!n) return 0;
  gxsXMLNode_t *e = new gxsXMLNode_t(element); 
  n->element = e;

  insert_after(last_subelement, n);
  return n;
}

gxsXMLSubElement *gxsXMLNode_t::add_sub_element(gxsXMLSubElement *n)
{
  if(!n) return 0;
  insert_after(last_subelement, n);
  return n;
}

gxsXMLElement &gxsXMLElement::operator=(const gxsXMLElement &ob) 
{
  if(this != &ob) Copy(ob);
  return *this;
}

void gxsXMLElement::Clear() 
{
  elements.ClearList();
}

void gxsXMLElement::Copy(const gxsXMLElement &ob) 
{
  elements.ClearList();
  gxListNode<gxsXMLNode_t> *ptr = ob.elements.GetHead();
  while(ptr) {
    elements.Add(ptr->data);
    ptr = ptr->next;
  }
}

gxsXMLNode &gxsXMLNode::operator=(const gxsXMLNode &ob) 
{
  if(this != &ob) Copy(ob);
  return *this;
}

void gxsXMLNode::Clear() 
{
  node_type = gxsXML_NODE_TYPE_NOT_SET;
  element_type = gxsXML_ELEMENT_TYPE_NOT_SET; 
  name.Clear();
  data.Clear();
  attributes.Clear();
  child_nodes.Clear();
  sax_end_parse = 0;
}

void gxsXMLNode::Copy(const gxsXMLNode &ob) 
{
  node_type = ob.node_type;
  element_type = ob.element_type;
  name = ob.name;
  data = ob.data;
  attributes = ob.attributes;
  child_nodes = ob.child_nodes;
  sax_end_parse = ob.sax_end_parse;
}

gxsXML &gxsXML::operator=(const gxsXML &ob) 
{
  if(this != &ob) Copy(ob);
  return *this;
}

void gxsXML::Clear() 
{
  read_buf_len = gxsXML_DEFAULT_READ_LEN;
  format_err_str();
  err_string << "None reported";
  doc_info.Clear();
  parser_type = gxsXML_PARSER_DOM;
  pi_list.Clear();
  dtd_list.Clear();
  cdata_list.Clear();
  comment_list.Clear();
  node_tree.Clear();
  xs.Clear();
}

void gxsXML::Copy(const gxsXML &ob) 
{
  read_buf_len = ob.read_buf_len;
  err_string = ob.err_string;
  doc_info   = ob.doc_info  ;
  parser_type = ob.parser_type;

  gxListNode<gxsXMLNode> *ptr;

  pi_list.ClearList();
  ptr = ob.pi_list.GetHead();
  while(ptr) {
    pi_list.Add(ptr->data);
    ptr = ptr->next;
  }

  dtd_list.ClearList();
  ptr = ob.dtd_list.GetHead();
  while(ptr) {
    dtd_list.Add(ptr->data);
    ptr = ptr->next;
  }

  cdata_list.ClearList();
  ptr = ob.cdata_list.GetHead();
  while(ptr) {
    cdata_list.Add(ptr->data);
    ptr = ptr->next;
  }

  comment_list.ClearList();
  ptr = ob.comment_list.GetHead();
  while(ptr) {
    comment_list.Add(ptr->data);
    ptr = ptr->next;
  }

  node_tree.ClearList();
  ptr = ob.node_tree.GetHead();
  while(ptr) {
    node_tree.Add(ptr->data);
    ptr = ptr->next;
  }

  xs.ClearList();
  ptr = ob.xs.GetHead();
  while(ptr) {
    xs.Add(ptr->data);
    ptr = ptr->next;
  }
}

int gxsXML::GetXMLAttribute(const gxString &tag_str, const gxString &attr_str, 
			    gxString &value)
{
  value.Clear();
  err_string.Clear();

  int start_tag, end_tag, offset;
  gxString sbuf;
  
  // 10/14/2026: The attribute name was found with a plain text search,
  // so it could match part of a longer name (id in uid="1") or text
  // inside an earlier value. Only match a whole name outside quotes.
  offset = -1;
  if(!attr_str.is_null()) {
    char quote = 0;
    int i;
    int len = tag_str.length();
    int alen = attr_str.length();
    for(i = 0; i < len; i++) {
      char c = tag_str[i];
      if(quote) {
	if(c == quote) quote = 0;
	continue;
      }
      if((c == '\"') || (c == '\'')) {
	quote = c;
	continue;
      }
      if((i > 0) && (tag_str[i-1] != ' ') && (tag_str[i-1] != '\t') && 
	 (tag_str[i-1] != '\r') && (tag_str[i-1] != '\n') && 
	 (tag_str[i-1] != '<')) continue;
      if(tag_str.Mid(i, alen) != attr_str) continue;
      int j = i + alen;
      while((j < len) && ((tag_str[j] == ' ') || (tag_str[j] == '\t') ||
			  (tag_str[j] == '\r') || (tag_str[j] == '\n'))) j++;
      if((j < len) && (tag_str[j] == '=')) {
	offset = i;
	break;
      }
    }
  }
  if(offset == -1) {
    format_err_str();
    err_string << "Missing tag attribute: " << attr_str.c_str();
    return 0;
  }
  offset = tag_str.Find("=", offset);
  if(offset == -1) {
    format_err_str();
    err_string << "Bad attribute missing = character: " << attr_str.c_str();
    return 0;
  }
  offset++;
  // 10/14/2026: Allow spaces after the = sign and do not read past
  // the end of the string when the value is missing.
  while((offset < (int)tag_str.length()) &&
	((tag_str[offset] == ' ') || (tag_str[offset] == '\t'))) offset++;
  if(offset >= (int)tag_str.length()) {
    format_err_str();
    err_string << "Invalid tag attribute: " << attr_str.c_str();
    return 0;
  }
  if((tag_str[offset] == '\"') || (tag_str[offset] == '\'')) {
    sbuf << clear << tag_str[offset]; 
    start_tag = offset+1;
    end_tag = tag_str.Find(sbuf, start_tag);
    if(end_tag == -1) {
      format_err_str();
      err_string << "Bad attribute missing closing" 
		 << sbuf.c_str() << " character: " << attr_str.c_str();
      return 0;
    }
    // 10/14/2026: An empty value ("") returned the rest of the tag
    gxString sstr1 = gxsXML_mid(tag_str, start_tag, (end_tag-start_tag));
    value = sstr1;
  }
  else {
    format_err_str();
    err_string << "Invalid tag attribute: " << attr_str.c_str();
    return 0;
  }

  return 1;
} 

int gxsXML::LoadXMLFile(const char *fname)
{
  err_string.Clear();
  doc_info.Clear();
  doc_info.fname << clear << fname;
  doc_info.xml_style_sheet.Clear();

  // 10/14/2026: The node lists were not cleared, so loading a second
  // file into the same object added to the nodes of the first file.
  pi_list.ClearList();
  dtd_list.ClearList();
  cdata_list.ClearList();
  comment_list.ClearList();
  node_tree.ClearList();
  xs.ClearList();

  int rv = load_file(fname);
  if(rv != 0) {
    if(err_string.is_null()) {
      format_err_str();
      err_string << "Error loading and parsing file: " << fname;
    }
    return 0;
  }
  return 1;
}

int gxsXML::is_xml(DiskFileB &xml_fp, unsigned read_length)
{
  err_string.Clear();

  int rv = xml_fp.df_Rewind(); // Start at the beginning of the file
  if(rv != DiskFileB::df_NO_ERROR) {
    format_err_str();
    err_string << "Fatal I/O error occurred rewinding file";
    return 0;
  }
  if((read_length < gxsXML_MIN_READ_LEN) || 
     (read_length > gxsXML_MAX_READ_LEN)) {
    read_length = gxsXML_DEFAULT_READ_LEN;
  }

  char *read_buf = new char[read_length];
  memset(read_buf, 0, read_length);

  rv = xml_fp.df_Read((char *)read_buf, read_length);
  if(rv != DiskFileB::df_NO_ERROR) {
    if(rv != df_EOF_ERROR) {
      format_err_str();
      err_string << "Fatal I/O error occurred reading file";
      delete[] read_buf;
      return 0;
    }
  }

  gxString xml_doc_str = "<?xml ?>";
  if(xml_fp.df_gcount() < xml_doc_str.length()) {
    format_err_str();
    err_string << "Bad XML document file too short to contain header";
    delete[] read_buf;
    return 0;
  }

  gxString line_buf;
  line_buf.SetString(read_buf, xml_fp.df_gcount());
  delete[] read_buf;

  // Looking for start of XML document header.
  // Example: <?xml version="1.0" encoding="ISO-8859-1"?>
  gxString XML_hdr_start = "<?xml";
  gxString XML_hdr_end = "?>";
  int start_tag = line_buf.Find(XML_hdr_start);
  int end_tag = line_buf.Find(XML_hdr_end);
  int is_xml_doc = 0;

  if((start_tag != -1) && (end_tag != -1)) {
    is_xml_doc = 1;

    start_tag += XML_hdr_start.length();
    // 10/14/2026: An empty header returned the rest of the buffer
    gxString mid_s = gxsXML_mid(line_buf, start_tag, (end_tag-start_tag));
    mid_s.TrimLeadingSpaces(); mid_s.TrimTrailingSpaces();
    
    // Parse all known header attributes here
    gxString curr_attribute = "version";
    gxString value;
    if(GetXMLAttribute(mid_s, curr_attribute, value)) {
      doc_info.xml_version_str << clear << value;
    }
    curr_attribute = "encoding";
    if(GetXMLAttribute(mid_s, curr_attribute, value)) {
      doc_info.xml_encoding << clear << value;
    }
    curr_attribute = "standalone";
    if(GetXMLAttribute(mid_s, curr_attribute, value)) {
      doc_info.xml_standalone << clear << value;
    }
  }
  
  if(!is_xml_doc) {
    format_err_str();
    err_string << "Bad XML document no header found in file";
  }

  return is_xml_doc;
}

void gxsXML::CleanTagName(const gxString &tag, gxString &tag_name)
{
  tag_name = tag;
  tag_name.DeleteBeforeIncluding("<");
  tag_name.DeleteAfterIncluding(" ");
  tag_name.DeleteAfterIncluding(">");
  tag_name.TrimLeadingSpaces();
  tag_name.TrimTrailingSpaces();
  tag_name.TrimLeading('/');
  tag_name.TrimTrailing('/');
}

int gxsXML::HasAttributes(const gxString &tag)
{
  if(tag.Find("=") != -1) return 1;
  return 0;
}

void gxsXML::CleanAttributes(const gxString &tag, gxString &attrib)
{
  attrib.Clear();
  if(!HasAttributes(tag)) return;
  attrib = tag;
  attrib.DeleteBeforeIncluding("<");
  attrib.DeleteBeforeIncluding(" ");
  attrib.DeleteAfterIncluding(">");
  attrib.TrimLeadingSpaces();
  attrib.TrimTrailingSpaces();
  attrib.TrimTrailing('/');
  // 10/14/2026: Remove the space before the "/" of an empty element
  // tag. It was kept and another space was added on each write.
  attrib.TrimTrailingSpaces();
}

int gxsXML::find_tag(gxString &node, int offset, int &start, int &end)
{
  int i = 0;
  start = end = -1;

  // 10/14/2026: Removed the "></" to "> </" replacement. It changed 
  // the text of CDATA sections and turned empty elements into elements
  // holding a space. Empty elements are handled by explode_nodes().
  int len = node.length();

  for(i = offset; i < len; i++) {
    if(node[i] == '<') {
      // 10/14/2026: Skip CDATA sections so tags inside them are
      // not treated as elements.
      if(node.Mid(i, 9) == "<![CDATA[") {
	int ecoffset = node.Find("]]>", i);
	if(ecoffset == -1) return 0;
	i = ecoffset + 2;
	start = -1;
	continue;
      }
      start = i;
      continue;
    }
    if(node[i] == '>') {
      end = i+1;
      return 1;
    }
  }
  return 0;
}

// 10/24/2026: Added. Adds the text in "node" from "start" up to "end" to
// "node_list" as a node with no name, if it is not only white space.
// Leading and trailing white space is removed, so writing and loading a
// document again does not add to it.
static gxString gxsXML_trim_text(const gxString &s, int start, int end)
// 10/24/2026: Returns the text from "start" up to "end" without its
// leading and trailing white space.
{
  gxString buf;
  if(end > (int)s.length()) end = (int)s.length();
  if((start < 0) || (end <= start)) return buf;
  const char *p = s.c_str();
  while((start < end) && ((p[start] == ' ') || (p[start] == '\t') ||
			  (p[start] == '\r') || (p[start] == '\n'))) start++;
  while((end > start) && ((p[end-1] == ' ') || (p[end-1] == '\t') ||
			  (p[end-1] == '\r') || (p[end-1] == '\n'))) end--;
  return gxsXML_mid(s, start, (end-start));
}

static void gxsXML_add_text_node(gxString &node, int start, int end,
				 gxList<gxsXMLNode_t> &node_list)
{
  gxString text = gxsXML_trim_text(node, start, end);
  if(text.is_null()) return;
  gxsXMLNode_t n;
  n.data = text;
  node_list.Add(n);
}

int gxsXML::has_sub_elements(const gxsXMLElement &element)
{
  gxListNode<gxsXMLNode_t> *ptr = element.elements.GetHead();
  while(ptr) {
    if(has_sub_elements(ptr->data.data)) return 1;
    ptr = ptr->next;
  }
  return 0;
}

// 10/14/2026: Rewritten. The old code only followed the first element
// at each level that had nested elements. Any elements after it at the
// same level were lost, and deeper levels were not recorded, so the
// tree could not be written back with the original nesting. The old
// code also wrote debug trace text to std::cout.
int gxsXML::explode_nodes(gxString &node, gxsXMLNode &n)
{
  int rv = explode_nodes(node, n.child_nodes.elements);
  if(rv < 0) return rv;

  gxListNode<gxsXMLNode_t> *ptr = n.child_nodes.elements.GetHead();
  while(ptr) {
    if(has_sub_elements(ptr->data.data)) {
      gxString nodes = ptr->data.data;
      ptr->data.data.Clear();
      int srv = explode_sub_elements(ptr->data, nodes, 1);
      if(srv < 0) return srv;
    }
    ptr = ptr->next;
  }

  return rv;
}

// 10/14/2026: Added. Adds the elements in "nodes" to the sub-element
// list of "child" at the given depth, followed by the elements nested
// inside each of them. Returns -1 if an error occurs.
int gxsXML::explode_sub_elements(gxsXMLNode_t &child, gxString &nodes, 
				 int depth)
{
  gxList<gxsXMLNode_t> elements;
  int rv = explode_nodes(nodes, elements);
  if(rv < 0) return rv;

  gxListNode<gxsXMLNode_t> *cptr = elements.GetHead();
  while(cptr) {
    gxsXMLNode_t element;
    element.name = cptr->data.name;
    element.attributes = cptr->data.attributes;
    element.depth = depth;
    if(has_sub_elements(cptr->data.data)) {
      element.has_siblings = 1;
      if(!child.AddSubElement(element)) {
	format_err_str();
	err_string << "Memory allocation error exploding child nodes";
	return -1;
      }
      Handle_Sibling_Event(child);
      gxString sub_nodes = cptr->data.data;
      rv = explode_sub_elements(child, sub_nodes, depth+1);
      if(rv < 0) return rv;
    }
    else {
      element.data = cptr->data.data;
      element.has_siblings = 0;
      if(!child.AddSubElement(element)) {
	format_err_str();
	err_string << "Memory allocation error exploding child nodes";
	return -1;
      }
      Handle_SubChild_Event(child);
    }
    cptr = cptr->next;
  }

  return 0;
}

// 10/14/2026: Rewritten. The old code matched the first end tag with
// the same name, so an element containing an element with the same
// name was cut short. It also moved the first CDATA section found
// anywhere in the text to the first element that ended, which could
// be the wrong element, and wrote debug trace text to std::cout.
int gxsXML::explode_nodes(gxString &node, gxList<gxsXMLNode_t> &node_list)
{
  node_list.ClearList();

  if(!strip_comments(node)) return -1;

  int num_nodes = 0;
  int tag_offset = 0;
  int start, end;
  gxString name;

  // 10/24/2026: Text between the elements (mixed content) is kept as
  // nodes with no name, in document order. It used to be dropped.
  int text_offset = 0;

  while(find_tag(node, tag_offset, start, end)) {
    tag_offset = end;
    if(start < 0) continue; // A ">" character in the text

    gxString curr_tag = node.Mid(start, (end-start));
    if(curr_tag.length() < 3) continue;

    // Skip end tags with no start tag and processing instructions
    if((curr_tag[1] == '/') || (curr_tag[1] == '?') || 
       (curr_tag[1] == '!')) continue;

    gxsXML_add_text_node(node, text_offset, start, node_list);
    
    gxsXMLNode_t n;
    CleanAttributes(curr_tag, n.attributes); 
    CleanTagName(curr_tag, name);
    n.name = name;

    if(curr_tag.Right(2) == "/>") { // Empty element
      node_list.Add(n);
      num_nodes++;
      text_offset = end;
      continue;
    }

    int end_offset = gxsXML_find_closing_tag(node, name, end);
    if(end_offset == -1) {
      format_err_str();
      err_string << "Missing closing tag: </" << name.c_str() << ">";
      return -1;
    }

    n.data = gxsXML_mid(node, end, (end_offset-end));
    if(has_sub_elements(n.data)) n.has_siblings = 1;
    node_list.Add(n);
    num_nodes++;
    tag_offset = end_offset + name.length() + 3;
    text_offset = tag_offset;
  }

  // Text after the last element. Text is only kept when it is mixed
  // with elements. Text with no elements stays in the element data.
  if(num_nodes > 0) {
    gxsXML_add_text_node(node, text_offset, (int)node.length(), node_list);
  }

  return num_nodes;
}

void gxsXML::format_err_str() 
{
  err_string.Clear();
  err_string << "XML ERROR: "; 
}

char *gxsXML::SetError(const char *s)
{
  format_err_str();
  if(s) err_string << s;
  return err_string.c_str();
}

char *gxsXML::GetError()
{
  if(err_string.is_null()) {
    format_err_str();
    err_string << "None reported";
  }
  return err_string.c_str();
}

int gxsXML::test_comments(const gxString &pd)
{
  gxString sbuf = pd;
  return strip_comments(sbuf);
}

int gxsXML::strip_comments(gxString &pd)
{
  int coffset = 0;

  while(coffset != -1) {
    coffset = pd.Find("<!--");
    if(coffset != -1) {
      int ecoffset = pd.Find("-->", coffset);
      if(ecoffset != -1) {
	ecoffset += 3;
	pd.DeleteAt(coffset, (ecoffset-coffset));
      }
      else {
	format_err_str();
	err_string << "Missing closing comment tag";
	return 0;
      }
    }
    else {
      break;
    }
  }

  return 1;
}

int gxsXML::has_sub_elements(const gxString &node_data)
{
  // 10/14/2026: Removed debug trace output to std::cout. Remove all 
  // CDATA sections, not just the first one, before looking for tags.
  gxString pd = node_data;
  gxsXML_strip_cdata(pd);
  if((pd.Find("<") != -1) && (pd.Find(">") != -1)) {
    return 1;
  }
  return 0;
}

int gxsXML::load_file(const char *fname)
// Load and process the XML file. 
// Returns 0 if the file is loaded successfully.
// Returns a non-zero value if an error occurs.
{
  DiskFileB xml_fp;
  err_string.Clear();
  doc_info.fname << clear << fname;

  // Open the xml file
  xml_fp.df_Open(fname);
  if(!xml_fp) {
    format_err_str();
    err_string << "Cannot open file: " << fname;
    return 1;
  }
  
  if((read_buf_len < gxsXML_MIN_READ_LEN) || 
     (read_buf_len > gxsXML_MAX_READ_LEN)) {
    read_buf_len = gxsXML_DEFAULT_READ_LEN;
  }

  // Check to see if this a valid XML document 
  if(!is_xml(xml_fp, read_buf_len)) return 1;

  // Clear any optional document attribute warnings
  err_string.Clear();

  int rv = xml_fp.df_Rewind(); // Start at the beginning of the file
  if(rv != DiskFileB::df_NO_ERROR) {
    format_err_str();
    err_string << "Fatal I/O error occurred rewinding file: " << fname;
    return 1;
  }

  char *read_buf = new char[read_buf_len];
  gxString line_chunk;
  int looking_for_end_tag = 0;
  int looking_for_start_tag = 0;
  int start_tag, end_tag;
  gxString XML_start_tag = "<";
  gxString XML_end_tag = ">";
  int found_end_of_root = 0;
  int has_root = 0;
  int set_root = 0;
  gxString sbuf;
  gxsXMLNodeType node_type;
  gxString root_name, end_of_root; 
  gxString parent_name, end_of_parent;
  gxString root_data;
  int num_parents = 0;
  gxString root_text; // 10/24/2026: Text between the parent elements
  gxsXMLNode root_node;

  // Clear previous error string, if any
  err_string.Clear();
  int has_error = 0;

  while(!xml_fp.df_EOF()) {
    if(has_error == 1) break;

    memset(read_buf, 0, read_buf_len); // Clear the buffer before each read
    rv = xml_fp.df_Read((char *)read_buf, read_buf_len);
    if(rv != DiskFileB::df_NO_ERROR) {
      if(rv != df_EOF_ERROR) {
	format_err_str();
	err_string << "Fatal I/O error occurred reading file: " << fname;
	has_error = 1;
	break;
      }
    }

    if(looking_for_start_tag) {
      // We need to find the start of the next tag
      line_chunk.Cat(read_buf, xml_fp.df_gcount());
    }
    else if(looking_for_end_tag) {
      // We need to find the rest of the current tag 
      line_chunk.Cat(read_buf, xml_fp.df_gcount());
    }
    else {
      line_chunk.Clear();
      line_chunk.SetString(read_buf, xml_fp.df_gcount());
    }

    if(!has_root) {
      if(!test_comments(line_chunk)) {
	has_error = 1;
	break;
      }
    }

    int curr_offset = 0;
    while(curr_offset != -1) {
      if((has_root) && (num_parents == 0)) {
	if(!strip_comments(line_chunk)) {
	  has_error = 1;
	  break;
	}
	root_data << line_chunk;
      }

      int next_tag = line_chunk.Find(XML_start_tag, curr_offset);
      // 10/24/2026: Keep the text between the parent elements of the
      // root. It used to be dropped. It is added to the tree as a text
      // node before the next parent or the end of the root.
      if((has_root) && (!set_root) && (num_parents > 0) && 
	 (!found_end_of_root)) {
	int text_end = (next_tag == -1) ? (int)line_chunk.length() : next_tag;
	if(text_end > curr_offset) {
	  root_text << gxsXML_mid(line_chunk, curr_offset, 
				 (text_end-curr_offset));
	}
      }
      curr_offset = next_tag;
      if(curr_offset == -1) {
	break;
      }

      start_tag = curr_offset;
      looking_for_start_tag = 0;
      looking_for_end_tag = 1;
      curr_offset++;

      // 10/14/2026: Signed compare (length()-1 wrapped when the chunk was empty)
      if(curr_offset >= (int)line_chunk.length()) {
	line_chunk.DeleteAt(0, start_tag);
	looking_for_start_tag = 1;
	break;
      }

      // Reset the node type
      node_type = gxsXML_NODE_TYPE_NOT_SET;

      char next_char = line_chunk[curr_offset];
      int read_next_chunk = 0;
      switch(next_char) {
	case '?': // Processing instruction
	  XML_end_tag = "?>";
	  node_type = gxsXML_PROC_NODE;
	  break;

	case '!': // DTD, CDATA, or Comment
	  // Ensure and DTD or CDATA is on a full line
	  if((curr_offset+gxsXML_MIN_READ_LEN) > (line_chunk.length()-1)) {
	    if(!xml_fp.df_EOF()) { // Make sure we are not at the EOF
	      read_next_chunk = 1;
	      break;
	    }
	  }
	  if(line_chunk.Mid(start_tag, 9) == "<![CDATA[") {
	    XML_end_tag = "]]>";
	    node_type = gxsXML_CDATA_NODE;
	    break;
	  }

	  // Check for XML DTDs
	  // DOCTYPE - Can have internal DTD subset contained in: 
	  //           <!DOCTYPE xxx [ \n xxx \n]> ... ]> 
	  // ELEMENT - Define element type name and permissible sub-elements
	  // ENTITY - tag is used to define replaceable content
	  // ATTLIST - Defines attributes of permissible and default values
	  // NOTATION - Points to notation explicitely defined in the DTD
	  // INCLUDE - Include conditional section
	  // IGNORE - Ignore conditional section
	  if((line_chunk.Mid(start_tag, 9) == "<!DOCTYPE") ||
	     (line_chunk.Mid(start_tag, 9) == "<!ELEMENT") ||
	     (line_chunk.Mid(start_tag, 8) == "<!ENTITY") ||
	     (line_chunk.Mid(start_tag, 9) == "<!ATTLIST") ||
	     (line_chunk.Mid(start_tag, 10) == "<!NOTATION") ||
	     (line_chunk.Mid(start_tag, 9) == "<!INCLUDE") ||
	     (line_chunk.Mid(start_tag, 8) == "<!IGNORE") ){
	    // Look for internal DTD subsets
	    int eol = line_chunk.Find("\n", start_tag);
	    if(eol == -1) eol = line_chunk.Find("\r\n", start_tag);
	    if(eol != -1) {
	      gxString dtd_mid = line_chunk.Mid(start_tag, eol-start_tag);
	      dtd_mid.FilterString("\n");
	      dtd_mid.FilterString("\r\n");
	      dtd_mid.TrimTrailingSpaces();
	      if(dtd_mid[dtd_mid.length()-1] == '[') {
		XML_end_tag = "]>";
		node_type = gxsXML_DTD_NODE;
		break;
	      }
	      else {
		XML_end_tag = ">";
		node_type = gxsXML_DTD_NODE;
		break;
	      }
	    }
	    else {
	      read_next_chunk = 1;
	      break;
	    }
	  }

	  if((line_chunk[curr_offset+1] == '-') && 
	     (line_chunk[curr_offset+2] == '-')) {
	    XML_end_tag = "-->";
	    node_type = gxsXML_COMMENT_NODE;
	    break;
	  }
	  break;

	default:
	  XML_end_tag = ">"; // Reset to the default end of tag char
	  if(!has_root) {
	    // The root node is the first non-PI and non-DTD tag
	    set_root = 1; // Signal to set the root node
	    has_root = 1; 
	    found_end_of_root = 0;
	    node_type = gxsXML_ROOT_NODE;
	  }
	  break;
      } // End of swtich
      if(read_next_chunk) { 
	// Re-read any missing line data needed to complete the tag 
	line_chunk.DeleteAt(0, start_tag);
	looking_for_start_tag = 1;
	break;
      }
      // End of PI, DTD, CDATA, and Comment read
      
      curr_offset = line_chunk.Find(XML_end_tag, curr_offset);
      if(curr_offset == -1) {
 	looking_for_end_tag = 1;
 	line_chunk.DeleteAt(0, start_tag);
 	break;
      }
      curr_offset += XML_end_tag.length();
      end_tag = curr_offset;
      
      gxString xml_tag = line_chunk.Mid(start_tag, (end_tag-start_tag));
      looking_for_end_tag = 0;

      if(!has_root) { // We have not found the document root yet
	gxsXMLNode n;
	switch(node_type) {
	  case gxsXML_PROC_NODE:
	    n.node_type = gxsXML_PROC_NODE;
	    n.attributes = xml_tag;
	    Handle_PI_Event(n);
	    if(n.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	    if(xml_tag.Find("xml-stylesheet") != -1) {
	      doc_info.xml_style_sheet = xml_tag; 
	    }

	    if(parser_type == gxsXML_PARSER_DOM) {
	      pi_list.Add(n);
	    }
	    break;
	  case gxsXML_DTD_NODE:
	    // 10/14/2026: Was stored as gxsXML_PROC_NODE
	    n.node_type = gxsXML_DTD_NODE;
	    n.attributes = xml_tag;
	    Handle_DTD_Event(n);
	    if(n.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	    if(parser_type == gxsXML_PARSER_DOM) {
	      dtd_list.Add(n);
	    }
	    break;
	  case gxsXML_CDATA_NODE:
	    // 10/14/2026: Was stored as gxsXML_PROC_NODE
	    n.node_type = gxsXML_CDATA_NODE;
	    n.attributes = xml_tag;
	    Handle_CDATA_Event(n);
	    if(n.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	    if(parser_type == gxsXML_PARSER_DOM) {
	      cdata_list.Add(n);
	    }
	    break;
	  case gxsXML_COMMENT_NODE:
	    // 10/14/2026: Was stored as gxsXML_PROC_NODE
	    n.node_type = gxsXML_COMMENT_NODE;
	    n.attributes = xml_tag;
	    Handle_Comment_Event(n);
	    if(n.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	    if(parser_type == gxsXML_PARSER_DOM) {
	      comment_list.Add(n);
	    }
	    break;
	  default:
	    format_err_str();
	    err_string << "Invalid formatting element before root node";
	  break;
	}
	continue; // Do not process root, parent, sub-parents, or child nodes
      }

      if((has_root) && (set_root)) {
	CleanAttributes(xml_tag, root_node.attributes); 
	CleanTagName(xml_tag, root_name);
	end_of_root << clear << "</" << root_name << ">";
	set_root = 0;
	num_parents = 0;
	root_node.name = root_name;
	root_node.node_type = gxsXML_ROOT_NODE;
	line_chunk.DeleteBeforeIncluding(xml_tag);
	curr_offset = 0;
	continue; // Do not process parent, parent elements, or child nodes
      }

      if(!end_of_root.is_null()) {

	if(xml_tag == end_of_root) {
	  if(end_of_root.Left(5) == "</xs:") {
	    has_root = set_root = 0;
	    end_of_root.Clear();
	    root_name.Clear();
	    found_end_of_root = 0;
	    gxListNode<gxsXMLNode> *ntptr = node_tree.GetHead();
	    while(ntptr) {
	      xs.Add(ntptr->data);
	      ntptr = ntptr->next;
	    }
	    node_tree.ClearList();
	    Handle_XS_Event();
	    continue; 
	  }
	}

	if(num_parents == 0) {
	  root_data.DeleteAfterIncluding("<");
	  root_data.ReplaceString("\r\n", "\n");
	  root_data.TrimLeading('\n');
	  root_data.TrimTrailing('\n');
	  root_data.TrimLeadingSpaces();
	  root_data.TrimTrailingSpaces();
	  root_node.data = root_data;
	  root_data.Clear();

	  // Add the root node here to make sure we get all the
	  // root data before the first parent tag
	  Handle_Root_Event(root_node);
	  if(root_node.sax_end_parse == 1) {
	    xml_fp.df_Close();
	    delete[] read_buf;
	    return 0;
	  }
	  if(parser_type == gxsXML_PARSER_DOM) {
	    node_tree.Add(root_node);
	  }
	}

	// 10/24/2026: Add any text found since the last parent element
	if(!root_text.is_null()) {
	  gxsXMLNode tn;
	  tn.data = gxsXML_trim_text(root_text, 0, (int)root_text.length());
	  root_text.Clear();
	  if(!tn.data.is_null()) {
	    tn.node_type = gxsXML_TEXT_NODE;
	    Handle_Parent_Event(tn);
	    if(tn.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	    if(parser_type == gxsXML_PARSER_DOM) {
	      node_tree.Add(tn);
	    }
	  }
	}
	if(xml_tag == end_of_root) { // We are at the end of this tree
	  has_root = set_root = 0; // Signal to stop collecting nodes
	  end_of_root.Clear();
	  root_name.Clear();
	  found_end_of_root = 1;
	  // Do not process parent, sub-parents, or child nodes
	  delete[] read_buf;
	  return 0;
	}
      }
      
      if((has_root) && (!found_end_of_root)) {
	// 10/24/2026: Add any text found since the last parent element
	if(!root_text.is_null()) {
	  gxsXMLNode tn;
	  tn.data = gxsXML_trim_text(root_text, 0, (int)root_text.length());
	  root_text.Clear();
	  if(!tn.data.is_null()) {
	    tn.node_type = gxsXML_TEXT_NODE;
	    Handle_Parent_Event(tn);
	    if(tn.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	    if(parser_type == gxsXML_PARSER_DOM) {
	      node_tree.Add(tn);
	    }
	  }
	}
	num_parents++; 
	
	CleanTagName(xml_tag, parent_name);

	if(xml_tag.Right(2) == "/>") {
	  // This is a parent node with no data
	  gxsXMLNode n;
	  n.name = parent_name;
	  n.data.Clear();
	  n.node_type = gxsXML_PARENT_NODE;
	  CleanAttributes(xml_tag, n.attributes); 
	  Handle_Parent_Event(n);
	  if(n.sax_end_parse == 1) {
	    xml_fp.df_Close();
	    delete[] read_buf;
	    return 0;
	  }
	  if(parser_type == gxsXML_PARSER_DOM) {
	    node_tree.Add(n);
	  }
	  end_of_parent.Clear();
	  continue;
	}
	end_of_parent << clear << "</" << parent_name << ">";

	// 10/14/2026: Removed the 02/09/2012 "></" to "> </" replacement.
	// It changed the text of CDATA sections and turned an empty parent 
	// into one holding a space. The matching end tag is now found by 
	// gxsXML_find_closing_tag(), which handles empty elements.
	line_chunk.DeleteAt(0, (start_tag+xml_tag.length()));

	// 10/14/2026: Find the matching end tag. The old code used the
	// first end tag with the same name, which belonged to a nested
	// element when the parent contained an element with its own name.
	int eop_pos = gxsXML_find_closing_tag(line_chunk, parent_name, 0);
	if(eop_pos != -1) {
	  // 10/14/2026: Mid() returns the rest of the string for 0 chars
	  gxString pd = gxsXML_mid(line_chunk, 0, eop_pos);
	  gxsXMLNode n;
	  if(!strip_comments(pd)) {
	    has_error = 1;
	    break;
	  }
	  n.name = parent_name;
	  // 10/14/2026: Set the node type and attributes before the child
	  // event is called. Removed debug trace output to std::cout.
	  n.node_type = gxsXML_PARENT_NODE;
	  CleanAttributes(xml_tag, n.attributes);
	  if(has_sub_elements(pd)) { 
	    n.data.Clear();
	    if(explode_nodes(pd, n) < 0) {
	      has_error = 1;
	      break;
	    }
	    Handle_Child_Event(n);
	    if(n.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	  }
	  else {
	    n.data = pd;
	  }
	  n.node_type = gxsXML_PARENT_NODE;
	  CleanAttributes(xml_tag, n.attributes);
	  Handle_Parent_Event(n);
	  if(n.sax_end_parse == 1) {
	    xml_fp.df_Close();
	    delete[] read_buf;
	    return 0;
	  }
	  if(parser_type == gxsXML_PARSER_DOM) {
	    node_tree.Add(n);
	  }
	  // 10/14/2026: Delete up to the matching end tag found above
	  line_chunk.DeleteAt(0, (eop_pos+end_of_parent.length()));
	  parent_name.Clear();
	  xml_tag.Clear();
	  curr_offset = 0;
	  continue; // Do not process past this point
	}
	else { 
	  // Continue to read the file until we collect the enitre parent node
	  while(!xml_fp.df_EOF()) {
	    if(has_error == 1) break;
	    // Clear the buffer before each read
	    memset(read_buf, 0, read_buf_len); 
	    rv = xml_fp.df_Read((char *)read_buf, read_buf_len);
	    if(rv != DiskFileB::df_NO_ERROR) {
	      if(rv != df_EOF_ERROR) {
		format_err_str();
		err_string << "Fatal I/O error occurred reading file: " 
			   << fname;
		has_error = 1;
		break;
	      }
	    }
	    line_chunk.Cat(read_buf, xml_fp.df_gcount());
	    // 10/14/2026: Look for the matching end tag
	    if(gxsXML_find_closing_tag(line_chunk, parent_name, 0) != -1) break;
	  }
	  if(has_error == 1) break;
	  int eop_pos = gxsXML_find_closing_tag(line_chunk, parent_name, 0);
	  if(eop_pos != -1) {
	    // 10/14/2026: Mid() returns the rest of the string for 0 chars
	    gxString pd = gxsXML_mid(line_chunk, 0, eop_pos);
	    if(!strip_comments(pd)) {
	      has_error = 1;
	      break;
	    }
	    gxsXMLNode n;
	    n.name = parent_name;
	    // 10/14/2026: Set the node type and attributes before the child
	    // event is called. Removed debug trace output to std::cout.
	    n.node_type = gxsXML_PARENT_NODE;
	    CleanAttributes(xml_tag, n.attributes);
	    if(has_sub_elements(pd)) { 
	      n.data.Clear();
	      if(explode_nodes(pd, n) < 0) {
		has_error = 1;
		break;
	      }
	      Handle_Child_Event(n);
	      if(n.sax_end_parse == 1) {
		xml_fp.df_Close();
		delete[] read_buf;
		return 0;
	      }
	    }
	    else {
	      n.data = pd;
	    }
	    n.node_type = gxsXML_PARENT_NODE;
	    CleanAttributes(xml_tag, n.attributes);
	    Handle_Parent_Event(n);
	    if(n.sax_end_parse == 1) {
	      xml_fp.df_Close();
	      delete[] read_buf;
	      return 0;
	    }
	    if(parser_type == gxsXML_PARSER_DOM) {
	      node_tree.Add(n);
	    }

	    // 10/14/2026: Delete up to the matching end tag found above
	    line_chunk.DeleteAt(0, (eop_pos+end_of_parent.length()));
	    parent_name.Clear();
	    xml_tag.Clear();
	    curr_offset = 0;
	    continue; // Do not process past this point
	  }
	  else {
	    format_err_str();
	    if(xml_tag.Find("<!--") != -1) {
	      err_string << "Missing closing comment tag or nested comments";
	    }
	    else {
	      err_string << "No closing tag for: " << xml_tag;
	    }
	    has_error = 1;
	    break;
	  }
	}
      } // End of parent node processing
    } // End of tag search loop
  } // End of file read loop
  

  // Close the filem free memory buffers, and return 
  xml_fp.df_Close();
  delete[] read_buf;

  if(has_error == 1) {
    if(err_string.is_null()) {
      format_err_str();
      err_string << "Parsing error occurred reading file: " << fname;
    }
  }

  return has_error;
}

int gxsXML:: WriteXS(gxString &xs_string)
{
  if(xs.IsEmpty()) {
    format_err_str();
    err_string << "No XML Schema data is loaded in memory";
    return 0;
  }
  return write_xml_tree(xs, 0, &xs_string);
}

int gxsXML::WriteTree(gxString &tree_string)
{
  if(node_tree.IsEmpty()) {
    format_err_str();
    err_string << "No XML file or data is loaded in memory";
    return 0;
  }
  return write_xml_tree(node_tree, 0, &tree_string);
}

int gxsXML::WriteXMLFile(const char *fname)
{

  if(node_tree.IsEmpty()) {
    format_err_str();
    err_string << "No XML file or data is loaded in memory";
    return 0;
  }

  if(!fname) {
    format_err_str();
    err_string << "Null pointer passing file name to WriteXMLFile function";
    return 0;
  }

  DiskFileB xml_fp;
  err_string.Clear();
  doc_info.fname << clear << fname;

  xml_fp.df_Create(fname);
  if(!xml_fp) {
    format_err_str();
    err_string << "Cannot write to file: " << fname;
    return 0; // 10/14/2026: Returned 1 (success) when the create failed
  }

  gxListNode<gxsXMLNode> *ptr;
  gxString sbuf;

  if(pi_list.IsEmpty()) {
    sbuf << "<?xml";
    if(doc_info.xml_version_str.is_null()) {
      doc_info.xml_version_str = "1.0";
    }
    sbuf << " version=\"" << doc_info.xml_version_str << "\"";

    if(!doc_info.xml_encoding.is_null()) {
      sbuf << " encoding=\"" << doc_info.xml_encoding << "\"";
    }
    if(doc_info.xml_standalone == "yes") {
      sbuf << " standalone=\"yes\"";
    }
    sbuf << "?>" << "\n";

    if(!doc_info.xml_style_sheet.is_null()) {
      sbuf << doc_info.xml_style_sheet << "\n";
    }
    
    xml_fp << sbuf.c_str();
    if(xml_fp.df_GetError() != DiskFileB::df_NO_ERROR) {
      format_err_str();
      err_string << "Fatal error writing to file: " << fname;
      xml_fp.df_Close();
      return 0;
    }
  }
  else {
    ptr = pi_list.GetHead();
    while(ptr) {
      xml_fp << ptr->data.attributes.c_str() << "\n";
      if(xml_fp.df_GetError() != DiskFileB::df_NO_ERROR) {
	format_err_str();
	err_string << "Fatal error writing to file: " << fname;
	xml_fp.df_Close();
	return 0;
      }
      ptr = ptr->next;
    }
  }

  xml_fp << "\n";

  if(!comment_list.IsEmpty()) {
    ptr = comment_list.GetHead();
    while(ptr) {
      xml_fp << ptr->data.attributes.c_str() << "\n";
      if(xml_fp.df_GetError() != DiskFileB::df_NO_ERROR) {
	format_err_str();
	err_string << "Fatal error writing to file: " << fname;
	xml_fp.df_Close();
	return 0;
      }
      ptr = ptr->next;
    }
    xml_fp << "\n";
  }

  if(!dtd_list.IsEmpty()) {
    ptr = dtd_list.GetHead();
    while(ptr) {
      xml_fp << ptr->data.attributes.c_str() << "\n";
      if(xml_fp.df_GetError() != DiskFileB::df_NO_ERROR) {
	format_err_str();
	err_string << "Fatal error writing to file: " << fname;
	xml_fp.df_Close();
	return 0;
      }
      ptr = ptr->next;
    }
    xml_fp << "\n";
  }

  if(!cdata_list.IsEmpty()) {
    ptr = cdata_list.GetHead();
    while(ptr) {
      xml_fp << ptr->data.attributes.c_str() << "\n";
      if(xml_fp.df_GetError() != DiskFileB::df_NO_ERROR) {
	format_err_str();
	err_string << "Fatal error writing to file: " << fname;
	xml_fp.df_Close();
	return 0;
      }
      ptr = ptr->next;
    }
    xml_fp << "\n";
  }

  if(!xs.IsEmpty()) {
    if(!write_xml_tree(xs, &xml_fp, 0)) return 0; 
    xml_fp << "\n";
  }

  if(!write_xml_tree(node_tree, &xml_fp, 0)) return 0;  

  xml_fp.df_Close();
  return 1;
}

int gxsXML::write_xml_tree(gxList<gxsXMLNode> &xml_tree, 
			   DiskFileB *outfile, gxString *outstr)
{
  if((!outfile) && (!outstr)) {
    format_err_str();
    err_string << "No output stream specified to output XML tree";
    return 0;
  }
  if(outstr) outstr->Clear();

  gxString root_name;
  int i;

  gxListNode<gxsXMLNode> *lptr = xml_tree.GetHead();
  while(lptr) {
    if(lptr->data.node_type == gxsXML_ROOT_NODE) {
      root_name = lptr->data.name;
      if(outstr) {
	*(outstr) << "<" << lptr->data.name.c_str();
      }
      if(outfile) {
	*(outfile) << "<" << lptr->data.name.c_str();
	if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	  format_err_str();
	  err_string << outfile->df_ExceptionMessage();
	  return 0;
	}
      }

      if(!lptr->data.attributes.is_null()) {
	if(outstr) {
	  *(outstr) << " " << lptr->data.attributes.c_str();
	}
	if(outfile) {
	  *(outfile) << " " << lptr->data.attributes.c_str();
	  if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	    format_err_str();
	    err_string << outfile->df_ExceptionMessage();
	    return 0;
	  }
	}
      }
      if(outstr) {
	*(outstr) << ">" << "\n";
      }
      if(outfile) {
	*(outfile) << ">" << "\n";
	if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	  format_err_str();
	  err_string << outfile->df_ExceptionMessage();
	  return 0;
	}
      }
      if(!lptr->data.data.is_null()) {
	if(outstr) {
	  *(outstr) << lptr->data.data.c_str() << "\n";
	}
	if(outfile) {
	  *(outfile) << lptr->data.data.c_str() << "\n";
	  if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	    format_err_str();
	    err_string << outfile->df_ExceptionMessage();
	    return 0;
	  }
	}
      }
    }
   
    // 10/24/2026: Text found between the parent elements of the root
    if((lptr->data.node_type == gxsXML_TEXT_NODE) && 
       (!lptr->data.data.is_null())) {
      if(outstr) {
	*(outstr) << "  " << lptr->data.data.c_str() << "\n";
      }
      if(outfile) {
	*(outfile) << "  " << lptr->data.data.c_str() << "\n";
	if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	  format_err_str();
	  err_string << outfile->df_ExceptionMessage();
	  return 0;
	}
      }
    }

    if(lptr->data.node_type == gxsXML_PARENT_NODE) {
      gxString curr_parent = lptr->data.name;
      int parent_is_closed = 0;
      if(outstr) {
	*(outstr) << "  <" << lptr->data.name.c_str();
      }
      if(outfile) {
	*(outfile) << "  <" << lptr->data.name.c_str();
	if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	  format_err_str();
	  err_string << outfile->df_ExceptionMessage();
	  return 0;
	}
      }
      if(!lptr->data.attributes.is_null()) {
	if(outstr) {
	  *(outstr) << " " << lptr->data.attributes.c_str();
	}
	if(outfile) {
	  *(outfile) << " " << lptr->data.attributes.c_str();
	  if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	    format_err_str();
	    err_string << outfile->df_ExceptionMessage();
	    return 0;
	  }
	}
      }
      if(lptr->data.child_nodes.elements.IsEmpty()) { 
	if(lptr->data.data.is_null()) {
	  if(outstr) {
	    *(outstr) << " />" << "\n";
	  }
	  if(outfile) {
	    *(outfile) << " />" << "\n";
	    if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	      format_err_str();
	      err_string << outfile->df_ExceptionMessage();
	      return 0;
	    }
	  }
	}
	else {
	  if(outstr) {
	    *(outstr) << ">" << lptr->data.data.c_str() << "</" 
		      << curr_parent << ">" << "\n";
	  }
	  if(outfile) {
	    *(outfile) << ">" << lptr->data.data.c_str() << "</" 
		       << curr_parent.c_str() << ">" << "\n";
	    if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	      format_err_str();
	      err_string << outfile->df_ExceptionMessage();
	      return 0;
	    }
	  }
	}
	parent_is_closed = 1;
      }
      else {
	if(outstr) {
	  *(outstr) << ">" << "\n";
	}
	if(outfile) {
	  *(outfile) << ">" << "\n";
	  if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	    format_err_str();
	    err_string << outfile->df_ExceptionMessage();
	    return 0;
	  }
	}
      }

      if(!lptr->data.child_nodes.elements.IsEmpty()) {
	gxListNode<gxsXMLNode_t> *cptr = lptr->data.child_nodes.elements.GetHead();
	while(cptr) {
	  gxString curr_child = cptr->data.name;
	  int child_is_closed = 0;

	  // 10/24/2026: A child with no name is text found between the
	  // elements of the parent and is written as text.
	  if(curr_child.is_null()) {
	    if(!cptr->data.data.is_null()) {
	      if(outstr) {
		*(outstr) << "    " << cptr->data.data.c_str() << "\n";
	      }
	      if(outfile) {
		*(outfile) << "    " << cptr->data.data.c_str() << "\n";
		if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
		  format_err_str();
		  err_string << outfile->df_ExceptionMessage();
		  return 0;
		}
	      }
	    }
	    cptr = cptr->next;
	    continue;
	  }

	  if(outstr) {
	    *(outstr) << "    <" << cptr->data.name;
	  }
	  if(outfile) {
	    *(outfile) << "    <" << cptr->data.name.c_str();
	    if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	      format_err_str();
	      err_string << outfile->df_ExceptionMessage();
	      return 0;
	    }
	  }

	  if(!cptr->data.attributes.is_null()) {
	    if(outstr) {
	      *(outstr) << " " << cptr->data.attributes.c_str();
	    }
	    if(outfile) {
	      *(outfile) << " " << cptr->data.attributes.c_str();
	      if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
		format_err_str();
		err_string << outfile->df_ExceptionMessage();
		return 0;
	      }
	    }
	  }

	  if(!cptr->data.HasSubElements()) {
	    if(cptr->data.data.is_null()) {
	      if(outstr) {
		*(outstr) << " />" << "\n";
	      }
	      if(outfile) {
		*(outfile) << " />" << "\n";
		if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
		  format_err_str();
		  err_string << outfile->df_ExceptionMessage();
		  return 0;
		}
	      }
	    }
	    else {
	      if(outstr) {
		*(outstr) << ">" << cptr->data.data.c_str() << "</" 
			  << curr_child << ">" << "\n";
	      }
	      if(outfile) {
		*(outfile) << ">" << cptr->data.data.c_str() << "</" 
			   << curr_child.c_str() << ">" << "\n";
		if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
		  format_err_str();
		  err_string << outfile->df_ExceptionMessage();
		  return 0;
		}
	      }

	    }
	    child_is_closed = 1;
	  }
	  else {
	    if(outstr) {
	      *(outstr) << ">" << "\n";
	    }
	    if(outfile) {
	      *(outfile) << ">" << "\n";
	      if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
		format_err_str();
		err_string << outfile->df_ExceptionMessage();
		return 0;
	      }
	    }
	  }
	  // 10/14/2026: Rewritten. The old code tracked the nesting with
	  // the has_siblings flags only. Every level it opened stayed open
	  // until the end of the child unless the caller closed it with
	  // CloseSubChildAtSibling(), so trees loaded from files with more 
	  // than one nested level were written with the wrong nesting. It
	  // also changed the is_closed flags of the tree while writing.
	  // The nesting level of each sub-element is now taken from its
	  // depth member (set by the parser, AddSubChild() and AddSibling()).
	  if(cptr->data.HasSubElements()) {
	    gxsXMLSubElement *subptr; 
	    int num_sub = 0;
	    for(subptr = cptr->data.GetSubElements(); subptr; 
		subptr = subptr->next) num_sub++;
	    int *depths = new int[num_sub];

	    // Work out the nesting level of each sub-element. Elements
	    // with no depth set use the has_siblings, is_closed and 
	    // sub_child members the same way the old code did.
	    int curr = 0;
	    for(subptr = cptr->data.GetSubElements(); subptr; 
		subptr = subptr->next, curr++) {
	      gxsXMLNode_t *e = subptr->element;
	      int d = 1;
	      if(curr > 0) {
		gxsXMLNode_t *pe = subptr->prev->element;
		int max_depth = depths[curr-1];
		if(!pe->is_closed) max_depth++;
		if(e->depth > 0) {
		  d = e->depth;
		}
		else {
		  d = depths[curr-1];
		  if(pe->is_closed) {
		    if(pe->sub_child) {
		      gxsXMLSubElement *sptr = cptr->data.GetSubElements();
		      int k = 0;
		      while(sptr && (k < curr)) {
			if(sptr == pe->sub_child) {
			  d = depths[k];
			  break;
			}
			sptr = sptr->next;
			k++;
		      }
		    }
		  }
		  else if(pe->has_siblings) {
		    d++;
		  }
		}
		if(d > max_depth) d = max_depth;
	      }
	      if(d < 1) d = 1;
	      depths[curr] = d;
	    }

	    gxString sbuf;
	    curr = 0;
	    for(subptr = cptr->data.GetSubElements(); subptr; 
		subptr = subptr->next, curr++) {
	      gxsXMLNode_t *e = subptr->element;
	      int d = depths[curr];
	      int next_depth = 1;
	      if(subptr->next) next_depth = depths[curr+1];

	      sbuf.Clear();
	      for(i = 0; i < d; i++) sbuf << " ";
	      // 10/24/2026: A sub-element with no name is text found between
	      // elements and is written as text. It never contains elements.
	      if(e->name.is_null()) next_depth = (next_depth > d) ? d : next_depth;
	      else sbuf << "      <" << e->name;
	      if((!e->name.is_null()) && (!e->attributes.is_null())) {
		sbuf << " " << e->attributes;
	      }
	      if(next_depth > d) { // This element contains the next one
		sbuf << ">" << e->data << "\n";
	      }
	      else {
		if(e->name.is_null()) {
		  sbuf << "      " << e->data << "\n";
		}
		else if(e->data.is_null()) {
		  sbuf << " />" << "\n";
		}
		else {
		  sbuf << ">" << e->data << "</" << e->name << ">" << "\n";
		}

		// Close the levels that end after this element
		int level;
		for(level = d-1; level >= next_depth; level--) {
		  int k = curr;
		  gxsXMLSubElement *pptr = subptr;
		  while(pptr && (depths[k] != level)) {
		    pptr = pptr->prev;
		    k--;
		  }
		  if(!pptr) break;
		  for(i = 0; i < level; i++) sbuf << " ";
		  sbuf << "      </" << pptr->element->name << ">" << "\n";
		}
	      }

	      if(outstr) {
		*(outstr) << sbuf;
	      }
	      if(outfile) {
		*(outfile) << sbuf.c_str();
		if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
		  format_err_str();
		  err_string << outfile->df_ExceptionMessage();
		  delete[] depths;
		  return 0;
		}
	      }
	    }
	    delete[] depths;
	  } // End of sub elements check
	  
	  if(!child_is_closed) {
	    if(outstr) {
	      *(outstr) << "    </" << curr_child.c_str() << ">" << "\n";
	    }
	    if(outfile) {
	      *(outfile) << "    </" << curr_child.c_str() << ">" << "\n";
	      if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
		format_err_str();
		err_string << outfile->df_ExceptionMessage();
		return 0;
	      }
	    }
	  }
	  cptr = cptr->next;
	}
      }
      if(!parent_is_closed) {
	if(outstr) {
	  *(outstr) << "  </" << curr_parent.c_str() << ">" << "\n";
	}
	if(outfile) {
	  *(outfile) << "  </" << curr_parent.c_str() << ">" << "\n";
	  if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
	    format_err_str();
	    err_string << outfile->df_ExceptionMessage();
	    return 0;
	  }
	}
      }
    }

    lptr = lptr->next;
  }

  if(outstr) {
    *(outstr) << "</" << root_name << ">" << "\n";
  }
  if(outfile) {
    *(outfile) << "</" << root_name.c_str() << ">" << "\n";
    if(outfile->df_GetError() != DiskFileB::df_NO_ERROR) {
      format_err_str();
      err_string << outfile->df_ExceptionMessage();
      return 0;
    }
  }

  return 1;
}

gxsXMLNode *gxsXML::AddRoot(const char *name, const char *attributes, const char *data)
{
  if(!name) {
    format_err_str();
    err_string << "Passing null pointer to AddRoot function";
    return 0;
  }

  if(!node_tree.IsEmpty()) {
    format_err_str();
    err_string << "XML file or data is already loaded in memory";
    return 0;
  }

  gxsXMLNode root_node;
  root_node.name << clear << name;
  if(data) root_node.data << clear << data;
  if(attributes) root_node.attributes << clear << attributes; 
  root_node.node_type = gxsXML_ROOT_NODE;
  

  gxListNode<gxsXMLNode> *ptr = node_tree.Add(root_node);
  if(!ptr) {
    format_err_str();
    err_string << "Error adding root node to XML tree";
    return 0;
  }

  return &ptr->data;
}

gxsXMLNode *gxsXML::AddParent(const char *name, const char *attributes, const char *data)
{
  if(!name) {
    format_err_str();
    err_string << "Passing null pointer to AddParent function";
    return 0;
  }

  if(node_tree.IsEmpty()) {
    format_err_str();
    err_string << "XML tree has no root node";
    return 0;
  }

  gxsXMLNode n;
  n.name << clear << name;
  if(data) n.data << clear << data;
  if(attributes) n.attributes << clear << attributes; 
  n.node_type = gxsXML_PARENT_NODE;
  

  gxListNode<gxsXMLNode> *ptr = node_tree.Add(n);
  if(!ptr) {
    format_err_str();
    err_string << "Error adding parent node to XML tree";
    return 0;
  }

  return &ptr->data;
}

gxsXMLNode_t *gxsXML::AddChild(gxsXMLNode *parent, const char *name,
			       const char *attributes, const char *data)
{
  if((!parent) || (!name)) {
    format_err_str();
    err_string << "Passing null pointer to AddChild function";
    return 0;
  }

  gxsXMLNode_t element;
  element.name << clear << name;
  if(data) element.data << clear << data;
  if(attributes) element.attributes << clear << attributes; 

  gxListNode<gxsXMLNode_t> *ptr = parent->child_nodes.elements.Add(element);
  if(!ptr) {
    format_err_str();
    err_string << "Error adding child node to XML tree";
    return 0;
  }

  return &ptr->data;
}

int gxsXML::CloseChild(gxsXMLNode_t *child)
{
  if(!child) {
    format_err_str();
    err_string << "Passing null pointer to CloseChild function";
    return 0;
  }
  child->is_closed = 1;
  return 1;
}

int gxsXML::CloseSubChildAtSibling(gxsXMLSubElement *sub_child, gxsXMLSubElement *sibling)
{
  if((!sibling) || (!sub_child)){
    format_err_str();
    err_string << "Passing null pointer to CloseSubChildAtSibling function";
    return 0;
  }
  sibling->element->is_closed = 1;
  sibling->element->sub_child = sub_child;

  return 1;
}

gxsXMLSubElement *gxsXML::AddSubChild(gxsXMLNode_t *child, const char *name,
				      const char *attributes, const char *data)
{
  if((!child) || (!name)) {
    format_err_str();
    err_string << "Passing null pointer to AddSubChild function";
    return 0;
  }

  gxsXMLNode_t element;
  element.name << clear << name;
  if(data) element.data << clear << data;
  if(attributes) element.attributes << clear << attributes; 
  child->has_siblings = 1;
  element.depth = 1; // 10/14/2026: Directly inside the child element

  gxsXMLSubElement *ptr = child->AddSubElement(element);
  if(!ptr) {
    format_err_str();
    err_string << "Error adding sibling node to XML tree";
    return 0;
  }

  return ptr;
}

gxsXMLSubElement *gxsXML::AddSibling(gxsXMLNode_t *child, gxsXMLSubElement *sibling, 
				     const char *name,
				     const char *attributes, const char *data)
{
  if((!sibling) || (!name) || (!child)) {
    format_err_str();
    err_string << "Passing null pointer to AddSibling function";
    return 0;
  }
  
  sibling->element->has_siblings = 1;

  gxsXMLNode_t e;
  e.name << clear << name;
  if(data) e.data << clear << data;
  if(attributes) e.attributes << clear << attributes; 

  // 10/14/2026: The new element is nested one level inside "sibling"
  if(sibling->element->depth > 0) e.depth = sibling->element->depth + 1;

  gxsXMLSubElement *ptr = child->AddSubElement(e);
  if(!ptr) {
    format_err_str();
    err_string << "Error adding sibling node to XML tree";
    return 0;
  }

  return ptr;
}

int gxsXML::EncodeString(const gxString &unencoded_str, gxString &encoded_str)
{
  return EncodeString(unencoded_str.c_str(), encoded_str);
}

int gxsXML::EncodeString(const char *unencoded_str, gxString &encoded_str)
{
  if(!unencoded_str) return 0;
  encoded_str = unencoded_str;
  encoded_str.ReplaceString("&", "&amp;");
  encoded_str.ReplaceString("<", "&lt;");
  encoded_str.ReplaceString(">", "&gt;");
  encoded_str.ReplaceString("\'", "&apos;");
  encoded_str.ReplaceString("\"", "&quot;");
  return 1;
} 

int gxsXML::DecodeString(const gxString &encoded_str, gxString &unencoded_str)
{
  return DecodeString(encoded_str.c_str(), unencoded_str);
}

int gxsXML::DecodeString(const char *encoded_str, gxString &unencoded_str)
{
  if(!encoded_str) return 0;
  unencoded_str = encoded_str;
  // 10/14/2026: Replace &amp; last. Replacing it first decoded text
  // twice, so "&amp;lt;" was turned into "<" instead of "&lt;".
  unencoded_str.ReplaceString("&lt;", "<");
  unencoded_str.ReplaceString("&gt;", ">");
  unencoded_str.ReplaceString("&apos;", "\'");
  unencoded_str.ReplaceString("&quot;", "\"");
  unencoded_str.ReplaceString("&amp;", "&");
  return 1;
} 

// Derived class interface used to process SAX events
void gxsXML::Handle_PI_Event(gxsXMLNode & /* n */)
{

}

void gxsXML::Handle_DTD_Event(gxsXMLNode & /* n */)
{

}

void gxsXML::Handle_CDATA_Event(gxsXMLNode & /* n */)
{

}

void gxsXML::Handle_Comment_Event(gxsXMLNode & /* n */)
{

}

void gxsXML::Handle_XS_Event()
{

}

void gxsXML::Handle_Root_Event(gxsXMLNode & /* n */)
{

}

void gxsXML::Handle_Parent_Event(gxsXMLNode & /* n */)
{

}

void gxsXML::Handle_Child_Event(gxsXMLNode & /* n */)
{

}

void gxsXML::Handle_SubChild_Event(gxsXMLNode_t & /* n */)
{

}

void gxsXML::Handle_Sibling_Event(gxsXMLNode_t & /* n */)
{

}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
