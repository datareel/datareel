// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsxml.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

DataReel XML parser.
*/
// ----------------------------------------------------------- //  
#ifndef __GXS_XML_HPP__
#define __GXS_XML_HPP__

#include "gxdlcode.h"

#include "gxstring.h"
#include "gxlist.h"
#include "dfileb.h"
#include "membuf.h"

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(disable:4251) // Disable dll-interface template warning
#endif

// Constants
const unsigned gxsXML_DEFAULT_READ_LEN = 4096; // Optimum buffer size
const unsigned gxsXML_MIN_READ_LEN = 256;      // One line length
const unsigned gxsXML_MAX_READ_LEN = gxsXML_DEFAULT_READ_LEN * 16;

enum gxsXMLNodeType {
  gxsXML_NODE_TYPE_NOT_SET = 0,

  // Hierarchal tree node types
  gxsXML_ROOT_NODE,
  gxsXML_PARENT_NODE,
  gxsXML_CHILD_NODE,
  gxsXML_SUBELEMENT_NODE,

  // XML document processing types
  gxsXML_COMMENT_NODE, // Comment nodes
  gxsXML_PROC_NODE,    // Processing instruction nodes
  gxsXML_DTD_NODE,     // Document type declaration nodes
  gxsXML_CDATA_NODE    // CDATA type
  // CDATA contains information expressed in a non-XML format
};

enum gxsXMLElementType {
  gxsXML_ELEMENT_TYPE_NOT_SET = 0,

  gxsXML_TEXT_ELEMENT,      // Text nodes
  gxsXML_ATTRIBUTE_ELEMENT, // Attribute nodes
  gxsXML_COMMENT_ELEMENT,   // Comment nodes
  gxsXML_NAMESPACE_ELEMENT  // Namespace nodes
};

enum gxsXMLParserType {
  gxsXML_PARSER_TYPE_NOT_SET = 0,
  gxsXML_PARSER_DOM, // DOM (Document Object Model) loads entire tree in memory
  gxsXML_PARSER_SAX  // SAX (Simple API for XML) event driven node processing
};

struct GXDLCODE_API gxsXMLDocInfo
{
  gxsXMLDocInfo() { Clear(); }
  ~gxsXMLDocInfo() { }
  gxsXMLDocInfo(const gxsXMLDocInfo &ob) {
    Copy(ob);
  }
  gxsXMLDocInfo &operator=(const gxsXMLDocInfo &ob);

  void Clear();
  void Copy(const gxsXMLDocInfo &ob);

  gxString fname;
  gxString xml_version_str;
  gxString xml_encoding;
  gxString xml_style_sheet;
  gxString xml_standalone;
};

// Forward class calling conventions
struct gxsXMLSubElement;

struct GXDLCODE_API gxsXMLNode_t
{
  gxsXMLNode_t();
  ~gxsXMLNode_t();
  gxsXMLNode_t(const gxsXMLNode_t &ob);
  gxsXMLNode_t &operator=(const gxsXMLNode_t &ob);

  void Clear();
  void Copy(const gxsXMLNode_t &ob);
  gxsXMLSubElement *AddSubElement(gxsXMLNode_t &element);
  gxsXMLSubElement *GetSubElements() { return subelements; }
  gxsXMLSubElement *GetLastSubElement() { return last_subelement; }
  int HasSubElements();
  int HasSiblings();

  // Subelement processing functions
  void delete_last_subelement();
  void destroy_subelements();
  void delete_subelement(gxsXMLSubElement *n);
  void detach_subelement(gxsXMLSubElement *n);
  void insert_before(gxsXMLSubElement *pos, gxsXMLSubElement *n);
  void insert_after(gxsXMLSubElement *pos, gxsXMLSubElement *n);
  gxsXMLSubElement *add_sub_element(gxsXMLSubElement *n);

  gxString name;
  gxString data;
  gxString attributes;
  int has_siblings;
  int is_closed;
  gxsXMLSubElement *sub_child;

 private:
  gxsXMLSubElement *subelements;
  gxsXMLSubElement *last_subelement;
};

struct GXDLCODE_API gxsXMLSubElement
{
  gxsXMLSubElement() { 
    next = prev = 0; 
    element = 0;
  }
  gxsXMLSubElement *next;
  gxsXMLSubElement *prev;
  gxsXMLNode_t *element;
};

struct GXDLCODE_API gxsXMLElement
{
  gxsXMLElement() { Clear(); }
  ~gxsXMLElement() { Clear(); }
  gxsXMLElement(const gxsXMLElement &ob) {
    Copy(ob);
  }
  gxsXMLElement &operator=(const gxsXMLElement &ob);

  void Clear();
  void Copy(const gxsXMLElement &ob);

  gxList<gxsXMLNode_t> elements;
};

