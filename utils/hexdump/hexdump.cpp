// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: hexdump.cpp 
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/13/1996 
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program description and details ------------- // 
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
 
This program is used to display the contents of a specified
file, in both hexadecimal and ASCII, to the console. 
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <iostream>
#include <iomanip>
#include <fstream>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hxcrc.h"
#include "dfileb.h"
#include "strutil.h"
#include "gxd_ver.h"

// Hexdump version number and program name
const double HexdumpVersionNumber = gxDatabaseVersion;

#if defined (__64_BIT_DATABASE_ENGINE__)
const char *ProgramName = "hexdump64";
#else // Use the 32-bit version by default
const char *ProgramName = "hexdump";
#endif

// Program constants
const int MAX_LINE = 255;  // Maximum characters per line
const int hxBuffSize = 16; // Maximum number of bytes per line
const int decBuffSize = 8; // Maximum number of bytes per line

// Program globals
char in_file[MAX_LINE];    // Input file 
char out_file[MAX_LINE];   // Optional output file name
int prompting = 0;         // Enable prompting during console dump
int use_console = 1;       // Dump to console if no output file is specified
int hx_decimal_mode = 0;   // Display in decimal mode (Default is HEX)
int c_array_mode = 0;      // Display output in a C array
int building_bin_file = 0; // True if building binary file from text file
int strip_comments = 0;    // True if not adding comments
int display_crc32 = 0;     // True if displaying 32-bit crc checksum
int display_crc16 = 0;     // True if displaying 16-bit crc checksum
int NumLines = 8;          // Default maximum number of lines to display

// Table for HEX to binary conversions
const char *bintab[16] = {
  "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
  "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"
};

// Program Functions 
void ProcessArgs(int argc, char *argv[]);
void HelpMessage(const char *program_name, const double version_number);
int HexFileDump(DiskFileB &infile, std::ostream &stream);
int CArrayDump(DiskFileB &infile, std::ostream &stream);
int DecFileDump(DiskFileB &infile, std::ostream &stream);
void BuildBinaryFile(char *ifname, char *ofname);
void TruncateLine(char *line, char tline[MAX_LINE], char delimiter1 = '\t',
		  char delimiter2 = ';');
int text_parse(char *str, char words[MAXWORDS][MAXWORDLENGTH],
	       int*numwords, char sepchar1 = ' ', char sepchar2 = ' ');

void HelpMessage(const char *program_name, const double version_number)
{
  char gxuffer[255];
  sprintf(gxuffer, "%.3f", version_number);
  std::cout << "\n";
  std::cout << "Hexadecimal file dump program version "
       << gxuffer  << "\n";
  std::cout << "Usage: " << program_name << " [switches] infile "
       << "outfile (optional)" << "\n";
  std::cout << "Switches:  -?      = Display this help message." << "\n";
  std::cout << "           -a      = Display in a C array." << "\n";
  std::cout << "           -c      = Display a 32-bit CRC checksum value." 
	      << "\n";
  std::cout << "           -C      = Display a 16-bit CRC checksum value." 
	      << "\n";
  std::cout << "           -d      = Display in decimal mode (Default is \
HEX)." << "\n";
  std::cout << "           -p#     = Prompting after displaying # line: \
-p10" << "\n";
  std::cout << "           -r      = Rebuild binary file from hex dump text \
file.\n" << "                     Only works for files created in hex \
mode." << "\n";
  std::cout << "           -s      = Strip comments from output." << "\n";
  std::cout << "\n";
  std::cout << "           -x      = Convert 32-bit HEX number to DEC: \
-Xfefe" << "\n";
  std::cout << "           -X      = Convert signed 32-bit HEX number to \
DEC." << "\n";
  std::cout << "           -B      = Convert 32-bit HEX number to binary: \
-Bfefe" << "\n";
  std::cout << "           -D      = Convert DEC number to HEX: -D23" 
	      << "\n";
  std::cout << "\n";
  exit(0);
}

