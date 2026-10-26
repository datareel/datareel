// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: systime.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

Thread-safe time stamp functions used by the log file, mail and
HTTP classes. Use std::chrono for time arithmetic.

Changes:
==============================================================
10/26/2026: DataReel 7 replaces the SysTime and SysTimeUnit
classes with these header-only functions in the datareel
namespace. smtp_time() now returns an RFC 5322 date with a
numeric time zone offset.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_SYSTIME_HPP__
#define __GX_SYSTIME_HPP__

#include <ctime>
#include <string>

namespace datareel {

// Thread-safe localtime() and gmtime()
inline std::tm local_tm(std::time_t t)
{
  std::tm tm_buf{};
#if defined (_WIN32)
  localtime_s(&tm_buf, &t);
#else
  localtime_r(&t, &tm_buf);
#endif
  return tm_buf;
}

inline std::tm utc_tm(std::time_t t)
{
  std::tm tm_buf{};
#if defined (_WIN32)
  gmtime_s(&tm_buf, &t);
#else
  gmtime_r(&t, &tm_buf);
#endif
  return tm_buf;
}

// Format a time with strftime() codes, local time unless utc is true
inline std::string time_string(const char *fmt, std::time_t t,
			       bool utc = false)
{
  std::tm tm_buf = utc ? utc_tm(t) : local_tm(t);
  char buf[256];
  std::size_t n = std::strftime(buf, sizeof(buf), fmt, &tm_buf);
  return std::string(buf, n);
}

inline std::string time_string(const char *fmt, bool utc = false)
{
  return time_string(fmt, std::time(nullptr), utc);
}

// Oct 26, 2026 14:05:09 (October with full_month_name)
inline std::string date_time_string(bool full_month_name = false)
{
  return time_string(full_month_name ? "%B %d, %Y %H:%M:%S" :
		     "%b %d, %Y %H:%M:%S");
}

// Oct 26 14:05:09
inline std::string syslog_time()
{
  return time_string("%b %d %H:%M:%S");
}

// Mon, 26 Oct 2026 14:05:09 -0500 (RFC 5322, used by SMTP)
inline std::string smtp_time()
{
  return time_string("%a, %d %b %Y %H:%M:%S %z");
}

// Mon, 26 Oct 2026 19:05:09 GMT (RFC 9110, used by HTTP)
inline std::string http_time()
{
  return time_string("%a, %d %b %Y %H:%M:%S GMT", true);
}

// 2026-10-26T19:05:09Z
inline std::string iso8601_utc()
{
  return time_string("%Y-%m-%dT%H:%M:%SZ", true);
}

} // namespace datareel

#endif // __GX_SYSTIME_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
