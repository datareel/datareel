// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsutils.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/31/2026
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

Headers and standalone functions used for low-level network
operations.

Changes:
==============================================================
10/31/2026: ParseServiceFileEntry() copied the port/protocol field
into a 255 byte stack buffer with strcpy(), so a long field overflowed
it; the port is now read in place (digits only, 0-65535). 
InChecksum16() summed into a signed int, which overflowed (undefined 
behaviour, wrong checksum) for buffers over about 64 KB; the sum is
unsigned and folded as it grows.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdlib.h>
#include "gxsutils.h"

#ifdef __BCC32__
#pragma warn -8071
#endif

GXDLCODE_API int ParseServiceFileEntry(char *entry, char *service, int *port, 
				       char *protocol, char *aliases, 
				       char *comment)
// Standalone function used to parse a single line of a services file.
// Returns true if no error were encountered or false if the line
// does not contain a valid service, port, and protocol entry.
//
// Services file format :
// <service name>  <port number>/<protocol>  [aliases...]   [#<comment>]
// 
{
  int len, offset, i = 0;
  int byte_count = strlen(entry);

  // Reset all the values
  service[0] = 0;
  *port = 0;
  protocol[0] = 0;
  aliases[0] = 0;
  comment[0] = 0;

  // Trim leading tabs and spaces
  while((entry[i] == ' ') || (entry[i] == '\t')) i++;
  byte_count -= i;
  if(byte_count <= 0) return 0;
  memmove(entry, entry+i, byte_count);
  entry[byte_count] = 0;

  // Filter out comments and blank lines
  switch(entry[0]) {
    case '#': return 0;
    case '\n': return 0;
    case '\r': return 0;
    case '\0': return 0;
    default: break;
  }
  
  // Parse the service name
  for(i = 0; i < byte_count; i ++) {
    if((entry[i] == ' ') || (entry[i] == '\t')) break;
    service[i] = entry[i];
  }
  service[i] = 0; // Null terminate the string

  // Parse the port number and protocol
  while((entry[i] == ' ') || (entry[i] == '\t')) i++;
  byte_count -= i;
  if(byte_count <= 0) return 0;
  memmove(entry, entry+i, byte_count);
  for(i = 0; i < byte_count; i ++) {
    if((entry[i] == ' ') || (entry[i] == '\t')) break;
    protocol[i] = entry[i];
  }
  protocol[i] = 0; 
  if(protocol[0] != 0)  {
    len = strlen(protocol);
    // 10/31/2026: strcpy() of the field into a 255 byte stack buffer
    // overflowed for a long field. The port number is read in place,
    // digits only, and must be 0-65535 (atoi() overflowed).
    for(offset = 0; offset < len; offset++) {
      if(protocol[offset] == '/') break;
    }
    if((offset > 0) && (offset < len)) {
      long p = 0;
      for(int n = 0; n < offset; n++) {
	if(protocol[n] < '0' || protocol[n] > '9' || n >= 5) { p = -1; break; }
	p = (p * 10) + (protocol[n] - '0');
      }
      if(p < 0 || p > 65535) p = 0;
      *port = (int)p;
      offset++;
      memmove(protocol, protocol+offset, (len-offset));
      protocol[len-offset] = 0; // Null terminate the string
    }
  }

  // Parse any aliases on this line
  while((entry[i] == ' ') || (entry[i] == '\t')) i++;
  byte_count -= i;
  if(byte_count <= 0) return 1; // The aliase and comment is optional
  memmove(entry, entry+i, byte_count);
  for(i = 0; i < byte_count; i ++) {
    if((entry[i] == '#') || (entry[i] == '\t')) break;
    aliases[i] = entry[i];
  }
  aliases[i] = 0; // Null terminate the string
  if(aliases[0] != 0) { // Trim all trailing spaces
    offset = len = strlen(aliases);
    while(offset--) if(aliases[offset] != ' ') break;
    if(offset < len) aliases[offset+1] = 0;
  }

  // Parse any comments on this line
  while((entry[i] == ' ') || (entry[i] == '\t') || (entry[i] == '#')) i++;
  byte_count -= i;
  if(byte_count <= 0) return 1; // The comment is optional
  memmove(entry, entry+i, byte_count);

  for(i = 0; i < byte_count; i ++) {
    if((entry[i] == '\r') || (entry[i] == '\n')) break;
    comment[i] = entry[i];
  }
  comment[i] = 0; // Null terminate the string

  entry[0] = 0; // Reset the entry string
  return 1;     // No errors were encountered
}

GXDLCODE_API u_short InChecksum16(u_short *addr, int len)
// Checksum routine for Internet Protocol family headers taken from
// Mike Muuss' in_cksum() function and his comments from the original
// ping program. 
//
// Author: Mike Muuss
// U. S. Army Ballistic Research Laboratory
// December, 1983
{
  int nleft = len; // 10/14/2026: register removed (not valid in C++17)
  u_short *w = addr;
  u_short answer;
  // 10/31/2026: Unsigned and folded as it grows. A signed int sum 
  // overflowed for buffers over about 64 KB.
  unsigned int sum = 0;

  // Our algorithm is simple, using a 32 bit accumulator (sum),
  // we add sequential 16 bit words to it, and at the end, fold
  // back all the carry bits from the top 16 bits into the lower
  // 16 bits.
  while(nleft > 1)  {
    sum += *w++;
    nleft -= 2;
    if(sum & 0x80000000U) sum = (sum & 0xffff) + (sum >> 16);
  }

  // mop up an odd byte, if necessary
  if(nleft == 1) {
    u_short	u = 0;
    
    *(u_char *)(&u) = *(u_char *)w ;
    sum += u;
  }
  
  // add back carry outs from top 16 bits to low 16 bits
  sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
  sum += (sum >> 16);                 // add carry

  // Ignore BCC32 conversion warning
  // PC-lint 09/14/2005: Ingnore unsigned type warning 
  answer = (u_short)~sum;             // truncate to 16 bits

  return (answer);
}

#ifdef __BCC32__
#pragma warn .8071
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
