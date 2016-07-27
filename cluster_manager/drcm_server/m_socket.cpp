// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
// Date Last Modified: 07/27/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This file is part of the DataReel software distribution.

Datareel is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version. 

Datareel software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with the DataReel software distribution.  If not, see
<http://www.gnu.org/licenses/>.

Extra socket functions for Datareel cluster manager.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <errno.h>
#include "m_socket.h"

int get_fd_socket_error(int &error_number, gxString &sbuf)
{
  error_number = errno;
  return fd_error_to_string(error_number, sbuf);
}

CM_MessageHeader::CM_MessageHeader() 
{ 
  clear(); 
  set_word(checkword, NET_CHECKWORD); 
}

void CM_MessageHeader::clear() 
{ 
  memset(checkword, 0, 4);
  memset(bytes, 0, 4);
  memset(datagram_size, 0, 4);
  memset(cluster_command, 0, 4);
  set_word(datagram_size, 1500);
  memset(sha1sum, 0, 40);
}

void CM_MessageHeader::copy(const CM_MessageHeader &ob) 
{
  memmove(checkword, ob.checkword, 4);
  memmove(bytes, ob.bytes, 4);
  memmove(datagram_size, ob.datagram_size, 4);
  memmove(cluster_command, ob.cluster_command, 4);
  memmove(sha1sum, ob.sha1sum, 40);
}

int CM_MessageHeader::get_word(char bytes[4])
{
  int a, b, c, d;
  a = (int)bytes[0];
  b = (int)bytes[1];
  c = (int)bytes[2];
  d = (int)bytes[3];
  
  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;
  return a + b + c + d;
}

void CM_MessageHeader::set_word(char bytes[4], int val)
{
  bytes[0] = (unsigned char)(val & 0xFF);
  bytes[1] = (unsigned char)((val & 0xFF00)>>8);
  bytes[2] = (unsigned char)((val & 0xFF0000)>>16);
  bytes[3] = (unsigned char)((val & 0xFF000000)>>24);
}

CM_MessageHeader::CM_MessageHeader(const CM_MessageHeader &ob) 
{ 
  copy(ob); 
}

CM_MessageHeader &CM_MessageHeader::operator=(const CM_MessageHeader &ob) 
{ 
  if(this != &ob) {
    copy(ob);
  }
  return *this;
}

