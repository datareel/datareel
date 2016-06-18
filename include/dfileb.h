// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: dfileb.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

The disk file class is a general-purpose base class used handle
file and directory functions.

Changes:
==============================================================
03/10/2002: The DiskFileB class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used 
by the DiskFileB class. This change was made to support large files
and NTFS file system enhancements for non-database applications. 

03/10/2002: The DiskFileB class now includes a full compliment of 
<< and >> overloads used to insert and extract integers, floating 
points, strings, and characters types. 

03/10/2002: Added the bin, text, hex, dec, flush, and clear 
DiskFileB class I/O manipulator functions used to format I/O 
within an I/O statement.

03/24/2002: Added the DiskFileB::df_gcount() and df_pcount() 
process control functions used to return the total number of 
bytes following read and write operations. 

05/18/2003: Added buffer overflow exception to the
DiskFileB error code enumeration.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_DISK_FILE_BASE_HPP__
#define __GX_DISK_FILE_BASE_HPP__

#include "gxdlcode.h"

#include "gxdfptr.h"
#include "futils.h"

#if defined (__64_BIT_DATABASE_ENGINE__)
#include "strutil.h" // __LLWORD__ and __ULLWORD__ string converters
#endif

// --------------------------------------------------------------
// Type definitions
// --------------------------------------------------------------
#if defined (__64_BIT_DATABASE_ENGINE__)
typedef __LLWORD__ df_StreamPos; // 64-bit stream position
#else
typedef __LWORD__ df_StreamPos; // 32-bit stream position
#endif

// Checksum function pointers for user-defined checksum routines. 
// The checksum function must return 0 to for success or a non-zero 
// value to indicate and error. The checksum value is passed back in the
// *val pointer. The length of val is passed back in the *len pointer.
typedef int (*dfChecksumRoutine)(const char *fname, unsigned char *val, unsigned *len);
// --------------------------------------------------------------

// --------------------------------------------------------------
// Constants 
// --------------------------------------------------------------
const unsigned df_MAX_LINE_LENGTH = 1024; // Maximum line length
const unsigned df_MAX_NAME_LENGTH = 1024; // Maximum length of a file name
const unsigned df_MAX_DIR_LENGTH = 1024;  // Maximum length of a directory name
const unsigned df_DEF_BUF_SIZE = 4096;    // Default buffer size         
const FAU_t df_CurrPosition = (FAU_t)-1;  // Current file position
const unsigned df_MAX_CHECKSUM_VALUE = 255; // Max length of checksum values 

// Maximum number of file error codes defined in the
// gxsFile error codes enumeration. NOTE: This number
// must match the number of error codes in enumeration
// and the df_FILE_ERROR_MESSAGES array.
const unsigned df_MAX_FILE_ERRORS = 45;
// --------------------------------------------------------------

class GXDLCODE_API DiskFileB
{
public: // Enumerations

  enum { // DiskFileB error codes
    df_NO_ERROR = 0,       // No errors reported
    df_INVALID_ERROR_CODE, // Invalid file error code

    // File error exception codes
    df_BUFFER_OVERFLOW, // Buffer overflow error
    df_CLOSE_ERROR,     // Error closing the file
    df_COPY_ERROR,      // Error copying file
    df_CREATE_ERROR,    // Could not create the file
    df_EOF_ERROR,       // End of file error
    df_EXIST_ERROR,     // The file or dir does not exist
    df_FLUSH_ERROR,     // Error flushing the disk buffers
    df_MODE_ERROR,      // An invalid access/open mode specified
    df_NOT_READY_ERROR, // File not ready (failed or closed file)
    df_OPEN_ERROR,      // Could not open the file
    df_READ_ERROR,      // An error occurred during a read operation
    df_REMOVE_ERROR,    // Could remove the specified file
    df_RENAME_ERROR,    // Could not rename the specified file
    df_SEEK_ERROR,      // An error occurred during a seek operation
    df_WRITE_ERROR,     // An error occurred during a write operation

    // Directory error exception codes
    df_CHDIR_ERROR,  // Could not change directory
    df_MKDIR_ERROR,  // Could not create directory
    df_RMDIR_ERROR,   // Could not remove directory

    // 09/07/2006:
    // Extened error codes added to support the new copy function
    df_NULL_POINTER_ERROR,

