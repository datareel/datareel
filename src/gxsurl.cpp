// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsurl.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 06/17/2016
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

The gxsURL class is used to parse uniform resource locators.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "gxsurl.h"
#include "strutil.h"

// Initialize all static data members
gxString gxsURL::dynamic_file_types = ".asp .aspx .cfm .php .cgi .jsp";
gxString gxsURL::dynamic_dir_types = "/bin/ /cgi-bin/";

// This following list of URL protocols is a combination
// standard and non-standard URI schemes taken from:
// http://www.w3.org/pub/WWW/Addressing/schemes.html
const char *gxsPROTOCOL_STRINGS[gxsMAX_PROTOCOL_STRINGS] =
{
  "Unknown_URL_protocol:", // The URL protocol specified is not known
  "about:",       // Client-Side JavaScript Reference
  "acap:",        // ACAP -- Application Configuration Access Protocol
  "afp:",         // Definition of afp: URLs for use with Service Location
  "afs:",         // Reserved, per Internet Standard list of url schemes 
  "callto:",      // NetMeeting Hyperlink on a Web Page
  "chttp:",       // The Caching Protocol in Authoring Content for RealPlayer 
  "cid:",         // Content-ID and Message-ID Uniform Resource Locators 
  "clsid:",       // Identifies OLE/COM classes. related to OBJECT spec 
  "data:",        // Data: URL scheme
  "file:",        // Host-specific file names URL RFC  
  "finger:",      // Finger protocol URL
  "ftp:",         // File Transfer protocol URL
  "gopher:",      // Gopher protocol URL
  "hdl:",         // CNRI handle system 
  "http:",        // Hypertext Transfer Protocol URL 
  "https:",       // HTTP over SSL
  "iioploc:",     // Interoperable Naming Joint Revised Submission
  "ilu:",         // ILU types, string binding handles 
  "imap:",        // IMAP URL scheme 
  "ior:",         // CORBA interoperable object reference 
  "java:",        // Identifies java classes 
  "javascript:",  // Client-Side JavaScript Reference
  "jdbc:",        // Used in Java SQL API 
  "ldap:",        // An LDAP URL Format
  "lifn:",        // BFD -- Bulk File distribution
  "mailto:",      // Electronic mail address
  "mid:",         // Content-ID and Message-ID Uniform Resource Locators
  "news:",        // USENET news
  "nfs:",         // NFS URL Scheme
  "nntp:",        // USENET news using NNTP access URL
  "path:",        // Path spec 
  "pop:",         // POP URL Scheme
  "pop3:",        // A POP3 URL Interface
  "printer:",     // Definition of printer: URLs for use with Service Location
  "prospero:",    // Prospero Directory Service URL
  "res:",         // Res Protocol
  "rtsp:",        // Real Time Streaming Protocol (RTSP)
  "rvp:",         // Rendezvous Protocol
  "rlogin:",      // Remote login
  "rwhois:",      // The RWhois Uniform Resource Locator
  "rx:",          // Remote Execution, per Mobile GUI On The Web
  "sdp:",         // SDP URL Scheme
  "service:",     // Service Templates and service
  "sip:",         // SIP URL Scheme
  "shttp:",       // ???
  "snews:",       // NNTP over SSL
  "stanf:",       // Stable Network Filenames 
  "telnet:",      // Reference to interactive sessions URL RFC 
  "tip:",         // Transaction Internet Protocol Version 3.0
  "tn3270:",      // Reserved, per Internet Standard list of url schemes 
  "tv:",          // Uniform Resource Locators for Television Broadcasts
  "uuid:",        // The UUID addressing scheme
  "wais:",        // Wide Area Information Servers URL 
  "whois:",       // Distributed directory service
  "whodp:"        // WhoDP: Widely Hosted Object Data Protocol
};

gxsURLInfo::gxsURLInfo() 
{ 
  proxy = 0;
  Reset();
}

gxsURLInfo::~gxsURLInfo() 
{ 
  // PC-lint 09/14/2005: Ignore PC-lint warning for freeing
  // gxsURLInfo::proxy. No memory is allocated for the data member
  // in this version of the library.
}

GXDLCODE_API int operator==(const gxsURLInfo &a, const gxsURLInfo &b) 
// Overloaded == operator added to work with gxs linked list class.
{
  if(a.url == b.url) return 1; 
  return 0;
}

GXDLCODE_API int operator!=(const gxsURLInfo &a, const gxsURLInfo &b)
// Overloaded != operator added to work with gxs linked list class.
{
  if(a.url != b.url) return 1; 
  return 0;
}

gxsURLInfo& gxsURLInfo::operator=(const gxsURLInfo &ob) 
{ 
  if(&ob != this) Copy(ob); 
  return *this; 
}

