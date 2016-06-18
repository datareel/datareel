// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxshttp.cpp
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

HTTP client class.

Changes:
==============================================================
09/07/2001: Fixed the gxsHTTPClient::RequestHeader() function
to remove all characters after the end of the header marker.

09/27/2001: Added new gxsHTTPClient::Request() functions to 
replace the slower gxsHTTPClient::RequestFile() functions. The 
gxsHTTPClient::RequestFile() functions have been depreciated 
in version 4.11 and retained for backward compatibility.

01/24/2002: Added (const char *) and (unsigned) type casts
to all FindMatch() function calls to correct type errors
encountered during Linux library builds

04/05/2002: Removed the gxDEVICE_CONSOLE case from the 
gxsHTTPClient::Write() function. Writing to the console is
only used for test purposes.

04/08/2002: Changed the gxsHTTPClient() constructor to use
the version number set in the gxs_ver.h header file to set 
the HTTP client version number.

05/05/2002: Added the __GUI_THREAD_SAFE__ preprocessor directive 
used to conditionally omit functions calls that are not safe to
use within multithreaded GUI applications.

10/03/2002: Modified the gxsHTTPClient::RequestHeader() function
to reset the HTTP header before parsing it.

10/03/2002: Modified the gxsHTTPClient::SimpleRequest(), 
gxsHTTPClient::RequestHeader(), and gxsHTTPClient::Request() functions 
to use timeouts and external loop control.

12/19/2005: Added SSL class and functions used to support HTTPS
connections.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "gxshttp.h"
#include "gxshttpc.h"
#include "gxs_b64.h"
#include "strutil.h"
#include "gxs_ver.h"

#if defined (__wxWIN201__) || (__wxWIN291__)
#include "wx2incs.h"  // Include files for wxWindows version 2.X
#endif

#if defined (__wxWIN302__)
#include "wx3incs.h"  // Include files for wxWindows version 3.X
#endif

#ifdef __BCC32__
#pragma warn -8057
#pragma warn -8080
#endif

gxsHTTPClient::gxsHTTPClient(int CacheSize, int seconds, int useconds) 
  : cache(CacheSize) 
{
  HTTP_VERSION_STRING = "HTTP/1.0\r\n";
  HTTP_AGENT_STRING = "User-Agent: gxsHTTPLibrary/";
  HTTP_AGENT_STRING << gxSocketVersion;
  HTTP_AGENT_STRING << "\r\n";
  HTTP_ACCEPT_STRING = "Accept: */*\r\n";
  HTTP_PRAGMA_STRING = "Pragma: no-cache\r\n";
  HTTP_DEFAULT_FILE_REQUEST_STRING = " /";

  // Default device settings
  ready_for_writing = 1; 
  ready_for_reading = 1;
  cache.Connect(this); 
  write_error = read_error = 0;
  recv_loop = 1;
  time_out_sec = seconds;
  time_out_usec = useconds;

  record_proto = 0; // Do not record session headers by default
}

gxsHTTPClient::~gxsHTTPClient() 
{ 
  // PC-lint 09/12/2005: Ignore file_ptr memory leak warnings because
  // the function caller deletes the file_ptr variable. 
}

char *gxsHTTPClient::GetMethod(int method)
{
  return (char *)gxsHTTPMethodMessage(method);
}

int gxsHTTPClient::ConnectClient(const char *host, int port)
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
 
int gxsHTTPClient::SimpleRequest(const gxsURLInfo &u, gxdFPTR *stream)
// Request a document with no header information. Returns 0 if no
// errors occur.
{
  file_ptr = stream;
  write_error = read_error = 0;
  bytes_received = 0;

  if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;

  // Request the specified file from the server
  gxString request;
  BuildSimpleRequestString(u, gxsHTTP_GET, request);
  if(Send(request.c_str(), request.length()) < 0) return socket_error;
    
  gxDeviceTypes o_device = gxDEVICE_DISK_FILE;     // Output device
  gxDeviceTypes i_device = gxDEVICE_MEMORY_BUFFER; // Input device
  
  int nRet = 0;
  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    // Setup a pointer to the cache buckets
    gxDeviceCachePtr p(cache, o_device, i_device); 
    
    nRet = RawRead(rxBuffer, __HTTP_PACKET_SIZE__);
    if(nRet < 0) return socket_error;
    
    bytes_received += nRet;

    // Exit if the server closed the connection
    if(nRet == 0) break;

    // Load the cache buckets
    p.Alloc(nRet);
  }

  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsHTTPClient::RequestHeader(const gxsURLInfo &u, gxsHTTPHeader &hdr)
