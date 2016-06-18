// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxsurl.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
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

The gxsURL class is used to parse uniform resource locators.
*/
// ----------------------------------------------------------- // 
#ifndef __GX_URL_HPP__
#define __GX_URL_HPP__

#include "gxdlcode.h"

#include "gxstring.h"

// Structure containing info on a URL. 
struct GXDLCODE_API gxsURLInfo
{
  gxsURLInfo();
  ~gxsURLInfo();
  gxsURLInfo(const gxsURLInfo &ob) { Copy(ob); }
  gxsURLInfo& operator=(const gxsURLInfo &ob);
  void Copy(const gxsURLInfo &ob);
  void Reset();

  // Overloaded operators
  GXDLCODE_API friend int operator==(const gxsURLInfo &a, const gxsURLInfo &b);
  GXDLCODE_API friend int operator!=(const gxsURLInfo &a, const gxsURLInfo &b);

  // URL information
  gxString url;	            // Unchanged URL 
  gxString proto;	    // URL protocol 
  gxString host;            // Extracted hostname 
  gxString path, dir, file; // Path, as well as dir and file 
  gxString user, passwd;    // Username and password 
  gxsURLInfo *proxy;        // The exact string to pass to proxy server 
  gxString local;           // The local filename of the URL document
  gxString referer;	    // Source that requested URI was obtained
  int port;                 // Port number
  int proto_type;           // Enumerated value representing a protocol type 
  char ftp_type;            // FTP type

  // Members used by HTTP clients
  gxString parent_directory; // This resource's parent directory
  gxString local_file;       // Local file name for cached copy

  // 07/27/2006: Members added to handle dynamic Web pages
  gxString query_string; // Query string parsed from URL 
  int is_dynamic;        // True if this is a dynamic Web page,
                         // with or without a URL query string. 
  gxString dynamic_bin_dir;        // Binary directory of active page
  gxString dynamic_file_extension; // Dot extension of active page
  gxString local_query_file; // Local .html file name used to
                             // store dynamic Web content.

  // 03/30/2009: Added to handle JS session IDs
  // Example: <a href="/switchsite.ds;jsessionid=54D3A5C9DC8A767DF64B3A377FB3EA8B">
  gxString sessionid;
};

// Maximum number of reconized URL protocols.
// Adjust this value to match the number of
// protocols defined in the gxsPROTOCOL_STRINGS
// arrary and enumeration.
const int gxsMAX_PROTOCOL_STRINGS = 56;

