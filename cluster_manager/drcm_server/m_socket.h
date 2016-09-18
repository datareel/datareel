// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
// Date Last Modified: 09/17/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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
#ifndef __DRCM_SOCKET_HPP__
#define __DRCM_SOCKET_HPP__

#include "gxdlcode.h"
#include "drcm_server.h"

const int NET_CHECKWORD = 0X1010FEFE;
const int DEFAULT_PORT = 53897;
const int DEFAULT_TIMEOUT = 300;
const int DEFAULT_TIMEOUT_US = 0;
const int DEFAULT_DATAGRAM_SIZE = 1500;

enum CM_CMD { // Cluster Manager Commands
  CM_CMD_NAK = 0,
  CM_CMD_ACK,
  CM_CMD_PING,
  CM_CMD_KEEPALIVE,
  CM_CMD_GETSTATS
};

const char DEFAULT_DATAGRAM_FILL = 'X';

struct CM_MessageHeader {
  CM_MessageHeader();
  ~CM_MessageHeader() { }
  CM_MessageHeader(const CM_MessageHeader &ob);
  CM_MessageHeader &operator=(const CM_MessageHeader &ob);

  void clear();
  void copy(const CM_MessageHeader &ob);
  int get_word(char bytes[4]);
  void set_word(char bytes[4], int val);

  char checkword[4];
  char bytes[4];
  char datagram_size[4];
  char cluster_command[4];
  char sha1sum[40];
};

// Client socket type used to associate client sockets other data types.
struct CMClientSocket_t
{
  CMClientSocket_t() {
    seq_num = 0;
    r_port = 1;
    client_socket = -1;
  }
  ~CMClientSocket_t() { }

  gxsSocket_t client_socket;
  int seq_num; // Sequence number for log entries
  gxString client_name;
  int r_port;
  CM_MessageHeader cmhdr;
};

// Send a keep alive message to a CM node
// Return 0 for pass, 1 for fail
// error_level = -1 for fatal error
// error_level = 1 for socket write error
// error_level = 2 for read timeout error
// error_level = 3 for read error
int send_keep_alive(CMnode *n, int &error_level, int timeout_secs = 1, int timeout_usecs = 0);


// Get string value for errno. Returns the error_number provided by caller
int fd_error_to_string(int error_number, gxString &sbuf);


#endif // __DRCM_SOCKET_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