// Request a document header passing back the header information in the
// "hdr" variable. Returns 0 if no errors occur.
{
  if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;

  gxString request;
  BuildRequestString(u, gxsHTTP_HEAD, request);
  
  gxString header;
  int nRet = 0;

  // Request a header for the specified file
  if(Send(request.c_str(), request.length()) < 0) return socket_error;
  
  while(recv_loop) { // Read the file header into the header string
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
    
    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    nRet = RawRead(rxBuffer, __HTTP_PACKET_SIZE__);
    if(nRet < 0) return socket_error;

    rxBuffer[nRet] = '\0'; // Null terminate the receive buffer
    header += rxBuffer;

    // Exit if the server closed the connection
    if(nRet == 0) break;

  }

  // 09/07/2001: Remove everything after the end of the header marker.
  // Some Web server return the entire document when the header is
  // requested.
  header.DeleteAfter("\r\n\r\n");


  // 10/03/2002: Reset the header before parsing it. This change was
  // made for functions that reuse the header multiple times. In the 
  // event that the site does not return a header the status members
  // will be reset rather than passing back the previous values.
  hdr.Reset();
  ParseHTTPHeader(u, header, hdr);
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsHTTPClient::Request(const gxsURLInfo &u, gxsHTTPHeader &hdr,
			   gxdFPTR *stream, gxsHTTPHeader *compare_hdr,
			   HTTPClientDisplayFunc display)
// Request a file and write it to the specified stream. Returns 0 
// if no errors occur. This function will download and parse the
// document's header and pass back the header information in the 
// "hdr" variable. The caller must test the header's status code
// to ensure that this request was successfully processed or if
// further action must be taken in order to complete the request.
// If the compare_hdr pointer is a non-null value the document
// header's modification time field will be compared to the compare
// header's modification time fields. The document will only be
// downloaded if the document's modification time has changed
// since the last download.
{
  gxString sbuf;
  int print_num_bytes = 1;
  double elapsed_time = (double)0;
  FAU_t total_bytes = (FAU_t)0;
  FAU_t curr_count = (FAU_t)0;
  FAU_t curr_percent = (FAU_t)0;
  FAU_t byps = (FAU_t)0;
  FAU_t bps = (FAU_t)0;
  const int report_percent = 10;
  time_t begin = (time_t)0;
  time_t end = (time_t)0;
  double r_percent, c_percent;
  file_ptr = stream;
  write_error = read_error = 0;
  bytes_received = 0;

  if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;

  // Request the specified file from the server
  gxString request;
  BuildRequestString(u, gxsHTTP_GET, request);
  if(Send(request.c_str(), request.length()) < 0) return socket_error;
  
  gxDeviceTypes o_device = gxDEVICE_DISK_FILE;    // Output device
  gxDeviceTypes i_device = gxDEVICE_MEMORY_BUFFER; // Input device
  
  int nRet = 0;
  int byte_count;
  int found_header = 0;
  gxString header;

  if(display) {
    // Get the current time before entering loop
    // PC-lint 09/12/2005: Function is considered dangerous
    time(&begin);
  }

  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    // Setup a pointer to the cache buckets
    gxDeviceCachePtr p(cache, o_device, i_device); 

    nRet = RawRead(rxBuffer, __HTTP_PACKET_SIZE__);

    curr_count++;
    total_bytes += nRet;
    
    // A socket error occurred
    if(nRet < 0) return socket_error;

    // Exit if the server closed the connection
    if(nRet == 0) break;

    if(!found_header) {
      header.Cat(rxBuffer, nRet);
      if((header.Find("\r\n\r\n") != -1) || (header.Find("\n\n") != -1)) {

	found_header = 1; // Found the end of header marker
	
	// Remove everything after the end of header marker
	if(!header.DeleteAfter("\r\n\r\n")) {
	  header.DeleteAfter("\n\n");
	}

	// Parse the HTTP header
	ParseHTTPHeader(u, header, hdr); 
	if((hdr.http_status < 200) || (hdr.http_status > 299)) {
	  // There is a problem with the document so do not attempt a download
	  return socket_error = gxSOCKET_HTTPDOC_ERROR; 
	}

	if(compare_hdr) {
	  Close();
	  if(hdr.http_last_modified == compare_hdr->http_last_modified) {
	    // The current document is the same as the on the server
	    return socket_error = gxSOCKET_NO_ERROR; 
	  }
	  else {
	    // Signal to the caller that this file has to be 
	    // overwritten and requested again.
	    return socket_error = gxSOCKET_FILEMODTIME_ERROR;
	  }
	}

	// Move the document body over the header information
	unsigned CRLF_len = strlen("\r\n\r\n");
	int offset = FindMatch((const char *)rxBuffer, "\r\n\r\n", 
			       (unsigned)0, (unsigned)nRet);
	if(offset == -1) {
	  offset = FindMatch((const char *)rxBuffer, "\n\n", 
			     (unsigned)0, (unsigned)nRet);
	  CRLF_len = strlen("\n\n");
	}
	offset += CRLF_len;
	byte_count = nRet - offset;
	bytes_received += byte_count;
	char *s_ptr = (char *)rxBuffer;
	memmove(s_ptr, s_ptr+offset, byte_count); 
      }
      else { // Part of the header was received
	continue; // Continue until the entire header is received
      }
    }
    else { // Found the header 

      byte_count = nRet;
      bytes_received += nRet;
      if(display) {
	if(hdr.length > 0) {
	  if(print_num_bytes) {
	    sbuf << clear << "Downloading " << shn << hdr.length << " bytes";
	    print_num_bytes = 0;
	    // PC-lint 09/12/2005: Ignore type warning for function pointer
	    display(sbuf);
	  }
	  if(curr_count > 1) {
	    if((curr_count * 1024) >= 
	       (hdr.length/report_percent)) {
	      // PC-lint 09/12/2005: Possible loss of fraction, all int valuse assigned to double
	      r_percent = (curr_percent++ * report_percent) / 100;
	      c_percent = hdr.length * r_percent;
	      if(c_percent >= (hdr.length - (total_bytes-header.length()))) {
		curr_count = 0;
		if((FAU_t)hdr.length != FAU_t(total_bytes-header.length())) {
		  // Display progress so far if we are not at 100%
		  sbuf << clear << "Downloaded " << shn 
		       << (total_bytes-header.length()) << " bytes of "
		       << hdr.length;
		  // PC-lint 09/12/2005: Ignore type warning for function pointer
		  display(sbuf);
		}
	      }
	    }
	  }
	}
	else { // The server did not return a document length
	  if(print_num_bytes) {
	    sbuf << clear << "The server did not return a document length.";
	    // PC-lint 09/12/2005: Ignore type warning for function pointer
	    display(sbuf); 
	    print_num_bytes = 0;
	  }
	}
      }
    }
    // Load the cache buckets
    p.Alloc(byte_count);
  }

  if(display) {
    // Get the current time after loop is completed 
    // PC-lint 09/12/2005: Function is considered dangerous
    time(&end); 
    
    // Calculate the elapsed time in seconds. 
    // PC-lint 09/12/2005: Function is considered dangerous
    elapsed_time = difftime(end, begin);

    sbuf.Precision(2); 
    sbuf << clear << "Downloaded " << shn << (total_bytes - header.length())
	 << " bytes in ";
    if(elapsed_time != (double)0) {
      if(elapsed_time <= (double)60) {
	sbuf << elapsed_time << " seconds" << "\n";
      }
      else {
	sbuf << (elapsed_time/60) << " minutes" << "\n";
      }
    }
    else {
      sbuf << "less than 1 second" << "\n";
    }
    // Calculate the number of bits per second
    if(elapsed_time != (double)0) {
      byps = (FAU_t)(total_bytes/elapsed_time);
      bps = byps * 8;
      sbuf << "Download bytes per second rate = " << byps  << " BytesPS";
      sbuf << "\n" << "LAN/WAN connection speed = " << bps << " BPS";
    }
    else {
      sbuf << "Download bytes per second rate = 100%";
      sbuf << "\n" << "LAN/WAN connection speed = 100%";
    }
    // PC-lint 09/12/2005: Ignore type warning for function pointer
    display(sbuf);
  }
  
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsHTTPClient::Request(const gxsURLInfo &u, gxsHTTPHeader &hdr,
			   MemoryBuffer &mbuf)
// Request a file and write it to the specified stream. Returns 0 
// if no errors occur. This function will download and parse the
// document's header and pass back the header information in the 
// "hdr" variable. The caller must test the header's status code
// to ensure that this request was successfully processed or if
// further action must be taken in order to complete the request.
{
  bytes_received = 0;

  if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;

  // Request the specified file from the server
  gxString request;
  BuildRequestString(u, gxsHTTP_GET, request);
  if(Send(request.c_str(), request.length()) < 0) return socket_error;
  
  int nRet = 0;
  int byte_count;
  int found_header = 0;
  gxString header;

  // Clear the memory buffer
  mbuf.Clear();

  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
    
    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    nRet = RawRead(rxBuffer, __HTTP_PACKET_SIZE__);
    if(nRet < 0) return socket_error;
    
    // Exit if the server closed the connection
    if(nRet == 0) break;

    if(!found_header) {
      header.Cat(rxBuffer, nRet);
      if((header.Find("\r\n\r\n") != -1) || (header.Find("\n\n") != -1)) {
	found_header = 1; // Found the end of header marker

	// Remove everything after the end of header marker
	if(!header.DeleteAfter("\r\n\r\n")) {
	  header.DeleteAfter("\n\n");
	}

	// Parse the HTTP header
	ParseHTTPHeader(u, header, hdr); 
	if((hdr.http_status < 200) || (hdr.http_status > 299)) {
	  // There is a problem with the document so do not attempt a download
	  return socket_error = gxSOCKET_HTTPDOC_ERROR; 
	}

	// Move the document body over the header information
	unsigned CRLF_len = strlen("\r\n\r\n");
	int offset = FindMatch((const char *)rxBuffer, "\r\n\r\n", 
			       (unsigned)0, (unsigned)nRet);
	if(offset == -1) {
	  offset = FindMatch((const char *)rxBuffer, "\n\n", 
			     (unsigned)0, (unsigned)nRet);
	  CRLF_len = strlen("\n\n");
	}
	offset += CRLF_len;
	byte_count = nRet - offset;
	bytes_received += byte_count;
	char *s_ptr = (char *)rxBuffer;
	memmove(s_ptr, s_ptr+offset, byte_count); 
      }
      else { // Part of the header was received
	continue; // Continue until the entire header is received
      }
    }
    else {
      byte_count = nRet;
      bytes_received += nRet;
    }

    // Copy the file to a memory buffer
    mbuf.Cat((char *)rxBuffer, byte_count);
  }
  
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

void gxsHTTPClient::Read(void *buf, unsigned Bytes, gxDeviceTypes dev) 
// Read a specified number of bytes and copy them to the device cache.
{
  switch(dev) {
    case gxDEVICE_DISK_FILE:
      if(gxdFPTRRead(file_ptr, (char *)buf, Bytes) != Bytes) {
	read_error = (int)gxDEVICE_DISK_FILE;
      }
      break;
            
    case  gxDEVICE_STREAM_SOCKET:
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
      ::wxYield(); // Yield to the calling process invoking the socket read
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

      if(RawRead((char *)buf, Bytes) < 0) {
	read_error = (int)gxDEVICE_STREAM_SOCKET;
      }
      break;

    case gxDEVICE_MEMORY_BUFFER:
      memmove(buf, rxBuffer, Bytes);
      break;

    default:
      break;
  }
}
  
void gxsHTTPClient::Write(const void *buf, unsigned Bytes,
			  gxDeviceTypes dev) 
// Write a specified number of bytes from the device cache to
// the output device.
{
  switch(dev) {
    case gxDEVICE_DISK_FILE:
      if(gxdFPTRWrite(file_ptr, (char *)buf, Bytes) != Bytes) {
	write_error = (int)gxDEVICE_DISK_FILE;
      }
      break;
      
    case  gxDEVICE_STREAM_SOCKET:
      if(Send((char *)buf, Bytes) < 0) {
	write_error =  (int)gxDEVICE_STREAM_SOCKET;
      }
      break;

    default:
      break;
  }
}
 
int gxsHTTPClient::ParseHTTPHeader(const gxsURLInfo &u,
				   const gxString &header, gxsHTTPHeader &hdr)
// Parse an HTTP header passing back the header information in the
// "hdr" variable. Returns 0 if no errors occur.
{
  hdr.http_header = header;
  gxString dup_header(header);
  
  char status[1024], status2[1024], rest[1024];
  status[0] = status2[0] = rest[0] = 0;
  int offset, index;
  gxString sbuf, ibuf, atoibuf;
  
  // Read the headers status line
  sscanf(dup_header.c_str(), "HTTP/%f %d %[^\r\n]", 
	 &hdr.http_version, &hdr.http_status, status);

  if (hdr.http_status == gxsHTTP_STATUS_UNAUTHORIZED) { 
    // The 401 (unauthorized) response message is used by an origin server
    // to challenge the authorization of a user agent. This response must
    // include a WWW-Authenticate header field containing at least one
    // challenge applicable to the requested resource. 
    hdr.authentication_needed = 1;
  }
  else if (hdr.http_status == gxsHTTP_STATUS_FORBIDDEN) {
    hdr.authentication_scheme = hdr.realm = hdr.auth_cookie = "\0";
  }
  else if ((hdr.http_status >= 200) && (hdr.http_status <= 299)) {
    if (hdr.http_status == gxsHTTP_STATUS_NO_CONTENT) {
      hdr.no_cache = 1;
      hdr.length = 0;
    }
  }
  else if ((hdr.http_status >= 500) && (hdr.http_status <= 599)) { 
    // Proxy error 
  }
  else {
    hdr.not_found = 1;
  }
  
  // In HTTP/1.1 connections are assumed to be persistent
  // unless otherwise notified.
  if (hdr.http_version >= 1.1F)
    hdr.keep_alive = 1;
  else
    hdr.keep_alive = 0;

  // Get the rest of the header parameters
  offset = dup_header.IFind("Server:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.current_server = status;
  }
  offset = dup_header.IFind("Location:");
  if(offset != -1) {
    // The Location response-header field defines the exact location of
    // the resource that was identified by the Request-URI. For 3xx
    // responses, the location must indicate the server's preferred URL
    // for automatic redirection to the resource. Only one absolute URL is
    // allowed.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.location = status;
  }
  offset = dup_header.IFind("Last-Modified:");
  if(offset != -1) {
    // The Last-Modified entity-header field indicates the date and time
    // at which the sender believes the resource was last modified.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.http_last_modified = status;
  }
  offset = dup_header.IFind("Date:");
  if(offset != -1) {
    // The Date general-header field represents the date 
    // and time at which the message was originated.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.date = status;
  }
  offset = dup_header.IFind("Expires:");
  if(offset != -1) {
    // The Expires entity-header field gives the date/time after which
    // the entity should be considered stale. This allows information
    // providers to suggest the volatility of the resource, or a date
    // after which the information may no longer be valid. Applications
    // must not cache this entity beyond the date given.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.http_expires = status;
  }
  offset = dup_header.IFind("ETag:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.etag = status;
  }
  offset = dup_header.IFind("Content-Encoding:");
  if(offset != -1) {
    // The Content-Encoding entity-header field is used as a modifier
    // to the media-type. When present, its value indicates what additional
    // content coding has been applied to the resource, and thus what
    // decoding mechanism must be applied in order to obtain the media-type
    // referenced by the Content-Type header field. The Content-Encoding is
    // primarily used to allow a document to be compressed without
    // losing the identity of its underlying media type.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.content_encoding = status;
  }
  offset = dup_header.IFind("WWW-Authenticate:");
  if(offset != -1) {
    // If a request is authenticated and a realm specified, the same
    // credentials should be valid for all other requests within this
    // realm. 
    if(sscanf(dup_header.c_str()+offset, "%*[^:]: %s %[^\r\n]",
	      status, status2) == 2) {
      hdr.authentication_scheme = status;
      hdr.realm = status2;
    }
  }
  offset = dup_header.IFind("Keep-Alive:");
  if((hdr.http_version == 1.0F) && (offset != -1)) {
    if(sscanf(dup_header.c_str()+offset, "%*[^:]: timeout=%d, max=%d",
	      &hdr.timeout, &hdr.max_conns) == 2) {
      hdr.keep_alive = 1;
    }
    else if(sscanf(dup_header.c_str()+offset, "%*[^:]: max=%d, timeout=%d",
		   &hdr.max_conns, &hdr.timeout) == 2) {
      hdr.keep_alive = 1;
    }
  }
  offset = dup_header.IFind("Connection:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^,\r\n],%[^,\r\n]",
	   status, status2);
    sbuf = status;
    offset = sbuf.IFind("keep-alive");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("persist");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("close");
    if(offset != -1) hdr.keep_alive = 0;

    sbuf = status2;
    offset = sbuf.IFind("keep-alive");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("persist");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("close");
    if(offset != -1) hdr.keep_alive = 0;
  }
  offset = dup_header.IFind("Content-Type:");
  if(offset != -1) {
    // The Content-Type entity-header field indicates the media type
    // of the Entity-Body sent to the recipient or, in the case of
    // the HEAD method, the media type that would have been sent had
    // the request been a GET. 
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^;\r\n]", status);
    char ext[256];
    sscanf(status, "%*[^/]/%[^;]", ext);
    hdr.file_extension = ".";
    hdr.file_extension += ext;

    hdr.content_type = hdr.mime_type = status;

#if defined(__DOS__) || defined(__WIN32__)
    if(hdr.mime_type == "audio/x-wav")
      hdr.file_extension = ".wav";
    else if(hdr.mime_type == "image/x-ms-bmp")
      hdr.file_extension = ".bmp";
    else if(hdr.mime_type == "application/x-msvideo")
      hdr.file_extension = ".avi";
#endif
  }
  offset = dup_header.IFind("Content-Length:");
  if(offset != -1) {
    // The Content-Length entity-header field 
    // indicates the size of the Entity-Body.
    offset += strlen("Content-Length:");
    char *src = (char *)(dup_header.c_str()+offset);
    while(*src == ' ') src++;
    atoibuf = src;
    hdr.length = atoibuf.Atol();
  }

  offset = dup_header.IFind("Pragma:");
  if(offset != -1) {
    // The Pragma general-header field is used to include implementation-
    // specific directives that may apply to any recipient along the
    // request/response chain. All pragma directives specify optional
    // behavior from the viewpoint of the protocol; however, some systems
    // may require that behavior be consistent with the directives.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.pragma = status;
    offset = hdr.pragma.IFind("no-cache");
    if(offset != -1) hdr.no_cache = 1;
  }

  offset = dup_header.IFind("Cache-Control:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.cache_control = status;
    offset = hdr.cache_control.IFind("no-cache");
    if(offset != -1) hdr.no_cache = 1;
  }
  
  offset = dup_header.IFind("Accept-Ranges:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset,"%*[^:]: %s", status);
    sbuf = status;
    if(CaseICmp(sbuf, "bytes") == 0) hdr.accept_ranges = 1;
  }

  offset = 0;
  sbuf = header;
  while(recv_loop) { // Parse all the cookies
    offset = sbuf.IFind("Set-Cookie:", offset);
    if((offset != -1) && (hdr.use_cookies)) {
      if(sscanf(dup_header.c_str()+offset, "%*[^:]: %[^=]=%[^;\r\n]; %[^\r\n]",
		status, status2, rest) >= 2) {
	gxsNetscapeCookie citem;
	citem.host = u.host;
	citem.name = status;
	citem.value = status2;
	citem.secure = 0;
	
	ibuf = rest;
	index = ibuf.IFind("Expires");
	if(index != -1) {
	  ibuf.DeleteAt(0, index);
	  ibuf.DeleteBeforeIncluding("=");
	  ibuf.DeleteAfterIncluding(";");
	  citem.expires = ibuf;
	}
	ibuf = rest;
	index = ibuf.IFind("Domain");
	if(index != -1) {
	  ibuf.DeleteAt(0, index);
	  ibuf.DeleteBeforeIncluding("=");
	  ibuf.DeleteAfterIncluding(";");
	  citem.domain = ibuf;
	}
	ibuf = rest;
	index = ibuf.IFind("Path");
	if(index != -1) {
	  ibuf.DeleteAt(0, index);
	  ibuf.DeleteBeforeIncluding("=");
	  ibuf.DeleteAfterIncluding(";");
	  citem.path = ibuf;
	}
	ibuf = rest;
	index = ibuf.IFind("Secure");
	if(index != -1) {
	  citem.secure = 1;
	}
	hdr.netscape_cookies.Insert(citem);
      }
    }
    if(offset == -1) break;
    offset++;
  }

  if(record_proto) {
    proto_string << header;
  }
  
  return 0;
}