struct GXDLCODE_API gxsXMLNode
{
  gxsXMLNode() { Clear(); }
  ~gxsXMLNode() { }
  gxsXMLNode(const gxsXMLNode &ob) {
    Copy(ob);
  }
  gxsXMLNode &operator=(const gxsXMLNode &ob);

  void Clear();
  void Copy(const gxsXMLNode &ob);

  gxString name; // Name of the node in the XML file          
  gxString data; // Data for nodes with no children
  gxString attributes; // List of attributes for this node
  gxsXMLNodeType node_type; // Type of node root, parent, child
  gxsXMLElementType element_type; // Type of element
  gxsXMLElement child_nodes;  // Data for nodes with children
  int sax_end_parse; // Signal from SAX event to stop file load
};

class GXDLCODE_API gxsXML : public DiskFileB
{
 public:
  gxsXML() { Clear(); }
  ~gxsXML() { }
  gxsXML(const gxsXML &ob) { Copy(ob); }
  gxsXML &operator=(const gxsXML &ob);

 public:
  char *GetError();
  char *SetError(const char *s);
  void Clear();
  void Copy(const gxsXML &ob);
  int EncodeString(const char *unencoded_str, gxString &encoded_str);
  int EncodeString(const gxString &unencoded_str, gxString &encoded_str);
  int DecodeString(const char *encoded_str, gxString &unencoded_str);
  int DecodeString(const gxString &encoded_str, gxString &unencoded_str);

 protected: // Internal processing functions
  int load_file(const char *fname);
  int find_tag(gxString &node, int offset, int &start, int &end);
  int explode_nodes(gxString &node, gxList<gxsXMLNode_t> &node_list);
  int explode_nodes(gxString &node, gxsXMLNode &n);
  int is_xml(DiskFileB &xml_fp, unsigned read_length);
  void format_err_str();
  int has_sub_elements(const gxString &node_data);
  int has_sub_elements(const gxsXMLElement &element);
  int strip_comments(gxString &pd);
  int test_comments(const gxString &pd);
  int write_xml_tree(gxList<gxsXMLNode> &xml_tree, 
		     DiskFileB *outfile, gxString *outstr);

 public: // Parser functions
  int GetXMLAttribute(const gxString &tag_str, const gxString &attr_str, 
		      gxString &value);
  void CleanTagName(const gxString &tag, gxString &tag_name);
  int HasAttributes(const gxString &tag);
  void CleanAttributes(const gxString &tag, gxString &attrib);

 public: // Input functions
  int LoadXMLFile(const char *fname);
  gxsXMLNode *AddRoot(const char *name, 
		      const char *attributes = 0, const char *data = 0);
  gxsXMLNode *AddParent(const char *name,
			const char *attributes = 0, const char *data = 0);
  gxsXMLNode_t *AddChild(gxsXMLNode *parent, const char *name,
			 const char *attributes = 0, const char *data = 0);
  int CloseChild(gxsXMLNode_t *child);
  gxsXMLSubElement *AddSubChild(gxsXMLNode_t *child, const char *name,
				const char *attributes = 0, const char *data = 0);
  int CloseSubChildAtSibling(gxsXMLSubElement *sub_child, gxsXMLSubElement *sibling);
  gxsXMLSubElement *AddSibling(gxsXMLNode_t *child, gxsXMLSubElement *sibling, 
			       const char *name,
			       const char *attributes = 0, const char *data = 0);

 public: // Output functions
  int WriteXS(gxString &xs_string);
  int WriteTree(gxString &tree_string);
  int WriteXMLFile(const char *fname);

 public:
  // SAX (Simple API for XML) parsing events
  virtual void Handle_PI_Event(gxsXMLNode &n);
  virtual void Handle_DTD_Event(gxsXMLNode &n);
  virtual void Handle_CDATA_Event(gxsXMLNode &n);
  virtual void Handle_Comment_Event(gxsXMLNode &n);
  virtual void Handle_XS_Event();
  virtual void Handle_Root_Event(gxsXMLNode &n);
  virtual void Handle_Parent_Event(gxsXMLNode &n);
  virtual void Handle_Child_Event(gxsXMLNode &n);
  virtual void Handle_SubChild_Event(gxsXMLNode_t &n);
  virtual void Handle_Sibling_Event(gxsXMLNode_t &n);

 public:
  gxsXMLDocInfo doc_info;
  gxsXMLParserType parser_type;
  gxList<gxsXMLNode> pi_list;      // Process instruction list
  gxList<gxsXMLNode> comment_list; // Comment list before root
  gxList<gxsXMLNode> dtd_list;     // Document type declaration list
  gxList<gxsXMLNode> cdata_list;   // CDATA list before root
  gxList<gxsXMLNode> xs;           // XML Schema
  gxList<gxsXMLNode> node_tree;    // Hierarchal node structure  

 protected:
  unsigned read_buf_len;
  gxString err_string;
};

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(default:4251) // Restore previous warning
#endif

#endif // __GXS_XML_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


