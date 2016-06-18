// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsrss.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

DataReel RSS classes.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxsrss.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

void gxRSSfeed::Clear()
{
  xml_version = "1.0";
  rss_version = "2.0";
  encoding.Clear();
}

int gxRSSfeed::WriteFeed(gxString &feed) 
{
  if(rss_version.Atoi() < 2) {
    return write_rss1_feed(feed);
  }

  return write_rss2_feed(feed);
}

int gxRSSfeed::encode_xhtml(const gxString &unencoded_str, gxString &encoded_str)
{
  encoded_str = unencoded_str;
  encoded_str.ReplaceString("&", "&amp;");
  encoded_str.ReplaceString("<", "&lt;");
  encoded_str.ReplaceString(">", "&gt;");
  encoded_str.ReplaceString("\'", "&apos;");
  encoded_str.ReplaceString("\"", "&quot;");
  return 1;
} 

int gxRSSfeed::write_rss2_feed(gxString &feed)
{
  feed.Clear();
  gxString xhtml_description;

  feed << "<?xml version=\"" << xml_version << "\""; 
  if(!encoding.is_null()) feed << " encoding=\"" << encoding << "\"";
  feed << "?>" << "\n";
  feed << "\n";
  feed << "<rss version=\"" << rss_version << "\">" << "\n";

  feed << "  " << "<channel>" << "\n";
  feed << "    " << "<title>" << channel.title << "</title>" << "\n";
  feed << "    " << "<link>" << channel.link << "</link>" << "\n";
  encode_xhtml(channel.description, xhtml_description); 
  feed << "    " << "<description>" << "\n";
  feed << "      " << xhtml_description << "\n"; 
  feed << "    " << " </description>" << "\n";
  
  if(!channel.language.is_null()) {
    feed << "    " << "<language>" << channel.language << "</language>" << "\n";
  }
  if(!channel.copyright.is_null()) {
    feed << "    " << "<copyright>" << channel.copyright << "</copyright>" << "\n"; 
  }
  if(!channel.managingeditor.is_null()) {
    feed << "    " << "<managingEditor>" << channel.managingeditor << "</managingEditor>" << "\n";
  }
  if(!channel.webmaster.is_null()) {
    feed << "    " << "<webMaster>" << channel.webmaster << "</webMaster>" << "\n";
  }
  if(!channel.pubdate.is_null()) {
    feed << "    " << "<pubDate>" << channel.pubdate << "</pubDate>" << "\n";
  }
  if(!channel.lastbuilddate.is_null()) {
    feed << "    " << "<lastBuildDate>" << channel.lastbuilddate << "</lastBuildDate>" << "\n";
  }
  if(!channel.category.is_null()) {
    feed << "    " << "<category>" << channel.category << "</category>" << "\n";
  }
  if(!channel.generator.is_null()) {
    feed << "    " << "<generator>" << channel.generator << "</generator>" << "\n";
  }
  if(!channel.docs.is_null()) {
    feed << "    " << "<docs>" << channel.docs << "</docs>" << "\n";
  }
  if(!channel.cloud.is_null()) {
    feed << "    " << "<cloud>" << channel.cloud << "<channel>" << "\n";
  }
  if(!channel.ttl.is_null()) {
    feed << "    " << "<ttl>" << channel.ttl << "</ttl>" << "\n";
  }
 
  feed << "\n";

  if((!channel.image.url.is_null()) && (!channel.image.link.is_null()) &&
     (!channel.image.title.is_null()) && (!channel.image.description.is_null())) {
    feed << "    " << "<image>" << "\n";
    feed << "      " << "<url>" << channel.image.url << "</url>" << "\n";
    feed << "      " << "<link>" << channel.image.link << "</link>" << "\n";
    encode_xhtml(channel.image.description, xhtml_description); 
    feed << "      " << "<description>" << "\n";
    feed << "        " << xhtml_description << "\n";
    feed << "      " << "</description>" << "\n";
    feed << "      " << "<title>" << channel.image.title << "</title>" << "\n";
    if(!channel.image.width.is_null()) {
      feed << "      " << "<width>" << channel.image.width << "</width>" << "\n";
    }
    if(!channel.image.height.is_null()) {
      feed << "      " << "<height>" << channel.image.height << "</height>" << "\n";
    }
    feed << "    " << "</image>" << "\n";
    feed << "\n";
  }

  if((!channel.textinput.title.is_null()) && (!channel.textinput.description.is_null()) &&
     (!channel.textinput.name.is_null()) && (!channel.textinput.link.is_null())) {
    feed << "    " << "<textinput>" << "\n";
    feed << "      " << "<title>" << channel.textinput.title << "</title>" << "\n";
    encode_xhtml(channel.textinput.description, xhtml_description); 
    feed << "      " << "<description>" << "\n";
    feed << "        " << xhtml_description << "\n";
    feed << "      " << "</description>" << "\n";
    feed << "      " << "<name>" << channel.textinput.name << "</name>" << "\n";
    feed << "      " << "<link>" << channel.textinput.link << "</link>" << "\n";
    feed << "    " << "</textinput>" << "\n";
    feed << "\n";
  }

  gxListNode<gxRSSitem> *iptr = channel.items.GetHead();
  while(iptr) {
    feed << "    " << "<item>" << "\n";
    feed << "      " << "<title>" << iptr->data.title << "</title>" << "\n";
    feed << "      " << "<link>" << iptr->data.link << "</link>" << "\n";
    encode_xhtml(iptr->data.description, xhtml_description); 
    feed << "      " << "<description>" << "\n";
    feed << "        " << xhtml_description << "\n";    
    feed << "      " << "</description>" << "\n";
    if(!iptr->data.pubdate.is_null()) {
      feed << "      " << "<pubDate>" << iptr->data.pubdate << "</pubDate>" << "\n";
    }
    if(!iptr->data.guid.is_null()) {
      feed << "      " << "<guid>" << iptr->data.guid << "</guid>" << "\n";
    }
    if(!iptr->data.author.is_null()) {
      feed << "      " << "<author>" << iptr->data.author << "</author>" << "\n";
    }
    if(!iptr->data.category.is_null()) {
      feed << "      " << "<category>" << iptr->data.category << "</category>" << "\n";
    }
    if(!iptr->data.comments.is_null()) {
      feed << "      " << "<comments>" << iptr->data.comments << "</comments>" << "\n";
    }
    if(!iptr->data.enclosure.is_null()) {
      feed << "      " << "<enclosure>" << iptr->data.enclosure << "</enclosure>" << "\n";
    }
    if(!iptr->data.source.is_null()) {
      feed << "      " << "<source>" << iptr->data.source << "</source>" << "\n";
    }

    feed << "    " << "</item>" << "\n";
    feed << "\n";
    iptr = iptr->next;
  }

  feed << "  " << "</channel>" << "\n";

  feed << "</rss>" << "\n";
  
  return 1;
}

