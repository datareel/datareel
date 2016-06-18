// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxstypes.h 
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

This include file contains cross-platform type definitions and
enumerations used throughout the GX socket library.

Changes:
==============================================================
07/10/2001: Added __AIX__ conditional directive for the IBM xlC 
C and C++ Compilers.

07/19/2001: IANA service file updates for DataReel database 
port name and number:
-- Services file entries --
gxs-data-port   2073/tcp   DataReel Database Socket
gxs-data-port   2073/udp   DataReel Database Socket

04/15/2002: Added the gxSOCKET_FILELENGTH_ERROR and
gxSOCKET_FILEMODTIME_ERROR error codes to the 
gxSocketError enumeration.

06/10/2002: Added embedded protocol error codes to the 
gxSocketError enumeration.

11/17/2003: Added gxSOCKET_DISCONNECTED_ERROR to the gxSocketError 
enumeration. The exception signals that a socket has been gracefully 
closed during an revc() call when the recv() call returns zero bytes.

12/19/2005: Added new error codes to gxSocketError enumeration.

12/19/2005: Added new ports and services to gxsServices enumeration.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_SOCKET_TYPES_HPP__
#define __GX_SOCKET_TYPES_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Platform specific include files
// --------------------------------------------------------------
#if defined (__WIN32__)
#include <winsock.h> // Windows socket functions
#include <time.h>    // Time value functions

#elif defined (__UNIX__)
#include <unistd.h>     // UNIX standard function definitions 
#include <sys/types.h>  // Type definitions
#include <netdb.h>      // Net DB structures
#include <arpa/inet.h>  // Inet functions
#include <netinet/in.h> // Structures defined by the internet system
#include <sys/socket.h> // Definitions related to sockets
#include <sys/time.h>   // Time value functions
#else
#error You must define a target platform:\n __WIN32__ or __UNIX__
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Type definitions
// --------------------------------------------------------------
typedef int gxsAddressFamily;  // Address family to which a socket belongs
typedef int gxsProtocolFamily; // Protocol family to which a socket belongs
typedef int gxsSocket_t;       // Type used to ID sockets
typedef int gxsPort_t;         // Data type used for port numbers
typedef sockaddr_in gxsSocketAddress; // Socket Internet address
typedef servent gxsServent;           // Service name and protocol information
typedef hostent gxsHostNameInfo;      // Host name information
typedef in_addr gxsInternetAddress;   // Internet address structure

// 03/15/2001: Socket length definition
// POSIX defines the socket length type as an unsigned integer value
// NOTE: Under WIN32 and some UNIX platforms this value is defined as
// an signed integer so a conditional directive has been added for each
// platform with the default value of an unsigned integer.
#if defined (__WIN32__)
typedef int gxsSocketLength_t;
#elif defined (__UNIX__) && defined (__HPUX__)
typedef int gxsSocketLength_t;
#elif defined (__UNIX__) && defined (__SOLARIS__)
typedef int gxsSocketLength_t;
#elif defined(__UNIX__) && defined (__AIX__)
typedef socklen_t gxsSocketLength_t;
#else 
typedef unsigned gxsSocketLength_t;
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Constants 
// --------------------------------------------------------------
const unsigned gxsBUF_SIZE     =  1024; // Fixed string buffer length
const unsigned gxsMAX_NAME_LEN =  256;  // Maximum string name length
const unsigned gxsRX_BUF_SIZE  =  4096; // Default receive buffer size
const unsigned gxsTX_BUF_SIZE  =  4096; // Default transmit buffer size
const unsigned gxsMAX_DATAGRAM = 65507; // Maximum datagram size

// IANA assigned port number for database block sockets
const int gxsDATA_PORT = 2073;
// --------------------------------------------------------------

// --------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------
enum gxSocketError {    // gxSocket exception codes
  gxSOCKET_NO_ERROR = 0,       // No socket errors reported
  gxSOCKET_INVALID_ERROR_CODE, // Invalid socket error code

