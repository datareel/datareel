// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxtelnet.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
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

Embedded telnet classes used with applications that require use of the 
telnet protocol as a keyboard and screen interface to the TCP stack.
This implementation supports NVT (Network Virtual Terminal) terminal 
negotiation. The character set is restricted to 7 bits sent as a byte 
value with the most significant bit cleared. The Interpret as command 
(ICA) escape sequence (0xFF) is sent by either a client or server as 
a prefix to a telnet command.

Changes:
==============================================================
06/02/2002: Modified the gxsTelnetClient::SetTermOption() to 
return the correct error condition in the err variable.

09/19/2005: Modified the RecvInitSeq() and  RecvString() functions 
to signal that the socket has been gracefully closed if the RawRead() 
call returns 0, indicating that the remote end of the socket has been 
closed.

09/19/2005: Modified the SendString() function to check the server 
type before returning an end of line sequence.  
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdlib.h>
#include "gxtelnet.h"

#ifdef __BCC32__
#pragma warn -8057
#pragma warn -8080
#endif

gxsTelnetClient::gxsTelnetClient() 
{ 
  time_out_sec = 5; // Default blocking timeout value   
  time_out_usec = 0; 
  vt_escape_seq = 0;
  output = 0;
  output_char_waiting = 0;
  state = telnet_data; 
  SetTermType(gxsTEL_NVT_TERM); // Default terminal type
  DataProc = &gxsTelnetClient::WriteToTerminal;
  server_type = 1; // Default to UNIX
}
  
gxsTelnetClient::~gxsTelnetClient()
{

}

void gxsTelnetClient::SetTermType(const char *s)
// Function used to set a custom terminal type.
{
  if(!s) { // Prevent program crash if "s" is a null pointer
    strcpy(term_string, "NVT");
  }
  else {
    strcpy(term_string, s);
  }
  term_type = gxsTEL_NVT_TERM;
}

void gxsTelnetClient::SetTermType(gxsTelnetTermTypes ttype)
// Set the terminal type for this client session.
{ 
  switch(ttype) {
    case gxsTEL_VT100F_TERM : // Filtered VT100/ANSI terminal emulation
      strcpy(term_string, "VT100");
      term_type = gxsTEL_VT100F_TERM;
      return;
    case gxsTEL_ENV_TERM  :   // Use term type set in environment 
      // PC-lint 09/14/2005: getenv is considered dangerous
      if(getenv("TERM")) { // The TERM variable is set
	strcpy(term_string, getenv("TERM"));
	term_type = gxsTEL_ENV_TERM;
	return;
      }
      break;
    default:
      break;
  }

  // Default to NVT - Network Virtual Terminal
  strcpy(term_string, "NVT");
  term_type = gxsTEL_NVT_TERM;
}

gxSocketError gxsTelnetClient::ConnectClient(const char *host, int port)
// Function used to connect a telnet client to a server. Returns zero if no
// errors occur.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, port, (char *)host) < 0) return socket_error;
  }
  else {
    return socket_error;
  }

  if(Connect() < 0) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsTelnetClient::WaitForReply()
// Returns false if a reply time is longer then the timeout values. 
{
  return ReadSelect(gxsocket, time_out_sec, time_out_usec);
}