void gxsURLInfo::Copy(const gxsURLInfo &ob)
{
  url = ob.url; 
  proto = ob.proto; 
  host = ob.host; 
  port = ob.port;
  ftp_type = ob.ftp_type; 
  path = ob.path; 
  dir = ob.dir; 
  file = ob.file;
  user = ob.user; 
  passwd = ob.passwd; 
  proxy = ob.proxy; 
  local = ob.local;
  referer = ob.referer; 
  proto_type = ob.proto_type; 
  local_file = ob.local_file; 
  parent_directory = ob.parent_directory; 
  query_string = ob.query_string; 
  is_dynamic = ob.is_dynamic;
  local_query_file = ob.local_query_file; 
  dynamic_file_extension = ob.dynamic_file_extension;
  dynamic_bin_dir = ob.dynamic_bin_dir;
  sessionid = ob.sessionid;
}

void  gxsURLInfo::Reset()
{
  url.Clear();
  proto.Clear();
  host.Clear();
  path.Clear(); 
  dir.Clear(); 
  file.Clear();
  user.Clear();
  passwd.Clear();
  local.Clear();
  referer.Clear();
  port = 80;
  proto_type = 0;
  ftp_type = 'I';
  parent_directory.Clear();
  local_file.Clear();
  query_string.Clear();
  is_dynamic = 0;
  dynamic_bin_dir.Clear();
  dynamic_file_extension.Clear();
  local_query_file.Clear();
  sessionid.Clear();
}

char *gxsURL::GetProtocolString(int protocol)
{
  // Check for bad protocol values
  if(protocol > gxsMAX_PROTOCOL_STRINGS) protocol = gxs_Unknown_URL_protocol;
  return (char *)gxsPROTOCOL_STRINGS[protocol]; 
}

const char *gxsURL::GetProtocolString(int protocol) const
{
  // Check for bad protocol values
  if(protocol > gxsMAX_PROTOCOL_STRINGS) protocol = gxs_Unknown_URL_protocol;
  return (char *)gxsPROTOCOL_STRINGS[protocol]; 
}

int gxsURL::GetProtocolType(const gxString &protocol)
{
  char *s;
  int i; // Index of protocol types 

  for(s = (char *)gxsPROTOCOL_STRINGS, i = 0; *s; s++, i++) {
    if(CaseICmp(protocol, gxsPROTOCOL_STRINGS[i]) == 0)
      return i; // Corresponds to the recognized URL protocol enumeration
  }
  return gxsURL::gxs_Unknown_URL_protocol;
}

int gxsURL::ParseProtocol(const gxString &url, gxString &proto_name,
			  int &proto_type)
// Parse the protocol string in the specified URL. Passes back the
// protocol name in the "proto_name" variable and a numerical value
// that corresponds the string's position in the gxsPROTOCOL_STRINGS
// array. Returns false if no protocol is found in the URL. 
{
  int i;

  // Set the default values in case the protocol is unknown or not found
  proto_type = gxs_Unknown_URL_protocol;
  proto_name = gxsPROTOCOL_STRINGS[gxs_Unknown_URL_protocol];

  // 09/10/2006: Must account for dynamic pages and targets
  // Example: http://www.example.com/leaving.php?http://www.example.com
  gxString sbuf = url;
  sbuf.DeleteAfterIncluding("?");
  sbuf.DeleteAfterIncluding("#");

  // 03/30/2009: Account for JS session IDs
  // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
  sbuf.DeleteAfterIncluding(";");

  for(i = 0; i < gxsMAX_PROTOCOL_STRINGS; i++) {
    char *s = (char *)gxsPROTOCOL_STRINGS[i];
    if(sbuf.IFind(s, strlen(s), 0) != -1) {
      proto_type = i; // Corresponds to the recognized URL protocol enumeration
      proto_name = s;
      return 1;
    }
  }
    
  return 0;
}

void gxsURL::ProcessFTPType(const gxString &url, char &ftp_type)
// Determine the FTP type.
{
  const char *gxsFTP_TYPE_STRING = ";type=";
  gxString sbuf(url);
  int offset = sbuf.Find(gxsFTP_TYPE_STRING);
  if(offset == -1) { 
    ftp_type = '\0'; 
    return; 
  }
  offset += strlen(gxsFTP_TYPE_STRING); 
  ftp_type = sbuf[offset];
}

