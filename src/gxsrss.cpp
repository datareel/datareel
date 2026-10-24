// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsrss.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
// Date Last Modified: 10/30/2025
// Copyright (c) 2001-2025 DataReel Software Development
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

// 10/14/2026: Helper added to encode a field for output. The old code
// only encoded the description fields, so a title, link or other field
// containing a &, < or > character made the feed invalid XML.
static gxString gxRSS_encode(gxRSSfeed *feed, const gxString &s)
{
  gxString buf;
  feed->encode_xhtml(s, buf);
  return buf;
}

void gxRSSfeed::Clear()
{
  xml_version = "1.0";
  rss_version = "2.0";
  encoding.Clear();

  // 10/14/2026: Clear() did not clear the channel data or its item
  // list, so a cleared feed still wrote the old channel and items.
  channel.title.Clear();
  channel.link.Clear();
  channel.description.Clear();
  channel.language.Clear();
  channel.copyright.Clear();
  channel.managingeditor.Clear();
  channel.webmaster.Clear();
  channel.pubdate.Clear();
  channel.lastbuilddate.Clear();
  channel.category.Clear();
  channel.generator.Clear();
  channel.docs.Clear();
  channel.cloud.Clear();
  channel.ttl.Clear();
  channel.rating.Clear();
  channel.skiphours.Clear();
  channel.skipdays.Clear();
  channel.items.ClearList();
  channel.image.url.Clear();
  channel.image.link.Clear();
  channel.image.title.Clear();
  channel.image.description.Clear();
  channel.image.width.Clear();
  channel.image.height.Clear();
  channel.textinput.title.Clear();
  channel.textinput.description.Clear();
  channel.textinput.name.Clear();
  channel.textinput.link.Clear();
}

