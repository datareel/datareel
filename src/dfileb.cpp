// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: dfileb.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 06/05/2016
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

The disk file class is a general-purpose base class used handle
file and directory functions.

Changes:
==============================================================
03/10/2002: The DiskFileB class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used 
by the DiskFileB class. This change was made to support large files
and NTFS file system enhancements for non-database applications. 

05/25/2002: Fixed possible out of bounds pointer in the 
DiskFileB::df_LoadNameBuffer() function.

05/18/2003: Added buffer overflow exception to the
df_FILE_ERROR_MESSAGES[] array.

09/21/2005: Replaced DiskFileB::df_copy() function code with
new code that will execute faster.

09/07/2006: Updated DiskFileB::df_copy() function.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "dfileb.h"
#include "ustring.h"

#ifdef __BCC32__
#pragma warn -8057
#pragma warn -8080
#endif

const char *df_FILE_ERROR_MESSAGES[df_MAX_FILE_ERRORS] = {
  "Disk file exception: No errors reported", // df_NO_ERROR
  "Disk file exception: Invalid error code", // df_INVALID_ERROR_CODE 

  // File error exception strings
  "Disk file exception: Buffer overflow",                            // BUF OF
  "Disk file exception: Error closing the file",                     // CLOSE
  "Disk file exception: A fatal error occurred while copying",       // COPY
  "Disk file exception: Could not create the specified file",        // CREATE
  "Disk file exception: End of file error occurred",                 // EOF   
  "Disk file exception: The file/directory does not exist",          // EXIST
  "Disk file exception: Error flushing the file buffers",            // FLUSH
  "Disk file exception: An bad/invalid access mode was specified",   // MODE
  "Disk file exception: File not ready (failed or closed file)",     // N_READY
  "Disk file exception: Cannot open the specified file",             // OPEN
  "Disk file exception: A fatal file read error occurred",           // READ
  "Disk file exception: Could remove the specified file",            // REMOVE
  "Disk file exception: Could not rename the specified file",        // RENAME
  "Disk file exception: An error occurred during a seek operation",  // SEEK
  "Disk file exception: A fatal file write error occurred",          // WRITE  

  // Directory error exception strings
  "Disk file exception: Could not change directory",               // CHDIR
  "Disk file exception: Could not create the specified directory", // MKDIR
  "Disk file exception: Could not remove the specified directory", // RMDIR

  // 09/07/2006: Extended error code strings
  "Disk file exception: Null pointer error", // df_NULL_POINTER_ERROR

  "Disk file exception: Bad or invalid file name",      // df_BAD_FILENAME_ERROR
  "Disk file exception: Invalid destination file name", // df_BAD_FILENAME_DEST_ERROR
  "Disk file exception: Invalid source file name",      // df_BAD_FILENAME_SRC_ERROR
  "Disk file exception: Bad or invalid directory name", // df_BAD_DIRNAME_ERROR
  "Disk file exception: Invalid destination directory name", // df_BAD_DIRNAME_DEST_ERROR
  "Disk file exception: Invalid source directory name",      // df_BAD_DIRNAME_SRC_ERROR
  "Disk file exception: File checksum error",                // df_CHECKSUM_ERROR
  "Disk file exception: Destination file checksum error",    // df_CHECKSUM_DEST_ERROR
  "Disk file exception: Source file checksum error",         // df_CHECKSUM_SRC_ERROR
  "Disk file exception: File already exists",                // df_FILE_EXISTS_ERROR
  "Disk file exception: Destination file already exists",    // df_FILE_EXISTS_DEST_ERROR
  "Disk file exception: Source file already exists",         // df_FILE_EXISTS_SRC_ERROR
  "Disk file exception: File length error",                  // df_GET_FILELENGTH_ERROR
  "Disk file exception: Destination file length error",      // df_GET_FILELENGTH_DEST_ERROR
  "Disk file exception: Source file length error",           // df_GET_FILELENGTH_SRC_ERROR
  "Disk file exception: Error reading file timestamp",       // df_GET_TIMESTAMP_ERROR
  "Disk file exception: Error reading destination file timestamp", // df_GET_TIMESTAMP_DEST_ERROR
  "Disk file exception: Error reading source file timestamp", // df_GET_TIMESTAMP_SRC_ERROR
  "Disk file exception: File not file",                       // df_NOFILE_EXISTS_ERROR
  "Disk file exception: Destination file not found",          // df_NOFILE_EXISTS_DEST_ERROR
  "Disk file exception: Source file not found",               // df_NOFILE_EXISTS_SRC_ERROR
  "Disk file exception: Error setting file timestamp",        // df_SET_TIMESTAMP_ERROR
  "Disk file exception: Error setting destination file timestamp", // df_SET_TIMESTAMP_DEST_ERROR
  "Disk file exception: Error setting source file timestamp"       // df_SET_TIMESTAMP_SRC_ERROR
};

const char *DiskFileB::df_ExceptionMessage()
// Returns a null terminated string that can
// be used to log or print a disk file exception.
{
  if(df_last_error > (int)(df_MAX_FILE_ERRORS-1)) {
    df_last_error = DiskFileB::df_INVALID_ERROR_CODE;
  }
  return df_FILE_ERROR_MESSAGES[df_last_error];
}