int gxsURL::CleanUserName(const gxString &url, gxString &clean_url)
// Removes the username and password string from a url and
// passes back a clean url in the "clean_url" variable. Returns
// false if the url does not contain a username or password.
{
  gxString sbuf(url);
  clean_url.Clear();
  const char *gxsHOSTNAME_ID = "://";
  
  // 09/10/2006: Must account for dynamic pages and targets
  // Example: http://username:passwd@www.example.com/leaving.php?http://www.example.com
  sbuf.DeleteAfterIncluding("?");
  sbuf.DeleteAfterIncluding("#");

  // 03/30/2009: Account for JS session IDs
  // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
  sbuf.DeleteAfterIncluding(";");

  // Identify where the host name starts and the protocol ends
  int hp_offset = sbuf.Find(gxsHOSTNAME_ID);

  // A URL hostname and protocol was not found
  if(hp_offset == -1) return 0;

  // Offset the hostname and protocol passed the gxsHOSTNAME_ID string
  hp_offset += strlen(gxsHOSTNAME_ID);
  
  // Is there an `@' character preceeding the hostname
  int up_offset = sbuf.Find("@", hp_offset);

  // This URL does not contain a username or password
  if(up_offset == -1) {
    clean_url = url;
    return 0; 
  }

  unsigned name_len = up_offset - hp_offset; // Length of the username info
  sbuf.DeleteAt(hp_offset, (name_len + 1));  // Account for the '@' character
  clean_url = sbuf;
  return 1;
}

int gxsURL::ParseUserName(const gxString &url,
			  gxString &user, gxString &passwd,
			  gxString &clean_url)
// Find the optional username and password within the URL, as per
// RFC1738. Returns false if the url does not contain a username
// or password. Passes back a url without the username and password
// information in the "clean_url" variable.
{
  gxString sbuf(url);
  const char *gxsHOSTNAME_ID = "://";
  clean_url.Clear();
  user.Clear();
  passwd.Clear();

  // 09/14/2006: Set the default protocol if not set
  SetDefaultProto(sbuf);

  // 09/10/2006: Must account for dynamic pages and targets
  // Example: http://username:passwd@www.example.com/leaving.php?http://www.example.com
  sbuf.DeleteAfterIncluding("?");
  sbuf.DeleteAfterIncluding("#");

  // 03/30/2009: Account for JS session IDs
  // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
  sbuf.DeleteAfterIncluding(";");

  // Identify where the host name starts and the protocol ends
  int offset = sbuf.Find(gxsHOSTNAME_ID);

  // A URL Host Name was not found
  if(offset == -1) {
    clean_url = url;
    return 0;
  }

  sbuf.DeleteAt(0, (offset+strlen(gxsHOSTNAME_ID)));
  sbuf.TrimLeadingSpaces(); // Remove any leading spaces

  // Is there an `@' character preceeding the hostname
  offset = sbuf.Find("@");
  if(offset == -1) {
    clean_url = url;
    return 0; // No user name was found
  }

  gxString ubuf, pbuf;  ubuf = pbuf = sbuf;
  offset = ubuf.Find(":");
  if(offset != -1) { // Delete everything after the : char
    ubuf.DeleteAt(offset, (ubuf.length() - offset));
    user = ubuf;
  }

  // Decide whether you have only the username or both
  offset = pbuf.Find(":");
  if(offset != -1) { // Delete everything before the : char
    pbuf.DeleteAt(0, offset);
  }
  offset = pbuf.Find("@");
  if(offset != -1) { // Delete everything after the : char
    pbuf.DeleteAt(offset, (pbuf.length() - offset));
    pbuf.DeleteAt(0, 1); // Get rid of the : character
    if(pbuf.length() > 0) passwd = pbuf;
  }

  // Remove the username and password information from the url
  CleanUserName(url, clean_url);
  return 1;
}

int gxsURL::ParsePortNumber(const gxString &url, int &port)
{
  gxString sbuf;
  port = -1;
  if(!ParseHostName(url, sbuf, 0)) return 0;

  // Look for a port number and remove everything before it
  int offset = sbuf.Find(":");
  if(offset == -1) return 0; // No port number was found
  sbuf.DeleteAt(0, offset+1);
  port = sbuf.Atoi();
  if(!port) return 0; // The port number was not valid
  return 1;
}

int gxsURL::ParseHostName(const gxString &url, gxString &host,
			  int remove_port_number)
// Parse the hostname from a url. If the "remove_port_number
// variable is true the port number will be removed from the
// host name if a port number was specified. Returns true if
// a valid hostname was found in the string containing the
// URL.
{
  gxString sbuf(url);
  const char *gxsHOSTNAME_ID = "://";
  host.Clear();

  // 09/11/2006: Set the default protocol if not set
  SetDefaultProto(sbuf);

  // Identify where the host name starts in a URL
  int offset = sbuf.Find(gxsHOSTNAME_ID);

  // A URL Host Name was not found
  if(offset == -1) return 0;

  sbuf.DeleteAt(0, (offset+strlen(gxsHOSTNAME_ID)));
  sbuf.TrimLeadingSpaces();
  offset = sbuf.Find("/"); // Scan to the end of the host name
  if(offset != -1) { 
    // Remove everything after the host name
    sbuf.DeleteAt(offset, (sbuf.length() - offset));
  }
  else {
    // Remove everything following a trailing space
    offset = sbuf.Find(" "); 
    if(offset != -1) {
      sbuf.DeleteAt(offset, (sbuf.length() - offset));
    }
  }
  if(remove_port_number) { 
    // Look for a port number and remove it if it is found
    offset = sbuf.Find(":");
    if(offset != -1) {
      sbuf.DeleteAt(offset, (sbuf.length() - offset));
    }
  }

  // Return false if no hostname exists after the hostname ID
  if(sbuf.length() == 0) return 0;
  
  host = sbuf;
  return 1;
}

