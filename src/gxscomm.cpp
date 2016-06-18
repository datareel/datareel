// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxscomm.cpp
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

The gxSerialComm class is a base class used to open a serial 
port for bidirectional communication. It includes several 
low-level functions needed by derived classes to initalize a 
serial port, transmit, and receive data.

Changes:
==============================================================
07/10/2001: UNIX changes: Removed the 57600 and 115200 case 
labels to maintain compatibility with all UNIX variants that 
do not define baud rates higher then 19.2

09/20/2001: Corrected the gxSerialComm::RawRead() and RawWrite()
WINNT/2000 overlapped I/O errors by creating an overlapped I/O
event for every read and write call. This change does not affect
Windows 95/98 or UNIX read and write calls.

01/29/2002: Added the gxSerialComm::Close(scommDeviceHandle h)
function to allow application to close device handles other
than the one a gxSerialComm object is bound to.

01/29/2002: Modified the gxSerialComm::Close() function to 
test the device handle before closing the device. 

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

05/05/2002: Changed the WIN32 scommHARD_FLOW control option in 
the gxSerialComm::InitSerialPort() function from dcb.fRtsControl 
= DTR_CONTROL_HANDSHAKE; to dcb.fRtsControl = RTS_CONTROL_ENABLE;

01/11/2003: Modified the gxSerialComm::gxSerialComm() constructor
to initialize the COMMTIMEOUTS data structure under WIN32.

01/11/2003: Modified the gxSerialComm::Close() functions to 
restore the original terminal I/O values.

01/11/2003: Modified the gxSerialComm::OpenSerialPort() function
to save the original terminal I/O values.

01/11/2003: Modified the scommHARD_FLOW WIN32 case statement in
the gxSerialComm::InitSerialPort() function.

01/11/2003: Modified the gxSerialComm::InitSerialPort() function
to support baud rates higher than 19200 under Linux. To enable
baud rates greater than 19200 define one or all of the following
preprocessor directives:
__LINUX_BAUD_RATE_38400_SUPPORT__
__LINUX_BAUD_RATE_57600_SUPPORT__
__LINUX_BAUD_RATE_115200_SUPPORT__
__LINUX_BAUD_RATE_230400_SUPPORT__
__LINUX_BAUD_RATE_460800_SUPPORT__

01/11/2003: Added the following line status member functions
used under WIN32 and UNIX for monitor RS232 control status:
int gxSerialComm::DCD_Active();
int gxSerialComm::RI_Active();
int gxSerialComm::DSR_Active();
int gxSerialComm::CTS_Active();

02/09/2003: Modified the gxSerialComm::InitSerialPort() function
to support baud rates higher than 19200 under UNIX. To enable
baud rates greater than 19200 define one or all of the following
preprocessor directives:
__BAUD_RATE_38400_SUPPORT__
__BAUD_RATE_57600_SUPPORT__
__BAUD_RATE_115200_SUPPORT__
__BAUD_RATE_230400_SUPPORT__
__BAUD_RATE_460800_SUPPORT__

02/16/2003: Modified the gxSerialComm::Close() function to set the 
gxSerialComm::device_handle variable to INVALID_HANDLE_VALUE after 
closing the device handle under WIN32. This change was made to prevent 
an invalid handle exception in the ntdll.dll. 

02/16/2003: Modified the gxSerialComm::Close() function to set the 
gxSerialComm::device_handle variable to -1 after closing the device 
handle under UNIX.

05/18/2003: Added buffer overflow exception to the
gxsSerialCommExceptionMessages[] character array.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__UNIX__)
#include <fcntl.h>   // File control definitions
#endif

#if defined (__HPUX__)
#include <sys/modem.h> // Modem control line definitions
#endif

#include "gxscomm.h" 

#ifdef __BCC32__
#pragma warn -8057
#pragma warn -8071
#endif