void ProcessArgs(int argc, char *argv[])
// Process the program's argument list
{
  int i, ibuf;
  char *sbuf;
  unsigned long ulbuf, bbuf;
  long lbuf;
  
  for(i = 1; i < argc; i++ ) {
    if(*argv[i] == '-') {
      char sw = *(argv[i] +1);
      switch(sw) {
	case '?' :
	  HelpMessage(ProgramName, HexdumpVersionNumber);
	  break;
	  
	case 'a':
	  c_array_mode = 1;
	  break;

	case 'c':
	  display_crc32 = 1;
	  break;

	case 'C':
	  display_crc16 = 1;
	  break;

	case 'p' :
	  prompting = 1;
	  ibuf = atoi(&argv[i][2]);
	  if(ibuf > 0) NumLines = ibuf; 
	  break;

	case 'd' :
	  hx_decimal_mode = 1;
	  break;

	case 'r' :
	  building_bin_file = 1;
	  break;

	case 's' :
	  strip_comments = 1;
	  break;

	case 'x' : {
	  sbuf = &argv[i][2];
	  std::cout << "\n";
	  sscanf(sbuf, "%lx", &ulbuf); // 10/14/2026: %lx for unsigned long (was %x)
	  std::cout.setf(std::ios::uppercase);
	  std::cout << "0x" << std::hex << ulbuf << " = " << std::dec << ulbuf << "\n"; // 10/14/2026: std::hex/dec (hex/dec named DiskFileB manipulators and printed 1)
	  std::cout << "\n";
	  exit(0);
	  break;
	}
	  
	case 'X' : {
	  sbuf = &argv[i][2];
	  std::cout << "\n";
	  sscanf(sbuf, "%lx", (unsigned long *)&lbuf); // 10/14/2026: %lx for long (was %x)
	  std::cout.setf(std::ios::uppercase);
	  std::cout << "0x" << std::hex << lbuf << " = " << std::dec << lbuf << "\n";
	  std::cout << "\n";
	  exit(0);
	  break;
	}
	
	case 'D' : {
	  sbuf = &argv[i][2];
	  std::cout << "\n";
	  sscanf(sbuf, "%lu", &ulbuf); // 10/14/2026: %lu for unsigned long (was %u)
	  std::cout.setf(std::ios::uppercase);
	  std::cout << ulbuf << " = " << "0x" << std::hex << ulbuf << "\n";
	  std::cout << "\n";
	  exit(0);
	  break;
	}

	case 'B' : {
	  sbuf = &argv[i][2];
	  std::cout << "\n";
	  sscanf(sbuf, "%lx", &ulbuf); // 10/14/2026: %lx for unsigned long (was %x)
	  std::cout.setf(std::ios::uppercase);
	  std::cout << "0x" << std::hex << ulbuf << " = ";
	  bbuf = (ulbuf & 0xf0000000)>>28;
	  std::cout << bintab[bbuf] << ' ';
	  bbuf = (ulbuf & 0xf000000)>>24;
	  std::cout << bintab[bbuf] << ' ';
	  bbuf = (ulbuf & 0xf00000)>>20;
	  std::cout << bintab[bbuf] << ' ';
	  bbuf = (ulbuf & 0xf0000)>>16;
	  std::cout << bintab[bbuf] << ' ';
	  bbuf = (ulbuf & 0xf000)>>12;
	  std::cout << bintab[bbuf] << ' ';
	  bbuf = (ulbuf & 0xf00)>>8;
	  std::cout << bintab[bbuf] << ' ';
	  bbuf = (ulbuf & 0xf0)>>4;
	  std::cout << bintab[bbuf] << ' '; 
	  bbuf = ulbuf & 0x0f;
	  std::cout << bintab[bbuf] << "\n"; 
	  std::cout << "\n";
	  exit(0);
	  break;
	}
	
	default:
	  std::cerr << "\n";
	  std::cerr << "Unknown switch " << argv[i] << "\n";
	  std::cerr << "Exiting..." << "\n";
	  std::cerr << "\n";
	  exit(0);
	  break;
      }
    }
    else { 
      strcpy(in_file, argv[i]);      // Input file name
      if(argv[i+1]) {
	strcpy(out_file, argv[i+1]); // Output file name
	break;
      }
      else
	break;
    }
  }
}