class GXDLCODE_API gxsURL
{
public:
  // This following list of URL protocols is a combination
  // standard and non-standard URI schemes taken from:
  // http://www.w3.org/pub/WWW/Addressing/schemes.html
  enum { // Recognized URL protocols
    gxs_Unknown_URL_protocol = 0, // The URL protocol specified is not known
    gxs_about,       // Client-Side JavaScript Reference
    gxs_acap,        // ACAP -- Application Configuration Access Protocol
    gxs_afp,         // Definition of afp: URLs for use with Service Location
    gxs_afs,         // Reserved, per Internet Standard list of url schemes 
    gxs_callto,      // NetMeeting Hyperlink on a Web Page
    gxs_chttp,       // Caching Protocol in Authoring Content for RealPlayer 
    gxs_cid,         // Content-ID and Message-ID Uniform Resource Locators 
    gxs_clsid,       // Identifies OLE/COM classes. related to OBJECT spec 
    gxs_data,        // Data: URL scheme
    gxs_file,        // Host-specific file names URL RFC  
    gxs_finger,      // Finger protocol URL
    gxs_ftp,         // File Transfer protocol URL
    gxs_gopher,      // Gopher protocol URL
    gxs_hdl,         // CNRI handle system 
    gxs_http,        // Hypertext Transfer Protocol URL 
    gxs_https,       // HTTP over SSL (Secure Socket Layer)
    gxs_iioploc,     // Interoperable Naming Joint Revised Submission
    gxs_ilu,         // ILU types, string binding handles 
    gxs_imap,        // IMAP URL scheme 
    gxs_ior,         // CORBA interoperable object reference 
    gxs_java,        // Identifies java classes 
    gxs_javascript,  // Client-Side JavaScript Reference
    gxs_jdbc,        // Used in Java SQL API 
    gxs_ldap,        // An LDAP URL Format
    gxs_lifn,        // BFD -- Bulk File distribution
    gxs_mailto,      // Electronic mail address
    gxs_mid,         // Content-ID and Message-ID Uniform Resource Locators
    gxs_news,        // USENET news
    gxs_nfs,         // NFS URL Scheme
    gxs_nntp,        // USENET news using NNTP access URL
    gxs_path,        // Path spec 
    gxs_pop,         // POP URL Scheme
    gxs_pop3,        // A POP3 URL Interface
    gxs_printer,     // Definition of printer: URLs with Service Location
    gxs_prospero,    // Prospero Directory Service URL
    gxs_res,         // Res Protocol
    gxs_rtsp,        // Real Time Streaming Protocol (RTSP)
    gxs_rvp,         // Rendezvous Protocol
    gxs_rlogin,      // Remote login
    gxs_rwhois,      // The RWhois Uniform Resource Locator
    gxs_rx,          // Remote Execution, per Mobile GUI On The Web
    gxs_sdp,         // SDP URL Scheme
    gxs_service,     // Service Templates and service
    gxs_sip,         // SIP URL Scheme
    gxs_shttp,       // Secure http
    gxs_snews,       // NNTP over SSL
    gxs_stanf,       // Stable Network Filenames 
    gxs_telnet,      // Reference to interactive sessions URL RFC 
    gxs_tip,         // Transaction Internet Protocol Version 3.0
    gxs_tn3270,      // Reserved, per Internet Standard list of url schemes 
    gxs_tv,          // Uniform Resource Locators for Television Broadcasts
    gxs_uuid,        // The UUID addressing scheme
    gxs_wais,        // Wide Area Information Servers URL 
    gxs_whois,       // Distributed directory service
    gxs_whodp        // WhoDP: Widely Hosted Object Data Protocol
  };

public:
  gxsURL() { }
  ~gxsURL() { }
  
public: // Functions used to identify URL protocols and port numbers
  char *GetProtocolString(int protocol);
  const char *GetProtocolString(int protocol) const;
  int GetProtocolType(const gxString &protocol);
  int GetPortNumber(const gxString &url, int &port);
  
public: // Functions used to format and test URL strings
  void ProcessFTPType(const gxString &url, char &ftp_type);
  int CleanUserName(const gxString &url, gxString &clean_url);
  int HasProtocol(const gxString &url);
  int HasFile(const gxString &path, gxString &dir, gxString &file);
  int HasFile(const gxString &path);
  
public: // Functions used to parse uniform resource locators
  int ParseURL(const gxString &url, gxsURLInfo &u, int strict = 0);
  int ParseURL(const char *url, gxsURLInfo &u, int strict = 0); 
  int ParseDirectory(gxsURLInfo &u);
  int ParseProtocol(const gxString &url, gxString &proto_name, 
		    int &proto_type);
  int ParseUserName(const gxString &url, gxString &user, gxString &passwd,
		    gxString &clean_url);
  int ParsePortNumber(const gxString &url, int &port);
  int ParseHostName(const gxString &url, gxString &host,
		    int remove_port_number = 1);
  int ParseDirectory(const gxString &url, gxString &path, gxString &dir,
		     gxString &file);

public: // 07/27/2006: Functions added to handle dynamic Web pages
  int ParseDynamicPage(gxsURLInfo &u);
  int ParseDynamicPage(gxsURLInfo &u, 
		       const gxString &my_dynamic_file_types,
		       const gxString &my_dynamic_dir_types);
  int ParseDynamicPage(gxsURLInfo &u, 
		       const gxString &my_dynamic_file_types,
		       const gxString &my_dynamic_dir_types,
		       int &is_dynamic,
		       gxString &query_string,
		       gxString &file_extension,
		       gxString &bin_dir,
		       gxString &local_query_file);
  int ParseDynamicPage(const gxString &url, 
		       int &is_dynamic,
		       gxString &query_string,
		       gxString &file_extension,
		       gxString &bin_dir,
		       gxString &local_query_file);
  int ParseDynamicPage(const gxString &url, 
		       const gxString &my_dynamic_file_types,
		       const gxString &my_dynamic_dir_types,
		       int &is_dynamic,
		       gxString &query_string,
		       gxString &file_extension,
		       gxString &bin_dir,
		       gxString &local_query_file);
  int IsDynamicPage(const gxString &url);
  int IsDynamicPage(const gxString &url, 
		    const gxString &my_dynamic_file_types,
		    const gxString &my_dynamic_dir_types);
  
public: // 11/15/2006: Filter functions
  int FilterQueryCharacters(gxString &local_query_file);

public: // 07/27/2006: Data members added to handle dynamic Web pages
  static gxString dynamic_file_types;
  static gxString dynamic_dir_types;

public: // 07/28/2006: Helper functions
  int SetDefaultProto(gxString &url);
};

#endif // __GX_URL_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