    df_BAD_FILENAME_ERROR,
    df_BAD_FILENAME_DEST_ERROR,
    df_BAD_FILENAME_SRC_ERROR,
    df_BAD_DIRNAME_ERROR,
    df_BAD_DIRNAME_DEST_ERROR,
    df_BAD_DIRNAME_SRC_ERROR,
    df_CHECKSUM_ERROR,
    df_CHECKSUM_DEST_ERROR,
    df_CHECKSUM_SRC_ERROR,
    df_FILE_EXISTS_ERROR,
    df_FILE_EXISTS_DEST_ERROR,
    df_FILE_EXISTS_SRC_ERROR,
    df_GET_FILELENGTH_ERROR,
    df_GET_FILELENGTH_DEST_ERROR,
    df_GET_FILELENGTH_SRC_ERROR,
    df_GET_TIMESTAMP_ERROR,
    df_GET_TIMESTAMP_DEST_ERROR,
    df_GET_TIMESTAMP_SRC_ERROR,
    df_NOFILE_EXISTS_ERROR,
    df_NOFILE_EXISTS_DEST_ERROR,
    df_NOFILE_EXISTS_SRC_ERROR,
    df_SET_TIMESTAMP_ERROR,
    df_SET_TIMESTAMP_DEST_ERROR,
    df_SET_TIMESTAMP_SRC_ERROR
  };

  enum { // DiskFileB access/open mode enumeration
    df_READONLY,  // Open existing file with read access only
    df_WRITEONLY, // Open existing file with write access only
    df_READWRITE, // Open existing file with read and write access
    df_CREATE,    // Create new file and truncate existing file
    df_NO_CREATE, // Open existing file without truncating it
    df_APPEND,    // Append to end of file after opening
    df_NO_APPEND // Do not append to end of file after opening
  };

  enum { // DiskFileB seek modes
    df_SEEK_BEG, // Seek starting from the beginning of the file
    df_SEEK_CUR, // Seek starting from the current location
    df_SEEK_END  // Seek starting from the end of the file
  };

  enum { // DiskFileB I/O operation codes
    df_READ,         // A read was performed
    df_WRITE,        // A write operation was performed
    df_SEEK,         // A seek operation was preformed
    df_NO_OPERATION  // No operation was performed
  };

public:
  DiskFileB();
  DiskFileB(const char *fname, 
	    int mode = DiskFileB::df_READONLY, 
	    int create = DiskFileB::df_NO_CREATE,
	    int append = DiskFileB::df_NO_APPEND);
  virtual ~DiskFileB();

protected: 
  // Eliminate the need for reference counting by disallowing
  // coping and assignment. This will ensure that objects can
  // be safely destroyed because no copies of the object exist.
  DiskFileB(const DiskFileB &ob) { df_Copy(ob); } 
  DiskFileB& operator=(const DiskFileB &ob) { df_Copy(ob); return *this; }
  
public: // Dervied class interface
  virtual int df_Open(const char *fname, 
		      int mode = DiskFileB::df_READONLY, 
		      int append = DiskFileB::df_NO_APPEND);
  virtual int df_Create(const char *fname);
  virtual int df_ReOpen(gxdFPTR *f, int mode = DiskFileB::df_READONLY);
  virtual int df_Close();
  virtual int df_Rewind(int fl = 1); // Move to beginning of the file
  virtual int df_Append(int fl = 1); // Move to end of the file
  virtual int df_Flush();
  virtual FAU_t df_Tell();
  virtual int df_GetLine(char *sbuf, unsigned bytes = df_MAX_LINE_LENGTH, 
			 const char delimiter = '\n', 
			 int filter_linefeeds = 1);
  virtual int df_Get(char &c);
  virtual int df_Put(const char &c);
  virtual int df_Read(void *buf, unsigned bytes, 
		      FAU_t position = df_CurrPosition);
  virtual int df_Write(const void *buf, unsigned bytes, 
		       FAU_t position = df_CurrPosition);
  virtual int df_Seek(FAU_t position, int seek_mode = df_SEEK_BEG);
  virtual FAU_t df_SeekTo(FAU_t position);

public: // General purpose file and directory functions
  FAU_t df_FileSize(const char *fname);
  int df_Exists(const char *name);
  int df_IsDirectory(const char *dir_name);
  int df_IsFile(const char *fname);
  int df_chdir(const char *dir_name);
  int df_mkdir(const char *dir_name);
  int df_rmdir(const char *dir_name);
  int df_pwd(char *dir, unsigned max_len = df_MAX_DIR_LENGTH);
  void df_MakeDOSPath(char *path);
  int df_HasDriveLetter(const char *dir_name, char &letter);
  void df_MakeUNIXPath(char *path);
  void df_PathSimplify(const char *path, char *new_path, char path_sep = '/'); 
  int df_remove(const char *fname);
  int df_rename(const char *oldname, const char *newname);
  //  int df_copy(const char *from, const char *to);
  int df_copy(const char *from, const char *to, 
	      unsigned buf_size = df_DEF_BUF_SIZE, 
	      int overwrite = 0, 
	      dfChecksumRoutine CalcFileChecksum = 0,
	      unsigned char *checksum_value = 0,
	      unsigned *value_len = 0);
  int df_chmod(const char *fname, int pmode);
  int df_GenOutputFileName(const char *current_file, char *out_file, 
			   const char *extension);

public: // Synchronization functions
  void df_Release();

public: // Error handling functions
  int df_CheckError() { return df_last_error != DiskFileB::df_NO_ERROR; }
  int df_ClearError() { return df_last_error = DiskFileB::df_NO_ERROR; }
  int df_GetError() { return df_last_error; }
  int df_SetError(int err) { return df_last_error = err; }
  const char *df_ExceptionMessage();
  
public: // File status functions
  int df_IsOK() { return ((df_is_ok == 1) && (df_is_open == 1)); }
  int df_IsOpen() { return ((df_is_ok == 1) && (df_is_open == 1)); }
  int df_ReadOnly() { return df_ready_for_writing != 1; }
  int df_ReadyForReading() { return df_ready_for_reading == 1; }
  int df_ReadyForWriting() { return df_ready_for_writing == 1; }
  int df_LastOperation() { return df_last_operation; }
  int df_EOF();
  char *df_OpenFileName() { return (char *)df_open_file_name; }
  gxdFPTR *df_FileStream() { return df_fptr; }
  FAU_t df_GetEOF() { return df_file_length; }
  FAU_t df_Length() { return df_file_length; }