DiskFileB::DiskFileB()
{ 
  // Reset the file status and operation members
  df_fptr = 0;
  df_is_open = 0;
  df_is_ok = df_ready_for_reading = df_ready_for_writing = 0;
  df_last_error = DiskFileB::df_NO_ERROR;
  df_last_operation = DiskFileB::df_NO_OPERATION;
  df_file_length = (FAU_t)0;
  df_ClearNameBuffer();
  df_Clear();
}

DiskFileB::DiskFileB(const char *fname, int mode, int create, int append)
{
  df_is_open = df_is_ok = 0; // Set the open and ok flags
  if(create == DiskFileB::df_CREATE) {
    DiskFileB::df_Create(fname);
  }
  else {
    DiskFileB::df_Open(fname, mode, append);
  }
}

DiskFileB::DiskFileB(const char *fname, int mode, int create, 
		     int truncate, int sharing)
// NOTE: This constructor is depreciated.
// Ignore all BCC32 parameter never used warnings 
{
  df_is_open = df_is_ok = 0; // Set the open and ok flags
  if(create == DiskFileB::df_CREATE) {
    DiskFileB::df_Create(fname);
  }
  else {
    DiskFileB::df_Open(fname, mode);
  }
}

int DiskFileB::df_Open(const char *fname, int mode, int create, 
		       int truncate, int sharing)
// NOTE: This function is depreciated.
// Ignore all BCC32 parameter never used warnings
{
  if(create == DiskFileB::df_CREATE) {
    return df_Create(fname);
  }
  else {
    return df_Open(fname, mode);
  }
}

DiskFileB::~DiskFileB()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  if(df_fptr) DiskFileB::df_Close();
}

void DiskFileB::df_Copy(const DiskFileB &ob)
// Private member function used to copy DiskFileB objects.
// Copying or assignment should never be allowed unless
// each copy is accounted for by reference counting or 
// some other method.
{
  df_fptr = ob.df_fptr;
  df_is_open = ob.df_is_open;
  df_is_ok = ob.df_is_ok; 
  df_ready_for_reading = ob.df_ready_for_reading;
  df_ready_for_writing = ob.df_ready_for_writing;
  df_last_error = ob.df_last_error;
  df_last_operation = ob.df_last_operation; 
  df_LoadNameBuffer((const char *)ob.df_open_file_name);
  df_file_length = ob.df_file_length;
  df_output_bin = ob.df_output_bin;
  df_output_hex = ob.df_output_hex;
  df_output_pre = ob.df_output_pre;
  df_output_width = ob.df_output_width;
  df_output_fill = ob.df_output_fill;

}

void DiskFileB::df_LoadNameBuffer(const char *s)
// Private member function used to load the
// open file name into the name buffer.
{
  df_ClearNameBuffer();
  if(!s) return;
  unsigned len = strlen(s);

  // Prevent an overflow if the maximum name length is exceeded
  if(len > df_MAX_NAME_LENGTH) len = df_MAX_NAME_LENGTH-1; 

  // Load the name buffer
  memmove(df_open_file_name, s, len);
  df_open_file_name[len] = '\0'; // Ensure null termination
}

void DiskFileB::df_ClearNameBuffer()
// Private member function used to clear the name buffer.
{
  memset(df_open_file_name, 0, df_MAX_NAME_LENGTH);
}