int CArrayDump(DiskFileB &infile, std::ostream &stream)
{
  int i, j;
  char c[hxBuffSize];

  long len = infile.df_Length();
  long byte_total = 0;
  stream << "unsigned char CArray[" << len << "] = {\n";

  // Print all text in upper case characters
  stream.setf(std::ios::uppercase);

  while(!infile.df_EOF()) {

    if(infile.df_Read(c, hxBuffSize) != DiskFileB::df_NO_ERROR) {
      if(infile.df_GetError() != DiskFileB::df_EOF_ERROR) {
	stream << infile.df_ExceptionMessage() << "\n";
	infile.df_Close();
	return 0;
      }
    }
    i = infile.df_gcount();
    byte_total += i;
    stream << "  ";
    for(j = 0; j < i; j++) {
      long xx = (long)c[j];

      // Set fill chars and reset first 24 bits for one byte display
      stream << "0x" << std::setfill('0') << std::setw(2) << std::hex 
	     << (xx & 0xFF);
      if(byte_total == len) {
	if(j < (i-1)) stream << ",";
      }
      else {
	stream << ",";
      }
    }

    // Set the line spacing to a minimum of bytes per line
    for(; j < hxBuffSize; j++) stream << "   "; // Insert 3 spaces

    stream << "\n";
  }

  infile.df_Close();

  stream << "};" << "\n";

  return 1;
}

int HexFileDump(DiskFileB &infile, std::ostream &stream)
{
  int i, j;
  int count = 0;
  char c[hxBuffSize];

  // Print all text in upper case characters
  stream.setf(std::ios::uppercase);

  while(!infile.df_EOF()) {

    if(infile.df_Read(c, hxBuffSize) != DiskFileB::df_NO_ERROR) {
      if(infile.df_GetError() != DiskFileB::df_EOF_ERROR) {
	stream << infile.df_ExceptionMessage() << "\n";
	infile.df_Close();
	return 0;
      }
    }
    i = infile.df_gcount();
        
    for(j = 0; j < i; j++) {
      long xx = (long)c[j];

      // Set fill chars and reset first 24 bits for one byte display
      stream << std::setfill('0') << std::setw(2) << std::hex 
	     << (xx & 0xFF);
      stream << " "; // Print one space between the bytes
    }

    // Set the line spacing to a minimum of bytes per line
    for(; j < hxBuffSize; j++) stream << "   "; // Insert 3 spaces

    if(!strip_comments) {
      // Separate hex output by one tab and denote comments
      // with a semicolon.
      stream << "\t" << "; ";
	  
      // Filter out any non-printable characters
      for(j = 0; j < i; j++)
	if(isgraph(c[j])) stream << c[j];
	else stream << ".";
    }
    
    stream << "\n";
      
    if(use_console == 1 && prompting == 1) {
      count++;
      if(count == NumLines) {
	count = 0;
	stream << "\n";
	stream << "Press ENTER to continue: ";
	std::cin.get();
	stream << "\n";
      }
    }
  }

  infile.df_Close();

  return 1;
}

int DecFileDump(DiskFileB &infile, std::ostream &stream)
{
  int i, j;
  int count = 0;
  char c[decBuffSize];
  
  // Print all text in upper case characters
  stream.setf(std::ios::uppercase);

  while(!infile.df_EOF()) {

    if(infile.df_Read(c, decBuffSize) != DiskFileB::df_NO_ERROR) {
      if(infile.df_GetError() != DiskFileB::df_EOF_ERROR) {
	stream << infile.df_ExceptionMessage() << "\n";
	infile.df_Close();
	return 0;
      }
    }
    i = infile.df_gcount();
      
    for(j = 0; j < i; j++) {
      long xx = (long)c[j];

      // Set fill chars and reset first 24 bits for one byte display
	stream << std::setfill('0') << std::setw(3) << std::dec 
	       << (xx & 0xFF);
	stream << " "; // Print one space between the bytes
    }

    // Set the line spacing to a maximum number of bytes per line
    for(; j < decBuffSize; j++) stream << "    "; // Insert 4 spaces

    if(!strip_comments) {
      // Separate hex output by one tab and denote comments
      // with a semicolon.
      stream << "\t" << "; ";
	  
      // Filter out any non-printable characters
      for(j = 0; j < i; j++)
	if(isgraph(c[j])) stream << c[j];
	else stream << ".";
    }
    
    stream << "\n";
      
    if(use_console == 1 && prompting == 1) {
      count++;
      if(count == NumLines) {
	count = 0;
	stream << "\n";
	stream << "Press ENTER to continue: ";
	std::cin.get();
	stream << "\n";
      }
    }
  }

  infile.df_Close();

  return 1;
}