void gxsHTTPClient::BuildSimpleRequestString(const gxsURLInfo &u,
					     int method, 
					     gxString &request)
// Build a simple request string.
{
  gxString file_buffer;
  
  if(u.path.length() > 0) {
    file_buffer = u.path;
  }
  else { // No path was specified so request a directory listing
    if(HTTP_DEFAULT_FILE_REQUEST_STRING.length() > 0)
      file_buffer = HTTP_DEFAULT_FILE_REQUEST_STRING;
    else
      file_buffer = " /";
  }
 
  char sbuf[1024];
  sprintf(sbuf, "%s %s//%s:%d%s\r\n",
	  GetMethod(gxsHTTP_GET),
	  url.GetProtocolString(u.proto_type),
	  u.host.c_str(), 
	  u.port,
	  file_buffer.c_str());

  request = sbuf;
  // Ignore BCC32 warning 8057 method is never used
}

void gxsHTTPClient::BuildRequestString(const gxsURLInfo &u,
				       int method, 
				       gxString &request)
// Build a request string.
{
  request.Clear();
  
  // The user has supplied their own request header
  if(!HTTP_CUSTOM_CLIENT_HEADER.is_null()) {
    request << HTTP_CUSTOM_CLIENT_HEADER;
    if(record_proto) {
      proto_string << request;
    }
    return;
  }

  char sbuf[255];
  gxString file_buffer;
  int offset = 0;
  
  if(u.path.length() > 0) {
    file_buffer = u.path;
  }
  else { // No path was specified so request a directory listing
    if(HTTP_DEFAULT_FILE_REQUEST_STRING.length() > 0)
      file_buffer = HTTP_DEFAULT_FILE_REQUEST_STRING;
    else
      file_buffer = " /";
  }
  
  gxString wwwauth;
  if(u.user.length() > 0 && u.passwd.length() > 0) {
    gxsBasicAuthenticationEncode(u.user.c_str(), u.passwd.c_str(),
				 "Authorization", wwwauth);
  }
						    
  request = GetMethod(method);
  request += " ";
  request += file_buffer;
  request += ' ';
  if(HTTP_VERSION_STRING.length() > 0) {
    offset = HTTP_VERSION_STRING.IFind("\r\n");
    if(offset == -1) HTTP_VERSION_STRING += "\r\n";
    request += HTTP_VERSION_STRING;
  }
  else {
    request += "HTTP/1.0\r\n";
  }
  if(HTTP_AGENT_STRING.length() > 0) {
    offset = HTTP_AGENT_STRING.IFind("\r\n");
    if(offset == -1) HTTP_AGENT_STRING += "\r\n";
    request += HTTP_AGENT_STRING;
  }
  else {
    request += "User-Agent: gxsHTTPLibrary\r\n";
  }
  request += "Host: ";
  request += u.host;
  // 07/24/2006: IIS HTTP 1.1 servers will not accept the default port number
  // in the Host: header parameter. The port number can only be specified if
  // the port number does not equal port 80.
  // NOTE: In HTTP 1.1 the Host: header parameter is required.
  if(u.port != 80) {
    sprintf(sbuf, "%d", u.port);
    request += ":";
    request += sbuf;
  }
  request += "\r\n";
  if(HTTP_ACCEPT_STRING.length() > 0) {
    offset = HTTP_ACCEPT_STRING.IFind("\r\n");
    if(offset == -1) HTTP_ACCEPT_STRING += "\r\n";
    request += HTTP_ACCEPT_STRING;
  }
  if(HTTP_KEEP_ALIVE_STRING.length() > 0) {
    offset = HTTP_KEEP_ALIVE_STRING.IFind("\r\n");
    if(offset == -1) HTTP_KEEP_ALIVE_STRING += "\r\n";
    request += HTTP_KEEP_ALIVE_STRING;
  }
  if(HTTP_CONNECTION_STRING.length() > 0) {
    offset = HTTP_CONNECTION_STRING.IFind("\r\n");
    if(offset == -1) HTTP_CONNECTION_STRING += "\r\n";
    request += HTTP_CONNECTION_STRING;
  }
  if(HTTP_REFERER_STRING.length() > 0) {
    offset = HTTP_REFERER_STRING.IFind("\r\n");
    if(offset == -1) HTTP_REFERER_STRING += "\r\n";
    request += HTTP_REFERER_STRING;
  }
  if(wwwauth.length() > 0) {
    offset = wwwauth.IFind("\r\n");
    if(offset == -1) wwwauth += "\r\n";
    request += wwwauth;
  }
  if(HTTP_PRAGMA_STRING.length() > 0) {
    offset = HTTP_PRAGMA_STRING.IFind("\r\n");
    if(offset == -1) HTTP_PRAGMA_STRING += "\r\n";
    request += HTTP_PRAGMA_STRING;
  }
  if(HTTP_IF_MOD_STRING.length() > 0) {
    offset = HTTP_IF_MOD_STRING.IFind("\r\n");
    if(offset == -1) HTTP_IF_MOD_STRING += "\r\n";
    request += HTTP_IF_MOD_STRING;
  }
  if(HTTP_CUSTOM_STRINGS.length() > 0) {
    offset = HTTP_CUSTOM_STRINGS.IFind("\r\n");
    if(offset == -1) HTTP_CUSTOM_STRINGS += "\r\n";
    request += HTTP_CUSTOM_STRINGS;
  }
  if(HTTP_COOKIE_STRING.length() > 0) {
    offset = HTTP_COOKIE_STRING.IFind("\r\n");
    if(offset == -1) HTTP_COOKIE_STRING += "\r\n";
    request += HTTP_COOKIE_STRING;
  }

  // End of request
  request += "\r\n";

  if(record_proto) {
    proto_string << request;
  }
}