// NOTE: This array must contain the same number of exceptions as the
// internal error code enumeration. 
const int gxsMaxSCommExceptionMessages = 18;
const char *gxsSerialCommExceptionMessages[gxsMaxSCommExceptionMessages] = {
  "Serial port exception: No errors reported",               // NO_ERROR
  "Serial port exception: Invalid error code",               // INVALID_CODE
  "Serial port exception: Invalid baud rate",                // BAUDRATE_ERROR
  "Serial port exception: Buffer overflow",                  // BUFFER_OVERFLOW
  "Serial port exception: Invalid character size",           // CS_ERROR
  "Serial port exception: Invalid flow control",             // FLOWCONTROL
  "Serial port exception: Cannot initialize serial device",  // INIT_ERROR
  "Serial port exception: Invalid initialization parameter", // INVALIDPARM
  "Serial port exception: Cannot open serial device",        // OPEN_ERROR
  "Serial port exception: Invalid parity",                   // PARITY_ERROR
  "Serial port exception: Serial device receive error",      // RECEIVE_ERROR
  "Serial port exception: Invalid stop bit",                 // STOPBIT_ERROR
  "Serial port exception: Serial device transmit error",     // TRANSMIT_ERROR

  // Variable block exception messages
  "Serial port exception: Database block acknowledgment error", // BLOCKACK
  "Serial port exception: Bad database block header",           // BLOCKHEADER
  "Serial port exception: Bad database block size",             // BLOCKSIZE
  "Serial port exception: Database block synchronization error" // BLOCKSYNC
};

gxSerialComm::gxSerialComm()
{
  // Default setting 
  baud_rate = 9600;
  parity = 'N';
  character_size = 8;
  stop_bits = 1;
  flow_control = gxSerialComm::scommNO_FLOW_CONTROL;
  binary_mode = 1; 
  scomm_error = gxSerialComm::scomm_NO_ERROR;

#if defined (__WIN32__)
  memset(&dcb, 0, sizeof(dcb));
# ifdef COMMENT_ONLY
  /*
    ReadIntervalTimeout 
    Specifies the maximum time, in milliseconds, allowed to elapse between 
    the arrival of two characters on the communications line. 
    During a ReadFile operation, the time period begins when the first 
    character is received. If the interval between the arrival of any 
    two characters exceeds this amount, the ReadFile operation is completed 
    and any buffered data is returned. A value of zero indicates that interval 
    time-outs are not used. 
    A value of MAXDWORD, combined with zero values for both the 
    ReadTotalTimeoutConstant and ReadTotalTimeoutMultiplier members, 
    specifies that the read operation is to return immediately with the 
    characters that have already been received, even if no characters have 
    been received. 
    
    ReadTotalTimeoutMultiplier 
    Specifies the multiplier, in milliseconds, used to calculate the total 
    time-out period for read operations. For each read operation, this value 
    is multiplied by the requested number of bytes to be read. 
    
    ReadTotalTimeoutConstant 
    Specifies the constant, in milliseconds, used to calculate the total 
    time-out period for read operations. For each read operation, this value 
    is added to the product of the ReadTotalTimeoutMultiplier member and the 
    requested number of bytes. 
    A value of zero for both the ReadTotalTimeoutMultiplier and 
    ReadTotalTimeoutConstant members indicates that total time-outs are not 
    used for read operations. 
    
    WriteTotalTimeoutMultiplier 
    Specifies the multiplier, in milliseconds, used to calculate the total 
    time-out period for write operations. For each write operation, this value 
    is multiplied by the number of bytes to be written. 
    
    WriteTotalTimeoutConstant 
    Specifies the constant, in milliseconds, used to calculate the total 
    time-out period for write operations. For each write operation, this value 
    is added to the product of the WriteTotalTimeoutMultiplier member and the 
    number of bytes to be written. 
    A value of zero for both the WriteTotalTimeoutMultiplier and 
    WriteTotalTimeoutConstant members indicates that total time-outs are not 
    used for write operations. 
  */
# endif // of ifdef COMMENT_ONLY
  
  cto.ReadIntervalTimeout = 250 ; 
  cto.ReadTotalTimeoutMultiplier = 1 ; 
  cto.ReadTotalTimeoutConstant = 1 ; 
  cto.WriteTotalTimeoutMultiplier = 250 ; 
  cto.WriteTotalTimeoutConstant = 1 ; 

  ZeroMemory(&ov, sizeof(ov));
#endif

#if defined (__UNIX__)
  // Initialize the terminal I/O control structures
  memset(&options, 0, sizeof(options));
  memset(&oldtios, 0, sizeof(oldtios));
#endif

#if defined (__PCLINT_CHECK__)
  // PC-lint 05/02/2005: Initalization warning can be ignored
  device_handle = 0;
  bytes_read = 0;
  bytes_moved = 0;
  last_err = 0;
  modem_line_status = 0;
#endif
}

gxSerialComm::~gxSerialComm()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  Close();
}

void gxSerialComm::ReleaseSerialPort()
// This function is used to release the serial device
// bound in this object to another device. After a
// release call the gxSerialComm destructor will not
// automatically close the serial device opened by
// this object.
{
#if defined (__WIN32__)
  device_handle = INVALID_HANDLE_VALUE;
#elif defined (__UNIX__)
  device_handle = -1;
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
}