int gxsURL::ParseDirectory(const gxString &url, gxString &path,
			   gxString &dir, gxString &file)
// Build the directory and filename components of the path.  
{
  gxString sbuf(url);
  const char *gxsHOSTNAME_ID = "://";
  path.Clear();
  dir.Clear();
  file.Clear();

  // 09/11/2006: Set the default protocol if not set
  SetDefaultProto(sbuf);

  // Identify where the host name starts in a URL
  int offset = sbuf.Find(gxsHOSTNAME_ID);

  // A URL Host Name was not found
  if(offset == -1) return 0;

  sbuf.DeleteAt(0, (offset+strlen(gxsHOSTNAME_ID)));
  sbuf.TrimLeadingSpaces();

  gxString dir_buf = sbuf;
  dir_buf.DeleteAfterIncluding("?");
  dir_buf.DeleteAfterIncluding("#");

  // 03/30/2009: Account for JS session IDs
  // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
  dir_buf.DeleteAfterIncluding(";");

  offset = dir_buf.Find("/"); // Scan to the end of the host name
  if(offset != -1) { 
    // Remove everything before the host name
    sbuf.DeleteAt(0, offset);
  }
  else {
    path = "/";
    dir = "/";
    return 0; // No path specified in the url
  }

  // Remove everything following a trailing space
  offset = sbuf.Find(" "); 
  if(offset != -1) {
    sbuf.DeleteAt(offset, (sbuf.length() - offset));
  }
  path = sbuf;

  return HasFile(path, dir, file);
}

int gxsURL::HasProtocol(const gxString &url)
// Returns protocol type if the URL begins with a protocol.
{
  int i;

  // 09/10/2006: Must account for dynamic pages and targets
  // Example: http://www.example.com/leaving.php?http://www.example.com
  gxString sbuf = url;
  sbuf.DeleteAfterIncluding("?");
  sbuf.DeleteAfterIncluding("#");

  // 03/30/2009: Account for JS session IDs
  // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
  sbuf.DeleteAfterIncluding(";");

  for(i = 0; i < gxsMAX_PROTOCOL_STRINGS; i++) {
    char *s = (char *)gxsPROTOCOL_STRINGS[i];
    if(sbuf.IFind(s, strlen(s), 0) != -1)
      return i; // Corresponds to the recognized URL prototypes enumeration
  }
    
  return gxsURL::gxs_Unknown_URL_protocol;
}

