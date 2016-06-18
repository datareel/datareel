// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: wserror.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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

Standalone functions that can optionally be used with the
gxSocket class to display extended WinSock error information
following a socket error.

Changes:
==============================================================
07/10/2001: Changed include path for the gxstypes.h include file
from <gxstypes.h> to "gxstypes.h"
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "wserror.h"
#include "gxstypes.h"

GXDLCODE_API char *WinSockExceptionMessage(int wserror)
{
#if defined (__WIN32__)
  switch(wserror) {
    case 0 :
      return (char *)("WSANONE: No WinSock errors reported");
    case WSAEINTR :
      return (char *)("WSAEINTR: Interrupted function call");
    case WSAEBADF :
      return (char *)("WSAEBADF: Bad file number");
    case WSAEACCES :
      return (char *)("WSAEACCES: Permission denied");
    case WSAEFAULT :
      return (char *)("WSAEFAULT: Address fault");
    case WSAEINVAL :
      return (char *)("WSAEINVAL: Invalid argument");
    case WSAEMFILE :
      return (char *)("WSAEMFILE: Too many open files");
    case WSAEWOULDBLOCK :
      return (char *)("WSAEWOULDBLOCK: Resource temporarily unavailable");
    case WSAEINPROGRESS :
      return (char *)("WSAEINPROGRESS: Operation now in progress");
    case WSAEALREADY :
      return (char *)("WSAEALREADY: Operation already in progress");
    case WSAENOTSOCK :
      return (char *)("WSAENOTSOCK: Socket operation on non-socket");
    case WSAEDESTADDRREQ :
      return (char *)("WSAEDESTADDRREQ: Destination address required");
    case WSAEMSGSIZE :
      return (char *)("WSAEMSGSIZE: Message too long");
    case WSAEPROTOTYPE :
      return (char *)("WSAEPROTOTYPE: Protocol wrong type for socket");
    case WSAENOPROTOOPT :
      return (char *)("WSAENOPROTOOPT: Bad protocol option");
    case WSAEPROTONOSUPPORT :
      return (char *)("WSAEPROTONOSUPPORT: Protocol not supported");
    case WSAESOCKTNOSUPPORT :
      return (char *)("WSAESOCKTNOSUPPORT: Socket type not supported");
    case WSAEOPNOTSUPP :
      return (char *)("WSAEOPNOTSUPP: Operation not supported");
    case WSAEPFNOSUPPORT :
      return (char *)("WSAEPFNOSUPPORT: Protocol family not supported");
    case WSAEAFNOSUPPORT :
      return (char *)("WSAEAFNOSUPPORT: Address family not supported by \
protocol family");
    case WSAEADDRINUSE :
      return (char *)("WSAEADDRINUSE: Address already in use");
    case WSAEADDRNOTAVAIL :
      return (char *)("WSAEADDRNOTAVAIL: Cannot assign requested address");
    case WSAENETDOWN :
      return (char *)("WSAENETDOWN: Network is down");
    case WSAENETUNREACH :
      return (char *)("WSAENETUNREACH: Network is unreachable");
    case WSAENETRESET :
      return (char *)("WSAENETRESET: Network dropped connection on reset");
    case WSAECONNABORTED :
      return (char *)("WSAECONNABORTED: Software caused connection abort");
    case WSAECONNRESET :
      return (char *)("WSAECONNRESET: Connection reset by peer");
    case WSAENOBUFS :
      return (char *)("WSAENOBUFS: No buffer space available");
    case WSAEISCONN :
      return (char *)("WSAEISCONN: Socket is already connected");
    case WSAENOTCONN :
      return (char *)("WSAENOTCONN: Socket is not connected");
    case WSAESHUTDOWN :
      return (char *)("WSAESHUTDOWN: Cannot send after socket shutdown");
    case WSAETOOMANYREFS :
      return (char *)("WSAETOOMANYREFS: Too many references");
    case WSAETIMEDOUT :
      return (char *)("WSAETIMEDOUT: Connection timed out");
    case WSAECONNREFUSED :
      return (char *)("WSAECONNREFUSED: Connection refused");
    case WSAELOOP :
      return (char *)("WSAELOOP: Loop");
    case WSAENAMETOOLONG :
      return (char *)("WSAENAMETOOLONG: Name too long");
    case WSAEHOSTDOWN :
      return (char *)("WSAEHOSTDOWN: Host is down");
    case WSAEHOSTUNREACH :
      return (char *)("WSAEHOSTUNREACH: No route to host");
    case WSAENOTEMPTY :
      return (char *)("WSAENOTEMPTY: Directory not empty");
    case WSAEPROCLIM :
      return (char *)("WSAEPROCLIM: Too many processes");
    case WSAEUSERS :
      return (char *)("WSAEUSERS: Too many users");
    case WSAEDQUOT :
      return (char *)("WSAEDQUOT: Disk quota");
    case WSAESTALE :
      return (char *)("WSAESTALE: Stale");
    case WSAEREMOTE :
      return (char *)("WSAEREMOTE: Remote");
    case WSASYSNOTREADY :
      return (char *)("WSASYSNOTREADY: Network subsystem is unavailable");
    case WSAVERNOTSUPPORTED :
      return (char *)("WSAVERNOTSUPPORTED: WINSOCK.DLL version out of range");
    case WSANOTINITIALISED :
      return (char *)("WSANOTINITIALISED: Successful WSAStartup() not yet \
performed");
    case WSAEDISCON :
      return (char *)("WSAEDISCON: Graceful shutdown in progress");
    case WSAHOST_NOT_FOUND :
      return (char *)("WSAHOST_NOT_FOUND: Host not found");
    case WSATRY_AGAIN :
      return (char *)("WSATRY_AGAIN: Non-authoritative host not found");
    case WSANO_RECOVERY :
      return (char *)("WSANO_RECOVERY: Non-recoverable error");
    case WSANO_DATA :
      return (char *)("WSANO_DATA: Valid name, no data record of requested \
type");

  // Winsock version 2.0 and higher
#ifdef WSAENOMORE
    case WSAENOMORE :
      return (char *)("WSAENOMORE: WinSock error");
#endif

#ifdef WSAECANCELLED
    case WSAECANCELLED :
      return (char *)("WSAECANCELLED: Cancelled");
#endif

#ifdef WSAEINVALIDPROCTABLE
    case WSAEINVALIDPROCTABLE :
      return (char *)("WSAEINVALIDPROCTABLE: Invalid procedure table from \
service provider");
#endif

#ifdef WSAEINVALIDPROVIDER
    case WSAEINVALIDPROVIDER :
      return (char *)("WSAEINVALIDPROVIDER: Invalid service provider version \
number");
#endif

#ifdef WSAEPROVIDERFAILEDINIT
    case WSAEPROVIDERFAILEDINIT :
      return (char *)("WSAEPROVIDERFAILEDINIT: Unable to initialize a \
service provider");
#endif

#ifdef WSASYSCALLFAILURE
    case WSASYSCALLFAILURE :
      return (char *)("WSASYSCALLFAILURE: System call failure");
#endif

#ifdef WSASERVICE_NOT_FOUND
    case WSASERVICE_NOT_FOUND :
      return (char *)("WSASERVICE_NOT_FOUND: Service not found");
#endif

#ifdef WSATYPE_NOT_FOUND
    case WSATYPE_NOT_FOUND :
      return (char *)("WSATYPE_NOT_FOUND: Type not found");
#endif

#ifdef WSA_E_NO_MORE
    case WSA_E_NO_MORE :
      return (char *)("WSA_E_NO_MORE: WinSock error");
#endif

#ifdef WSA_E_CANCELLED
    case WSA_E_CANCELLED :
      return (char *)("WSA_E_CANCELLED: WinSock error");
#endif

#ifdef WSAEREFUSED
    case WSAEREFUSED :
      return (char *)("WSAEREFUSED: Refused");
#endif

    default:
      return (char *)("WSAERROR: Unknown WinSock error reported");      
  }
#else
  return (char *)("WSAERROR: This is not a WIN32 Platform");
#endif
};

GXDLCODE_API void WinSockSetException(int err)
{
#if defined (__WIN32__) 
  WSASetLastError(err);
#endif
}

GXDLCODE_API int WinSockExceptionNumber()
{
#if defined (__WIN32__) 
  return WSAGetLastError();
#else
  return 0;
#endif
}

GXDLCODE_API char *WinSockExceptionMessage()
{
#if defined (__WIN32__) 
  return WinSockExceptionMessage(WSAGetLastError());
#else
  return (char *)("WSAERROR: This is not a WIN32 Platform");
#endif
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