int gxSerialComm::OpenSerialPort(char *device_name)
// Open a serial device for read/write operations. 
{
#if defined (__WIN32__)
  // Open com ports 1 through 9 for read/write access.
  // Valid device names for Windows are: COM1, COM2, etc.
  device_handle = CreateFile(device_name,
			     GENERIC_READ | GENERIC_WRITE,
			     FILE_SHARE_READ | FILE_SHARE_WRITE,
			     NULL,
			     OPEN_EXISTING,
			     FILE_FLAG_OVERLAPPED,
			     NULL);
  
  if(device_handle == INVALID_HANDLE_VALUE) {
    scomm_error = gxSerialComm::scomm_OPEN_ERROR;
    return -1;
  }
  return scommREAD_WRITE; // Device open for read/write access

#elif defined (__UNIX__)
  // NOTE: UNIX systems will not allow you to open tty devices
  // for both reading and writing unless run the program as root
  // or the owner of that device file. The permissions for TTY
  // devices defaults to crw--w--w- or crw-r--r--. This function
  // will try to open the device for read/write, read, and then write.
  // The O_NOCTTY bit is set so that no controlling tty is assigned.
  // The O_NDELAY bit is set to enable nonblocking mode for this
  // device. This will allow all read operations to return if there
  // is no input immediately available. NOTE: If the O_NONBLOCK bit 
  // is set write operations will not work properly under HPUX 10.20.
  device_handle = open(device_name, O_RDWR | O_NOCTTY | O_NDELAY);

  if(device_handle > 0) {
    tcgetattr(device_handle, &oldtios); // Save current port settings
  }

  if(device_handle < 0) {
    device_handle = open(device_name, O_RDONLY | O_NOCTTY | O_NDELAY);
  }
  else {
    return scommREAD_WRITE;
  }
  
  if(device_handle < 0) {
    device_handle = open(device_name, O_WRONLY | O_NOCTTY | O_NDELAY);
  }
  else { 
    tcgetattr(device_handle, &oldtios); // Save current port settings
    return scommREAD_ONLY;
  }

  if(device_handle < 0) {
    scomm_error = gxSerialComm::scomm_OPEN_ERROR;
    return -1;
  }
  else {
   return scommWRITE_ONLY;
  }

#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
}