  // Process control functions
  unsigned df_gcount() { return df_bytes_read; }
  unsigned df_pcount() { return df_bytes_moved; }
  unsigned df_BytesRead() { return df_bytes_read; }
  unsigned df_BytesMoved() { return df_bytes_moved; }
  unsigned df_SetBytesRead(int bytes = 0) { return df_bytes_read = bytes; }
  unsigned df_SetBytesMoved(int bytes = 0) { return df_bytes_moved = bytes; }
  unsigned *df_GetBytesRead() { return &df_bytes_read; }
  unsigned *df_GetBytesMoved() { return &df_bytes_moved; }

protected: // Internal processing functions
  void df_FilterLineFeed(char &c);
  void df_Copy(const DiskFileB &ob);
  void df_LoadNameBuffer(const char *s);
  void df_ClearNameBuffer();

protected: // Stream file handles
  gxdFPTR *df_fptr;  // Stream file handle used for all I/O operations  

protected: // String members
  char df_open_file_name[df_MAX_NAME_LENGTH]; // Name of the open file 
  
protected: // File status members
  int df_is_open;           // True if the file is open
  int df_is_ok;             // True if the file status is good
  int df_ready_for_reading; // True if the file is ready for reading
  int df_ready_for_writing; // True if the file is ready for writing

protected: // File operation members
  FAU_t df_file_length;  // File length
  int df_last_error;     // Numerical value representing the last file error
  int df_last_operation; // The last file operation performed

protected: // Process control variables
  unsigned df_bytes_read;  // Number of bytes read following a read operation
  unsigned df_bytes_moved; // Number of bytes written following a write

public: // Overloaded operators
  int operator!() const { return ((df_is_ok == 0) || (df_is_open == 0)); }
  int operator!() { return ((df_is_ok == 0) || (df_is_open == 0)); }
  operator const int () const { 
    return ((df_is_ok == 1) && (df_is_open == 1)); 
  }
  operator int () { return ((df_is_ok == 1) && (df_is_open == 1)); }
  DiskFileB &operator<<(DiskFileB & (*_f)(DiskFileB&));
  DiskFileB &operator<<(const char *s);
  DiskFileB &operator<<(const unsigned char *s);
  DiskFileB &operator<<(const char c);
  DiskFileB &operator<<(const unsigned char c);
  DiskFileB &operator<<(const long val);
  DiskFileB &operator<<(const unsigned long val);
  DiskFileB &operator<<(const int val);
  DiskFileB &operator<<(const unsigned int val);
  DiskFileB &operator<<(const short val);
  DiskFileB &operator<<(const unsigned short val);
  DiskFileB &operator<<(const float val);
  DiskFileB &operator<<(const double val);
  DiskFileB &operator>>(char *s);
  DiskFileB &operator>>(unsigned char *s);
  DiskFileB &operator>>(char &c);
  DiskFileB &operator>>(unsigned char &c);
  DiskFileB &operator>>(long &val);
  DiskFileB &operator>>(unsigned long &val);
  DiskFileB &operator>>(int &val);
  DiskFileB &operator>>(unsigned int &val);
  DiskFileB &operator>>(short &val);
  DiskFileB &operator>>(unsigned short &val);
  DiskFileB &operator>>(float &val);
  DiskFileB &operator>>(double &val);

#if defined (__64_BIT_DATABASE_ENGINE__)
  DiskFileB &operator<<(const __LLWORD__ val);
  DiskFileB &operator<<(const __ULLWORD__ val);
  DiskFileB &operator>>(__LLWORD__ &val);
  DiskFileB &operator>>(__ULLWORD__ &val);
#endif

public: // Functions used by DiskFileB I/O manipulators
  void df_Bin() { df_output_bin = 1; }
  void df_Text() { df_output_bin = 0; }
  void df_Dec() { df_output_hex = 0; }
  void df_Hex() { df_output_hex = 1; }
  void df_Oct() { df_output_hex = 2; }
  void df_SHN() { df_output_hex = 3; }
  void df_Precision(int i = 1) { df_output_pre = i; }
  void df_SetWidth(int i = 0) { df_output_width = i; }
  void df_SetFill(char c = ' ') { df_output_fill = c; }
  void df_Clear();
  void df_Reset() {
    df_output_hex = 0; df_output_pre = 1; 
    df_output_width = 0; df_output_fill = ' ';
  }

protected: // Members used by DiskFileB I/O manipulators
  int df_output_bin;   // True for binary mode, false for text
  int df_output_hex;   // 0-decimal, 1-hex, 2-octal, 3-SHN
  int df_output_pre;   // Floating point precision;
  int df_output_width; // Minimum string width
  char df_output_fill; // Fill character

public: // Depreciated types functions
  enum {
    df_TRUNCATE = -1000,
    df_SHARE = -1001
  };

public: // Depreciated functions
  DiskFileB(const char *fname, int mode, int create, 
	    int truncate, int sharing);
  int df_Open(const char *fname, int mode, int create, 
	      int truncate, int sharing);
  int df_GetFileError() { return df_last_error; }
  FAU_t df_FilePosition() { return df_Tell(); }
  const char *DiskFileExceptionMessage() { return df_ExceptionMessage(); }
};