int gxsURL::GetPortNumber(const gxString &url, int &port)
// Obtain a port number according to the protocol specified
// in the URL. If the port number cannot be determined or 
// is not known this function will return false and set the 
// port number to port 80.
{
  int protocol = HasProtocol(url);
  switch(protocol) {
    case gxs_about : // Client-Side JavaScript Reference
      port = 2019;
      break;
    case gxs_acap : // ACAP -- Application Configuration Access Protocol
      port = 674;
      break;
    case gxs_afp : // Definition of afp or use with Service Location
      port = 548;
      break;
    case gxs_afs : // Reserved Internet Standard list of url schemes 
      port = 1483;
      break;
    case gxs_callto : // NetMeeting Hyperlink on a Web Page
      port = 80; // ???
      break;
    case gxs_chttp : // Caching Protocol in Authoring Content for RealPlayer 
      port = 80; // ???
      break;
    case gxs_cid : // Content-ID and Message-ID Uniform Resource Locators
      port = 80; // ???
      break;
    case gxs_clsid : // Identifies OLE/COM classes
      port = 80; // ???
      break;
    case gxs_data : // Data URL scheme
      port = 80; // ???
      break;
    case gxs_file :  // Host-specific file names URL RFC  
      port = 80; // ???
      break;
    case gxs_finger : // Finger protocol URL
      port = 79;
      break;
    case gxs_ftp : // File Transfer protocol URL
      port = 21;
      break;
    case gxs_gopher : // Gopher protocol URL
      port = 70;
      break;
    case gxs_hdl : // CNRI handle system 
      port = 2641; // ???
      break;
    case gxs_http :  // Hypertext Transfer Protocol URL 
      port = 80;
      break;
    case gxs_https : // HTTP over SSL (Secure Socket Layer)
      port = 443;
      break;
    case gxs_iioploc : // Interoperable Naming Joint Revised Submission
      port = 80; // ???
      break;
    case gxs_ilu : // ILU types
      port = 80; // ???
      break;
    case gxs_imap : // IMAP URL scheme 
      port = 143;
      break;
    case gxs_ior : // CORBA interoperable object reference 
      port = 80; // ???
      break;
    case gxs_java : // Identifies java classes 
      port = 80; // ???
      break;
    case gxs_javascript : // Cient-Side JavaScript Reference
      port = 80; // ???
      break;
    case gxs_jdbc : // Used in Java SQL API 
      port = 80; // ???
      break;
    case gxs_ldap : // An LDAP URL Format
      port = 389;
      break;
    case gxs_lifn : // BFD -- Bulk File distribution
      port = 80;
      break;
    case gxs_mailto : // Electronic mail address
      port = 25;
      break;
    case gxs_mid : // Content-ID and Message-ID Uniform Resource Locators
      port = 80;
      break;
    case gxs_news : // USENET news
      port = 144;
      break;
    case gxs_nfs : // NFS URL Scheme
      port = 2049;
      break;
    case gxs_nntp : // USENET news using NNTP access URL
      port = 119;
      break;
    case gxs_path : // Path spec 
      port = 117;
      break;
    case gxs_pop : // POP URL Scheme
      port = 109;
      break;
    case gxs_pop3 : // A POP3 URL Interface
      port = 110;
      break;
    case gxs_printer : // Definition of printer
      port = 515;
      break;
    case gxs_prospero : // Prospero Directory Service URL
      port = 191;
      break;
    case gxs_res : // Res Protocol
      port = 80; // ???
      break;
    case gxs_rtsp : // Real Time Streaming Protocol (RTSP)
      port = 554;
      break;
    case gxs_rvp : // Rendezvous Protocol
      port = 80; // ???
      break;
    case gxs_rlogin : // Remote login
      port = 513;
      break;
    case gxs_rwhois : // The RWhois Uniform Resource Locator
      port = 4321;
      break;
    case gxs_rx : // Remote Execution
      port = 512;
      break;
    case gxs_sdp : // SDP URL Scheme
      port = 80; // ???
      break; 
    case gxs_service :  // Service Templates and service
      port = 80; // ???
      break;
    case gxs_sip : // SIP URL Scheme
      port = 5060;
      break;
    case gxs_shttp : // Secure http
      port = 443;
      break;
    case gxs_snews : // NNTP over SSL
      port = 80; // ???
      break;
    case gxs_stanf : // Stable Network Filenames 
      port = 80; // ???
      break;
    case gxs_telnet : // Reference to interactive sessions URL RFC 
      port = 23;
      break;
    case gxs_tip : // Transaction Internet Protocol Version 3.0
      port = 80; // ???
      break;
    case gxs_tn3270 : // Reserved per Internet Standard list of url schemes 
      port = 23; // ???
      break;
    case gxs_tv : // Uniform Resource Locators for Television Broadcasts
      port = 80; // ???
      break;
    case gxs_uuid : // The UUID addressing scheme
      port = 80; // ???
      break;
    case gxs_wais : // Wide Area Information Servers URL 
      port = 80; // ???
      break;
    case gxs_whois : // Distributed directory service
      port = 43;
      break;
    case gxs_whodp : // WhoDP: Widely Hosted Object Data Protocol
      port = 80; // ???
      break;
    default:
      port = 80;
      return 0;
  }

  return 1;
}

int gxsURL::ParseURL(const gxString &url, gxsURLInfo &u, int strict)
// Extract the specified URL in the following format:
// URL protocol://username:password@hostname:port/path/filename
// Extracts the hostname terminated with `/' or `:'.
// Extracts the port number terminated with `/', or chosen for the protocol.
// The directory name equals everything after the hostname.
// The URL information will be passed back in the "u" variable. 
// Returns false if any errors occur during the parsing
// operation.
{
  u.url = url;

  int rv = ParseProtocol(url, u.proto, u.proto_type);

  if(strict && !rv) return 0;

  gxString clean_url;
  ParseUserName(url, u.user, u.passwd, clean_url);

  if(!ParseHostName(clean_url, u.host)) return 0;

  if(!ParsePortNumber(clean_url, u.port)) GetPortNumber(clean_url, u.port);

  if(u.proto_type == gxs_ftp) ProcessFTPType(clean_url, u.ftp_type);

  ParseDirectory(clean_url, u.path, u.dir, u.file);

  ParseDynamicPage(u);  

  return 1; // No errors reported
}

int gxsURL::HasFile(const gxString &path)
// Returns true if the path has a file associated with it.
{
  gxString d, f;
  return HasFile(path, d, f);
}

int gxsURL::ParseDirectory(gxsURLInfo &u)
// Build the directory and filename components of the path
// specified in the gxsURLInfo object.  
{
  return HasFile(u.path, u.dir, u.file);
}

int gxsURL::ParseURL(const char *url, gxsURLInfo &u, int strict)
{
  gxString sbuf(url);
  return ParseURL(sbuf, u, strict);
}

