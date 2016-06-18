// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsrss.h
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

DataReel RSS classes.

RSS Specifications:
http://en.wikipedia.org/wiki/Resource_Description_Framework
http://en.wikipedia.org/wiki/RSS_(protocol)
http://cyber.law.harvard.edu/rss/rss.html
http://web.resource.org/rss/1.0/spec
http://www.rss-specifications.com/rss-specifications.htm
*/
// ----------------------------------------------------------- //  
#ifndef __GXS_RSS_HPP__
#define __GXS_RSS_HPP__

#include "gxdlcode.h"
#include "gxstring.h"
#include "gxlist.h"

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(disable:4251) // Disable dll-interface template warning
#endif

struct GXDLCODE_API gxRSSimage
{
  // Required elements
  gxString url;   // URL of a GIF, JPEG or PNG image that represents the channel.
  gxString link;  // URL of the site, when the channel is rendered, the image is a link to the site.
  gxString title; // Describes the image, it's used in the ALT attribute of the HTML <img> tag when 
                  // the channel is rendered in HTML. 

  // Optional elements
  gxString description; // Contains text that is included in the TITLE attribute 
                        // of the link formed around the image in the HTML rendering.
  gxString width;       // Maximum value for width is 144, default value is 88. 
  gxString height;      // Maximum value for height is 400, default value is 31.
};

struct GXDLCODE_API gxRSStextinput
{
  gxString title;       // The label of the Submit button in the text input area.
  gxString description; // Explains the text input area.
  gxString name;        // The name of the text object in the text input area.
  gxString link;        // The URL of the CGI script that processes text input requests.
};

struct GXDLCODE_API gxRSSitem
{
  // Required
  gxString title;       // The title of the item
  gxString link;        // The URL of the item.
  gxString description; // The item synopsis.

  // Required in RSS version 2.0
  gxString pubdate; // Indicates when the item was published: Sun, 19 May 2002 15:21:36 GMT
  gxString guid;    // A string that uniquely identifies the item.

  // Optional 
  gxString author;    // Address of the author of the item.
  gxString category;  // Includes the item in one or more categories.
  gxString comments;  // URL of a page for comments relating to the item.
  gxString enclosure; // Describes a media object that is attached to the item. More.
  gxString source;    // The RSS channel that the item came from. More.
};

struct GXDLCODE_API gxRSSchannel 
{
  // Required channel elements
  gxString title;       // The name of the channel.
  gxString link;        // The URL to the HTML website corresponding to the channel
  gxString description; // Phrase or sentence describing the channel.   

  // Optional channel elements 
  gxString language;       // The language the channel is written in.
  gxString copyright;      // Copyright notice for content in the channel.
  gxString managingeditor; // Email address for person responsible for editorial content.
  gxString webmaster;      // Email address for person responsible for technical issues.
  gxString pubdate;        // The publication date for the content in the channel: 
                           // Date and Time Specification of RFC 822
                           // Sat, 07 Sep 2002 00:00:01 GMT
  gxString lastbuilddate;  // The last time the content of the channel changed:
                           // Sat, 07 Sep 2002 09:42:31 GMT
  gxString category;  // Specify one or more categories that the channel belongs to.
  gxString generator; // A string indicating the program used to generate the channel.
  gxString docs;      // A URL that points to the documentation for the format used in the RSS file.
  gxString cloud;     // Allows processes to register with a cloud to be notified of updates to the channel, 
                      // implementing a lightweight publish-subscribe protocol for RSS feeds.
  gxString ttl;       // Time to live is a number of minutes that indicates how long a channel can be 
                      // cached before refreshing from the source.
  gxString rating;    // The PICS rating for the channel.
  gxString skiphours; // A hint for aggregators telling them which hours they can skip.
  gxString skipdays;  // A hint for aggregators telling them which days they can skip

  // Optional channel sub-elements
  gxList<gxRSSitem> items;  // List of channel items 
  gxRSSimage image;         // Specifies a GIF, JPEG or PNG image that can be displayed with the channel.
  gxRSStextinput textinput; // Specifies a text input box that can be displayed with the channel.
};

class GXDLCODE_API gxRSSfeed
{
public:
  gxRSSfeed() { Clear(); }
  ~gxRSSfeed() { }

public:
  int WriteFeed(gxString &feed);
  void Clear();

public: // Internal processing functions
  int write_rss1_feed(gxString &feed);
  int write_rss2_feed(gxString &feed);
  int encode_xhtml(const gxString &unencoded_str, gxString &encoded_str);

public: 
  // RSS required feed elements
  gxRSSchannel channel;

  // Document formatting members
  gxString xml_version;
  gxString rss_version;
  gxString encoding;
};

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(default:4251) // Restore previous warning
#endif

#endif // __GXS_RSS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