int gxsHTTPClient::SimpleRequest(const gxsURLInfo &u, MemoryBuffer &mbuf)
// Request a document with no header information and write it to a
// memory buffer. Returns 0 if no errors occur.
{
  bytes_received = 0;

  if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;

  // Request the specified file from the server
  gxString request;
  BuildSimpleRequestString(u, gxsHTTP_GET, request);
  if(Send(request.c_str(), request.length()) < 0) return socket_error;

  // Clear the memory buffer
  mbuf.Clear();

  int nRet = 0;
  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    nRet = RawRead(rxBuffer, __HTTP_PACKET_SIZE__);
    if(nRet < 0) return socket_error;
    
    bytes_received += nRet;

    // Exit if the server closed the connection
    if(nRet == 0) break;
    
    // Copy the file to a memory buffer
    mbuf.Cat((char *)rxBuffer, (unsigned)nRet);
  }

  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsHTTPClient::RequestFile(const gxsURLInfo &u, const gxsHTTPHeader &hdr,
			       gxdFPTR *stream)
// This function is depreciated in version 4.11 and has been retained for 
// backward compatibility. Request a file and write it to the specified stream.
// Returns 0 if no errors occur.
{
  // Ignore BCC32 warning 8057 hdr is never used
  gxsHTTPHeader http_header;
  return Request(u, http_header, stream);
}

int gxsHTTPClient::RequestFile(const gxsURLInfo &u, const gxsHTTPHeader &hdr,
			       MemoryBuffer &mbuf)
// This function is depreciated in version 4.11 and has been retained for 
// backward compatibility. Request a file and write it to the specified buffer.
// Returns 0 if no errors occur.
{
  // Ignore BCC32 warning 8057 hdr is never used
  gxsHTTPHeader http_header;
  return Request(u, http_header, mbuf);
}

#if defined (__USE_GX_SSL_EXTENSIONS__)

