// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsutils.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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

Headers and standalone functions used for low-level network
operations.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_NETWORK_UTILITIES__HPP__
#define __GX_NETWORK_UTILITIES__HPP__

#include "gxdlcode.h"

#include "gxsocket.h"

// --------------------------------------------------------------
// Constants 
// --------------------------------------------------------------
// ICMP Constants
const int gxsECHO_REQ_DATASIZE =  32; // Echo Request Data size
const int gxsICMP_ECHO_REPLY = 0;     // ICMP echo reply
const int gxsICMP_ECHO_REQUEST = 8;   // ICMP echo request
// --------------------------------------------------------------

// --------------------------------------------------------------
// Data structures
// --------------------------------------------------------------
// Ensure network structures are not padded
#if defined (__HPUX__)
#pragma pack 1
#else
#pragma pack(1)
#endif

// IP Header in accordance with RFC 791
// http://www.faqs.org/rfcs/rfc791.html
struct GXDLCODE_API gxsIPHeader
{
  u_char version_and_ihl;     // Version and Internet header length
  u_char type_of_service;     // Type of service
  short total_length;         // Total length
  short sender_id;            // Identification
  short flag_and_frag_offset; // Flags and fragment offset
  u_char time_to_live;        // Time to live
  u_char protocol;            // Protocol
  u_short checksum;           // Checksum
  gxsInternetAddress ia_source;	     // Internet Address - Source
  gxsInternetAddress ia_destination; // Internet Address - Destination
};

// ICMP Header in accordance with RFC 792
// http://www.faqs.org/rfcs/rfc792.html
struct GXDLCODE_API gxsICMPHeader
{
  u_char type;        // Type (8 for echo message / 0 for echo reply message)
  u_char code;        // Code
  u_short checksum;   // Checksum
  u_short identifier; // Identifier
  u_short sequence;   // Sequence
  char data;          // Data
};

// ICMP echo request packet
struct GXDLCODE_API gxsEchoRequest
{
  gxsICMPHeader icmp_header;
  int time_sent;
  char data[gxsECHO_REQ_DATASIZE];
};

// ICMP echo reply packet
struct GXDLCODE_API gxsEchoReply
{
  gxsIPHeader ip_header;
  gxsEchoRequest echo_request;
  char data[gxsECHO_REQ_DATASIZE];
};

// Revert back to the default byte alignment
#if defined (__HPUX__)
#pragma pack
#elif defined (__SOLARIS__)
#pragma pack(4) // The Solaris compiler will only accept 1, 2, or 4
#else
#pragma pack()
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Standalone functions
// --------------------------------------------------------------
GXDLCODE_API u_short InChecksum16(u_short *addr, int len);
GXDLCODE_API int ParseServiceFileEntry(char *entry, char *service, int *port, 
				       char *protocol, char *aliases, char *comment);
// --------------------------------------------------------------

#endif // __GX_NETWORK_UTILITIES__HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