int gxsURL::HasFile(const gxString &path, gxString &dir, gxString &file)
// Returns true if the path has a file associated with it.
// The directory and file name will be passed back in the
// "dir" and "file" variables.
{
  dir.Clear();
  file.Clear();

  // No path specified so this must be the top level directory
  if((path.is_null()) || (path == "/")) { 
    dir = "/";
    return 0; 
  }

  gxString sbuf(path);
  if(sbuf[sbuf.length()-1] == '/') {
    // This is a complete path with no file name specified
    // Example: http://www.example.com/docs/
    dir = path;
    if(dir[dir.length()-1] == '/') dir.DeleteAfterLastIncluding("/");
    return 0;
  }

  int has_file = 0;
  int offset = 0;
  int index = 0;
  
  sbuf.TrimLeadingSpaces(); // Trim any leading spaces
  
  // Remove everything following a trailing space
  sbuf.DeleteAfterIncluding(" ");
  
  // 09/10/2006: Must account for dynamic pages and targets
  // Example: <a href="/leaving.php?http://www.example.com>
  gxString dir_buf = sbuf;

  dir_buf.DeleteAfterIncluding("?");
  dir_buf.DeleteAfterIncluding("#");

  // 03/30/2009: Account for JS session IDs
  // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
  dir_buf.DeleteAfterIncluding(";");

  // 11/17/2006: Account for queries that reference the 
  // default root index file without a file name.
  // www.example.com/?ContentID=2083
  if(dir_buf == "/") {
    dir = "/";
    return has_file = 0;
  }

  // 11/17/2006: Account for queries that reference a 
  // default index file without a file name.
  // www.example.com/documents/?ContentID=2083
  if(dir_buf[dir_buf.length()-1] == '/') {
    dir = dir_buf;
    dir.TrimTrailing('/');
    return has_file = 0;
  }

  offset = 0;
  while(offset != -1) { // Look for a file name following the last foward slash
    offset = dir_buf.Find("/", offset);
    if(offset != -1) index = offset;
    if(offset == -1) break;
    offset++;
  }

  if(index > 0) { // Found a directory path 
    dir = sbuf;
    // Look for a file name after the last foward slash
    offset = dir.Find(".", index);
    if(offset != -1) {
      dir.DeleteAt(index, (dir.length() - index));
      sbuf.DeleteAt(0, ++index);
      if(sbuf.length() > 0) { 
	has_file = 1; 
	file = sbuf; 
      }
    }
    else { // There are characters after the last forward slash
      // This could be a directory or a file name some so mark
      // the file name with a question mark to tell the application
      // to check the file type before downloading it.
      has_file = 0; // Assume that this is a directory
      file = "?";
    }
  }
  else { // No path was specified but there are characters after the slash
    offset = sbuf.Find(".");
    if(offset == -1) { // This is part of a path name
      dir = sbuf;
    }
    else { // This is a file name in the home directory
      dir = "/"; 
      sbuf.DeleteAt(0, 1); // Get rid of the forward slash

      file = sbuf;
      has_file = 1;
    }
  }

  return has_file;
}

int gxsURL::ParseDynamicPage(gxsURLInfo &u) 
{
  return ParseDynamicPage(u, 
			  gxsURL::dynamic_file_types, 
			  gxsURL::dynamic_dir_types);
}

int gxsURL::ParseDynamicPage(gxsURLInfo &u, 
			     const gxString &my_dynamic_file_types,
			     const gxString &my_dynamic_dir_types)
{
  return ParseDynamicPage(u, 
			  my_dynamic_file_types, 
			  my_dynamic_dir_types,
			  u.is_dynamic,
			  u.query_string,
			  u.dynamic_file_extension,
			  u.dynamic_bin_dir,
			  u.local_query_file);
}


int gxsURL::ParseDynamicPage(gxsURLInfo &u, 
			     const gxString &my_dynamic_file_types,
			     const gxString &my_dynamic_dir_types,
			     int &is_dynamic,
			     gxString &query_string,
			     gxString &file_extension,
			     gxString &bin_dir,
			     gxString &local_query_file)
{
  return ParseDynamicPage(u.url, 
			  my_dynamic_file_types, 
			  my_dynamic_dir_types,
			  u.is_dynamic,
			  u.query_string,
			  u.dynamic_file_extension,
			  u.dynamic_bin_dir,
			  u.local_query_file);
}


int gxsURL::IsDynamicPage(const gxString &url)
{
  int is_dynamic;
  gxString query_string;
  gxString file_extension;
  gxString bin_dir;
  gxString local_query_file;
  
  return ParseDynamicPage(url, 
			  gxsURL::dynamic_file_types, 
			  gxsURL::dynamic_dir_types,
			  is_dynamic,
			  query_string,
			  file_extension,
			  bin_dir,
			  local_query_file);
  
}