// DiskFileB class I/O manipulator functions used to format I/O
// within an I/O statement.
inline DiskFileB& bin(DiskFileB &s) { s.df_Bin(); return s; }
inline DiskFileB& text(DiskFileB &s) { s.df_Text(); return s; }
inline DiskFileB& hex(DiskFileB &s) { s.df_Hex(); return s; }
inline DiskFileB& dec(DiskFileB &s) { s.df_Dec(); return s; }
inline DiskFileB& oct(DiskFileB &s) { s.df_Oct(); return s; }
inline DiskFileB& shn(DiskFileB &s) { s.df_SHN(); return s; }
inline DiskFileB& reset(DiskFileB &s) { s.df_Reset(); return s; }
inline DiskFileB& flush(DiskFileB &s) { s.df_Flush(); return s; }
inline DiskFileB& clear(DiskFileB &s) { s.df_Clear(); return s; }
inline DiskFileB& resetw(DiskFileB &s) { s.df_SetWidth(0); return s; }
inline DiskFileB& setw1(DiskFileB &s) { s.df_SetWidth(1); return s; }
inline DiskFileB& setw2(DiskFileB &s) { s.df_SetWidth(2); return s; }
inline DiskFileB& setw4(DiskFileB &s) { s.df_SetWidth(4); return s; }
inline DiskFileB& setw8(DiskFileB &s) { s.df_SetWidth(8); return s; }
inline DiskFileB& resetp(DiskFileB &s) { s.df_Precision(1); return s; }
inline DiskFileB& setp1(DiskFileB &s) { s.df_Precision(1); return s; }
inline DiskFileB& setp2(DiskFileB &s) { s.df_Precision(2); return s; }
inline DiskFileB& setp4(DiskFileB &s) { s.df_Precision(4); return s; }
inline DiskFileB& setp8(DiskFileB &s) { s.df_Precision(8); return s; }

#endif // __GX_DISK_FILE_BASE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