void BuildBinaryFile(char *ifname, char *ofname)
{
  std::cout << "Building new binary file from hex dump text file..." << "\n";
  
  if(ifname[0] == 0) {
    std::cout << "\n";
    std::cout << "You must specify a valid hex dump text file." << "\n";
    std::cout << "\n";
    exit(0);
  }

  // Default output file name if none specified on command line
  if(ofname[0] == 0) ofname = (char *)"outfile.bin";

  DiskFileB infile(ifname);
  if(!infile) {
    std::cerr << "Could not open " << ifname << "\n";
    exit(0);
  }

  DiskFileB outfile(ofname, DiskFileB::df_READWRITE, DiskFileB::df_CREATE);
  if(!outfile) {
    std::cerr << "Could not create " << ofname << "\n";
    exit(0);
  }

  char words[MAXWORDS][MAXWORDLENGTH];
  char LineBuffer[MAX_LINE];
  char InputBuffer[MAX_LINE];
  int i, num, byte_count = 0;
  long word_val;
  unsigned char byte_val;
  unsigned long CRC32 = 0xffffffffL;
  unsigned long outfile_CRC32, CRC32_check;
  hxCRC crc;
  
  // Clear the buffers
  for(i = 0; i < MAX_LINE; i++) LineBuffer[i] = '\0';
  for(i = 0; i < MAX_LINE; i++) InputBuffer[i] = '\0';
  
  while(!infile.df_EOF()) {
    infile.df_GetLine(LineBuffer, MAX_LINE);

    // Truncate the line when the first tab is found
    TruncateLine(LineBuffer, InputBuffer);

    if(text_parse(InputBuffer, words, &num) == 1) {
      std::cerr << "Parse error! Exiting..." << "\n";
      std::cerr << "\n";
      exit(0);
    }

    // Write the hex values in the text file to a binary file
    for(i = 0; i < num; i++) {
      if(words[i][0] != 0 && (strlen(words[i]) == 2)) { // 10/14/2026: Test the word, not the array address
	sscanf(words[i], "%lx", (unsigned long *)&word_val); // 10/14/2026: %lx for long (was %x)
	word_val &= 0xFF; // Reset the first 24 bits
	byte_val = (unsigned char) word_val;

	// Calculate a 32-bit CRC for each byte value
	CRC32=crc.crc32tab[(CRC32 ^ word_val)&0xFF] ^ ((CRC32>>8)&0x00ffffffL);

	outfile.df_Write((unsigned char *) &byte_val, sizeof(byte_val));
	byte_count++;
      }
    }
  
    // Clear the buffers
    for(i = 0; i < MAX_LINE; i++) LineBuffer[i] = '\0';
    for(i = 0; i < MAX_LINE; i++) InputBuffer[i] = '\0';
  }

  // Finish 32-bit CRC calculation for each byte value
  CRC32 ^= 0xffffffffL;

  // Calculate a 32-bit CRC for the output file 
  outfile_CRC32 = crc.calcCRC32(outfile);
  CRC32_check = CRC32 ^ outfile_CRC32; // XOR the CRC values
  if(CRC32_check != 0) { 
    std::cout << "Checksum error in " << ofname << "\n";   
    std::cout << "File fails CRC-32 test." << "\n";
    std::cout << "CRC of bytes written = ";
    std::cout.setf(std::ios::uppercase);
    std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex
		<< CRC32 << "\n";
    std::cout.unsetf(std::ios::uppercase);
    std::cout << "File CRC = ";
    std::cout.setf(std::ios::uppercase);
    std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex
		<< outfile_CRC32 << "\n";
    std::cout.unsetf(std::ios::uppercase);
    std::cout << "\n";
  }
  else {
    std::cout << "Finished." << "\n";
    std::cout << byte_count << " bytes written to " << ofname << "\n";
    std::cout << "\n";
  }
  infile.df_Close();
  outfile.df_Close();
}

void TruncateLine(char *line, char tline[MAX_LINE],
		  char delimiter1, char delimiter2)
// Used to truncate a line of text starting at the first occurrence
// of the delimiter characters.
{
  int len = strlen(line);
  int i, offset = 0;

  for(i = 0; i < len; i++) {
    if((line[i] == delimiter1) || (line[i] == delimiter2)) {
      offset = i;
      break;
    }
  }
  
  if(!offset) { // Delimiter character was not found
    tline[len] = 0; // Ensure null termination
    strcpy(tline, line);
    return; 
  }
  
  len -= offset;
  tline[offset] = 0; // Ensure null termination
  memmove(tline, line, offset);
}