int gxSerialComm::InitSerialPort(int ignore_brk)
{
#if defined (__WIN32__)
  // Set timeouts
# ifdef COMMENT_ONLY
  // units = ms
  typedef struct _COMMTIMEOUTS {  
    DWORD ReadIntervalTimeout; 
    DWORD ReadTotalTimeoutMultiplier; 
    DWORD ReadTotalTimeoutConstant; 
    DWORD WriteTotalTimeoutMultiplier; 
    DWORD WriteTotalTimeoutConstant; 
   } COMMTIMEOUTS,*LPCOMMTIMEOUTS; 
# endif // of ifdef COMMENT_ONLY

  // SetCommTimeouts failed
  if(!SetCommTimeouts(device_handle, &cto)) {
    scomm_error = gxSerialComm::scomm_INIT_ERROR;
    return -1;
  }

  // Set the device-control block structure
  dcb.DCBlength = sizeof(dcb);

  // Set the baud rates
  switch(baud_rate) {
    case 110: 
      dcb.BaudRate = CBR_110;
      break;
    case 300:
      dcb.BaudRate = CBR_300;
      break;
    case 600: 
      dcb.BaudRate = CBR_600;
      break;
    case 1200: 
      dcb.BaudRate = CBR_1200;
      break;
    case 2400: 
      dcb.BaudRate = CBR_2400;
      break;
    case 4800: 
      dcb.BaudRate = CBR_4800;
      break;
    case 9600: 
      dcb.BaudRate = CBR_9600;
      break;
    case 14400: 
      dcb.BaudRate = CBR_14400;
      break;
    case 19200: 
      dcb.BaudRate = CBR_19200;
      break;
    case 38400: 
      dcb.BaudRate = CBR_38400;
      break;
    case 56000: 
      dcb.BaudRate = CBR_56000;
      break;
    case 57600: 
      dcb.BaudRate = CBR_57600;
      break;
    case 115200: 
      dcb.BaudRate = CBR_115200;
      break;
    case 128000: 
      dcb.BaudRate = CBR_128000;
      break;
    case 256000: 
      dcb.BaudRate = CBR_256000;
      break;
    default:
      scomm_error = gxSerialComm::scomm_BAUDRATE_ERROR;
      return -1; // Invalid baud rate
  }

  // Set the parity
  switch(parity) {
    case 'n' : case 'N' :
      dcb.Parity = NOPARITY;
      break;
    case 'e' : case 'E' :
      dcb.Parity = EVENPARITY;
      break;
    case 'o' : case 'O' :
      dcb.Parity = ODDPARITY;
      break;
    case 'm' : case 'M' :
      dcb.Parity = MARKPARITY;
      break;
    case 's' : case 'S' :
      // Space parity is setup the same as no parity 
      dcb.Parity = NOPARITY;
      break;
    default:
      scomm_error = gxSerialComm::scomm_PARITY_ERROR;
      return -1; // Invalid parity
  }
    
  // Set the stop bits
  switch(stop_bits) {
    case 1 :
      dcb.StopBits = ONESTOPBIT;
      break;
    case 15 :
      dcb.StopBits = ONE5STOPBITS;
      break;
    case 2 :
      dcb.StopBits = TWOSTOPBITS;
      break;
    default:
      scomm_error = gxSerialComm::scomm_STOPBIT_ERROR;
      return -1; // Invalid stop bit
  }
  
  // Set the character size
  switch(character_size) {
    case 7 :
    dcb.ByteSize = 7;
      break;
    case 8 :
    dcb.ByteSize = 8;
      break;
    default:
      scomm_error = gxSerialComm::scomm_CS_ERROR;
      return -1; // Invalid character size
  }
  
  // NOTE: The Win32 API does not support non-binary mode transfers
  dcb.fBinary = binary_mode;

  switch(flow_control) {
    case scommHARD_FLOW :
#if 1
      dcb.fRtsControl  = RTS_CONTROL_ENABLE;
      dcb.fOutxCtsFlow = 0;
      dcb.fDtrControl  = DTR_CONTROL_HANDSHAKE;
      dcb.fOutxDsrFlow = 1;
#else
      dcb.fDtrControl = DTR_CONTROL_ENABLE;
      dcb.fOutxCtsFlow = 1;
      dcb.fRtsControl  = DTR_CONTROL_HANDSHAKE;
#endif
      break;
    case scommXON_XOFF: {
      // Ignore BCC32 conversion warnings
      int DC1 = 17; char xon = DC1;
      int DC2 = 19; char xoff = DC2;
      dcb.XonChar = xon;
      dcb.XoffChar = xoff;
      dcb.XonLim = 1024;
      dcb.XoffLim = 1024;
      dcb.fDtrControl = DTR_CONTROL_DISABLE;
      dcb.fOutxCtsFlow = 0;
      dcb.fRtsControl = RTS_CONTROL_DISABLE;
      dcb.fOutxDsrFlow = 0;
    }
    break;
    case scommSOFT_FLOW :
      dcb.fDtrControl = DTR_CONTROL_DISABLE;
      dcb.fOutxCtsFlow = 0;
      dcb.fRtsControl = RTS_CONTROL_DISABLE;
      dcb.fOutxDsrFlow = 0;
      break;
    case scommNO_FLOW_CONTROL :
      dcb.fDtrControl = DTR_CONTROL_DISABLE;
      dcb.fOutxCtsFlow = 0;
      dcb.fRtsControl = RTS_CONTROL_DISABLE;
      dcb.fOutxDsrFlow = 0;
      break;
    default:
      scomm_error = gxSerialComm::scomm_FLOWCONTROL_ERROR;
      return -1; // Invalid flow control
  }

  // SetCommState failed
  if(!SetCommState(device_handle, &dcb)) {
    scomm_error = gxSerialComm::scomm_INIT_ERROR;
    return -1;
  }
  
#elif defined (__UNIX__)
  // Reset the port options
  memset(&options, 0, sizeof(options));

  // Set the baud rates
  switch(baud_rate) {
    case 0: // 0 baud (drop DTR)
      cfsetispeed(&options, B0);
      cfsetospeed(&options, B0);
      break;
    case 50: 
      cfsetispeed(&options, B50);
      cfsetospeed(&options, B50);
      break;
    case 75: 
      cfsetispeed(&options, B75);
      cfsetospeed(&options, B75);
      break;
    case 110: 
      cfsetispeed(&options, B110);
      cfsetospeed(&options, B110);
      break;
    case 134: 
      cfsetispeed(&options, B134);
      cfsetospeed(&options, B134);
      break;
    case 150: 
      cfsetispeed(&options, B150);
      cfsetospeed(&options, B150);
      break;
    case 200: 
      cfsetispeed(&options, B200);
      cfsetospeed(&options, B200);
      break;
    case 300: 
      cfsetispeed(&options, B300);
      cfsetospeed(&options, B300);
      break;
    case 600: 
      cfsetispeed(&options, B600);
      cfsetospeed(&options, B600);
      break;
    case 1200: 
      cfsetispeed(&options, B1200);
      cfsetospeed(&options, B1200);
      break;
    case 1800: 
      cfsetispeed(&options, B1800);
      cfsetospeed(&options, B1800);
      break;
    case 2400: 
      cfsetispeed(&options, B2400);
      cfsetospeed(&options, B2400);
      break;
    case 4800: 
      cfsetispeed(&options, B4800);
      cfsetospeed(&options, B4800);
      break;
    case 9600: 
      cfsetispeed(&options, B9600);
      cfsetospeed(&options, B9600);
      break;
    case 19200: 
      cfsetispeed(&options, B19200);
      cfsetospeed(&options, B19200);
      break;
    case 38400: 
#if defined (__LINUX_BAUD_RATE_38400_SUPPORT__) || defined (__BAUD_RATE_38400_SUPPORT__)
      cfsetispeed(&options, B38400);
      cfsetospeed(&options, B38400);
      break;
#else
      scomm_error = gxSerialComm::scomm_BAUDRATE_ERROR;
      return -1; // Invalid baud rate
#endif
    case 57600: 
#if defined (__LINUX_BAUD_RATE_57600_SUPPORT__) || defined (__BAUD_RATE_57600_SUPPORT__)
      cfsetispeed(&options, B57600);
      cfsetospeed(&options, B57600);
      break;
#else
      scomm_error = gxSerialComm::scomm_BAUDRATE_ERROR;
      return -1; // Invalid baud rate
#endif
    case 115200: 
#if defined (__LINUX_BAUD_RATE_115200_SUPPORT__) || defined (__BAUD_RATE_115200_SUPPORT__)
      cfsetispeed(&options, B115200);
      cfsetospeed(&options, B115200);
      break ;
#else
      scomm_error = gxSerialComm::scomm_BAUDRATE_ERROR;
      return -1; // Invalid baud rate
#endif
    case 230400: 
#if defined (__LINUX_BAUD_RATE_230400_SUPPORT__) || defined (__BAUD_RATE_230400_SUPPORT__)
      cfsetispeed(&options, B230400);
      cfsetospeed(&options, B230400);
      break ;
#else
      scomm_error = gxSerialComm::scomm_BAUDRATE_ERROR;
      return -1; // Invalid baud rate
#endif
    case 460800: 
#if defined (__LINUX_BAUD_RATE_460800_SUPPORT__) || defined (__BAUD_RATE_460800_SUPPORT__)
      cfsetispeed(&options, B460800);
      cfsetospeed(&options, B460800);
      break;
#else
      scomm_error = gxSerialComm::scomm_BAUDRATE_ERROR;
      return -1; // Invalid baud rate
#endif
    default:
      scomm_error = gxSerialComm::scomm_BAUDRATE_ERROR;
      return -1; // Invalid baud rate
  }

  // Set the character size, parity and stop bits
  if((character_size == 8) && (parity == 'N') && (stop_bits == 1)) {
    // No parity (8N1) 
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_iflag = IGNPAR;
  }
  else if((character_size == 7) && (parity == 'E') && (stop_bits == 1)) {
    // Even parity (7E1) 
    options.c_cflag |= PARENB;
    options.c_cflag &= ~PARODD;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS7;
    options.c_iflag |= (INPCK | ISTRIP);
  }
  else if((character_size == 7) && (parity == 'O') && (stop_bits == 1)) {
    // Odd parity (7O1) 
    options.c_cflag |= PARENB;
    options.c_cflag |= PARODD;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS7;
    options.c_iflag |= (INPCK | ISTRIP);
  }
  else if((character_size == 7) && (parity == 'M') && (stop_bits == 1)) {
    // Mark parity is simulated by using 2 stop bits (7M1)
    options.c_cflag &= ~PARENB;
    options.c_cflag |= CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS7;
    options.c_iflag |= (INPCK | ISTRIP);
  }
  else if((character_size == 7) && (parity == 'S') && (stop_bits == 1)) {
    // Space parity is setup the same as no parity (7S1)
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS7;
    options.c_iflag |= (INPCK | ISTRIP);
  }
  else {
    scomm_error = gxSerialComm::scomm_INVALIDPARM;
    return -1; // Invalid character size, parity and stop bits combination
  }
  
  switch(flow_control) {
    case scommHARD_FLOW :
#if defined(CRTSCTS)
      options.c_cflag |= CRTSCTS;
      break;
#else
      break; // Hard flow control is not supported
#endif

    case scommXON_XOFF :
      options.c_iflag |= (IXON | IXOFF | IXANY);
#if defined(CRTSCTS)
      options.c_cflag &= ~CRTSCTS;
#endif
      break;

    case scommSOFT_FLOW :
#if defined(CRTSCTS)
      options.c_cflag &= ~CRTSCTS;
      break;
#else
      break; // Hard flow control is not supported
#endif

    case scommNO_FLOW_CONTROL :
#if defined(CRTSCTS)
      options.c_cflag &= ~CRTSCTS;
      break;
#else
      break; // Hard flow control is not supported
#endif

    default:
      scomm_error = gxSerialComm::scomm_FLOWCONTROL_ERROR;
      return -1; // Invalid flow control
  }

  if(!binary_mode) { // Set the port for canonical input (line-oriented) 
    // Input characters are put into a buffer until a CR (carriage return)
    // or LF (line feed) character is received.
    options.c_lflag |= (ICANON | ECHOE);

    // Postprocess the output.
    // The ONLCR flag will map NL (linefeeds) to CR-NL on output.
    // The OLCUC flag will map characters to upper case for tty output.
    options.c_oflag = OPOST | ONLCR | OLCUC;
  }
  else { // Use raw input/output
    // Input characters are passed through exactly as they are received,
    // when they are received.
    options.c_lflag = 0;
    options.c_oflag = 0;
    if(ignore_brk) {
      options.c_iflag |= IGNBRK; // Ignore break's
    }
  }
  
  // Enable the receiver and set local mode
  options.c_cflag |= (CLOCAL | CREAD);
  
  // Initialize control characters if needed.
  // Default values can be found in /usr/include/termios.h, and are given
  // in the comments.

  options.c_cc[VTIME]    = 0;     // inter-character timer unused 
  options.c_cc[VMIN]     = 1;     // blocking read until 1 character arrives 

  // options.c_cc[VEOF]     = 4;     // Ctrl-d  
  // options.c_cc[VINTR]    = 0;     // Ctrl-c 
  // options.c_cc[VQUIT]    = 0;     // Ctrl-backslash 
  // options.c_cc[VERASE]   = 0;     // del 
  // options.c_cc[VKILL]    = 0;     // @ 
  // options.c_cc[VSTART]   = 0;     // Ctrl-q 
  // options.c_cc[VSTOP]    = 0;     // Ctrl-s 
  // options.c_cc[VSUSP]    = 0;     // Ctrl-z 
  // options.c_cc[VEOL]     = 0;     // '\0' 
  // options.c_cc[VWERASE]  = 0;     // Ctrl-w 
  // options.c_cc[VLNEXT]   = 0;     // Ctrl-v 
  // options.c_cc[VEOL2]    = 0;     // '\0' 

  // May not be defined in every UNIX variant
  // options.c_cc[VSWTC]    = 0;     // '\0' 
  // options.c_cc[VREPRINT] = 0;     // Ctrl-r 
  // options.c_cc[VDISCARD] = 0;     // Ctrl-u 
  
#if defined (FASYNC) // ASYNC I/O is a BDS feature
  // Make the file descriptor asynchronous
  fcntl(device_handle, F_SETFL, FASYNC);
#endif

  // Set the new options for the port. The TCSANOW constant specifies
  // that all changes should occur immediately without waiting for
  // output data to finish sending or input data to finish receiving. 
  tcflush(device_handle, TCIFLUSH); // Clean the serial line
  tcsetattr(device_handle, TCSANOW, &options);

#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

return 1; // No errors reported
}