  // Socket error codes
  gxSOCKET_ACCEPT_ERROR,         // Error accepting remote socket
  gxSOCKET_BIND_ERROR,           // Could not bind socket
  gxSOCKET_BUFOVER_ERROR,        // Buffer overflow 
  gxSOCKET_CONNECT_ERROR,        // Could not connect socket
  gxSOCKET_DISCONNECTED_ERROR,   // Socket has been disconnected
  gxSOCKET_FILELENGTH_ERROR,     // File length does not match expected value
  gxSOCKET_FILEMODTIME_ERROR,    // File modification time dates do not match
  gxSOCKET_FILESYSTEM_ERROR,     // A file system error occurred
  gxSOCKET_GETOPTION_ERROR,      // Error getting socket option
  gxSOCKET_HOSTNAME_ERROR,       // Could not resolve hostname
  gxSOCKET_INIT_ERROR,           // Initialization error
  gxSOCKET_LISTEN_ERROR,         // Listen error
  gxSOCKET_PEERNAME_ERROR,       // Get peer name error
  gxSOCKET_PROTOCOL_ERROR,       // Unknown protocol requested
  gxSOCKET_RECEIVE_ERROR,        // Receive error
  gxSOCKET_REQUEST_TIMEOUT,      // Request timed out
  gxSOCKET_SERVICE_ERROR,        // Unknown service requested
  gxSOCKET_SETOPTION_ERROR,      // Error setting socket option
  gxSOCKET_SOCKNAME_ERROR,       // Get socket name error
  gxSOCKET_SOCKETTYPE_ERROR,     // Unknown socket type requested
  gxSOCKET_TRANSMIT_ERROR,       // Transmit error

  // WIN32 WinSock Error Codes (Transparent to UNIX applications)
  gxSOCKET_WINSOCKCLEANUP_ERROR, // WSACleanup operation failed
  gxSOCKET_WINSOCKEINVAL_ERROR,  // Version specified not supported by DLL
  gxSOCKET_WINSOCKINIT_ERROR,    // Could not initialize Winsock
  gxSOCKET_WINSOCKNETWORK_ERROR, // Network subsystem is not ready
  gxSOCKET_WINSOCKVERSION_ERROR, // Requested version not supported
  
  // Exception codes added to handle database block errors
  gxSOCKET_BLOCKACK_ERROR,    // Database block acknowledgment error
  gxSOCKET_BLOCKHEADER_ERROR, // Bad database block header
  gxSOCKET_BLOCKSIZE_ERROR,   // Bad database block size
  gxSOCKET_BLOCKSYNC_ERROR,   // Database block synchronization error

  // Exception codes added to handle embedded protocol errors
  gxSOCKET_EB_PROTO_ERROR, // Embedded protocol reported error condition 
  gxSOCKET_FTP_ERROR,      // FTP protocol reported error condition
  gxSOCKET_HTTP_ERROR,     // HTTP protocol reported error condition
  gxSOCKET_HTTPDOC_ERROR,  // HTTP document error condition
  gxSOCKET_POP3_ERROR,     // POP3 protocol reported error condition
  gxSOCKET_SMTP_ERROR,     // SMTP protocol reported error condition
  gxSOCKET_TELNET_ERROR ,  // Telnet protocol reported error condition
  gxSOCKET_CRYPTO_ERROR,   // Crypto error
  gxSOCKET_DNS_ERROR,      // DNS protocol reported error condition
  gxSOCKET_HTTPS_ERROR,    // HTTPS protocol reported error condition
  gxSOCKET_IMAP_ERROR,     // IMAP protocol reported an error condition 
  gxSOCKET_LDAP_ERROR,     // LDAP protocol reported error condition
  gxSOCKET_PRINTER_ERROR,  // Line printer reported error condition
  gxSOCKET_NFS_ERROR,      // NFS protocol reported error condition
  gxSOCKET_NIS_ERROR,      // NIS protocol reported error condition
  gxSOCKET_NTP_ERROR,      // NTP protocol reported error condition
  gxSOCKET_RPC_ERROR,      // RPC protocol reported error condition 
  gxSOCKET_RSYNC_ERROR,    // Rsync protocol reported error condition 
  gxSOCKET_SFTP_ERROR,     // SFTP protocol reported an error condition
  gxSOCKET_SCP_ERROR,      // SCP protocol reported an error condition
  gxSOCKET_SSH_ERROR,      // SSH protocol reported an error condition
  gxSOCKET_SNMP_ERROR,     // SNMP protocol reported an error condition
  gxSOCKET_SSL_ERROR,      // SSL protocol reported an error condition
  gxSOCKET_SYSLOG_ERROR,   // Syslog protocol reported an error condition
  gxSOCKET_TFTP_ERROR,     // TFTP protocol reported an error condition