int gxsURL::IsDynamicPage(const gxString &url, 
			  const gxString &my_dynamic_file_types,
			  const gxString &my_dynamic_dir_types)
{
  int is_dynamic;
  gxString query_string;
  gxString file_extension;
  gxString bin_dir;
  gxString local_query_file;
  
  return ParseDynamicPage(url, 
			  my_dynamic_file_types, 
			  my_dynamic_dir_types,
			  is_dynamic,
			  query_string,
			  file_extension,
			  bin_dir,
			  local_query_file);
}

int gxsURL::ParseDynamicPage(const gxString &url, 
			     int &is_dynamic,
			     gxString &query_string,
			     gxString &file_extension,
			     gxString &bin_dir,
			     gxString &local_query_file)
{
  return ParseDynamicPage(url, 
			  gxsURL::dynamic_file_types, 
			  gxsURL::dynamic_dir_types,
			  is_dynamic,
			  query_string,
			  file_extension,
			  bin_dir,
			  local_query_file);
}

int gxsURL::FilterQueryCharacters(gxString &local_query_file)
// Filter function used to clean a local file name so that is can be stored
// to disk with a unique name for each dynamic page.
{
  // 09/10/2006: Must account for dynamic pages that reference other sites:
  // Example: <a href="/leaving.php?http://www.example.com>
  // Filter the well known characters used in Web queries
  local_query_file.ReplaceString("://", "_URL_");
  
  // Auto convert any hex codes in the URL. Some
  // Web sites use hex code in place of the query 
  // string characters.
  local_query_file.ConvertEscapeSeq('%', 2);

  // 03/30/2009: Use a string not just an underscore
  // Filter the well known characters used in Web queries
  local_query_file.ReplaceString("?", "_Q_");
  local_query_file.ReplaceString("|", "_PIPE_");
  local_query_file.ReplaceString("*", "_STAR_");
  local_query_file.ReplaceString("&", "_AND_");
  local_query_file.ReplaceString("+", "_PLUS_");
  local_query_file.ReplaceString("=", "_EQ_");
  local_query_file.ReplaceString("(", "_LP_");
  local_query_file.ReplaceString(")", "_RP_");
  
  // 03/30/2009: Use a string not just an underscore
  // Filter the any special characters used in Web queries
  local_query_file.ReplaceString("\"", "_QT_");
  local_query_file.ReplaceString("'", "_SQT_");
  local_query_file.ReplaceString("`", "_BQT_");
  local_query_file.ReplaceString("~", "_TIL_");
  local_query_file.ReplaceString("!", "_NOT_");
  local_query_file.ReplaceString("@", "_AT_");
  local_query_file.ReplaceString("#", "_NUM_");
  local_query_file.ReplaceString("$", "_STR_");
  local_query_file.ReplaceString("%", "_PER__");
  local_query_file.ReplaceString("^", "_CAR_");
  local_query_file.ReplaceString(";", "_SC_");
  local_query_file.ReplaceString(":", "_C_");  
  local_query_file.ReplaceString(",", "_COM_");
  local_query_file.ReplaceString("<", "_LT_");
  local_query_file.ReplaceString(">", "_GT_");
  local_query_file.ReplaceString("{", "_LB_");
  local_query_file.ReplaceString("}", "_RB_");
  
  // Collapse the path separators if any illegal characters were 
  // deleted between them .
  local_query_file.ReplaceString("//", "/");
  
  return 1;
}