void gxSerialComm::Close()
{
// Type definitions
#if defined (__WIN32__)
  if(device_handle != INVALID_HANDLE_VALUE) CloseHandle(device_handle);
  device_handle = INVALID_HANDLE_VALUE;
#elif defined (__UNIX__)
  if(device_handle != -1) {
    // Restore old settings if the device handle is still open
    tcflush(device_handle, (TCIFLUSH | TCOFLUSH));
    tcsetattr(device_handle, TCSANOW, &oldtios);
    
    // Close the device handle
    close(device_handle);
  }
  device_handle = -1;
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
}

void gxSerialComm::Close(scommDeviceHandle &h) 
{
#if defined (__WIN32__)
  if(h != INVALID_HANDLE_VALUE) CloseHandle(h);
  h = INVALID_HANDLE_VALUE;
#elif defined (__UNIX__)
  if(h != -1) close(h);
  h = -1;
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
}

int gxSerialComm::InitSerialPort(char *device_name, int sp, char pr, int cs,
				 int sb, int flow, int bin_mode, int ignore_brk)
// Initialize a serial device using the specified parameters. Returns
// -1 if an error occurred during initialization or the current access
// mode of the port (scommREAD_WRITE, scommREAD_ONLY, scommREAD_WRITE).
{
  int status = OpenSerialPort(device_name); 
  if(status < 0) return -1;

  SetBaudRate(sp);
  SetCharacterSize(cs);
  SetParity(pr);
  SetStopBits(sb);
  SetFlowControl(flow);
  if(bin_mode) BinaryMode(); else CharacterMode();

  if(InitSerialPort(ignore_brk) < 0) return -1;
  
  return status;
}