gxsHTTPSClient::gxsHTTPSClient(int CacheSize, int seconds, int useconds) 
  : cache(CacheSize) 
{
  // Default client header settings
  HTTP_VERSION_STRING = "HTTP/1.0\r\n";
  HTTP_AGENT_STRING = "User-Agent: gxsHTTPSLibrary/";
  HTTP_AGENT_STRING << gxSocketVersion;
  HTTP_AGENT_STRING << "\r\n";
  HTTP_ACCEPT_STRING = "Accept: */*\r\n";
  HTTP_PRAGMA_STRING = "Pragma: no-cache\r\n";
  HTTP_DEFAULT_FILE_REQUEST_STRING = " /";
 
  // Default device settings
  ready_for_writing = 1; 
  ready_for_reading = 1;
  cache.Connect(this); 
  write_error = read_error = 0;
  recv_loop = 1;
  time_out_sec = seconds;
  time_out_usec = useconds;

  record_proto = 0; // Do not record session headers by default
}

gxsHTTPSClient::~gxsHTTPSClient() 
{ 

}

char *gxsHTTPSClient::GetMethod(int method)
{
  return (char *)gxsHTTPMethodMessage(method);
}

int gxsHTTPSClient::ConnectClient(const char *host, int port)
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, port, (char *)host) < 0) return socket_error;
  }
  else {
    return socket_error;
  }
  if(Connect() < 0) return socket_error;

  // Open and init the SSL object
  if(openssl.OpenSSLSocket(gxsocket) != gxSSL_NO_ERROR) {
    return socket_error = gxSOCKET_SSL_ERROR;
  }

  // Connect the SSL socket after our TCP connection is made
  if(openssl.ConnectSSLSocket() != gxSSL_NO_ERROR) {
    return socket_error = gxSOCKET_SSL_ERROR;
  }

  if(openssl.HasVerifyCert()) {
    if(openssl.VerifyCert(host) != gxSSL_NO_ERROR) {
      return socket_error = gxSOCKET_SSL_ERROR;
    }
  }

  return socket_error = gxSOCKET_NO_ERROR;
}
 
int gxsHTTPSClient::SimpleRequest(const gxsURLInfo &u, gxdFPTR *stream)
// Request a document with no header information. Returns 0 if no
// errors occur.
{
  file_ptr = stream;
  write_error = read_error = 0;
  bytes_received = 0;

  if(!is_connected) { // If not connected, connect to the server
    if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;
  }

  // Request the specified file from the server
  gxString request;
  BuildSimpleRequestString(u, gxsHTTP_GET, request);

  int rv = openssl.SendSSLSocket(request.c_str(), request.length());
  if(openssl.HasError()) {
    if(openssl.GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_DISCONNECTED_ERROR; 
    }
    else {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_TRANSMIT_ERROR; 
    }
  }
  if(rv != (int)request.length()) {
    openssl.CloseSSLSocket();
    return socket_error = gxSOCKET_TRANSMIT_ERROR; 
  }

  gxDeviceTypes o_device = gxDEVICE_DISK_FILE;     // Output device
  gxDeviceTypes i_device = gxDEVICE_MEMORY_BUFFER; // Input device
  
  int nRet = 0;
  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    // Setup a pointer to the cache buckets
    gxDeviceCachePtr p(cache, o_device, i_device); 
    
    nRet = openssl.RawReadSSLSocket(rxBuffer, __HTTP_PACKET_SIZE__);

    // A socket error occurred
    if(nRet < 0) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_RECEIVE_ERROR;
    }

    if(nRet < 0) {
      openssl.CloseSSLSocket();
      return socket_error;
    }
    
    bytes_received += nRet;

    // Exit if the server closed the connection
    if(nRet == 0) break;

    // Load the cache buckets
    p.Alloc(nRet);
  }

  openssl.CloseSSLSocket();
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsHTTPSClient::RequestHeader(const gxsURLInfo &u, gxsHTTPHeader &hdr)
// Request a document header passing back the header information in the
// "hdr" variable. Returns 0 if no errors occur.
{
  if(!is_connected) { // If not connected, connect to the server
    if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;
  }

  gxString request;
  BuildRequestString(u, gxsHTTP_HEAD, request);
  
  gxString header;
  int nRet = 0;

  int rv = openssl.SendSSLSocket(request.c_str(), request.length());
  if(openssl.HasError()) {
    if(openssl.GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_DISCONNECTED_ERROR; 
    }
    else {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_TRANSMIT_ERROR; 
    }
  }
  if(rv != (int)request.length()) {
    openssl.CloseSSLSocket();
    return socket_error = gxSOCKET_TRANSMIT_ERROR; 
  }

  while(recv_loop) { // Read the file header into the header string
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }
    nRet = openssl.RawReadSSLSocket(rxBuffer, __HTTP_PACKET_SIZE__);
    if(nRet < 0) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_RECEIVE_ERROR;
    }

    rxBuffer[nRet] = '\0'; // Null terminate the receive buffer
    header += rxBuffer;

    // Exit if the server closed the connection
    if(nRet == 0) break;
  }
    
  // Remove everything after the end of the header marker.
  // Some Web server return the entire document when the header is
  // requested.
  header.DeleteAfter("\r\n\r\n");


  // Reset the header before parsing it. This change was
  // made for functions that reuse the header multiple times. In the 
  // event that the site does not return a header the status members
  // will be reset rather than passing back the previous values.
  hdr.Reset();
  ParseHTTPHeader(u, header, hdr);

  openssl.CloseSSLSocket();
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsHTTPSClient::Request(const gxsURLInfo &u, gxsHTTPHeader &hdr,
			   gxdFPTR *stream, gxsHTTPHeader *compare_hdr,
			   HTTPClientDisplayFunc display)