// Map error number to a string. NOTE: This is not a 
// redundent function. The errno values have different
// meanings for files, threads, sockets, etc. Also, the
// errno to string functions vary on Linux platforms.
// In order to remain consistent on all Linux varients
// and return string values for socket connections
// use this function to get the string value for errno
// following a socket call.
//
// Developers, if you find string messages that need to
// be changed, modifed and submit to git repo.
int fd_error_to_string(int error_number, gxString &sbuf)
{
  switch(error_number) {
    case 0:
      sbuf << clear << "Success";
      break;
    case EPERM: 
      sbuf << clear << "EPERM: Operation not permitted";
      break;
    case ENOENT:
      // sbuf << clear << "ENOENT: No such file or directory";
      sbuf << clear << "ENOENT: Cannot open another socket connection";
      break;
    case ESRCH:
      sbuf << clear << "ESRCH: No such process";
      break;
    case EINTR:
      sbuf << clear << "EINTR: Interrupted system call";
      break;
    case EIO:
      sbuf << clear << "EIO: I/O error";
      break;
    case ENXIO:
      sbuf << clear << "ENXIO: No such device or address";
      break;
    case E2BIG:
      sbuf << clear << "E2BIG: Argument list too long";
      break;
    case ENOEXEC:
      sbuf << clear << "ENOEXEC: Exec format error";
      break;
    case EBADF:
      sbuf << clear << "EBADF: Socket file decriptor is closed";
      break;
    case ECHILD:
      sbuf << clear << "ECHILD: No child processes";
      break;
      // For sockets use EWOULDBLOCK
      //case EAGAIN:
      //  sbuf << clear << "EAGAIN: Try again";
      //  break;
    case ENOMEM:
      sbuf << clear << "ENOMEM: Out of memory";
      break;
    case EACCES:
      sbuf << clear << "EACCES: Permission denied";
      break;
    case EFAULT:
      sbuf << clear << "EFAULT: Bad address";
      break;
    case ENOTBLK:
      sbuf << clear << "ENOTBLK: Block device required";
      break;
    case EBUSY:
      sbuf << clear << "EBUSY: Device or resource busy";
      break;
    case EEXIST:
      sbuf << clear << "EEXIST: File exists";
      break;
    case EXDEV:
      sbuf << clear << "EXDEV: Cross-device link";
      break;
    case ENODEV:
      sbuf << clear << "ENODEV: No such device";
      break;
    case ENOTDIR:
      sbuf << clear << "ENOTDIR: Not a directory";
      break;
    case EISDIR:
      sbuf << clear << "EISDIR: Is a directory";
      break;
    case EINVAL:
      sbuf << clear << "EINVAL: Invalid argument";
      break;
    case ENFILE:
      sbuf << clear << "ENFILE: File table overflow";
      break;
    case EMFILE:
      sbuf << clear << "EMFILE: Too many open files";
      break;
    case ENOTTY:
      sbuf << clear << "ENOTTY: Not a typewriter";
      break;
    case ETXTBSY:
      sbuf << clear << "ETXTBSY: Text file busy";
      break;
    case EFBIG:
      sbuf << clear << "EFBIG: File too large";
      break;
    case ENOSPC:
      sbuf << clear << "ENOSPC: No space left on device";
      break;
    case ESPIPE:
      sbuf << clear << "ESPIPE: Illegal seek";
      break;
    case EROFS:
      sbuf << clear << "EROFS: Read-only file system";
      break;
    case EMLINK:
      sbuf << clear << "EMLINK: Too many links";
      break;
    case EPIPE:
      sbuf << clear << "EPIPE: Broken pipe";
      break;
    case EDOM:
      sbuf << clear << "EDOM: Math argument out of domain of func";
      break;
    case ERANGE:
      sbuf << clear << "ERANGE: Math result not representable";
      break;
    case EDEADLK:
      sbuf << clear << "EDEADLK: Resource deadlock would occur";
      break;
    case ENAMETOOLONG:
      sbuf << clear << "ENAMETOOLONG: File name too long";
      break;
    case ENOLCK:
      sbuf << clear << "ENOLCK: No record locks available";
      break;
    case ENOSYS:
      sbuf << clear << "ENOSYS: Function not implemented";
      break;
    case ENOTEMPTY:
      sbuf << clear << "ENOTEMPTY: Directory not empty";
      break;
    case ELOOP:
      sbuf << clear << "ELOOP: Too many symbolic links encountered";
      break;
    case EWOULDBLOCK:
      sbuf << clear << "EWOULDBLOCK: Operation would block";
      break;
    case ENOMSG:
      sbuf << clear << "ENOMSG: No message of desired type";
      break;
    case EIDRM:
      sbuf << clear << "EIDRM: Identifier removed";
      break;
    case ECHRNG:
      sbuf << clear << "ECHRNG: Channel number out of range";
      break;
    case EL2NSYNC:
      sbuf << clear << "EL2NSYNC: Level 2 not synchronized";
      break;
    case EL3HLT:
      sbuf << clear << "EL3HLT: Level 3 halted";
      break;
    case EL3RST:
      sbuf << clear << "EL3RST: Level 3 reset";
      break;
    case ELNRNG:
      sbuf << clear << "ELNRNG: Link number out of range";
      break;
    case EUNATCH:
      sbuf << clear << "EUNATCH: Protocol driver not attached";
      break;
    case ENOCSI:
      sbuf << clear << "ENOCSI: No CSI structure available";
      break;
    case EL2HLT:
      sbuf << clear << "EL2HLT: Level 2 halted";
      break;
    case EBADE:
      sbuf << clear << "EBADE: Invalid exchange";
      break;
    case EBADR:
      sbuf << clear << "EBADR: Invalid request descriptor";
      break;
    case EXFULL:
      sbuf << clear << "EXFULL: Exchange full";
      break;
    case ENOANO:
      sbuf << clear << "ENOANO: No anode";
      break;
    case EBADRQC:
      sbuf << clear << "EBADRQC: Invalid request code";
      break;
    case EBADSLT:
      sbuf << clear << "EBADSLT: Invalid slot";
      break;
    // case EDEADLOCK:
    //   sbuf << clear << "EDEADLOCK: Bad font file format";
    //   break;
    case ENOSTR:
      sbuf << clear << "ENOSTR: Device not a stream";
      break;
    case ENODATA:
      sbuf << clear << "ENODATA: No data available";
      break;
    case ETIME:
      sbuf << clear << "ETIME: Timer expired";
      break;
    case ENOSR:
      sbuf << clear << "ENOSR: Out of streams resources";
      break;
    case ENONET:
      sbuf << clear << "ENONET: Machine is not on the network";
      break;
    case ENOPKG:
      sbuf << clear << "ENOPKG: Package not installed";
      break;
    case EREMOTE:
      sbuf << clear << "EREMOTE: Object is remote";
      break;
    case ENOLINK:
      sbuf << clear << "ENOLINK: Link has been severed";
      break;
    case EADV:
      sbuf << clear << "EADV: Advertise error";
      break;
    case ESRMNT:
      sbuf << clear << "ESRMNT: Srmount error";
      break;
    case ECOMM:
      sbuf << clear << "ECOMM: Communication error on send";
      break;
    case EPROTO:
      sbuf << clear << "EPROTO: Protocol error";
      break;
    case EMULTIHOP:
      sbuf << clear << "EMULTIHOP: Multihop attempted";
      break;
    case EDOTDOT:
      sbuf << clear << "EDOTDOT: RFS specific error";
      break;
    case EBADMSG:
      sbuf << clear << "EBADMSG: Not a data message";
      break;
    case EOVERFLOW:
      sbuf << clear << "EOVERFLOW: Value too large for defined data type";
      break;
    case ENOTUNIQ:
      sbuf << clear << "ENOTUNIQ: Name not unique on network";
      break;
    case EBADFD:
      sbuf << clear << "EBADFD: File descriptor in bad state";
      break;
    case EREMCHG:
      sbuf << clear << "EREMCHG: Remote address changed";
      break;
    case ELIBACC:
      sbuf << clear << "ELIBACC: Can not access a needed shared library";
      break;
    case ELIBBAD:
      sbuf << clear << "ELIBBAD: Accessing a corrupted shared library";
      break;
    case ELIBSCN:
      sbuf << clear << "ELIBSCN: .lib section in a.out corrupted";
      break;
    case ELIBMAX:
      sbuf << clear << "ELIBMAX: Attempting to link in too many shared libraries";
      break;
    case ELIBEXEC:
      sbuf << clear << "ELIBEXEC: Cannot exec a shared library directly";
      break;
    case EILSEQ:
      sbuf << clear << "EILSEQ: Illegal byte sequence";
      break;
    case ERESTART:
      sbuf << clear << "ERESTART: Interrupted system call should be restarted";
      break;
    case ESTRPIPE:
      sbuf << clear << "ESTRPIPE: Streams pipe error";
      break;
    case EUSERS:
      sbuf << clear << "EUSERS: Too many users";
      break;
    case ENOTSOCK:
      sbuf << clear << "ENOTSOCK: Socket operation on non-socket";
      break;
    case EDESTADDRREQ:
      sbuf << clear << "EDESTADDRREQ: Destination address required";
      break;
    case EMSGSIZE:
      sbuf << clear << "EMSGSIZE: Message too long";
      break;
    case EPROTOTYPE:
      sbuf << clear << "EPROTOTYPE: Protocol wrong type for socket";
      break;
    case ENOPROTOOPT:
      sbuf << clear << "ENOPROTOOPT: Protocol not available";
      break;
    case EPROTONOSUPPORT:
      sbuf << clear << "EPROTONOSUPPORT: Protocol not supported";
      break;
    case ESOCKTNOSUPPORT:
      sbuf << clear << "ESOCKTNOSUPPORT: Socket type not supported";
      break;
    case EOPNOTSUPP:
      sbuf << clear << "EOPNOTSUPP: Operation not supported on transport endpoint";
      break;
    case EPFNOSUPPORT:
      sbuf << clear << "EPFNOSUPPORT: Protocol family not supported";
      break;
    case EAFNOSUPPORT:
      sbuf << clear << "EAFNOSUPPORT: Address family not supported by protocol";
      break;
    case EADDRINUSE:
      sbuf << clear << "EADDRINUSE: Address already in use";
      break;
    case EADDRNOTAVAIL:
      sbuf << clear << "EADDRNOTAVAIL: Cannot assign requested address";
      break;
    case ENETDOWN:
      sbuf << clear << "ENETDOWN: Network is down";
      break;
    case ENETUNREACH:
      sbuf << clear << "ENETUNREACH: Network is unreachable";
      break;
    case ENETRESET:
      sbuf << clear << "ENETRESET: Network dropped connection because of reset";
      break;
    case ECONNABORTED:
      sbuf << clear << "ECONNABORTED: Software caused connection abort";
      break;
    case ECONNRESET:
      sbuf << clear << "ECONNRESET: Connection reset by peer";
      break;
    case ENOBUFS:
      sbuf << clear << "ENOBUFS: No buffer space available";
      break;
    case EISCONN:
      sbuf << clear << "EISCONN: Transport endpoint is already connected";
      break;
    case ENOTCONN:
      sbuf << clear << "ENOTCONN: Transport endpoint is not connected";
      break;
    case ESHUTDOWN:
      sbuf << clear << "ESHUTDOWN: Cannot send after transport endpoint shutdown";
      break;
    case ETOOMANYREFS:
      sbuf << clear << "ETOOMANYREFS: Too many references: cannot splice";
      break;
    case ETIMEDOUT:
      sbuf << clear << "ETIMEDOUT: Connection timed out";
      break;
    case ECONNREFUSED:
      sbuf << clear << "ECONNREFUSED: Connection refused";
      break;
    case EHOSTDOWN:
      sbuf << clear << "EHOSTDOWN: Host is down";
      break;
    case EHOSTUNREACH:
      sbuf << clear << "EHOSTUNREACH: No route to host";
      break;
    case EALREADY:
      sbuf << clear << "EALREADY: Operation already in progress";
      break;
    case EINPROGRESS:
      sbuf << clear << "EINPROGRESS: Operation now in progress";
      break;
    case ESTALE:
      sbuf << clear << "ESTALE: Stale file handle";
      break;
    case EUCLEAN:
      sbuf << clear << "EUCLEAN: Structure needs cleaning";
      break;
    case ENOTNAM:
      sbuf << clear << "ENOTNAM: Not a XENIX named type file";
      break;
    case ENAVAIL:
      sbuf << clear << "ENAVAIL: No XENIX semaphores available";
      break;
    case EISNAM:
      sbuf << clear << "EISNAM: Is a named type file";
      break;
    case EREMOTEIO:
      sbuf << clear << "EREMOTEIO: Remote I/O error";
      break;
    case EDQUOT:
      sbuf << clear << "EDQUOT: Quota exceeded";
      break;
    case ENOMEDIUM:
      sbuf << clear << "ENOMEDIUM: No medium found";
      break;
    case EMEDIUMTYPE:
      sbuf << clear << "EMEDIUMTYPE: Wrong medium type";
      break;
    case ECANCELED:
      sbuf << clear << "ECANCELED: Operation Canceled";
      break;
    case ENOKEY:
      sbuf << clear << "ENOKEY: Required key not available";
      break;
    case EKEYEXPIRED:
      sbuf << clear << "EKEYEXPIRED: Key has expired";
      break;
    case EKEYREVOKED:
      sbuf << clear << "EKEYREVOKED: Key has been revoked";
      break;
    case EKEYREJECTED:
      sbuf << clear << "EKEYREJECTED: Key was rejected by service";
      break;
    case EOWNERDEAD:
      sbuf << clear << "EOWNERDEAD: Owner died";
      break;
    case ENOTRECOVERABLE:
      sbuf << clear << "ENOTRECOVERABLE: State not recoverable";
      break;
    case ERFKILL:
      sbuf << clear << "ERFKILL: Operation not possible due to RF-kill";
      break;
    case EHWPOISON:
      sbuf << clear << "EHWPOISON: Memory page has hardware error";
      break;
    default:
      sbuf << clear << "UNKNOWN: An unkown error occured";
      break;
  }
  return error_number;
}