const char *gxSerialComm::SerialCommExceptionMessage()
// Returns a null terminated string that can
// be use to log or print a serial port exception.
{
  if(scomm_error > (gxsMaxSCommExceptionMessages-1))
    scomm_error = gxSerialComm::scomm_INVALID_ERROR_CODE;
  
  // Find the corresponding message in the exception array
  return gxsSerialCommExceptionMessages[scomm_error];
}

int gxSerialComm::RawRead(void *buf, int bytes)
// Read a specified number of bytes from the serial port
// and return whether or not the read was completed.
// Returns the number of bytes received or -1 if an
// error occurred.
{
  return RawRead(device_handle, buf, bytes);
}

int gxSerialComm::RawWrite(const void *buf, int bytes)
// Write a specified number of bytes to a serial port
// and return whether or not the write was complete.
// Returns the total number of bytes moved or -1 if
// an error occurred.
{
  return RawWrite(device_handle, buf, bytes);
}

int gxSerialComm::Recv(void *buf, int bytes) 
// Receive a specified number of bytes from a serial port
// and do not return until all the byte have been read.
// Returns the total number of bytes read or -1 if an
// error occurred.
{
  return Recv(device_handle, buf, bytes);
}

int gxSerialComm::Send(const void *buf, int bytes)
// Write a specified number of bytes to a serial port and do
// not return until all the bytes have been written. Returns
// the total number of bytes written or -1 if an error occurred.
{
  return Send(device_handle, buf, bytes);
}