int DiskFileB::df_Create(const char *fname)
// Create the specified file truncating any existing version.
// Returns df_NO_ERROR if no errors occur or a DiskFileB error 
// code if an error is encountered.
{ 
  // Close any open files
  if(df_Close() != DiskFileB::df_NO_ERROR) return df_last_error;

  // Open the file truncating any existing version
  df_fptr = gxdFPTRCreate(fname);
  
  // Could not open the specified file
  if(!df_fptr) {
    return df_last_error = DiskFileB::df_CREATE_ERROR;
  }

  // Record the file length to track the EOF and file size
  df_file_length = (FAU_t)0;

  // Set the file name and status members
  df_LoadNameBuffer(fname);
  df_is_open = df_is_ok = 1;
  df_ready_for_reading = df_ready_for_writing = 1;

  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_Open(const char *fname, int mode, int append)
// Open the specified file. The "mode" value must correspond to 
// one of the numerical values defined in the DiskFileB access
// mode enumeration. Returns df_NO_ERROR if no errors occur or 
// a DiskFileB error code if an error is encountered.
{
  // Close any open files
  if(df_Close() != DiskFileB::df_NO_ERROR) return df_last_error;

  if(mode == DiskFileB::df_READONLY) {
    df_fptr = gxdFPTROpen(fname, gxDBASE_READONLY);
  }
  else {
    df_fptr = gxdFPTROpen(fname, gxDBASE_READWRITE);
  }
  
  // Could not open the specified file
  if(!df_fptr) {
    return df_last_error = DiskFileB::df_OPEN_ERROR;
  }

  // Record the file length to track the EOF and file size
  df_file_length = df_FileSize(fname); 

  // Set the file name and status members
  df_LoadNameBuffer(fname);
  df_is_open = df_is_ok = 1;
  
  switch(mode) {
    case df_READONLY :
      df_ready_for_reading = 1;
      break;
    case df_WRITEONLY :
      df_ready_for_writing = 1;
      break;
    case df_READWRITE :
      df_ready_for_reading = 1;
      df_ready_for_writing = 1;
      break;
    default:
      return df_last_error = DiskFileB::df_MODE_ERROR;
  }

  if(append == df_APPEND) { // Move to the end of the file
    if(df_Append(0) != DiskFileB::df_NO_ERROR) return df_last_error;
  }

  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_Append(int fl)
 // Move to end of the file   
{
  if(!df_IsOK()) { 
    return df_last_error = DiskFileB::df_NOT_READY_ERROR;
  }

  if(fl) df_Flush();

  // NOTE: gxDBASE_SEEK_END does not work correctly when used
  // in wxWindows WIN32 applications using the NTFS option.
  if(gxdFPTRSeek(df_fptr, df_file_length, gxDBASE_SEEK_BEG) ==
     (FAU_t)-1) {
    return df_last_error = DiskFileB::df_SEEK_ERROR;
  }
  df_last_operation = DiskFileB::df_SEEK;

  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_Exists(const char *name)
// Tests to see if the specified directory of file name 
// exists. Returns true if the file or directory exists.

{
  return gxdFPTRExists(name);
}

void DiskFileB::df_Release()
// Reset the open file pointer to zero without closing the
// file or performing any other action. NOTE: This function is
// used to reset the file pointer when more then one object is
// referencing this file pointer and the file has been closed
// or the pointer has been deleted. After calling the release
// function the DiskFileB destructor will not close the open
// file handle. This will allow the file to remain open until 
// all threads accessing the open file handle have exited or
// released it.
{
  // Reset the file handle without closing it
  df_fptr = 0; 

  // Reset all the file status members
  df_is_open = 0;
  df_is_ok = df_ready_for_reading = df_ready_for_writing = 0;
  df_file_length = (FAU_t)0;
  df_last_error = DiskFileB::df_NO_ERROR;
  df_last_operation = DiskFileB::df_NO_OPERATION;
}

int DiskFileB::df_Close() 
// Close the open file. Returns a df_CLOSE_ERROR
// error code if an error was encountered.
{
  if(!df_IsOpen()) { // Reset the status members and return
    df_is_open = 0;
    df_is_ok = df_ready_for_reading = df_ready_for_writing = 0;
    df_last_operation = DiskFileB::df_NO_OPERATION;
    df_file_length = (FAU_t)0;
    df_Clear();
    return df_last_error = DiskFileB::df_NO_ERROR;
  }
  df_ClearNameBuffer();

  df_is_open = 0; // Reset the open flag before calling the close function
  if(gxdFPTRClose(df_fptr) != 0) {
    df_is_ok = 0; // Signal a problem with the partially closed file
    return df_last_error = DiskFileB::df_CLOSE_ERROR;
  }
  df_fptr = 0; // Reset the file pointer

  // Reset the file status and opeartion members
  df_ready_for_reading = df_ready_for_writing = 0;
  df_file_length = (FAU_t)0;
  df_Clear();

  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_Rewind(int fl)
// Rewind the file pointers to zero and
// flush any open disk buffers. Returns a
// df_SEEK_ERROR error code if an error
// was encountered.
{
  if(!df_IsOK()) { 
    return df_last_error = DiskFileB::df_NOT_READY_ERROR;
  }

  if(gxdFPTRSeek(df_fptr, (FAU_t)0, gxDBASE_SEEK_BEG) ==
     (FAU_t)-1) {
    return df_last_error = DiskFileB::df_SEEK_ERROR;
  }

  if(fl) df_Flush();

  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_Flush()
// Flush any open disk buffers. Returns a
// df_FLUSH_ERROR error code if an error
// was encountered. No error will be returned
// if the flush function is call for read only
// files.
{
  if((df_IsOK()) && (df_ReadyForWriting())) {
    if(gxdFPTRFlush(df_fptr) != 0) {
      return df_last_error = DiskFileB::df_FLUSH_ERROR;
    }
    df_Seek((FAU_t)0, df_SEEK_CUR);
  }
  return df_last_error = DiskFileB::df_NO_ERROR;
}

FAU_t DiskFileB::df_FileSize(const char *fname)
// Returns the file size of the specified file.
{
  FAU_t len = gxdFPTRFileSize(fname);
  if(len == (FAU_t)-1) len = (FAU_t)0;
  return len;
}

int DiskFileB::df_Read(void *buf, unsigned bytes, FAU_t position)
// Read a specified number of bytes into a buffer. Returns df_NO_ERROR
// if no errors occur or a DiskFileB error code if an error is
// encountered during a read call.
{
  if((df_IsOK()) && (df_ReadyForReading())) {
    if(position == df_CurrPosition) {
      if(df_last_operation == DiskFileB::df_WRITE) {
	if(df_Seek((FAU_t)0, DiskFileB::df_SEEK_CUR) != 
	   DiskFileB::df_NO_ERROR) {
	  return df_last_error;
	}
      }
    }
    else {
      if(df_Seek(position, DiskFileB::df_SEEK_BEG) != DiskFileB::df_NO_ERROR)
	return df_last_error;
    }

    df_bytes_read = gxdFPTRRead(df_fptr, buf, bytes);
    if(df_bytes_read == 0) df_bytes_read = bytes;
    if(df_bytes_read != bytes) {
      if(df_EOF())
	return df_last_error = DiskFileB::df_EOF_ERROR;
      else
	return df_last_error = DiskFileB::df_READ_ERROR;
    }
  }
  else { // Cannot read from the file
    return df_last_error = DiskFileB::df_READ_ERROR;
  }

  df_last_operation = DiskFileB::df_READ;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_Write(const void *buf, unsigned bytes, FAU_t position)
// Write a specified number of bytes from a buffer to the open file. 
// Returns df_NO_ERROR if no errors occur or a DiskFileB error code 
// if an error is encountered during a write call.
{
  if((df_IsOK()) && (df_ReadyForWriting())) {
    if(position == df_CurrPosition) {
      if(df_last_operation == DiskFileB::df_READ) {
	if(df_Seek((FAU_t)0, DiskFileB::df_SEEK_CUR) != 
	   DiskFileB::df_NO_ERROR) {
	  return df_last_error;
	}
      }
    }
    else {
      if(df_Seek(position, DiskFileB::df_SEEK_BEG) != DiskFileB::df_NO_ERROR)
	return df_last_error;
    }
    
    df_bytes_moved = gxdFPTRWrite(df_fptr, buf, bytes);
    if(df_bytes_moved != bytes) {
      // Update the EOF marker
      if(df_bytes_moved > 0) df_file_length += (FAU_t)df_bytes_moved;
      return df_last_error = DiskFileB::df_WRITE_ERROR;
    }
  }
  else { // Cannot write to the file
    return df_last_error = DiskFileB::df_WRITE_ERROR;
  }

  df_file_length += (FAU_t)bytes; // Increment the file_length variable
  df_last_operation = DiskFileB::df_WRITE;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_Seek(FAU_t position, int seek_mode)
// Move the file offset to the specified file position. Returns a
// df_SEEK_ERROR if an error occurred during a file seek.
{
  if(!df_IsOK()) {
    return df_last_error = DiskFileB::df_NOT_READY_ERROR;
  }

  FAU_t pos;
  switch(seek_mode) {
    case DiskFileB::df_SEEK_BEG :
      pos = gxdFPTRSeek(df_fptr, position, gxDBASE_SEEK_BEG);
      break;
    case DiskFileB::df_SEEK_CUR :
      pos = gxdFPTRSeek(df_fptr, position, gxDBASE_SEEK_CUR);
      break;
    case DiskFileB::df_SEEK_END :
      pos = gxdFPTRSeek(df_fptr, position, gxDBASE_SEEK_END);
      break;
    default: // An invalid operation was specified 
      return df_last_error = DiskFileB:: df_SEEK_ERROR;
  }
  
  if(pos == (FAU_t)-1) {
    // 06/02/2002: BCC32 warning detected that the pos variable was
    // not being evaluated in the function.
    return df_last_error = DiskFileB:: df_SEEK_ERROR;
  }

  df_last_operation = DiskFileB::df_SEEK;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

FAU_t DiskFileB::df_Tell()
// Returns the current file position after a read, write,
// or rewind operation. Returns -1 if an error occurred
// or an invalid operation was performed.
{
  if(!df_IsOK()) {
    df_last_error = df_NOT_READY_ERROR;
    return (FAU_t)-1;
  }
  return gxdFPTRTell(df_fptr);
}

int DiskFileB::df_ReOpen(gxdFPTR *f, int mode)
// Connects the file object pointer to another pointer.
// This function assumes the file has been opened by the
// calling function.  The calling function must tell this 
// function if the file was opened for read, write, or
// read/write access.
{
  // Close any open files
  if(df_Close() != DiskFileB::df_NO_ERROR) return df_last_error;

  df_fptr = f;
  df_is_open = 1;

  switch(mode) {
    case DiskFileB::df_READONLY :
      df_ready_for_reading = 1;
      break;
    case DiskFileB::df_WRITEONLY :
      df_ready_for_writing = 1;
      break;
    case DiskFileB::df_READWRITE :
      df_ready_for_reading = 1;
      df_ready_for_writing = 1;
      break;
    default:
      return df_last_error = DiskFileB::df_MODE_ERROR;
  }

  df_is_ok = 1;

  // Record the file length to track the EOF and file size
  df_file_length = gxdFPTRSeek(df_fptr, (FAU_t)0, gxDBASE_SEEK_END);
  if(df_file_length == -1) df_file_length = (FAU_t)0;

  // Move back to the beginning of the file stream
  df_file_length = gxdFPTRSeek(df_fptr, (FAU_t)0, gxDBASE_SEEK_BEG);

  return df_last_error = DiskFileB::df_NO_ERROR;
}

FAU_t DiskFileB::df_SeekTo(FAU_t position)
// Seek to the specified position, optimizing the seek
// operation by moving the file position indicator based
// on the current stream position. Returns the current
// file position after performing the seek to operation.
{
  FAU_t pos = df_Tell(); // Get the current stream position

  if(position == df_CurrPosition) { // Do not perform a seek operation
    return pos;
  }
  else if(position > pos) { // Seek forward to the specified address
    FAU_t offset = position - pos;
    df_Seek(offset, df_SEEK_CUR);
  }
  else if(position < pos) { // Seek backward to the specified address
    df_Seek(position, df_SEEK_BEG);
  }
  else { // Current file position equals the specified address
    // Find current the position
    df_Seek((FAU_t)0, df_SEEK_CUR);
  }
  
  return df_Tell(); // Return current file position after seeking
}

void DiskFileB::df_FilterLineFeed(char &c)
// Added to filter the end of line sequence from a line of text.
{
  switch(c) {
    case '\r': // WIN32
      c = '\0';
      break;
    case '\n': // WIN32 and UNIX 
      c = '\0';
      break;
    default:
      break;
  }
}

int DiskFileB::df_GetLine(char *sbuf, unsigned bytes, const char delimiter,
			  int filter_linefeeds)
// Get a line of text from the open file. Returns df_NO_ERROR
// if no errors occur or a DiskFileB error code if an error is
// encountered during a getline call. NOTE: This function
// assumes that a buffer equal to the size of the "bytes"
// variable has already been allocated. By default this
// function will use a '\n' delimiter and will filter all
// linefeeds from the string.
{
  char *p = sbuf;
  unsigned i;

  for(i = 0; i < bytes; i++) {

    // 09/09/2006: Added to prevent buffer overflow if the line
    // is longer than the number of bytes allocated for sbuf.
    if(i == (bytes-1)) {
      *(p+i) = '\0'; // Null terminate the string
      return df_last_error;
    }
    if(df_Read((p+i), 1) != DiskFileB::df_NO_ERROR) {
      if(df_last_error == DiskFileB::df_EOF_ERROR) {
	df_last_error = DiskFileB::df_NO_ERROR; // Clear EOF errors
      }
      i--; // Account for EOF marker
      break;
    }
    if(*(p+i) == delimiter) {
      break;
    }
  }

  i++; // Move past the last character read
  
  *(p+i) = '\0'; // Null terminate the string

  // Filter all WIN32/UNIX line feeds from the line of text
  if(filter_linefeeds) while(i--) df_FilterLineFeed(*(p+i));

  return df_last_error;
}

int DiskFileB::df_Get(char &c)
// Get a single character from the open file.
// Returns df_NO_ERROR if no errors occur or a
// DiskFileB error code if an error is encountered
// during a get call.
{
  df_Read((char *)&c, 1);
  if(df_last_error == DiskFileB::df_EOF_ERROR) {
    df_last_error = DiskFileB::df_NO_ERROR; // Clear EOF errors
  }
  return df_last_error;
}

int DiskFileB::df_Put(const char &c)
// Put a single character into the open file.
// Returns df_NO_ERROR if no errors occur or a
// DiskFileB error code if an error is encountered
// during a put call.
{
  df_Write((const char *)&c, 1);
  return df_last_error;
}

int DiskFileB::df_EOF() 
// Returns true if the EOF marker has been reached.
{
  FAU_t pos = gxdFPTRTell(df_fptr);
  if(pos >= (df_file_length)) return 1;
  return 0;
}

int DiskFileB::df_IsDirectory(const char *dir_name) 
// Returns true if the specified name is a directory.
{
  return futils_isdirectory(dir_name);
}

int DiskFileB::df_IsFile(const char *fname)
// Returns true if the specified name is any kind of file
// or false if this is a directory.
{
 return futils_isdirectory(fname) == 0;
}

void DiskFileB::df_MakeDOSPath(char *path)
// Make a DOS directory path by changing all
// forward slash path separators with back
// slashes.
{
  futils_makeDOSpath(path);
}

void DiskFileB::df_MakeUNIXPath(char *path)
// Make a UNIX directory path by changing all
// back slash path separators with forward
// slashes.
{
  futils_makeUNIXpath(path);
}

int DiskFileB::df_chdir(const char *dir_name)
// Change directory. Returns zero if successful.
{
  if(futils_chdir(dir_name) != 0) 
    return df_last_error = DiskFileB::df_CHDIR_ERROR;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_rmdir(const char *dir_name)
// Remove the specified directory. Returns zero if successful.
// The directory must be empty and not be the current working
// directory or the root directory.
{
  if(futils_rmdir(dir_name) != 0) 
    return df_last_error = DiskFileB::df_RMDIR_ERROR;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_HasDriveLetter(const char *dir_name, char &letter)
// DOS/WIN32 function used to parse the drive letter from the
// specified directory. Returns false if the path does not
// contain a drive letter. If a drive letter is found it will
// be passed back in the "letter" variable. 
{
  return futils_hasdriveletter(dir_name, &letter);
}

int DiskFileB::df_mkdir(const char *dir_name)
// Make the specified directory if it does not exist.
// Returns a non-zero value if an error occurs. UNIX
// file systems will use 755 permissions when new
// directories are created.
{
  if(futils_mkdir(dir_name) != 0)
    return df_last_error = DiskFileB::df_MKDIR_ERROR;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_remove(const char *fname)
{
  if(futils_remove(fname) != 0) 
    return df_last_error = DiskFileB::df_REMOVE_ERROR;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_rename(const char *oldname, const char *newname)
{
  if(futils_rename(oldname, newname) != 0)
    return df_last_error = DiskFileB::df_RENAME_ERROR;
  return df_last_error = DiskFileB::df_NO_ERROR;
}

int DiskFileB::df_chmod(const char *fname, int pmode)
// Change the file-permission settings. The "pmode"
// variable should be equal to df_READONLY, df_WRITEONLY,
// or df_READWRITE. Returns a non-zero value if an 
// error occurs.
{
  if(!df_Exists(fname)) return DiskFileB::df_EXIST_ERROR;

  switch(pmode) {
    case DiskFileB::df_READONLY :
      if(futils_chmod(fname, futils_READONLY) != 0)
	return df_last_error = DiskFileB::df_MODE_ERROR;
      break;
    case DiskFileB::df_WRITEONLY :
      if(futils_chmod(fname, futils_WRITEONLY) != 0)
	return df_last_error = DiskFileB::df_MODE_ERROR;
      break;
    case DiskFileB::df_READWRITE :
      if(futils_chmod(fname, futils_READWRITE) != 0)
	return df_last_error = DiskFileB::df_MODE_ERROR;
      break;
    default:
      return df_last_error = DiskFileB::df_MODE_ERROR;
  }

  return df_last_error = DiskFileB::df_NO_ERROR;
}

void DiskFileB::df_PathSimplify(const char *path, char *new_path,
				char path_sep)
// Function used to canonicalize a path and return a new path.
// The path separator should either be a forward slash for UNIX
// file systems or a backslash for DOS/WIN32 file systems. If no
// path separator is specified a forward slash will be used by
// default. Multiple path separators will be collapsed to a single
// path separator. Leading `./' paths and trailing `/.' paths will
// be removed. Trailing path separators will be removed. All non-
// leading `../' paths and trailing `..' paths are handled by 
// removing portions of the path. NOTE: This function assumes that
// the necessary memory has already been allocated for the "new_path"
// variable by the calling function.
{
  futils_pathsimplify(path, new_path, path_sep);
}

int DiskFileB::df_GenOutputFileName(const char *current_file, char *out_file, 
				    const char *extension)
// Generate a name for the output file using the "current_file" name with 
// the specified dot extension. NOTE: This function assumes that the 
// necessary memory has already been allocated for the "out_file"
// variable by the calling function. Returns a non-zero value if any
// errors occur.
{
  return futils_genoutputfilename(current_file, out_file, 
				  (char *)extension);
}

int DiskFileB::df_pwd(char *dir, unsigned max_len)
// Passes back the present working directory in the "dir"
// variable. Returns 0 if an error occurs. NOTE: This
// function assumes that the required amount of memory
// has already been allocated for the "dir" pointer. The
// "max_len" value must be at least one greater than the
// length of the pathname to be returned.
{
  if(futils_getcwd(dir, max_len) != 0) {
    return df_last_error = DiskFileB::df_EXIST_ERROR;
  }
  return df_last_error = DiskFileB::df_NO_ERROR;
}

void DiskFileB::df_Clear() 
// Reset all format specifiers and clear the last error.
{
  df_output_bin = 1; df_output_hex = 0;  
  df_output_pre = 1; df_output_width = 0;  
  df_output_fill = ' ';
  df_last_error = DiskFileB::df_NO_ERROR;
  df_last_operation = DiskFileB::df_NO_OPERATION;
  df_bytes_read = df_bytes_moved = 0;
}

DiskFileB& DiskFileB::operator<<(DiskFileB & (*_f)(DiskFileB&))
{
  // PC-lint 09/08/2005: Expected void type
  (*_f)(*(this));
  return *(this);
}

DiskFileB& DiskFileB::operator<<(const char *s)
{
  unsigned len = strlen(s);
  df_Write(s, len);
  return *this;
}

DiskFileB& DiskFileB::operator<<(const unsigned char *s)
{
  unsigned len = strlen((const char *)s);
  df_Write(s, len);
  return *this;
}

DiskFileB& DiskFileB::operator<<(const char c)
{
  if(df_output_width > 1) {
    int i = df_output_width - 1;
    while(i--) df_Write(&df_output_fill, sizeof(char));   
  }
  df_Write(&c, sizeof(char));
  return *this;
}

DiskFileB& DiskFileB::operator<<(const unsigned char c)
{
  if(df_output_width > 1) {
    int i = df_output_width - 1;
    while(i--) df_Write(&df_output_fill, sizeof(char));   
  }
  df_Write(&c, sizeof(char));
  return *this;
}

DiskFileB& DiskFileB::operator<<(const long val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(long));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      // 05/30/2002: Using type cast to int type to eliminate
      // warning: int format, long int arg (arg 3) when compiled
      // under Linux gcc.
      sprintf(sbuf, "%X", (int)val);
    }
    else if(df_output_hex == 2) { // Base 8
      sprintf(sbuf, "%o", (int)val);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      sprintf(sbuf, "%d", (int)val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const unsigned long val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(long));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      // 05/30/2002: Using type cast to int type to eliminate
      // warning: int format, long int arg (arg 3) when compiled
      // under Linux gcc.
      sprintf(sbuf, "%X", (unsigned int)val);
    }
    else if(df_output_hex == 2) { // Base 8
      sprintf(sbuf, "%o", (unsigned int)val);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      sprintf(sbuf, "%u", (unsigned int)val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const int val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(int));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      sprintf(sbuf, "%X", val);
    }
    else if(df_output_hex == 2) { // Base 8
      sprintf(sbuf, "%o", val);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      sprintf(sbuf, "%d", val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const unsigned int val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(int));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      sprintf(sbuf, "%X", val);
    }
    else if(df_output_hex == 2) { // Base 8
      sprintf(sbuf, "%o", val);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      sprintf(sbuf, "%u", val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const short val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(short));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      sprintf(sbuf, "%X", val);
    }
    else if(df_output_hex == 2) { // Base 8
      sprintf(sbuf, "%o", val);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      sprintf(sbuf, "%d", val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const unsigned short val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(short));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      sprintf(sbuf, "%X", val);
    }
    else if(df_output_hex == 2) { // Base 8
      sprintf(sbuf, "%o", val);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      sprintf(sbuf, "%u", val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const float val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(float));
  }
  else {
    char sbuf[81]; char fbuf[81];
    sprintf(fbuf, "%%.%df", df_output_pre);
    sprintf(sbuf, (const char *)fbuf, val);
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const double val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(double));
  }
  else {
    char sbuf[81]; char fbuf[81];
    sprintf(fbuf, "%%.%df", df_output_pre);
    sprintf(sbuf, (const char *)fbuf, val);
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator>>(char *s)
{
  df_GetLine(s, df_MAX_LINE_LENGTH, '\n', 1);
  return *this;
}

DiskFileB& DiskFileB::operator>>(unsigned char *s)
{
  df_GetLine((char *)s, df_MAX_LINE_LENGTH, '\n', 1);
  return *this;
}

DiskFileB& DiskFileB::operator>>(char &c)
{
  df_Read(&c, sizeof(char));
  return *this;
}

DiskFileB& DiskFileB::operator>>(unsigned char &c)
{
  df_Read(&c, sizeof(char));
  return *this;
}

DiskFileB& DiskFileB::operator>>(long &val)
{
  df_Read(&val, sizeof(long));
  return *this;
}

DiskFileB& DiskFileB::operator>>(unsigned long &val)
{
  df_Read(&val, sizeof(long));
  return *this;
}

DiskFileB& DiskFileB::operator>>(int &val)
{
  df_Read(&val, sizeof(int));
  return *this;
}

DiskFileB& DiskFileB::operator>>(unsigned int &val)
{
  df_Read(&val, sizeof(int));
  return *this;
}

DiskFileB& DiskFileB::operator>>(unsigned short &val)
{
  df_Read(&val, sizeof(short));
  return *this;
}

DiskFileB& DiskFileB::operator>>(short &val)
{
  df_Read(&val, sizeof(short));
  return *this;
}

DiskFileB& DiskFileB::operator>>(float &val)
{
  df_Read(&val, sizeof(float));
  return *this;
}

DiskFileB& DiskFileB::operator>>(double &val)
{
  df_Read(&val, sizeof(double));
  return *this;
}

#if defined (__64_BIT_DATABASE_ENGINE__)
DiskFileB& DiskFileB::operator<<(const __LLWORD__ val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(__LLWORD__));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      LLWORDToString(sbuf, val, 16);
    }
    else if(df_output_hex == 2) { // Base 8
      LLWORDToString(sbuf, val, 8);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      LLWORDToString(sbuf, val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator<<(const __ULLWORD__ val)
{
  if(df_output_bin) {
    df_Write(&val, sizeof(__ULLWORD__));
  }
  else {
    char sbuf[81];
    if(df_output_hex == 1) { // Base 16
      ULLWORDToString(sbuf, val, 16);
    }
    else if(df_output_hex == 2) { // Base 8
      ULLWORDToString(sbuf, val, 8);
    }
    else if(df_output_hex == 3) { // Shorthand notation
      UString byte_string;
      byte_string.SHN(); byte_string << val;
      df_Write((char *)byte_string.GetSPtr(), byte_string.length()); 
      return *this;
    }
    else { // Default to base 10
      ULLWORDToString(sbuf, val);
    }
    if((df_output_width > 0) && ((int)strlen(sbuf) < df_output_width)) {
      int i = df_output_width - strlen(sbuf);
      while(i--) df_Write(&df_output_fill, sizeof(char));   
    }
    df_Write((char *)sbuf, strlen(sbuf)); 
  }
  return *this;
}

DiskFileB& DiskFileB::operator>>(__LLWORD__ &val)
{
  df_Read(&val, sizeof(__LLWORD__));
  return *this;
}

DiskFileB& DiskFileB::operator>>(__ULLWORD__ &val)
{
  df_Read(&val, sizeof(__ULLWORD__));
  return *this;
}
#endif

int DiskFileB::df_copy(const char *from, const char *to, unsigned buf_size, 
		       int overwrite, dfChecksumRoutine CalcFileChecksum,
		       unsigned char *checksum_value, unsigned *value_len)
{
  if(!from) return df_last_error = DiskFileB::df_NULL_POINTER_ERROR;
  if(!to) return df_last_error = DiskFileB::df_NULL_POINTER_ERROR;
  
  if(!futils_exists(from)) {
    return df_last_error = DiskFileB::df_NOFILE_EXISTS_SRC_ERROR;
  }
  if(futils_isdirectory(from)) {
    return df_last_error = DiskFileB::df_BAD_FILENAME_SRC_ERROR;
  }
  if(futils_exists(to)) {
    if(futils_isdirectory(to)) {
      return df_last_error = DiskFileB::df_BAD_FILENAME_DEST_ERROR;
    }
  }
 
  if(!overwrite) {
    // Use file checksum values to see if the files are the same
    if(CalcFileChecksum) {
      if((futils_exists(from)) && (futils_exists(to))) {
	unsigned char src_sig[df_MAX_CHECKSUM_VALUE];
	unsigned char dest_sig[df_MAX_CHECKSUM_VALUE];
	memset(src_sig, 0, df_MAX_CHECKSUM_VALUE);
	memset(dest_sig, 0, df_MAX_CHECKSUM_VALUE);
	unsigned src_sig_len = 0;
	unsigned dest_sig_len = 0;
	if((*CalcFileChecksum)(from, src_sig, &src_sig_len) != 0) {
	  return df_last_error = DiskFileB::df_CHECKSUM_SRC_ERROR;
	}
	if((*CalcFileChecksum)(from, dest_sig, &dest_sig_len) != 0) {
	  return df_last_error = DiskFileB::df_CHECKSUM_DEST_ERROR;
	}
	if(src_sig_len != dest_sig_len) {
	  return df_last_error = DiskFileB::df_CHECKSUM_ERROR;
	}
	if(memcmp(src_sig, dest_sig, src_sig_len) == 0) {
	  if(checksum_value) memmove(checksum_value, src_sig, src_sig_len);
	  if(value_len) *(value_len) = src_sig_len;
	  return df_last_error = DiskFileB::df_NO_ERROR;
	}
	else {
	  overwrite = 1;
	}
      }
      else {
	overwrite = 1;
      }
    }
  }

  if(!overwrite) {
    // Use file size and mod time to see if the files are the same
    if((futils_exists(from)) && (futils_exists(to))) {
#if defined (__64_BIT_DATABASE_ENGINE__)
      __LLWORD__ from_len = futils_filelength(from);
      __LLWORD__ to_len = futils_filelength(to);
      if(from_len < (__LLWORD__)0) {
	return df_last_error = DiskFileB::df_GET_FILELENGTH_SRC_ERROR;
      }
      if(to_len < (__LLWORD__)0) {
	return df_last_error = DiskFileB::df_GET_FILELENGTH_DEST_ERROR;
      }
#else
      __ULWORD__ from_len = futils_filelength(from);
      __ULWORD__ to_len = futils_filelength(to);
      if(from_len < (__ULWORD__)0) {
	return df_last_error = DiskFileB::df_GET_FILELENGTH_SRC_ERROR;
      }
      if(to_len < (__ULWORD__)0) {
	return df_last_error = DiskFileB::df_GET_FILELENGTH_DEST_ERROR;
      }
#endif
      if(from_len == to_len) {
	// The file length is the same
	time_t access_time1, mod_time1, create_time1;
	if(futils_timestamp(from, access_time1, mod_time1, create_time1) != 0) {
	  return df_last_error = DiskFileB::df_GET_TIMESTAMP_SRC_ERROR;
	}
	time_t access_time2, mod_time2, create_time2;
	if(futils_timestamp(to, access_time2, mod_time2, create_time2) != 0) {
	  return df_last_error = DiskFileB::df_GET_TIMESTAMP_DEST_ERROR;
	}
	if(mod_time1 == mod_time2) {
	  return df_last_error = DiskFileB::df_NO_ERROR;
	}
	else {
	  overwrite = 1;
	}
      }
    }
    else {
      overwrite = 1;
    }
  }

  DiskFileB in(from);
  if(!in) return df_last_error = in.df_GetError();

  DiskFileB out(to, DiskFileB::df_READWRITE, DiskFileB::df_CREATE);
  if(!out) return df_last_error = out.df_GetError();

  char *buf = new char[buf_size];
  while(!in.df_EOF()) {
    if(in.df_Read(buf, buf_size) != DiskFileB::df_NO_ERROR) {
      if(in.df_GetError() != DiskFileB::df_EOF_ERROR) break;
      in.df_SetError(DiskFileB::df_NO_ERROR); // Reset the EOF error
    }
    if(out.df_Write(buf, in.df_gcount()) != DiskFileB::df_NO_ERROR) {
      in.df_Close();
      out.df_Close();
      delete[] buf;
      return df_last_error = out.df_GetError();
    }
  }
  delete[] buf;
  in.df_Close();
  out.df_Close();

  if(in.df_GetError() != DiskFileB::df_NO_ERROR) return df_last_error = in.df_GetError();
  if(out.df_GetError() != DiskFileB::df_NO_ERROR) return df_last_error = out.df_GetError();

  time_t access_time, mod_time, create_time;
  if(futils_timestamp(from, access_time, mod_time, create_time) != 0) {
    return df_last_error = DiskFileB::df_SET_TIMESTAMP_SRC_ERROR;
  }
  if(futils_set_timestamp(to, access_time, mod_time) != 0)  {
    return df_last_error = DiskFileB::df_SET_TIMESTAMP_DEST_ERROR;
  }

  return df_last_error = DiskFileB::df_NO_ERROR;
}

#ifdef __BCC32__
#pragma warn .8057
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