int gxRSSfeed::write_rss1_feed(gxString &feed)
{
  feed.Clear();
  gxString xhtml_description;
  
  feed << "<?xml version=\"" << xml_version << "\""; 
  if(!encoding.is_null()) feed << " encoding=\"" << encoding << "\"";
  feed << "?>" << "\n";
  
  feed << "\n";

  feed << "<rdf:RDF" << "\n"; 
  feed << " xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"" << "\n";
  feed << " xmlns=\"http://purl.org/rss/1.0/\">" << "\n";

  feed << "\n";

  feed << "<channel rdf:about=\"http://www.xml.com/xml/news.rss\">" << "\n";
  feed << "  " << "<title>" << channel.title << "</title>" << "\n";
  feed << "  " << "<link>" << channel.link << "</link>" << "\n";
  encode_xhtml(channel.description, xhtml_description); 
  feed << "  " << "<description>" << "\n";
  feed << "    " << xhtml_description << "\n"; 
  feed << "  " << " </description>" << "\n";
  feed << "  " << "<image rdf:resource=\"http://xml.com/universal/images/xml_tiny.gif\" />" << "\n";
  feed << "\n";
  feed << "  " << "<items>" << "\n";
  feed << "    " << "<rdf:Seq>" << "\n";
  feed << "    " << "<rdf:li rdf:resource=\"http://xml.com/pub/2000/08/09/xslt/xslt.html\" />" << "\n";
  feed << "    " << "<rdf:li rdf:resource=\"http://xml.com/pub/2000/08/09/rdfdb/index.html\" />" << "\n";
  feed << "    " << "</rdf:Seq>" << "\n";
  feed << "  " << "</items>" << "\n";
  feed << "  " << "<textinput rdf:resource=\"http://search.xml.com\" />" << "\n";
  feed << "  " << "</channel>" << "\n";

  feed << "\n";

  if((!channel.image.url.is_null()) && (!channel.image.link.is_null()) &&
     (!channel.image.title.is_null())) {
    feed << "<image  rdf:about=\"http://xml.com/universal/images/xml_tiny.gif\">" << "\n";
    feed << "  " << "<title>" << channel.image.title << "</title>" << "\n";
    feed << "  " << "<link>" << channel.image.link << "</link>" << "\n";
    feed << "  " << "<url>" << channel.image.url << "</url>" << "\n";
    feed << "</image>" << "\n";
    feed << "\n";
  }

  gxListNode<gxRSSitem> *iptr = channel.items.GetHead();
  while(iptr) {
    feed << "<item rdf:about=\"http://xml.com/pub/2000/08/09/xslt/xslt.html\">" << "\n";
    feed << "  " << "<title>" << iptr->data.title << "</title>" << "\n";
    feed << "  " << "<link>" << iptr->data.link << "</link>" << "\n";
    encode_xhtml(iptr->data.description, xhtml_description); 
    feed << "  " << "<description>" << "\n";
    feed << "    " << xhtml_description << "\n";    
    feed << "  " << "</description>" << "\n";


    feed << "</item>" << "\n";
    feed << "\n";
    iptr = iptr->next;
  }

  if((!channel.textinput.title.is_null()) && (!channel.textinput.description.is_null()) &&
     (!channel.textinput.name.is_null()) && (!channel.textinput.link.is_null())) {
    feed << "<textinput  rdf:about=\"http://search.xml.com\">" << "\n";
    feed << "  " << "<title>" << channel.textinput.title << "</title>" << "\n";
    encode_xhtml(channel.textinput.description, xhtml_description); 
    feed << "  " << "<description>" << "\n";
    feed << "    " << xhtml_description << "\n";
    feed << "  " << "</description>" << "\n";
    feed << "  " << "<name>" << channel.textinput.name << "</name>" << "\n";
    feed << "  " << "<link>" << channel.textinput.link << "</link>" << "\n";
    feed << "</textinput>" << "\n";
    feed << "\n";
  }

  feed << "</rdf:RDF>" << "\n";
  
  return 1;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