  // Exception code used to allow application to set error string
  gxSOCKET_APPDEF_ERROR // Socket application reported an error
};

enum gxsSocketLibraryVersion {
  // Platform specific library version constants

  // WIN32 supported WSA versions. NOTE: The WSA constants have no
  // meaning in UNIX implementations but will work transparently in
  // all UNIX applications.
  gxSOCKET_WSAVER_ONEZERO, // Testing purposes only
  gxSOCKET_WSAVER_ONEONE,  // Windows 95A/B/C, 98, 98SE, NT4.0 compatible
  gxSOCKET_WSAVER_TWOZERO, // Windows 98, 98SE, 2000 compatible
  gxSOCKET_WSAVER_TWOTWO   // Windows 98SE, 2000 compatible
};

enum gxsServices {
// Enumeration for common port numbers found in the /etc/services file
// on UNIX platforms or the \windows\services file on WIN32 platforms.
// A complete list of all assigned port numbers is maintained by IANA:
// http://www.iana.org

  // Common port assignments
  gxSOCKET_ECHO_PORT       = 7,   // Echo port
  gxSOCKET_FTPDATA_PORT    = 20,  // FTP data port
  gxSOCKET_FTP_PORT        = 21,  // FTP port
  gxSOCKET_SSH_PORT        = 22,  // Secure shell port
  gxSOCKET_TELNET_PORT     = 23,  // Telnet port
  gxSOCKET_SMTP_PORT       = 25,  // Simple mail transfer protocol port
  gxSOCKET_TIME_PORT       = 37,  // Time server
  gxSOCKET_NAME_PORT       = 42,  // Name server
  gxSOCKET_NAMESERVER_PORT = 53,  // Domain name server
  gxSOCKET_TFTP_PORT       = 69,  // Trivial file transfer port
  gxSOCKET_FINGER_PORT     = 79,  // Finger port
  gxSOCKET_HTTP_PORT       = 80,  // HTTP port
  gxSOCKET_POP_PORT        = 109, // Postoffice protocol 1
  gxSOCKET_POP2_PORT       = 109, // Postoffice protocol 2
  gxSOCKET_POP3_PORT       = 110, // Postoffice protocol 3
  gxSOCKET_SFTP_PORT       = 115, // Simple file transfer protocol
  gxSOCKET_NNTP_PORT       = 119, // Network news transfer protocol
  gxSOCKET_NTP_PORT        = 123, // Network time protocol
  gxSOCKET_IMAP_PORT       = 143, // Internet message access protocol
  gxSOCKET_SNMP_PORT       = 161, // SNMP port
  gxSOCKET_LDAP_PORT       = 389, // Lightweight directory access protocol
  gxSOCKET_HTTPS_PORT      = 443, // HTTPS port
  gxSOCKET_SYSLOG_PORT     = 514, // UNIX syslog port
  gxSOCKET_PRINTER_PORT    = 515, // UNIX line printer (lp) port
  gxSOCKET_SLDAP_PORT      = 636, // Secure LDAP
  gxSOCKET_RSYNC_PORT      = 873, // UNIX rsync port
  gxSOCKET_SIMAP_PORT      = 993, // Secure IMAP
  gxSOCKET_SPOP3_PORT      = 995, // Secure POP3

  // 07/19/2001: IANA assigned port number for database block sockets 
  gxSOCKET_DBS_PORT = 2073, // gxs-data-port (services file name)
  gxSOCKET_DEFAULT_PORT = 2073 // Default port if no port number is specified
};

enum gxsServerTypes {
  gxSOCKET_STREAM_SERVER,
  gxSOCKET_DATAGRAM_SERVER,
  gxSOCKET_SERIAL_PORT_SERVER,
  gxSOCKET_HTTP_SERVER,
  gxSOCKET_LOCAL_FILE_SYSTEM
};

enum gxClientTypes {
  gxSOCKET_STREAM_CLIENT,
  gxSOCKET_DATAGRAM_CLIENT,
  gxSOCKET_HTTP_CLIENT,
  gxSOCKET_SERIAL_PORT_CLIENT
};
// --------------------------------------------------------------

#endif // __GX_SOCKET_TYPES_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