// Return 0 for pass, 1 for fail
// error_level = -1 for fatal error
// error_level = 1 for socket write error
// error_level = 2 for read timeout error
// error_level = 3 for read error
int send_keep_alive(CMnode *n, int &error_level, int timeout_secs, int timeout_usecs)
{
  int error_number = 0;
  gxString message, sbuf;
  error_level = 0;
  gxSocket client(SOCK_DGRAM, servercfg->udpport, n->keep_alive_ip.c_str());
  if(!client) {
    if(servercfg->debug) {
      get_fd_socket_error(error_number, sbuf);
      message << clear << "ERROR - Fatal error starting CM keep alive client ENUM: " << error_number << " Message: " << sbuf;
      LogDebugMessage(message.c_str());
    }
    else {
      message << clear << "ERROR - Fatal error starting CM keep alive client";
      LogMessage(message.c_str());
    }
    error_level = -1; // Fatal error
    return 1;
  }

  CM_MessageHeader cmhdr;
  int hdr_size = sizeof(CM_MessageHeader);
  
  cmhdr.set_word(cmhdr.checkword, NET_CHECKWORD);
  memmove(cmhdr.sha1sum, servercfg->sha1sum, sizeof(cmhdr.sha1sum));
  cmhdr.set_word(cmhdr.cluster_command, CM_CMD_KEEPALIVE);

  int optVal = 1;
  client.SetSockOpt(SOL_SOCKET, SO_REUSEADDR, (char *)&optVal, sizeof(optVal));

  // Send the client header
  int bytes_moved = client.RawWriteTo(&cmhdr, sizeof(cmhdr));
  get_fd_socket_error(error_number, sbuf);
  if(bytes_moved != sizeof(cmhdr)) {
    if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
      message << clear << "ERROR - Error sending CM header to " << n->keep_alive_ip.c_str();
      LogDebugMessage(message.c_str());
      message << clear << "CM header send() Return: " << bytes_moved << " ENUM: " << error_number << " Message: " << sbuf;
      LogDebugMessage(message.c_str());
    }
    error_level = 1;
    return 1;
  }

  cmhdr.clear();
  int bytes_read = client.RemoteRecvFrom(&cmhdr, sizeof(cmhdr), timeout_secs, timeout_usecs);
  get_fd_socket_error(error_number, sbuf);
  if(bytes_read != sizeof(cmhdr)) {
    if(client.GetSocketError() == gxSOCKET_REQUEST_TIMEOUT) {
      if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
      message << clear << "ERROR - Receive timeout reading CM header from " << n->keep_alive_ip.c_str();
    }
      error_level = 2;
    }
    if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
      message << clear << "ERROR - Receiving CM header from " << n->keep_alive_ip.c_str();
      LogDebugMessage(message.c_str());
      message << clear << "CM header recvfrom() Return: " << bytes_read << " ENUM: " << error_number << " Message: " << sbuf;
      LogDebugMessage(message.c_str());
    }
    error_level = 3;
    return 1;
  }

  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