int gxsURL::ParseDynamicPage(const gxString &url, 
			     const gxString &my_dynamic_file_types,
			     const gxString &my_dynamic_dir_types,
			     int &is_dynamic,
			     gxString &query_string,
			     gxString &file_extension,
			     gxString &bin_dir,
			     gxString &local_query_file)
{
  is_dynamic = 0;
  query_string.Clear();
  file_extension.Clear();
  bin_dir.Clear();
  local_query_file.Clear();
  char words[MAXWORDS][MAXWORDLENGTH];
  int num_words, i;

  gxString url_buf = url;

  // Set the default host name ID and protocol string
  // if not already set by the caller.
  SetDefaultProto(url_buf);

  // Check for query sep character first converting
  // any hex codes in the URL to ASCII. Some
  // Web sites use hex code in place of the query 
  // string characters.
  url_buf.ConvertEscapeSeq('%', 2);
  if(url_buf.Find("?") != -1) {
    is_dynamic = 1;
    query_string = url_buf;
    query_string.DeleteBefore("?");
  }

  // Check for known dyanmic dot extenstions and bin directories
  gxString path, dir, file;
  ParseDirectory(url_buf, path, dir, file);

  if(file == "?") { // Assume this is a file 
    file = path;
    file.DeleteBeforeLastIncluding("/");
    file.FilterString("/");
    dir.FilterString(file);
  }

  if(!file.is_null()) { // Record the file's dot extension
    if(file.Find(".") != -1) {
      file_extension = file;
      // 09/10/2006: Must account for dynamic pages and targets
      file_extension.DeleteAfterIncluding("?");
      file_extension.DeleteAfterIncluding("#");

      // 03/30/2009: Account for JS session IDs
      // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
      file_extension.DeleteAfterIncluding(";");

      file_extension.DeleteBeforeLast(".");
    }
    else {
      file_extension.Clear(); // This file has no extention
    }
  }

  if(!is_dynamic) {
    if(!file_extension.is_null()) {
      if(parse(my_dynamic_file_types.c_str(), words, &num_words, ' ') == 0) {
	for(i = 0; i < num_words; i++) {
	  if(*words[i] == 0) break;
	  if(file_extension.IFind(words[i]) != -1) {
	    is_dynamic = 1;
	    break;
	  }	
	}
      }
      else {
	if(my_dynamic_file_types.IFind(file_extension, 
				       file_extension.length(), 0) != -1) {
	  is_dynamic = 1;
	}
      }
    }
  }

  if(!is_dynamic) { // Check for known binary dirs used on the Web
    if(!dir.is_null()) {
      bin_dir = dir;
      if(bin_dir[bin_dir.length()-1] == '/') {
	bin_dir.DeleteAfterLastIncluding("/");
      }
      bin_dir.DeleteBeforeLastIncluding("/");
      if(bin_dir[0] != '/') {
	bin_dir.InsertAt(0, "/", 1);
      }
      bin_dir << "/";
      if(my_dynamic_dir_types.IFind(bin_dir) != -1) {
	is_dynamic = 1;
      }
      bin_dir.FilterString("/");
    }
  }

  if(is_dynamic) { 
    if(!file.is_null()) {
      // Clean the local file name so that is can be stored
      // to disk with a unique name for each dynamic page.
      if(!file_extension.is_null()) {
	int file_is_dynamic_page = 0;

	if(parse(my_dynamic_file_types.c_str(), words, &num_words, ' ') == 0) {
	  for(i = 0; i < num_words; i++) {
	    if(*words[i] == 0) break;
	    if(file_extension.IFind(words[i]) != -1) {
	      file_is_dynamic_page = 1;
	      break;
	    }	
	  }
	}
	else {
	  if(my_dynamic_file_types.IFind(file_extension, file_extension.length(), 0) != -1) {
	    file_is_dynamic_page = 1;	 
	  }
	}

	if(file_is_dynamic_page) {
	  // We know this a dyanmic Web page so make the local copy a static HTML page.
	  local_query_file << clear << file << ".html";
	}
	else {
	  // If this is a Web object we must keep the 
	  // orginal file extension.
	  gxString ext_buf = file.Right(file_extension.length());
	  if(ext_buf != file_extension) {
	    local_query_file << clear << file << file_extension;
	  }
	  else { // Prevent double file extensions
	    local_query_file << clear << file;
	  }
	}
      }
      else {
	local_query_file << clear << file << ".html";
      }
    }
    else {
      // No file was contained in the URL so the we a 
      // performing a query on the default index page in 
      // this directory.
      if(query_string.is_null()) {
	local_query_file = "index.html"; 
      }
      else {
	local_query_file << clear << "index.html" << query_string << ".html"; 
      }
    }
  }
  
  if(is_dynamic) { 
    if(!local_query_file.is_null()) {
      // Clean the local file name so that is can be stored
      // to disk with a unique name for each dynamic page.
      FilterQueryCharacters(local_query_file);

      // 11/15/2006: Remove any path characters from the file name
      local_query_file.ReplaceString("/", "_");

      // 12/05/2007: Ensure that the local file has a dot extension 
      gxString static_ext = local_query_file.Right(5);
      static_ext.ToLower();
      if(static_ext != ".html") local_query_file << ".html";

    }
    else {
      local_query_file = "index.html"; 
    }
  }

  // Return true if this is a dynamic Web page
  return is_dynamic == 1;
}

int gxsURL::SetDefaultProto(gxString &url)
{
  gxString gxsHOSTNAME_ID = "://";

  // 09/10/2006: Must account for dynamic pages and targets
  // Example: /leaving.php?http://www.example.com
  gxString sbuf = url;
  sbuf.DeleteAfterIncluding("?");
  sbuf.DeleteAfterIncluding("#");

  // 03/30/2009: Account for JS session IDs
  // Example: /switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B
  sbuf.DeleteAfterIncluding(";");

  // Identify where the host name starts in a URL
  int offset = sbuf.Find(gxsHOSTNAME_ID);

  // No protocol string or hostname ID was found so
  // assume http:// on port 80
  if(offset == -1) {
    gxsHOSTNAME_ID.InsertAt(0, "http", 4);
    url.InsertAt(0, gxsHOSTNAME_ID);
    return 1; // The default proto was set
  }

  return 0; // No default proto was set
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

