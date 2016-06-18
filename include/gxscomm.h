// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxscomm.h
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

The gxSerialComm class is a base class used to open a serial 
port for bidirectional communication. It includes several 
low-level functions needed by derived classes to initialize a 
serial port, transmit, and receive data.

Changes:
==============================================================
01/29/2002: Added the gxSerialComm::Close(scommDeviceHandle h)
function to allow application to close device handles other
than the one a gxSerialComm object is bound to. 

01/29/2002: Added the gxSerialComm::ReleaseSerialPort()
function to release the serial device bound in a gxSerialComm
object to another device. After a release call the gxSerialComm 
destructor will not automatically close the serial device 
opened by this object.

01/29/2002: Added four new read and write function to
read and write data from a specified device handle:
int RawRead(scommDeviceHandle h, void *buf, int bytes);
int RawWrite(scommDeviceHandle h, const void *buf, int bytes);
int Recv(scommDeviceHandle h, void *buf, int bytes);
int Send(scommDeviceHandle h, const void *buf, int bytes);

01/11/2003: Added the following line status member functions
used under WIN32 and UNIX for monitor RS232 control status:
int gxSerialComm::DCD_Active();
int gxSerialComm::RI_Active();
int gxSerialComm::DSR_Active();
int gxSerialComm::CTS_Active();

01/11/2003: Added the  void gxSerialComm::SetTimeouts()
functions used to set a timeout value.

01/11/2003: Added the gxSerialComm::oldtios data member 
for use under UNIX.

01/11/2003: Added the gxSerialComm::cto data member for 
for use under WIN32.

01/11/2003: Added the gxSerialComm::modem_line_status data 
memeber for use under WIN32 and UNIX platforms.

02/16/2003: Changed the "scommDeviceHandle h" argument to a 
non-constant reference in the gxSerialComm::Close(scommDeviceHandle &h) 
function. This change was made to allow this version of the 
close function to close the device handle and set it to 
INVALID_HANDLE_VALUE under WIN32 or -1 under UNIX.

02/18/2003: Added the "int ignore_brk" argument to all versions of
the gxSerialComm::InitSerialPort() function. This allows true binary
mode with no character interpretation. 

05/18/2003: Added buffer overflow exception to the
gxSerialComm error code enumeration.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_SCOMM_HPP__
#define __GX_SCOMM_HPP__

#include "gxdlcode.h"

// Include files
#if defined (__WIN32__)
#include <windows.h>
#elif defined (__UNIX__)
#include <unistd.h>  // UNIX standard function definitions 
#include <termios.h> // POSIX terminal control definitions 
#include <sys/ioctl.h>
#include <string.h>
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

// Type definitions
#if defined (__WIN32__)
typedef HANDLE scommDeviceHandle;
#elif defined (__UNIX__)
typedef int scommDeviceHandle;
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

// Serial communications base class
class GXDLCODE_API gxSerialComm
{
public: // Enumerations
  enum {
    // Internal error codes used to report the last error
    scomm_NO_ERROR  = 0,      // No errors reported
    scomm_INVALID_ERROR_CODE, // Invalid error code

    scomm_BAUDRATE_ERROR,    // Invalid baud rate
    scomm_BUFFER_OVERFLOW,   // Buffer overflow error
    scomm_CS_ERROR,          // Invalid character size
    scomm_FLOWCONTROL_ERROR, // Invalid flow control
    scomm_INIT_ERROR,        // Cannot initialize serial device
    scomm_INVALIDPARM,       // Invalid initialization parameter
    scomm_OPEN_ERROR,        // Cannot open serial device
    scomm_PARITY_ERROR,      // Invalid parity
    scomm_RECEIVE_ERROR,     // Serial device receive error
    scomm_STOPBIT_ERROR,     // Invalid stop bit
    scomm_TRANSMIT_ERROR,    // Serial device transmit error

    // Exception codes added to handle database block errors
    scomm_BLOCKACK_ERROR,    // Database block acknowledgment error
    scomm_BLOCKHEADER_ERROR, // Bad database block header
    scomm_BLOCKSIZE_ERROR,   // Bad database block size
    scomm_BLOCKSYNC_ERROR    // Database block synchronization error
  };
  
  enum {
    // Flow control constants
    scommHARD_FLOW,
    scommSOFT_FLOW,
    scommXON_XOFF,
    scommNO_FLOW_CONTROL,