// Request a file and write it to the specified stream. Returns 0 
// if no errors occur. This function will download and parse the
// document's header and pass back the header information in the 
// "hdr" variable. The caller must test the header's status code
// to ensure that this request was successfully processed or if
// further action must be taken in order to complete the request.
// If the compare_hdr pointer is a non-null value the document
// header's modification time field will be compared to the compare
// header's modification time fields. The document will only be
// downloaded if the document's modification time has changed
// since the last download.
{
  gxString sbuf;
  int print_num_bytes = 1;
  double elapsed_time = (double)0;
  FAU_t total_bytes = (FAU_t)0;
  FAU_t curr_count = (FAU_t)0;
  FAU_t curr_percent = (FAU_t)0;
  FAU_t byps = (FAU_t)0;
  FAU_t bps = (FAU_t)0;
  const int report_percent = 10;
  time_t begin = (time_t)0;
  time_t end = (time_t)0;
  double r_percent, c_percent;
  file_ptr = stream;
  write_error = read_error = 0;
  bytes_received = 0;

  if(!is_connected) { // If not connected, connect to the server
    if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;
  }

  // Request the specified file from the server
  gxString request;
  BuildRequestString(u, gxsHTTP_GET, request);

  int rv = openssl.SendSSLSocket(request.c_str(), request.length());
  if(openssl.HasError()) {
    if(openssl.GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_DISCONNECTED_ERROR; 
    }
    else {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_TRANSMIT_ERROR; 
    }
  }
  if(rv != (int)request.length()) {
    openssl.CloseSSLSocket();
    return socket_error = gxSOCKET_TRANSMIT_ERROR; 
  }
  
  gxDeviceTypes o_device = gxDEVICE_DISK_FILE;    // Output device
  gxDeviceTypes i_device = gxDEVICE_MEMORY_BUFFER; // Input device
  
  int nRet = 0;
  int byte_count;
  int found_header = 0;
  gxString header;

  if(display) {
    // Get the current time before entering loop
    time(&begin);
  }

  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    // Setup a pointer to the cache buckets
    gxDeviceCachePtr p(cache, o_device, i_device); 

    nRet = openssl.RawReadSSLSocket(rxBuffer, __HTTP_PACKET_SIZE__);

    // A socket error occurred
    if(nRet < 0) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_RECEIVE_ERROR;
    }

    curr_count++;
    total_bytes += nRet;
    
    // Exit if the server closed the connection
    if(nRet == 0) break;

    if(!found_header) {
      header.Cat(rxBuffer, nRet);
      if((header.Find("\r\n\r\n") != -1) || (header.Find("\n\n") != -1)) {
	found_header = 1; // Found the end of header marker
	
	// Remove everything after the end of header marker
	if(!header.DeleteAfter("\r\n\r\n")) {
	  header.DeleteAfter("\n\n");
	}

	// Parse the HTTP header
	ParseHTTPHeader(u, header, hdr); 
	if((hdr.http_status < 200) || (hdr.http_status > 299)) {
	  // There is a problem with the document so do not attempt a download
	  return socket_error = gxSOCKET_HTTPDOC_ERROR; 
	}

	if(compare_hdr) {
	  openssl.CloseSSLSocket();
	  Close();
	  if(hdr.http_last_modified == compare_hdr->http_last_modified) {
	    // The current document is the same as the on the server
	    openssl.CloseSSLSocket();
	    return socket_error = gxSOCKET_NO_ERROR; 
	  }
	  else {
	    // Signal to the caller that this file has to be 
	    // overwritten and requested again.
	    openssl.CloseSSLSocket();
	    return socket_error = gxSOCKET_FILEMODTIME_ERROR;
	  }
	}

	// Move the document body over the header information
	unsigned CRLF_len = strlen("\r\n\r\n");
	int offset = FindMatch((const char *)rxBuffer, "\r\n\r\n", 
			       (unsigned)0, (unsigned)nRet);
	if(offset == -1) {
	  offset = FindMatch((const char *)rxBuffer, "\n\n", 
			     (unsigned)0, (unsigned)nRet);
	  CRLF_len = strlen("\n\n");
	}
	offset += CRLF_len;
	byte_count = nRet - offset;
	bytes_received += byte_count;
	char *s_ptr = (char *)rxBuffer;
	memmove(s_ptr, s_ptr+offset, byte_count); 
      }
      else { // Part of the header was received
	continue; // Continue until the entire header is received
      }
    }
    else { // Found the header 
      byte_count = nRet;
      bytes_received += nRet;
      if(display) {
	if(hdr.length > 0) {
	  if(print_num_bytes) {
	    sbuf << clear << "Downloading " << shn << hdr.length << " bytes";
	    print_num_bytes = 0;
	    display(sbuf);
	  }
	  if(curr_count > 1) {
	    if((curr_count * 1024) >= 
	       (hdr.length/report_percent)) {
	      r_percent = (curr_percent++ * report_percent) / 100;
	      c_percent = hdr.length * r_percent;
	      if(c_percent >= (hdr.length - (total_bytes-header.length()))) {
		curr_count = 0;
		if((FAU_t)hdr.length != FAU_t(total_bytes-header.length())) {
		  // Display progress so far if we are not at 100%
		  sbuf << clear << "Downloaded " << shn 
		       << (total_bytes-header.length()) << " bytes of "
		       << hdr.length;
		  display(sbuf);
		}
	      }
	    }
	  }
	}
	else { // The server did not return a document length
	  if(print_num_bytes) {
	    sbuf << clear << "The server did not return a document length.";
	    display(sbuf); 
	    print_num_bytes = 0;
	  }
	}
      }
    }
    // Load the cache buckets
    p.Alloc(byte_count);
  }

  if(display) {
    // Get the current time after loop is completed 
    time(&end); 
    
    // Calculate the elapsed time in seconds. 
    elapsed_time = difftime(end, begin);

    sbuf.Precision(2); 
    sbuf << clear << "Downloaded " << shn << (total_bytes - header.length())
	 << " bytes in ";
    if(elapsed_time != (double)0) {
      if(elapsed_time <= (double)60) {
	sbuf << elapsed_time << " seconds" << "\n";
      }
      else {
	sbuf << (elapsed_time/60) << " minutes" << "\n";
      }
    }
    else {
      sbuf << "less than 1 second" << "\n";
    }
    // Calculate the number of bits per second
    if(elapsed_time != (double)0) {
      byps = (FAU_t)(total_bytes/elapsed_time);
      bps = byps * 8;
      sbuf << "Download bytes per second rate = " << byps  << " BytesPS";
      sbuf << "\n" << "LAN/WAN connection speed = " << bps << " BPS";
    }
    else {
      sbuf << "Download bytes per second rate = 100%";
      sbuf << "\n" << "LAN/WAN connection speed = 100%";
    }
    display(sbuf);
  }

  openssl.CloseSSLSocket();  
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsHTTPSClient::Request(const gxsURLInfo &u, gxsHTTPHeader &hdr,
			   MemoryBuffer &mbuf)
// Request a file and write it to the specified stream. Returns 0 
// if no errors occur. This function will download and parse the
// document's header and pass back the header information in the 
// "hdr" variable. The caller must test the header's status code
// to ensure that this request was successfully processed or if
// further action must be taken in order to complete the request.
{
  bytes_received = 0;

  if(!is_connected) { // If not connected, connect to the server
    if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;
  }

  // Request the specified file from the server
  gxString request;
  BuildRequestString(u, gxsHTTP_GET, request);
  int rv = openssl.SendSSLSocket(request.c_str(), request.length());
  if(openssl.HasError()) {
    if(openssl.GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_DISCONNECTED_ERROR; 
    }
    else {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_TRANSMIT_ERROR; 
    }
  }
  if(rv != (int)request.length()) {
    openssl.CloseSSLSocket();
    return socket_error = gxSOCKET_TRANSMIT_ERROR; 
  }

  int nRet = 0;
  int byte_count;
  int found_header = 0;
  gxString header;

  // Clear the memory buffer
  mbuf.Clear();

  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    nRet = openssl.RawReadSSLSocket(rxBuffer, __HTTP_PACKET_SIZE__);

    if(nRet < 0) {
      openssl.CloseSSLSocket();
      return socket_error;
    }
    
    // Exit if the server closed the connection
    if(nRet == 0) break;

    if(!found_header) {
      header.Cat(rxBuffer, nRet);
      if((header.Find("\r\n\r\n") != -1) || (header.Find("\n\n") != -1)) {
	found_header = 1; // Found the end of header marker

	// Remove everything after the end of header marker
	if(!header.DeleteAfter("\r\n\r\n")) {
	  header.DeleteAfter("\n\n");
	}

	// Parse the HTTP header
	ParseHTTPHeader(u, header, hdr); 
	if((hdr.http_status < 200) || (hdr.http_status > 299)) {
	  // There is a problem with the document so do not attempt a download
	  return socket_error = gxSOCKET_HTTPDOC_ERROR; 
	}

	// Move the document body over the header information
	unsigned CRLF_len = strlen("\r\n\r\n");
	int offset = FindMatch((const char *)rxBuffer, "\r\n\r\n", 
			       (unsigned)0, (unsigned)nRet);
	if(offset == -1) {
	  offset = FindMatch((const char *)rxBuffer, "\n\n", 
			     (unsigned)0, (unsigned)nRet);
	  CRLF_len = strlen("\n\n");
	}
	offset += CRLF_len;
	byte_count = nRet - offset;
	bytes_received += byte_count;
	char *s_ptr = (char *)rxBuffer;
	memmove(s_ptr, s_ptr+offset, byte_count); 
      }
      else { // Part of the header was received
	continue; // Continue until the entire header is received
      }
    }
    else {
      byte_count = nRet;
      bytes_received += nRet;
    }

    // Copy the file to a memory buffer
    mbuf.Cat((char *)rxBuffer, byte_count);
  }
  
  openssl.CloseSSLSocket();
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

void gxsHTTPSClient::Read(void *buf, unsigned Bytes, gxDeviceTypes dev) 
// Read a specified number of bytes and copy them to the device cache.
{
  switch(dev) {
    case gxDEVICE_DISK_FILE:
      if(gxdFPTRRead(file_ptr, (char *)buf, Bytes) != Bytes) {
	read_error = (int)gxDEVICE_DISK_FILE;
      }
      break;
      
    case  gxDEVICE_STREAM_SOCKET:
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
      ::wxYield(); // Yield to the calling process invoking the socket read
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
	read_error = (int)gxDEVICE_STREAM_SOCKET;
	break;
    }
    if(openssl.RawReadSSLSocket((char *)buf, Bytes) <= 0) {
	read_error = (int)gxDEVICE_STREAM_SOCKET;
      }
      break;

    case gxDEVICE_MEMORY_BUFFER:
      memmove(buf, rxBuffer, Bytes);
      break;

    default:
      break;
  }
}
  
void gxsHTTPSClient::Write(const void *buf, unsigned Bytes,
			  gxDeviceTypes dev) 
// Write a specified number of bytes from the device cache to
// the output device.
{
  switch(dev) {
    case gxDEVICE_DISK_FILE:
      if(gxdFPTRWrite(file_ptr, (char *)buf, Bytes) != Bytes) {
	write_error = (int)gxDEVICE_DISK_FILE;
      }
      break;
      
    case  gxDEVICE_STREAM_SOCKET:
      if(openssl.SendSSLSocket((char *)buf, Bytes) <= 0) {
	write_error =  (int)gxDEVICE_STREAM_SOCKET;
      }
      break;

    default:
      break;
  }
}
 