gxSocketError gxsTelnetClient::SendString(const char *buf, int bytes)
{
  if(Send(buf, bytes) < 0) return socket_error = gxSOCKET_TRANSMIT_ERROR;

  if(server_type == 1) { // UNIX Server
    if(Send("\n", 1) < 0) return socket_error = gxSOCKET_TRANSMIT_ERROR;
  }
  else { // Windows Server
    if(Send("\r\n", 1) < 0) return socket_error = gxSOCKET_TRANSMIT_ERROR;
  }

  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsTelnetClient::RecvString(char *buf, int bytes, 
					  const char *str)
// Blocking receive function used to read a string from a telnet server
// following a connect or send. If the specified string is not received within
// the timeout period this function will return a non-zero value to indicate 
// an error. Returns zero if no errors occur.
{
  bytes_read = 0;             // Reset the byte counter
  const int rxBufSize = 4096; // Receive buffer size
  char rx_buffer[rxBufSize];  // Receive buffer
  unsigned char filtered_output; // NVT or VT100F filtered output
  int byte_count = 0;   // Number of byte inserted in the buf variable

  while(byte_count < bytes) { // Loop until the buffer is full
    if(!WaitForReply()) { 
      if(byte_count >= 0) buf[byte_count] = 0;
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;      
    }
    if((bytes_read = RawRead((char *)rx_buffer, rxBufSize)) > -1) {
      if(bytes_read == 0) {
	// Signal that the socket has been gracefully closed
	return socket_error = gxSOCKET_DISCONNECTED_ERROR;
      }
      char *raw_data = rx_buffer;
      while(bytes_read--) {
	if(ReadTelnetData(*raw_data++) != gxSOCKET_NO_ERROR) {
	  if(byte_count >= 0) buf[byte_count] = 0;
	  return socket_error;
	}
	if(GetChar(filtered_output)) { 
	  // Copy the NVT characters into the string buffer 
	  buf[byte_count++] = filtered_output;
	}
      }

      // Search for a matching string
      char *pattern = (char *)str;
      char *next = buf;
      int i = 0;
      while(i < byte_count && *pattern) {
	if(*next == *pattern) {
	  pattern++;
	  if(*pattern == 0) {
	    if(byte_count >= 0) buf[byte_count] = 0;
	    return socket_error = gxSOCKET_NO_ERROR; // Found matching string
	  }
	  next++;
	}
	else {
	  i++;
	  next++;
	  pattern = (char *)str;
	}
      } 
    }
    else { // An error occurred during the read
      if(byte_count >= 0) buf[byte_count] = 0;
      return socket_error = gxSOCKET_RECEIVE_ERROR;
    }
  }

  // The receieve buffer is full - buffer overflow
  if(byte_count >= 0) buf[byte_count] = 0;
  return socket_error = gxSOCKET_BUFOVER_ERROR;
}

gxSocketError gxsTelnetClient::RecvInitSeq()
// Function used to process the initial connection sequence sent by 
// the telnet server when the telnet session is first established. 
// Returns a non-zero value to indicate an error. Returns zero if no 
// errors occur.
{
  const int rxBufSize = 4096; // Receive buffer size
  char rx_buffer[rxBufSize];  // Receive buffer

  if(!WaitForReply()) return socket_error = gxSOCKET_REQUEST_TIMEOUT;
  bytes_read = RawRead((char *)rx_buffer, rxBufSize);
  if(bytes_read > -1) {
    if(bytes_read == 0) {
      // Signal that the socket has been gracefully closed
      return socket_error = gxSOCKET_DISCONNECTED_ERROR;
    }
    char *raw_data = rx_buffer;
    while(bytes_read--) {
      if(ReadTelnetData(*raw_data++) != gxSOCKET_NO_ERROR) {
	return socket_error;
      }
    }
  }
  else { // An error occurred during the read
    return socket_error = gxSOCKET_RECEIVE_ERROR;
  }
  return socket_error;
}

gxSocketError gxsTelnetClient::SendInitSeq()
// Function used to send an initialization sequence to the telnet server 
// if the server does not send any initialization commands when the telnet 
// session is first established. Returns a non-zero value to indicate an 
// error. Returns zero if no errors occur.
{
  // Set the init sequence
  // 255 251 024 255 253 003 255 251
  // 003 255 253 001 255 251 031 255
  // PC-lint 09/14/2005: Changed to unsigned char 
  unsigned char iseq[16];
  iseq[0] = gxsTEL_IAC;
  iseq[1] = gxsTEL_WILL;
  iseq[2] = gxsTELOPT_TERM;
  iseq[3] = gxsTEL_IAC;
  iseq[4] = gxsTEL_DO;
  iseq[5] = gxsTELOPT_SUPP;
  iseq[6] = gxsTEL_IAC;
  iseq[7] = gxsTEL_WILL;
  iseq[8] = gxsTELOPT_SUPP;
  iseq[9] = gxsTEL_IAC;
  iseq[10] = gxsTEL_DO;
  iseq[11] = gxsTELOPT_ECHO;
  iseq[12] = gxsTEL_IAC;
  iseq[13] = gxsTEL_WILL;
  iseq[14] = gxsTELOPT_NAWS;
  iseq[15] = gxsTEL_IAC;
  Send((const unsigned char *)iseq, 16);
  return socket_error;
}

gxSocketError gxsTelnetClient::SendCommand(unsigned char command, 
					   unsigned char option)
{
  unsigned char sbuf[3];
  sbuf[0] = gxsTEL_IAC;
  sbuf[1] = command;
  sbuf[2] = option;
  Send((char*)sbuf, 3);
  return socket_error;
}
 
int gxsTelnetClient::GetChar(unsigned char &c) 
// Public member function used to extract a single character 
// from the telnet input stream if a character is waiting to be
// read.  Passes back the character in the "c" variable. Returns
// true if a character is waiting to be read or false if no 
// characters are waiting.
{
  if(output_char_waiting == 1) c = output;
  return output_char_waiting == 1;
}

gxSocketError gxsTelnetClient::NVTWrite(unsigned char data)
// Private network virtual terminal write function used to write 
// a single byte. Returns zero if no errors occur.
{
  output_char_waiting = 0;
  output = 0;

  gxSocketError err = gxSOCKET_NO_ERROR;
  switch(data) {
    case 0 : // No operation
      break;
    default:
    output_char_waiting = 1;
    output = data;
    break;
  }
  return err;
}

gxSocketError gxsTelnetClient::WriteToTerminal(unsigned char data)
{
  output_char_waiting = 0;
  output = 0;

  gxSocketError err = gxSOCKET_NO_ERROR;
  switch(term_type) {
    case gxsTEL_NVT_TERM : case gxsTEL_ENV_TERM :
      NVTWrite(data);
      break;
	
    case gxsTEL_VT100F_TERM :
      VT100FWrite(data);
      break;
      
    default:
      NVTWrite(data);
      break;
  }
  return err;
}

gxSocketError gxsTelnetClient::SetUnknownOption(unsigned char code, 
						 unsigned char option)
{
  gxSocketError err = gxSOCKET_NO_ERROR;
  switch(code) {
    case gxsTEL_WILL: // Server wants to support an unknown option
      err = SendCommand(gxsTEL_DONT, option);
      break;
    case gxsTEL_WONT: // Server wants to disable support
      break;
    case gxsTEL_DO:   // Server wants to support something
      err = SendCommand(gxsTEL_WONT, option);
      break;
    case gxsTEL_DONT: // Server wants to disable something
      err = SendCommand(gxsTEL_WONT, option);
      break;      
    default:
      break;
  }
  return err;
}

gxSocketError gxsTelnetClient::SetEchoOption(unsigned char code, 
					      unsigned char option)
{
  gxSocketError err = gxSOCKET_NO_ERROR;
  switch(code) {
    case gxsTEL_WILL: // Server wants to echo data
      err = SendCommand(gxsTEL_DO, option);
      break;
    case gxsTEL_WONT: // Server does not want to echo
      err = SendCommand(gxsTEL_WILL, option);
      break;
    case gxsTEL_DO:   // Server is requesting a loopback
      err = SendCommand(gxsTEL_WONT, option);
      // PC-lint 09/14/2005: Missing break statement
      break;
    case gxsTEL_DONT: // Server does not want me to echo
      err = SendCommand(gxsTEL_WONT, option);
      break;
    default:
      err = SendCommand(gxsTEL_DONT, option);
      break;
  }
  return err;
}

gxSocketError gxsTelnetClient::SetSuppOption(unsigned char code, 
					     unsigned char option)
// Suppress go ahead option.
{
  gxSocketError err = gxSOCKET_NO_ERROR;
  switch(code) {
    case gxsTEL_WILL: // Server wants to suppress GA's
      err = SendCommand(gxsTEL_DO, gxsTELOPT_SUPP);
      err = SendCommand(gxsTEL_WILL, gxsTELOPT_SUPP);
      err = SendCommand(gxsTEL_DO, gxsTELOPT_ECHO);
      break;
    case gxsTEL_WONT: // Server wants to send GA's 
      err = SendCommand(gxsTEL_DONT, gxsTELOPT_SUPP);
      err = SendCommand(gxsTEL_WONT, gxsTELOPT_SUPP);
      break;
    case gxsTEL_DO:   // Server wants me to suppress GA's
      err = SendCommand(gxsTEL_DO, gxsTELOPT_SUPP);
      break;
    case gxsTEL_DONT: // Server wants me to send GA's
      err = SendCommand(gxsTEL_DONT, gxsTELOPT_SUPP);
      break;
    default:
      break;
  }
  return err;
  // Ignore BCC32 warning 8057 option is never used
}

gxSocketError gxsTelnetClient::SetTermOption(unsigned char code, 
					     unsigned char option)
// Sub-negotiate terminal type
{
  gxSocketError err = gxSOCKET_NO_ERROR;
  switch(code) {
    case gxsTEL_WILL:
      err = SendCommand(gxsTEL_DONT, option); // Do not want terminal info
    case gxsTEL_WONT:
      break;
    case gxsTEL_DO:
      err = SendCommand(gxsTEL_WILL, option); // Send term info when asked
    case gxsTEL_DONT:
      break;
    default:
      break;
  }

  return err;
}

gxSocketError gxsTelnetClient::NegotiateTermType(unsigned char data)
// Private member function used to negotiate the telnet terminal type. 
// Returns zero if no errors occur.
{
  output_char_waiting = 0;
  output = 0;

  gxSocketError err = gxSOCKET_NO_ERROR;
  if(data == 1) { // Server has requested our terminal type
    // PC-lint 09/14/2005: Changed to unsigned char 
    unsigned char sbuf[32];
    sbuf[0] = gxsTEL_IAC;
    sbuf[1] = gxsTEL_SB;
    sbuf[2] = gxsTELOPT_TERM;
    sbuf[3] = 0; // Our terminal type is
    strcpy((char *)&sbuf[4], term_string);
    int len = strlen((char *)&sbuf[4]);
    sbuf[4+len] = gxsTEL_IAC;
    sbuf[5+len] = gxsTEL_SE;
    Send(sbuf, (4+len+2));
    err = socket_error;
  }
  return err;
}

gxSocketError gxsTelnetClient::ReadTelnetData(unsigned char data)
{
  output_char_waiting = 0;
  output = 0;

  gxSocketError err = gxSOCKET_NO_ERROR;
  switch(state) {
    case telnet_data: // Process telnet state data
      switch(data) {
	case gxsTEL_IAC: 
	  state = telnet_code; 
	  break;
	default: 
	  return (this->*DataProc)(data);
      }
      break;
    case telnet_code: // Process telnet state codes
      state = telnet_data;
      switch(data) {
	case gxsTEL_IAC:
	  err = (this->*DataProc)(data);
	  break;
	case gxsTEL_SE:
	  DataProc = &gxsTelnetClient::WriteToTerminal;	  
	  break;
	case gxsTEL_NOP:
	  break;
	case gxsTEL_DM:
	  break;
	case gxsTEL_BRK:
	  break;
	case gxsTEL_IP:
	  break;
	case gxsTEL_AO:
	  break;
	case gxsTEL_AYT:
	  break;
	case gxsTEL_EC:
	  break;
	case gxsTEL_EL:
	  break;
	case gxsTEL_GA:
	  break;
	case gxsTEL_SB:
	  curr_code = gxsTEL_SB;
	  state = telnet_option;
	  break;
	case gxsTEL_WILL:
	  curr_code = gxsTEL_WILL;
	  state = telnet_option;
	  break;
	case gxsTEL_WONT:
	  curr_code = gxsTEL_WONT;
	  state = telnet_option;
	  break;
	case gxsTEL_DO:
	  curr_code = gxsTEL_DO;
	  state = telnet_option;
	  break;
	case gxsTEL_DONT:
	  curr_code = gxsTEL_DONT;
	  state = telnet_option;
	  break;
	default:
	  break;
      }
      break;
    case telnet_option: // Process telnet state options
      state = telnet_data;
      if(curr_code == gxsTEL_SB) {
	DataProc = &gxsTelnetClient::NegotiateTermType;
	break;
      }
      switch(data) {
	case gxsTELOPT_ECHO:
	  err = SetEchoOption(curr_code, data);
	  break;
	case gxsTELOPT_SUPP:
	  err = SetSuppOption(curr_code, data);
	  break;
	case gxsTELOPT_TERM:
	  err = SetTermOption(curr_code, data);
	  break;
	default:
	  err = SetUnknownOption(curr_code, data);
	  break;
      }
      break;
  }
  return err;
}

gxSocketError gxsTelnetClient::VT100FWrite(unsigned char data)
// Filtered VT100/ANSI terminal write function. NOTE: This 
// implementation will only filter-out VT100 and ANSI escape 
// sequences. This terminal type should only be used by embedded 
// telnet clients that do not support VT100/ANSI emulations but 
// must talk to servers that will not support other emulations. 
{
  output_char_waiting = 0;
  output = 0;

  gxSocketError err = gxSOCKET_NO_ERROR;

  if(vt_escape_seq == 1) {
    switch(data) {
      case 'm': case 'H' : case 'K' : case 'J' : case 'A' :
	break;
      default: // Reading VT100 escape sequence
	return gxSOCKET_NO_ERROR;
    }
  }

  switch(data) {
    case 0 :  // No operation  
      break;
    case 27 : // VT100/ANSI escape sequence
      vt_escape_seq = 1;
      break;
    case 'm': case 'H' : case 'K' : case 'J' : case 'A' :
      if(vt_escape_seq == 1) { // End of VT100 escape sequence
	vt_escape_seq = 0; 
	// m - VT100/ANSI Set Screen Attribute
	// H - VT100/ANSI Set Position
	// K - VT100/ANSI Erase Line
	// J - VT100/ANSI Erase Screen
	// A - VT100/ANSI Move Up
      }
      else {
	output = data;
	output_char_waiting = 1;
      }
      break;
    default: 
      output = data;
      output_char_waiting = 1;
      break;
  }
  return err;
}

#ifdef __BCC32__
#pragma warn .8057
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