int gxSerialComm::RawRead(scommDeviceHandle h, void *buf, int bytes)
// Read a specified number of bytes from a device handle
// and return whether or not the read was completed.
// Returns the number of bytes received or -1 if an
// error occurred.
{
#if defined (__WIN32__)
  OVERLAPPED r_ov;
  ZeroMemory(&r_ov, sizeof(r_ov));
  
  // Create event for overlapped I/O
  r_ov.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  
  if(r_ov.hEvent == INVALID_HANDLE_VALUE) {
    scomm_error = gxSerialComm::scomm_RECEIVE_ERROR;
    return -1;
  }

  if(!ReadFile(h, (char *)buf, bytes, &bytes_read, &r_ov)) {
    if(GetLastError() == ERROR_IO_PENDING) {
      if(!GetOverlappedResult(h, &r_ov, &bytes_read, TRUE)) {
	// Overlapped I/O error
	scomm_error = gxSerialComm::scomm_RECEIVE_ERROR;
	CloseHandle(r_ov.hEvent);
	return -1;
      }
    } 
    else {
      // ReadFile error
      scomm_error = gxSerialComm::scomm_RECEIVE_ERROR;
      CloseHandle(r_ov.hEvent);
      return -1;
    }
  }
  CloseHandle(r_ov.hEvent); // Close the overlapped event

#elif defined (__UNIX__)
  bytes_read = read(h, (char *)buf, bytes);
  if(bytes_read < 0) {
    scomm_error = gxSerialComm::scomm_RECEIVE_ERROR;
    return -1;
  }
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
  return (int)bytes_read;
}

int gxSerialComm::RawWrite(scommDeviceHandle h, const void *buf, int bytes)
// Write a specified number of bytes to a device handle
// and return whether or not the write was complete.
// Returns the total number of bytes moved or -1 if
// an error occurred.
{
#if defined (__WIN32__)
  OVERLAPPED w_ov;
  ZeroMemory(&w_ov, sizeof(w_ov));

  // Create event for overlapped I/O
  w_ov.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  if(w_ov.hEvent == INVALID_HANDLE_VALUE) {
    scomm_error = gxSerialComm::scomm_TRANSMIT_ERROR;
    return -1;
  }

  if(!WriteFile(h, (char *)buf, bytes, &bytes_moved, &w_ov)) {
    if(GetLastError() == ERROR_IO_PENDING) {
      if(!GetOverlappedResult(h, &w_ov, &bytes_moved, TRUE)) {
	// Overlapped I/O error
	scomm_error = gxSerialComm::scomm_TRANSMIT_ERROR;
	CloseHandle(w_ov.hEvent);
	return -1;
      }
    } 
    else {
      // WriteFile Error
      scomm_error = gxSerialComm::scomm_TRANSMIT_ERROR;
      CloseHandle(w_ov.hEvent);
      return -1;
    }
  }
  CloseHandle(w_ov.hEvent); // Close the overlapped event

#elif defined (__UNIX__)
  bytes_moved = write(h, (char *)buf, bytes);
  if(bytes_moved < 0) {
    scomm_error = gxSerialComm::scomm_TRANSMIT_ERROR;
    return -1;
  }
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
  return (int)bytes_moved;
}