int gxsHTTPSClient::ParseHTTPHeader(const gxsURLInfo &u,
				    const gxString &header, gxsHTTPHeader &hdr)
// Parse an HTTP header passing back the header information in the
// "hdr" variable. Returns 0 if no errors occur.
{
  hdr.http_header = header;
  gxString dup_header(header);
  
  char status[1024], status2[1024], rest[1024];
  status[0] = status2[0] = rest[0] = 0;
  int offset, index;
  gxString sbuf, ibuf, atoibuf;
  
  // Read the headers status line
  sscanf(dup_header.c_str(), "HTTP/%f %d %[^\r\n]", 
	 &hdr.http_version, &hdr.http_status, status);

  if (hdr.http_status == gxsHTTP_STATUS_UNAUTHORIZED) { 
    // The 401 (unauthorized) response message is used by an origin server
    // to challenge the authorization of a user agent. This response must
    // include a WWW-Authenticate header field containing at least one
    // challenge applicable to the requested resource. 
    hdr.authentication_needed = 1;
  }
  else if (hdr.http_status == gxsHTTP_STATUS_FORBIDDEN) {
    hdr.authentication_scheme = hdr.realm = hdr.auth_cookie = "\0";
  }
  else if ((hdr.http_status >= 200) && (hdr.http_status <= 299)) {
    if (hdr.http_status == gxsHTTP_STATUS_NO_CONTENT) {
      hdr.no_cache = 1;
      hdr.length = 0;
    }
  }
  else if ((hdr.http_status >= 500) && (hdr.http_status <= 599)) { 
    // Proxy error 
  }
  else {
    hdr.not_found = 1;
  }
  
  // In HTTP/1.1 connections are assumed to be persistent
  // unless otherwise notified.
  if (hdr.http_version >= 1.1F)
    hdr.keep_alive = 1;
  else
    hdr.keep_alive = 0;

  // Get the rest of the header parameters
  offset = dup_header.IFind("Server:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.current_server = status;
  }
  offset = dup_header.IFind("Location:");
  if(offset != -1) {
    // The Location response-header field defines the exact location of
    // the resource that was identified by the Request-URI. For 3xx
    // responses, the location must indicate the server's preferred URL
    // for automatic redirection to the resource. Only one absolute URL is
    // allowed.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.location = status;
  }
  offset = dup_header.IFind("Last-Modified:");
  if(offset != -1) {
    // The Last-Modified entity-header field indicates the date and time
    // at which the sender believes the resource was last modified.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.http_last_modified = status;
  }
  offset = dup_header.IFind("Date:");
  if(offset != -1) {
    // The Date general-header field represents the date 
    // and time at which the message was originated.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.date = status;
  }
  offset = dup_header.IFind("Expires:");
  if(offset != -1) {
    // The Expires entity-header field gives the date/time after which
    // the entity should be considered stale. This allows information
    // providers to suggest the volatility of the resource, or a date
    // after which the information may no longer be valid. Applications
    // must not cache this entity beyond the date given.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.http_expires = status;
  }
  offset = dup_header.IFind("ETag:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.etag = status;
  }
  offset = dup_header.IFind("Content-Encoding:");
  if(offset != -1) {
    // The Content-Encoding entity-header field is used as a modifier
    // to the media-type. When present, its value indicates what additional
    // content coding has been applied to the resource, and thus what
    // decoding mechanism must be applied in order to obtain the media-type
    // referenced by the Content-Type header field. The Content-Encoding is
    // primarily used to allow a document to be compressed without
    // losing the identity of its underlying media type.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.content_encoding = status;
  }
  offset = dup_header.IFind("WWW-Authenticate:");
  if(offset != -1) {
    // If a request is authenticated and a realm specified, the same
    // credentials should be valid for all other requests within this
    // realm. 
    if(sscanf(dup_header.c_str()+offset, "%*[^:]: %s %[^\r\n]",
	      status, status2) == 2) {
      hdr.authentication_scheme = status;
      hdr.realm = status2;
    }
  }
  offset = dup_header.IFind("Keep-Alive:");
  if((hdr.http_version == 1.0F) && (offset != -1)) {
    if(sscanf(dup_header.c_str()+offset, "%*[^:]: timeout=%d, max=%d",
	      &hdr.timeout, &hdr.max_conns) == 2) {
      hdr.keep_alive = 1;
    }
    else if(sscanf(dup_header.c_str()+offset, "%*[^:]: max=%d, timeout=%d",
		   &hdr.max_conns, &hdr.timeout) == 2) {
      hdr.keep_alive = 1;
    }
  }
  offset = dup_header.IFind("Connection:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^,\r\n],%[^,\r\n]",
	   status, status2);
    sbuf = status;
    offset = sbuf.IFind("keep-alive");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("persist");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("close");
    if(offset != -1) hdr.keep_alive = 0;

    sbuf = status2;
    offset = sbuf.IFind("keep-alive");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("persist");
    if(offset != -1) hdr.keep_alive = 1;
    offset = sbuf.IFind("close");
    if(offset != -1) hdr.keep_alive = 0;
  }
  offset = dup_header.IFind("Content-Type:");
  if(offset != -1) {
    // The Content-Type entity-header field indicates the media type
    // of the Entity-Body sent to the recipient or, in the case of
    // the HEAD method, the media type that would have been sent had
    // the request been a GET. 
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^;\r\n]", status);
    char ext[256];
    sscanf(status, "%*[^/]/%[^;]", ext);
    hdr.file_extension = ".";
    hdr.file_extension += ext;

    hdr.content_type = hdr.mime_type = status;

#if defined(__DOS__) || defined(__WIN32__)
    if(hdr.mime_type == "audio/x-wav")
      hdr.file_extension = ".wav";
    else if(hdr.mime_type == "image/x-ms-bmp")
      hdr.file_extension = ".bmp";
    else if(hdr.mime_type == "application/x-msvideo")
      hdr.file_extension = ".avi";
#endif
  }
  offset = dup_header.IFind("Content-Length:");
  if(offset != -1) {
    // The Content-Length entity-header field 
    // indicates the size of the Entity-Body.
    offset += strlen("Content-Length:");
    char *src = (char *)(dup_header.c_str()+offset);
    while(*src == ' ') src++;
    atoibuf = src;
    hdr.length = atoibuf.Atol();
  }

  offset = dup_header.IFind("Pragma:");
  if(offset != -1) {
    // The Pragma general-header field is used to include implementation-
    // specific directives that may apply to any recipient along the
    // request/response chain. All pragma directives specify optional
    // behavior from the viewpoint of the protocol; however, some systems
    // may require that behavior be consistent with the directives.
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.pragma = status;
    offset = hdr.pragma.IFind("no-cache");
    if(offset != -1) hdr.no_cache = 1;
  }

  offset = dup_header.IFind("Cache-Control:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset, "%*[^:]: %[^\r\n]", status);
    hdr.cache_control = status;
    offset = hdr.cache_control.IFind("no-cache");
    if(offset != -1) hdr.no_cache = 1;
  }
  
  offset = dup_header.IFind("Accept-Ranges:");
  if(offset != -1) {
    sscanf(dup_header.c_str()+offset,"%*[^:]: %s", status);
    sbuf = status;
    if(CaseICmp(sbuf, "bytes") == 0) hdr.accept_ranges = 1;
  }

  offset = 0;
  sbuf = header;
  while(recv_loop) { // Parse all the cookies
    offset = sbuf.IFind("Set-Cookie:", offset);
    if((offset != -1) && (hdr.use_cookies)) {
      if(sscanf(dup_header.c_str()+offset, "%*[^:]: %[^=]=%[^;\r\n]; %[^\r\n]",
		status, status2, rest) >= 2) {
	gxsNetscapeCookie citem;
	citem.host = u.host;
	citem.name = status;
	citem.value = status2;
	citem.secure = 0;
	
	ibuf = rest;
	index = ibuf.IFind("Expires");
	if(index != -1) {
	  ibuf.DeleteAt(0, index);
	  ibuf.DeleteBeforeIncluding("=");
	  ibuf.DeleteAfterIncluding(";");
	  citem.expires = ibuf;
	}
	ibuf = rest;
	index = ibuf.IFind("Domain");
	if(index != -1) {
	  ibuf.DeleteAt(0, index);
	  ibuf.DeleteBeforeIncluding("=");
	  ibuf.DeleteAfterIncluding(";");
	  citem.domain = ibuf;
	}
	ibuf = rest;
	index = ibuf.IFind("Path");
	if(index != -1) {
	  ibuf.DeleteAt(0, index);
	  ibuf.DeleteBeforeIncluding("=");
	  ibuf.DeleteAfterIncluding(";");
	  citem.path = ibuf;
	}
	ibuf = rest;
	index = ibuf.IFind("Secure");
	if(index != -1) {
	  citem.secure = 1;
	}
	hdr.netscape_cookies.Insert(citem);
      }
    }
    if(offset == -1) break;
    offset++;
  }
  
  if(record_proto) {
    proto_string << header;
  }
  return 0;
}