int gxRSSfeed::WriteFeed(gxString &feed) 
{
  // 10/14/2026: Only version 1.x uses the RSS 1.0 (RDF) format. The old
  // test (version < 2) also sent versions 0.91 and 0.92, which use the
  // same <rss> format as version 2.0, to the RDF writer.
  if(rss_version.Atoi() == 1) {
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

  // 10/14/2026: All text fields are now encoded with gxRSS_encode().
  feed << "<?xml version=\"" << gxRSS_encode(this, xml_version) << "\""; 
  if(!encoding.is_null()) feed << " encoding=\"" << gxRSS_encode(this, encoding) << "\"";
  feed << "?>" << "\n";
  feed << "\n";
  feed << "<rss version=\"" << gxRSS_encode(this, rss_version) << "\">" << "\n";

  feed << "  " << "<channel>" << "\n";
  feed << "    " << "<title>" << gxRSS_encode(this, channel.title) << "</title>" << "\n";
  feed << "    " << "<link>" << gxRSS_encode(this, channel.link) << "</link>" << "\n";
  encode_xhtml(channel.description, xhtml_description); 
  feed << "    " << "<description>" << "\n";
  feed << "      " << xhtml_description << "\n"; 
  feed << "    " << "</description>" << "\n";
  
  if(!channel.language.is_null()) {
    feed << "    " << "<language>" << gxRSS_encode(this, channel.language) << "</language>" << "\n";
  }
  if(!channel.copyright.is_null()) {
    feed << "    " << "<copyright>" << gxRSS_encode(this, channel.copyright) << "</copyright>" << "\n"; 
  }
  if(!channel.managingeditor.is_null()) {
    feed << "    " << "<managingEditor>" << gxRSS_encode(this, channel.managingeditor) << "</managingEditor>" << "\n";
  }
  if(!channel.webmaster.is_null()) {
    feed << "    " << "<webMaster>" << gxRSS_encode(this, channel.webmaster) << "</webMaster>" << "\n";
  }
  if(!channel.pubdate.is_null()) {
    feed << "    " << "<pubDate>" << gxRSS_encode(this, channel.pubdate) << "</pubDate>" << "\n";
  }
  if(!channel.lastbuilddate.is_null()) {
    feed << "    " << "<lastBuildDate>" << gxRSS_encode(this, channel.lastbuilddate) << "</lastBuildDate>" << "\n";
  }
  if(!channel.category.is_null()) {
    feed << "    " << "<category>" << gxRSS_encode(this, channel.category) << "</category>" << "\n";
  }
  if(!channel.generator.is_null()) {
    feed << "    " << "<generator>" << gxRSS_encode(this, channel.generator) << "</generator>" << "\n";
  }
  if(!channel.docs.is_null()) {
    feed << "    " << "<docs>" << gxRSS_encode(this, channel.docs) << "</docs>" << "\n";
  }
  if(!channel.cloud.is_null()) {
    // 10/14/2026: The end tag was written as <channel> instead of </cloud>
    // 10/24/2026: Written as an empty element with the cloud settings as
    // attributes, the form given in the RSS 2.0 specification. The text
    // form written before is not valid RSS.
    feed << "    " << "<cloud domain=\"" << gxRSS_encode(this, channel.cloud.domain) << "\"";
    feed << " port=\"" << gxRSS_encode(this, channel.cloud.port) << "\"";
    feed << " path=\"" << gxRSS_encode(this, channel.cloud.path) << "\"";
    feed << " registerProcedure=\"" << gxRSS_encode(this, channel.cloud.registerprocedure) << "\"";
    feed << " protocol=\"" << gxRSS_encode(this, channel.cloud.protocol) << "\" />" << "\n";
  }
  if(!channel.ttl.is_null()) {
    feed << "    " << "<ttl>" << gxRSS_encode(this, channel.ttl) << "</ttl>" << "\n";
  }
  // 10/14/2026: The rating, skipHours and skipDays elements were never 
  // written. skipHours and skipDays hold <hour> and <day> elements, so 
  // their text is written as given.
  if(!channel.rating.is_null()) {
    feed << "    " << "<rating>" << gxRSS_encode(this, channel.rating) << "</rating>" << "\n";
  }
  if(!channel.skiphours.is_null()) {
    feed << "    " << "<skipHours>" << channel.skiphours << "</skipHours>" << "\n";
  }
  if(!channel.skipdays.is_null()) {
    feed << "    " << "<skipDays>" << channel.skipdays << "</skipDays>" << "\n";
  }
 
  feed << "\n";

  // 10/14/2026: The image description is optional. The old code did not
  // write the image unless the description was set.
  if((!channel.image.url.is_null()) && (!channel.image.link.is_null()) &&
     (!channel.image.title.is_null())) {
    feed << "    " << "<image>" << "\n";
    feed << "      " << "<url>" << gxRSS_encode(this, channel.image.url) << "</url>" << "\n";
    feed << "      " << "<link>" << gxRSS_encode(this, channel.image.link) << "</link>" << "\n";
    if(!channel.image.description.is_null()) {
      encode_xhtml(channel.image.description, xhtml_description); 
      feed << "      " << "<description>" << "\n";
      feed << "        " << xhtml_description << "\n";
      feed << "      " << "</description>" << "\n";
    }
    feed << "      " << "<title>" << gxRSS_encode(this, channel.image.title) << "</title>" << "\n";
    if(!channel.image.width.is_null()) {
      feed << "      " << "<width>" << gxRSS_encode(this, channel.image.width) << "</width>" << "\n";
    }
    if(!channel.image.height.is_null()) {
      feed << "      " << "<height>" << gxRSS_encode(this, channel.image.height) << "</height>" << "\n";
    }
    feed << "    " << "</image>" << "\n";
    feed << "\n";
  }

  if((!channel.textinput.title.is_null()) && (!channel.textinput.description.is_null()) &&
     (!channel.textinput.name.is_null()) && (!channel.textinput.link.is_null())) {
    // 10/14/2026: RSS 2.0 names this element <textInput>
    feed << "    " << "<textInput>" << "\n";
    feed << "      " << "<title>" << gxRSS_encode(this, channel.textinput.title) << "</title>" << "\n";
    encode_xhtml(channel.textinput.description, xhtml_description); 
    feed << "      " << "<description>" << "\n";
    feed << "        " << xhtml_description << "\n";
    feed << "      " << "</description>" << "\n";
    feed << "      " << "<name>" << gxRSS_encode(this, channel.textinput.name) << "</name>" << "\n";
    feed << "      " << "<link>" << gxRSS_encode(this, channel.textinput.link) << "</link>" << "\n";
    feed << "    " << "</textInput>" << "\n";
    feed << "\n";
  }

  gxListNode<gxRSSitem> *iptr = channel.items.GetHead();
  while(iptr) {
    feed << "    " << "<item>" << "\n";
    feed << "      " << "<title>" << gxRSS_encode(this, iptr->data.title) << "</title>" << "\n";
    feed << "      " << "<link>" << gxRSS_encode(this, iptr->data.link) << "</link>" << "\n";
    encode_xhtml(iptr->data.description, xhtml_description); 
    feed << "      " << "<description>" << "\n";
    feed << "        " << xhtml_description << "\n";    
    feed << "      " << "</description>" << "\n";
    if(!iptr->data.pubdate.is_null()) {
      feed << "      " << "<pubDate>" << gxRSS_encode(this, iptr->data.pubdate) << "</pubDate>" << "\n";
    }
    if(!iptr->data.guid.is_null()) {
      feed << "      " << "<guid>" << gxRSS_encode(this, iptr->data.guid) << "</guid>" << "\n";
    }
    if(!iptr->data.author.is_null()) {
      feed << "      " << "<author>" << gxRSS_encode(this, iptr->data.author) << "</author>" << "\n";
    }
    if(!iptr->data.category.is_null()) {
      feed << "      " << "<category>" << gxRSS_encode(this, iptr->data.category) << "</category>" << "\n";
    }
    if(!iptr->data.comments.is_null()) {
      feed << "      " << "<comments>" << gxRSS_encode(this, iptr->data.comments) << "</comments>" << "\n";
    }
    if(!iptr->data.enclosure.is_null()) {
      feed << "      " << "<enclosure>" << gxRSS_encode(this, iptr->data.enclosure) << "</enclosure>" << "\n";
    }
    if(!iptr->data.source.is_null()) {
      feed << "      " << "<source>" << gxRSS_encode(this, iptr->data.source) << "</source>" << "\n";
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

  // 10/14/2026: The rdf:about and rdf:resource values were fixed sample
  // addresses from xml.com and the <items> list always held the same two
  // sample entries. They are now taken from the channel link, the image
  // URL, the text input link and the link of each item. All text fields
  // are now encoded with gxRSS_encode().
  int has_image = ((!channel.image.url.is_null()) && 
		   (!channel.image.link.is_null()) &&
		   (!channel.image.title.is_null()));
  int has_textinput = ((!channel.textinput.title.is_null()) && 
		       (!channel.textinput.description.is_null()) &&
		       (!channel.textinput.name.is_null()) && 
		       (!channel.textinput.link.is_null()));
  
  feed << "<?xml version=\"" << gxRSS_encode(this, xml_version) << "\""; 
  if(!encoding.is_null()) feed << " encoding=\"" << gxRSS_encode(this, encoding) << "\"";
  feed << "?>" << "\n";
  
  feed << "\n";

  feed << "<rdf:RDF" << "\n"; 
  feed << " xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"" << "\n";
  feed << " xmlns=\"http://purl.org/rss/1.0/\">" << "\n";

  feed << "\n";

  feed << "<channel rdf:about=\"" << gxRSS_encode(this, channel.link) << "\">" << "\n";
  feed << "  " << "<title>" << gxRSS_encode(this, channel.title) << "</title>" << "\n";
  feed << "  " << "<link>" << gxRSS_encode(this, channel.link) << "</link>" << "\n";
  encode_xhtml(channel.description, xhtml_description); 
  feed << "  " << "<description>" << "\n";
  feed << "    " << xhtml_description << "\n"; 
  feed << "  " << "</description>" << "\n";
  if(has_image) {
    feed << "  " << "<image rdf:resource=\"" << gxRSS_encode(this, channel.image.url) << "\" />" << "\n";
  }
  feed << "\n";
  feed << "  " << "<items>" << "\n";
  feed << "    " << "<rdf:Seq>" << "\n";
  gxListNode<gxRSSitem> *iptr = channel.items.GetHead();
  while(iptr) {
    feed << "    " << "<rdf:li rdf:resource=\"" << gxRSS_encode(this, iptr->data.link) << "\" />" << "\n";
    iptr = iptr->next;
  }
  feed << "    " << "</rdf:Seq>" << "\n";
  feed << "  " << "</items>" << "\n";
  if(has_textinput) {
    feed << "  " << "<textinput rdf:resource=\"" << gxRSS_encode(this, channel.textinput.link) << "\" />" << "\n";
  }
  feed << "  " << "</channel>" << "\n";

  feed << "\n";

  if(has_image) {
    feed << "<image rdf:about=\"" << gxRSS_encode(this, channel.image.url) << "\">" << "\n";
    feed << "  " << "<title>" << gxRSS_encode(this, channel.image.title) << "</title>" << "\n";
    feed << "  " << "<link>" << gxRSS_encode(this, channel.image.link) << "</link>" << "\n";
    feed << "  " << "<url>" << gxRSS_encode(this, channel.image.url) << "</url>" << "\n";
    feed << "</image>" << "\n";
    feed << "\n";
  }

  iptr = channel.items.GetHead();
  while(iptr) {
    feed << "<item rdf:about=\"" << gxRSS_encode(this, iptr->data.link) << "\">" << "\n";
    feed << "  " << "<title>" << gxRSS_encode(this, iptr->data.title) << "</title>" << "\n";
    feed << "  " << "<link>" << gxRSS_encode(this, iptr->data.link) << "</link>" << "\n";
    encode_xhtml(iptr->data.description, xhtml_description); 
    feed << "  " << "<description>" << "\n";
    feed << "    " << xhtml_description << "\n";    
    feed << "  " << "</description>" << "\n";
    feed << "</item>" << "\n";
    feed << "\n";
    iptr = iptr->next;
  }

  if(has_textinput) {
    feed << "<textinput rdf:about=\"" << gxRSS_encode(this, channel.textinput.link) << "\">" << "\n";
    feed << "  " << "<title>" << gxRSS_encode(this, channel.textinput.title) << "</title>" << "\n";
    encode_xhtml(channel.textinput.description, xhtml_description); 
    feed << "  " << "<description>" << "\n";
    feed << "    " << xhtml_description << "\n";
    feed << "  " << "</description>" << "\n";
    feed << "  " << "<name>" << gxRSS_encode(this, channel.textinput.name) << "</name>" << "\n";
    feed << "  " << "<link>" << gxRSS_encode(this, channel.textinput.link) << "</link>" << "\n";
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

