// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxtelnet.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
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

Embedded telnet classes used with applications that require use of the 
telnet protocol as a keyboard and screen interface to the TCP stack.
This implementation supports NVT (Network Virtual Terminal) terminal 
negotiation. The character set is restricted to 7 bits sent as a byte 
value with the most significant bit cleared. The Interpret as command 
(ICA) escape sequence (0xFF) is sent by either a client or server as 
a prefix to a telnet command.

Changes:
==============================================================
10/29/2002: Added the gxsTelnetClient::RecvInitSeq() function used to 
process the initial connection sequence sent by the telnet server when 
the telnet session is first established. 

10/29/2002: Added the gxsTelnetClient::SendInitSeq() function used to 
send an  initialization sequence to the telnet server if the server 
does not send any initialization commands when the telnet session is 
first established.

09/19/2005: Added the SetServerTypeUNIX() and SetServerTypeWindows() 
functions used to set the gxsTelnetClient::server_type variable. 
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_EMBEDDED_TELNET_LIB_HPP__
#define __GX_EMBEDDED_TELNET_LIB_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"

// Telnet command codes
const unsigned char gxsTEL_IAC   = 255; // Interpret as command
const unsigned char gxsTEL_DONT  = 254; // You are not to use this option
const unsigned char gxsTEL_DO    = 253; // Please, you use this option
const unsigned char gxsTEL_WONT  = 252; // I won't use option 
const unsigned char gxsTEL_WILL  = 251; // I will use option 
const unsigned char gxsTEL_SB    = 250; // Subnegotiate
const unsigned char gxsTEL_GA    = 249; // Go ahead
const unsigned char gxsTEL_EL    = 248; // Erase line          
const unsigned char gxsTEL_EC    = 247; // Erase character
const unsigned char gxsTEL_AYT   = 246; // Are you there
const unsigned char gxsTEL_AO    = 245; // Abort output
const unsigned char gxsTEL_IP    = 244; // Interrupt process
const unsigned char gxsTEL_BRK   = 243; // Break
const unsigned char gxsTEL_DM    = 242; // Data mark
const unsigned char gxsTEL_NOP   = 241; // No operation.
const unsigned char gxsTEL_SE    = 240; // End of subnegotiation
const unsigned char gxsTEL_EOR   = 239; // End of record
const unsigned char gxsTEL_ABORT = 238; // About process
const unsigned char gxsTEL_SUSP  = 237; // Suspend process
const unsigned char gxsTEL_xEOF  = 236; // End of file: EOF already used

// Telnet options 
const unsigned char gxsTELOPT_BIN    = 0;   // Binary transmission
const unsigned char gxsTELOPT_ECHO   = 1;   // Echo
const unsigned char gxsTELOPT_RECN   = 2;   // Reconnection
const unsigned char gxsTELOPT_SUPP   = 3;   // Suppress go ahead
const unsigned char gxsTELOPT_APRX   = 4;   // Approx message size negotiation
const unsigned char gxsTELOPT_STAT   = 5;   // Status
const unsigned char gxsTELOPT_TIM    = 6;   // Timing mark
const unsigned char gxsTELOPT_REM    = 7;   // Remote controlled trans/echo
const unsigned char gxsTELOPT_OLW    = 8;   // Output line width
const unsigned char gxsTELOPT_OPS    = 9;   // Output page size
const unsigned char gxsTELOPT_OCRD   = 10;  // Out carriage-return disposition
const unsigned char gxsTELOPT_OHT    = 11;  // Output horizontal tabstops
const unsigned char gxsTELOPT_OHTD   = 12;  // Out horizontal tab disposition
const unsigned char gxsTELOPT_OFD    = 13;  // Output formfeed disposition
const unsigned char gxsTELOPT_OVT    = 14;  // Output vertical tabstops
const unsigned char gxsTELOPT_OVTD   = 15;  // Output vertical tab disposition
const unsigned char gxsTELOPT_OLD    = 16;  // Output linefeed disposition
const unsigned char gxsTELOPT_EXT    = 17;  // Extended ASCII character set
const unsigned char gxsTELOPT_LOGO   = 18;  // Logout
const unsigned char gxsTELOPT_BYTE   = 19;  // Byte macro
const unsigned char gxsTELOPT_DATA   = 20;  // Data entry terminal
const unsigned char gxsTELOPT_SUP    = 21;  // supdup protocol
const unsigned char gxsTELOPT_SUPO   = 22;  // supdup output
const unsigned char gxsTELOPT_SNDL   = 23;  // Send location
const unsigned char gxsTELOPT_TERM   = 24;  // Terminal type
const unsigned char gxsTELOPT_EOR    = 25;  // End of record
const unsigned char gxsTELOPT_TACACS = 26;  // Tacacs user identification
const unsigned char gxsTELOPT_OM     = 27;  // Output marking
const unsigned char gxsTELOPT_TLN    = 28;  // Terminal location number
const unsigned char gxsTELOPT_3270   = 29;  // Telnet 3270 regime
const unsigned char gxsTELOPT_X3     = 30;  // X.3 PAD
const unsigned char gxsTELOPT_NAWS   = 31;  // Negotiate about window size
const unsigned char gxsTELOPT_TS     = 32;  // Terminal speed
const unsigned char gxsTELOPT_RFC    = 33;  // Remote flow control
const unsigned char gxsTELOPT_LINE   = 34;  // Linemode
const unsigned char gxsTELOPT_XDL    = 35;  // X display location
const unsigned char gxsTELOPT_ENVIR  = 36;  // Telnet environment option
const unsigned char gxsTELOPT_AUTH   = 37;  // Telnet authentication option
const unsigned char gxsTELOPT_NENVIR = 39;  // Telnet environment option
const unsigned char gxsTELOPT_EXTOP  = 255; // Extended-options-list