    // Device access constants
    scommREAD_ONLY,
    scommWRITE_ONLY,
    scommREAD_WRITE
  };

public:
  gxSerialComm();
  virtual ~gxSerialComm();
  
public:
  int DCD_Active();
  int RI_Active();
  int DSR_Active();
  int CTS_Active();
  unsigned long GetLineStatus();
  int OpenSerialPort(char *device_name);
  int InitSerialPort(int ignore_brk = 0);
  int InitSerialPort(char *device_name, int sp = 9600, char pr = 'N',
		     int cs = 8, int sb = 1,
		     int flow = gxSerialComm::scommNO_FLOW_CONTROL,
		     int bin_mode = 1, int ignore_brk = 0);
  void Close();
  void Close(scommDeviceHandle &h);
  int RawRead(scommDeviceHandle h, void *buf, int bytes);
  int RawWrite(scommDeviceHandle h, const void *buf, int bytes);
  int Recv(scommDeviceHandle h, void *buf, int bytes);
  int Send(scommDeviceHandle h, const void *buf, int bytes);
  int RawRead(void *buf, int bytes);
  int RawWrite(const void *buf, int bytes);
  int Recv(void *buf, int bytes);
  int Send(const void *buf, int bytes);

  void SetBaudRate(int br) { baud_rate = br; }
  void SetCharacterSize(int cs) { character_size = cs; }
  void SetParity(char p) { parity = p; }
  void SetStopBits(int sb) { stop_bits = sb; }
  void SetFlowControl(int f) { flow_control = f; }
  scommDeviceHandle DeviceHandle() { return device_handle; }
  void ReleaseSerialPort();
  
  // NOTE: The Win32 API does not support non-binary mode transfers
  void BinaryMode() { binary_mode = 1; }
  void CharacterMode() { binary_mode = 0; }

  // Exception handling functions
  int GetSerialCommError() { return scomm_error; }
  int GetSerialCommError() const { return scomm_error; }
  void SetSerialCommError(int err) { scomm_error = err; }
  void ResetSerialCommError() { scomm_error = scomm_NO_ERROR; }
  void ResetError() { scomm_error = scomm_NO_ERROR; }
  const char *SerialCommExceptionMessage();
  
#if defined (__UNIX__)
  void SetTimeouts(termios *newtios) {
    memcpy(&options.c_cc, &newtios->c_cc, sizeof(options.c_cc)); 
  }
  termios *GetTermIOS() { return &options; }
  int BytesRead() { return bytes_read; }
  int BytesMoved() { return bytes_moved; }
#endif

#if defined (__WIN32__)
  void SetTimeouts(COMMTIMEOUTS *pCTO) { memcpy(&cto, pCTO, sizeof(cto)); }
  DCB *GetDeviceCB() { return &dcb; }
  DWORD BytesRead() { return bytes_read; }
  OVERLAPPED *Overlap() { return &ov; }
  DWORD BytesMoved() { return bytes_moved; }
  DWORD gxsGetLastError() { return last_err; }
#endif

protected:
  scommDeviceHandle device_handle; // Device handle for the port
  int baud_rate;      // Baud rate
  int character_size; // Character size
  char parity;        // Parity 
  int stop_bits;      // Stop bits
  int flow_control;   // Flow control
  int binary_mode;    // True to enable raw reads and raw writes
  int scomm_error;    // The last reported serial port error
  
protected: // POSIX terminal (serial) interface extensions
#if defined (__UNIX__)
  termios oldtios; // Terminal control structure (backup)
  termios options; // Terminal control structure
  int bytes_read;  // Number of bytes read following a Read() call
  int bytes_moved; // Number of bytes written following a Write() call
  int modem_line_status;
#endif
  
protected: // WIN32 extensions
#if defined (__WIN32__)
  COMMTIMEOUTS cto;  // Device's timeout settings
  DCB dcb;           // Device-control block structure
  DWORD bytes_read;  // Address of number of bytes read following Read()
  OVERLAPPED ov;     // Pointer to structure needed for overlapped I/O  
  DWORD bytes_moved; // Pointer to number of bytes written following Write()
  DWORD last_err;    // Used to store the last error reported
  unsigned long modem_line_status;
#endif
};

#endif // __GX_SCOMM_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