void gxsHTTPSClient::BuildSimpleRequestString(const gxsURLInfo &u,
					     int method, 
					     gxString &request)
// Build a simple request string.
{
  gxString file_buffer;
  
  if(u.path.length() > 0) {
    file_buffer = u.path;
  }
  else { // No path was specified so request a directory listing
    if(HTTP_DEFAULT_FILE_REQUEST_STRING.length() > 0)
      file_buffer = HTTP_DEFAULT_FILE_REQUEST_STRING;
    else
      file_buffer = " /";
  }
 
  char sbuf[1024];
  sprintf(sbuf, "%s %s//%s:%d%s\r\n",
	  GetMethod(gxsHTTP_GET),
	  url.GetProtocolString(u.proto_type),
	  u.host.c_str(), 
	  u.port,
	  file_buffer.c_str());

  request = sbuf;
  // Ignore BCC32 warning 8057 method is never used
}

void gxsHTTPSClient::BuildRequestString(const gxsURLInfo &u,
				       int method, 
				       gxString &request)
// Build a request string.
{
  request.Clear();
  
  // The user has supplied their own request header
  if(!HTTP_CUSTOM_CLIENT_HEADER.is_null()) {
    request << HTTP_CUSTOM_CLIENT_HEADER;
    if(record_proto) {
      proto_string << request;
    }
    return;
  }

  char sbuf[255];
  gxString file_buffer;
  int offset = 0;
  
  if(u.path.length() > 0) {
    file_buffer = u.path;
  }
  else { // No path was specified so request a directory listing
    if(HTTP_DEFAULT_FILE_REQUEST_STRING.length() > 0)
      file_buffer = HTTP_DEFAULT_FILE_REQUEST_STRING;
    else
      file_buffer = " /";
  }
  
  gxString wwwauth;
  if(u.user.length() > 0 && u.passwd.length() > 0) {
    gxsBasicAuthenticationEncode(u.user.c_str(), u.passwd.c_str(),
				 "Authorization", wwwauth);
  }
						    
  request = GetMethod(method);
  request += " ";
  request += file_buffer;
  request += ' ';
  if(HTTP_VERSION_STRING.length() > 0) {
    offset = HTTP_VERSION_STRING.IFind("\r\n");
    if(offset == -1) HTTP_VERSION_STRING += "\r\n";
    request += HTTP_VERSION_STRING;
  }
  else {
    request += "HTTP/1.0\r\n";
  }
  if(HTTP_AGENT_STRING.length() > 0) {
    offset = HTTP_AGENT_STRING.IFind("\r\n");
    if(offset == -1) HTTP_AGENT_STRING += "\r\n";
    request += HTTP_AGENT_STRING;
  }
  else {
    request += "User-Agent: gxsHTTPSLibrary\r\n";
  }
  request += "Host: ";
  request += u.host;

  // 07/24/2006: IIS HTTP 1.1 servers will not accept the default port number
  // in the Host: header parameter. The port number can only be specified if
  // the port number does not equal port 443.
  // NOTE: In HTTP 1.1 the Host: header parameter is required.
  if(u.port != 443) {
    sprintf(sbuf, "%d", u.port);
    request += ":";
    request += sbuf;
  }
  request += "\r\n";
  if(HTTP_ACCEPT_STRING.length() > 0) {
    offset = HTTP_ACCEPT_STRING.IFind("\r\n");
    if(offset == -1) HTTP_ACCEPT_STRING += "\r\n";
    request += HTTP_ACCEPT_STRING;
  }
  if(HTTP_KEEP_ALIVE_STRING.length() > 0) {
    offset = HTTP_KEEP_ALIVE_STRING.IFind("\r\n");
    if(offset == -1) HTTP_KEEP_ALIVE_STRING += "\r\n";
    request += HTTP_KEEP_ALIVE_STRING;
  }
  if(HTTP_CONNECTION_STRING.length() > 0) {
    offset = HTTP_CONNECTION_STRING.IFind("\r\n");
    if(offset == -1) HTTP_CONNECTION_STRING += "\r\n";
    request += HTTP_CONNECTION_STRING;
  }
  if(HTTP_REFERER_STRING.length() > 0) {
    offset = HTTP_REFERER_STRING.IFind("\r\n");
    if(offset == -1) HTTP_REFERER_STRING += "\r\n";
    request += HTTP_REFERER_STRING;
  }
  if(wwwauth.length() > 0) {
    offset = wwwauth.IFind("\r\n");
    if(offset == -1) wwwauth += "\r\n";
    request += wwwauth;
  }
  if(HTTP_PRAGMA_STRING.length() > 0) {
    offset = HTTP_PRAGMA_STRING.IFind("\r\n");
    if(offset == -1) HTTP_PRAGMA_STRING += "\r\n";
    request += HTTP_PRAGMA_STRING;
  }
  if(HTTP_IF_MOD_STRING.length() > 0) {
    offset = HTTP_IF_MOD_STRING.IFind("\r\n");
    if(offset == -1) HTTP_IF_MOD_STRING += "\r\n";
    request += HTTP_IF_MOD_STRING;
  }
  if(HTTP_CUSTOM_STRINGS.length() > 0) {
    offset = HTTP_CUSTOM_STRINGS.IFind("\r\n");
    if(offset == -1) HTTP_CUSTOM_STRINGS += "\r\n";
    request += HTTP_CUSTOM_STRINGS;
  }
  if(HTTP_COOKIE_STRING.length() > 0) {
    offset = HTTP_COOKIE_STRING.IFind("\r\n");
    if(offset == -1) HTTP_COOKIE_STRING += "\r\n";
    request += HTTP_COOKIE_STRING;
  }
  // End of request
  request += "\r\n";

  if(record_proto) {
    proto_string << request;
  }
}

int gxsHTTPSClient::SimpleRequest(const gxsURLInfo &u, MemoryBuffer &mbuf)
// Request a document with no header information and write it to a
// memory buffer. Returns 0 if no errors occur.
{
  bytes_received = 0;

  if(!is_connected) { // If not connected, connect to the server
    if(ConnectClient(u.host.c_str(), u.port) != 0) return socket_error;
  }

  // Request the specified file from the server
  gxString request;
  BuildSimpleRequestString(u, gxsHTTP_GET, request);
  int rv = openssl.SendSSLSocket(request.c_str(), request.length());
  if(openssl.HasError()) {
    if(openssl.GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_DISCONNECTED_ERROR; 
    }
    else {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_TRANSMIT_ERROR; 
    }
  }
  if(rv != (int)request.length()) {
    openssl.CloseSSLSocket();
    return socket_error = gxSOCKET_TRANSMIT_ERROR; 
  }

  // Clear the memory buffer
  mbuf.Clear();

  int nRet = 0;
  while(recv_loop) { // Read the file into the device cache
#if defined (__wxWIN201__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN291__) && defined (__GUI_THREAD_SAFE__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif
#if defined (__wxWIN302__)
    ::wxYield(); // Yield to the calling process invoking the HTTP process
#endif

    if(!ReadSelect(gxsocket, time_out_sec, time_out_usec)) {
      openssl.CloseSSLSocket();
      return socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }

    nRet = openssl.RawReadSSLSocket(rxBuffer, __HTTP_PACKET_SIZE__);

    if(nRet < 0) {
      openssl.CloseSSLSocket();
      return socket_error;
    }
    
    bytes_received += nRet;

    // Exit if the server closed the connection
    if(nRet == 0) break;
    
    // Copy the file to a memory buffer
    mbuf.Cat((char *)rxBuffer, (unsigned)nRet);
  }

  openssl.CloseSSLSocket();
  Close();
  return socket_error = gxSOCKET_NO_ERROR;
}

#endif // __USE_GX_SSL_EXTENSIONS__

#ifdef __BCC32__
#pragma warn .8057
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