int text_parse(char *str, char words[MAXWORDS][MAXWORDLENGTH],
	       int*numwords, char sepchar1, char sepchar2)
// General purpose text parser. Returns 0 if successful or 1 if a
// parse error occurred.
{
  int i = 0;
  char newword[MAXWORDLENGTH];
  *numwords = 0;

  // First skip over leading blanks. Stop if an ASCII NULL is seen
  while (1) {
    if (*str == '\0') return 0;
    if (*str != ' ') break;
    str++;
  }
  
  while(1) {
    // Check to see if there is room for another word in the array
    if(*numwords == MAXWORDS) return 1;

    i = 0;
    // PC-lint 10/12/2005: Possible access of out-of-bounds pointer
    while (i < MAXWORDLENGTH-1) {
      if(*str == 0 || *str == sepchar1 || *str == sepchar2) break;
      newword[i] = *str;
      str++;
      i++;
    }

    newword[i] = 0; // Ensure an ASCII null at end of newword. 
    strcpy (words[*numwords], newword);  // Install into array 
    (*numwords)++;
    
    // If stopped by an ASCII NULL above, exit loop
    if(*str == 0) break;
    
    str++;
    if(*str == 0) break;
  }
  return 0;
}

int main(int argc, char *argv[])
{
  // If no argument is given print usage message to the screen 
  if(argc < 2) {
    HelpMessage(ProgramName, HexdumpVersionNumber);
    return 0;
  }
  
  // Process the programs command line arguments
  ProcessArgs(argc, argv);

  // If building a binary file from a hex dump text file,
  // execute the build function and terminate the program.
  if(building_bin_file == 1)  {
    BuildBinaryFile(in_file, out_file);
    return 0;
  }
  
  if(in_file[0] == 0 ) {
    std::cout << "\n";
    std::cout << "You must specify a valid input file name." << "\n";
    std::cout << "\n";
    return 0;
  }

  DiskFileB infile(in_file);
  if(!infile) {
    std::cerr << "\n";
    std::cerr << "Cannot open file: " << in_file << "\n";
    std::cerr << "Exiting..." << "\n";
    std::cerr << "\n";
    return 1;
  }

  if(display_crc32) {
    hxCRC crc;
    std::cout << in_file << "\n";
    std::cout << "CRC-32 = ";
    std::cout.setf(std::ios::uppercase);
    unsigned long checksum = crc.calcCRC32(infile);
    std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex
		<< checksum << "\n";
    std::cout.unsetf(std::ios::uppercase);
    return 0;
  }

  if(display_crc16) {
    hxCRC crc;
    std::cout << in_file << "\n";
    std::cout << "CRC-16 = ";
    std::cout.setf(std::ios::uppercase);
    unsigned short checksum = crc.calcCRC16(infile);
    std::cout << "0x" << std::setfill('0') << std::setw(4) << std::hex
		<< checksum << "\n";
    std::cout.unsetf(std::ios::uppercase);
    std::cout << "CCITT CRC-16 = ";
    std::cout.setf(std::ios::uppercase);
    checksum = crc.calcCRC16(infile, 0);
    std::cout << "0x" << std::setfill('0') << std::setw(4) << std::hex
		<< checksum << "\n";
    std::cout.unsetf(std::ios::uppercase);
    return 0;
  }
  
  if(out_file[0] == 0 ) { // Write output to stdout
    use_console = 1;
    if(c_array_mode == 1) {
      CArrayDump(infile, std::cout);
    }
    else if(hx_decimal_mode == 1)
      DecFileDump(infile, std::cout);
    else
      HexFileDump(infile, std::cout);
  }
  else { // Write the output to a file
    // Using the ostream classes allows the data to be written
    // to a file, the stdout, or stderr.
    std::fstream stream(out_file, std::ios::out|std::ios::trunc);
    if(!stream) {
      std::cerr << "\n";
      std::cerr << "Cannot create " << out_file << "\n";
      std::cerr << "\n";
      return 1;
    }
    use_console = 0;
    std::cerr << "Processing file..." << "\n";
    if(c_array_mode == 1) {
      CArrayDump(infile, stream);
      stream.close();
    }
    else if(hx_decimal_mode == 1) {
      DecFileDump(infile, stream);
      stream.close();
    }
    else {
      HexFileDump(infile, stream);
      stream.close();
    }
    std::cerr << "Finished." << "\n";
  }

  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