enum gxsTelnetTermTypes { // Terminal types
  gxsTEL_NVT_TERM,    // NVT - Network Virtual Terminal
  gxsTEL_VT100F_TERM, // Filtered VT100/ANSI terminal emulation
  gxsTEL_ENV_TERM     // Use term type set in environment space
};

enum gxsTelnetState { // Telnet transition state enumeration
  telnet_data,   // Telnet data byte
  telnet_code,   // Telnet code
  telnet_option  // Telnet option
};

// Telnet client class
class GXDLCODE_API gxsTelnetClient : public gxSocket
{
public:
  gxsTelnetClient();
  virtual ~gxsTelnetClient();

public:
  gxSocketError RecvInitSeq();
  gxSocketError SendInitSeq();
  gxSocketError ReadTelnetData(unsigned char data);
  int OutputCharWaiting() { return output_char_waiting == 1; }
  int GetChar(unsigned char &c);
  gxSocketError ConnectClient(const char *host, 
			       int port = gxSOCKET_TELNET_PORT);
  gxSocketError RecvString(char *buf, int bytes, const char *str);
  gxSocketError SendString(const char *buf, int bytes);
  void SetTermType(gxsTelnetTermTypes ttype = gxsTEL_NVT_TERM);
  void SetTermType(const char *s);
  char *GetTermType() { return (char *)term_string; }
  int WaitForReply();
  void SetTimeOut(int seconds, int useconds) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }
  gxSocket *GetSocket() { return (gxSocket*)this; }
  void SetServerTypeUNIX() { server_type = 1; }
  void SetServerTypeWindows() { server_type = 0; }

protected: // Telnet command and option functions
  gxSocketError SendCommand(unsigned char command, unsigned char option);
  gxSocketError SetUnknownOption(unsigned char code, unsigned char option);
  gxSocketError SetEchoOption(unsigned char code, unsigned char option);
  gxSocketError SetSuppOption(unsigned char code, unsigned char option);
  gxSocketError SetTermOption(unsigned char code, unsigned char option);

protected: // Telnet terminal options
  gxSocketError NVTWrite(unsigned char data);
  gxSocketError VT100FWrite(unsigned char data);
  gxSocketError WriteToTerminal(unsigned char data);
  gxSocketError NegotiateTermType(unsigned char data);

protected: // Function pointers
  gxSocketError (gxsTelnetClient::*DataProc)(unsigned char data);

protected:
  unsigned char output;         // Output data following ProcessTelnetData()
  int output_char_waiting;      // True if an output character is waiting
  gxsTelnetTermTypes term_type; // Terminal type used for this connection
  char term_string[16];         // Terminal type name
  gxsTelnetState state;         // Telnet transition state
  unsigned char curr_code;      // Current telnet command code
  gxSocket *gx_socket;          // Socket used for host connections
  int vt_escape_seq;            // True if reading a VT100 escape sequence
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout
  int server_type;   // Added to set the server type
};

#endif // __GX_EMBEDDED_TELNET_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