int gxSerialComm::Recv(scommDeviceHandle h, void *buf, int bytes)
// Receive a specified number of bytes from a device handle
// and do not return until all the byte have been read.
// Returns the total number of bytes read or -1 if an
// error occurred.
{
  int br = 0;               // Byte counter
  int num_read = 0;         // Actual number of bytes read
  int num_req = (int)bytes; // Number of bytes requested 
  char *p = (char *)buf;    // Pointer to the buffer

  while(br < bytes) { // Loop unitl the buffer is full
    if((num_read = RawRead(h, p, num_req-br)) > 0) {
      br += num_read;   // Increment the byte counter
      p += num_read;    // Move the buffer pointer for the next read
    }
    if(num_read < 0) {
      scomm_error = gxSerialComm::scomm_RECEIVE_ERROR;
      return -1; // An error occurred during the read
    }
  }
  
  bytes_read = br; // Undate the object's byte counter
  return bytes_read;
}

int gxSerialComm::Send(scommDeviceHandle h, const void *buf, int bytes)
// Write a specified number of bytes to a device handle and do
// not return until all the bytes have been written. Returns
// the total number of bytes written or -1 if an error occurred.
{
  int bm = 0;                // Byte counter
  int num_moved = 0;         // Actual number of bytes written
  int num_req = (int)bytes;  // Number of bytes requested 
  char *p = (char *)buf;     // Pointer to the buffer

  while(bm < bytes) { // Loop unitl the buffer is empty
    if((num_moved = RawWrite(h, p, num_req-bm)) > 0) {
      bm += num_moved;  // Increment the byte counter
      p += num_moved;   // Move the buffer pointer for the next read
    }
    if(num_moved < 0) {
      scomm_error = gxSerialComm::scomm_TRANSMIT_ERROR;
      return -1; // An error occurred during the read
    }
  }
  
  bytes_moved = bm; // Update the object's byte counter
  return bytes_moved;
}

unsigned long gxSerialComm::GetLineStatus()
{
#if defined (__UNIX__)
  ioctl(device_handle, TIOCMGET, &modem_line_status);
#elif defined (__WIN32__)
  GetCommModemStatus(device_handle, &modem_line_status);
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
  return modem_line_status;
}

int gxSerialComm::CTS_Active()
{
#if defined (__UNIX__)
#define CTS_ACTIVE TIOCM_CTS
#elif defined (__WIN32__)
#define CTS_ACTIVE MS_CTS_ON
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

  if((GetLineStatus() & CTS_ACTIVE) == CTS_ACTIVE) {
    return 1;
  }
  return 0;
}

int gxSerialComm::DSR_Active()
{
#if defined (__UNIX__)
#define DSR_ACTIVE TIOCM_DSR
#elif defined (__WIN32__)
#define DSR_ACTIVE MS_DSR_ON
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

  if((GetLineStatus() & DSR_ACTIVE) == DSR_ACTIVE) {
    return 1;
  }
  return 0;
}

int gxSerialComm::RI_Active()
{
#if defined (__UNIX__)
#define RING_ACTIVE TIOCM_RNG
#elif defined (__WIN32__)
#define RING_ACTIVE MS_RING_ON
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

  if((GetLineStatus() & RING_ACTIVE) == RING_ACTIVE) {
    return 1;
  }
  return 0;
}

int gxSerialComm::DCD_Active()
{
#if defined (__UNIX__)
#define RLSD_ACTIVE TIOCM_CD
#elif defined (__WIN32__)
#define RLSD_ACTIVE MS_RLSD_ON
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

  if((GetLineStatus() & RLSD_ACTIVE) == RLSD_ACTIVE) {
    return 1;
  }
  return 0;
}

#ifdef __BCC32__
#pragma warn .8057
#pragma warn .8071
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
