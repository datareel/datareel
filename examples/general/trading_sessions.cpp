// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: trading_sessions.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

US stock market sessions with CDate and the systime functions.

CDate is DataReel's 4-byte date (month, day, 16-bit year), the type to
store in a database record. It parses and prints M/D/YYYY and compares
dates, but it does no calendar arithmetic, so this program adds the
day-of-week, next-day and trading-day math with std::tm and mktime().
The systime.h functions format time stamps (time_string(),
iso8601_utc()) and give thread-safe local and UTC broken-down times.

The rules shown (all times US Eastern, ET):

  pre-market   4:00 to 9:30
  regular      9:30 to 16:00
  post-market  16:00 to 20:00
  closed       20:00 to 4:00, weekends, market holidays

A holiday list for 2026 is built in (the NYSE full-day closures).
Early closes (1:00 PM the day after Thanksgiving and on Christmas Eve)
end the regular session at 13:00 and the post-market at 17:00.

The program prints:
 1. The session for a set of sample ET times, including weekends,
    holidays and the session edges (9:30 belongs to regular, 16:00
    to post-market).
 2. The trading days in November 2026 and the next trading day after
    some dates.
 3. The current session in New York right now, when the system has
    the America/New_York time zone (POSIX systems; set TZ yourself
    on others).

Usage: trading_sessions [M/D/YYYY HH:MM]

  With a date and a time (ET) it prints only that time's session.

Exits with 0 when every sample matches its expected session.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include "cdate.h"
#include "strutil.h"
#include "systime.h"

using namespace datareel;

enum Session { CLOSED, PRE_MARKET, REGULAR, POST_MARKET };

static const char *session_name(Session s)
{
  switch(s) {
    case PRE_MARKET: return "pre-market";
    case REGULAR: return "regular";
    case POST_MARKET: return "post-market";
    default: return "closed";
  }
}

// NYSE full-day closures in 2026
static const std::vector<CDate> HOLIDAYS_2026 = {
  CDate(1, 1, 2026),   // New Year's Day
  CDate(1, 19, 2026),  // Martin Luther King Jr. Day
  CDate(2, 16, 2026),  // Washington's Birthday
  CDate(4, 3, 2026),   // Good Friday
  CDate(5, 25, 2026),  // Memorial Day
  CDate(6, 19, 2026),  // Juneteenth
  CDate(7, 3, 2026),   // Independence Day (observed, July 4 is a Saturday)
  CDate(9, 7, 2026),   // Labor Day
  CDate(11, 26, 2026), // Thanksgiving
  CDate(12, 25, 2026), // Christmas
};

// Early closes: regular session ends at 13:00, post-market at 17:00
static const std::vector<CDate> EARLY_CLOSES_2026 = {
  CDate(11, 27, 2026), // Day after Thanksgiving
  CDate(12, 24, 2026), // Christmas Eve
};

static bool in_list(const std::vector<CDate> &list, const CDate &d)
{
  for(const CDate &h : list) if(h == d) return true;
  return false;
}

// Day of the week, 0 = Sunday. CDate has no calendar math, so convert
// to a std::tm and let mktime() fill in the weekday. Noon avoids any
// daylight saving edge.
static int weekday(const CDate &d)
{
  std::tm t{};
  t.tm_year = (int)d.GetYear() - 1900;
  t.tm_mon = d.GetMonth() - 1;
  t.tm_mday = d.GetDay();
  t.tm_hour = 12;
  t.tm_isdst = -1;
  std::mktime(&t);
  return t.tm_wday;
}

// The next calendar day. mktime() normalizes day 32 into the next month.
static CDate next_day(const CDate &d)
{
  std::tm t{};
  t.tm_year = (int)d.GetYear() - 1900;
  t.tm_mon = d.GetMonth() - 1;
  t.tm_mday = d.GetDay() + 1;
  t.tm_hour = 12;
  t.tm_isdst = -1;
  std::mktime(&t);
  return CDate((__UBYTE__)(t.tm_mon + 1), (__UBYTE__)t.tm_mday, (gxUINT16)(t.tm_year + 1900));
}

static bool is_trading_day(const CDate &d)
{
  int wd = weekday(d);
  if(wd == 0 || wd == 6) return false;
  return !in_list(HOLIDAYS_2026, d);
}

static CDate next_trading_day(const CDate &d)
{
  CDate n = next_day(d);
  while(!is_trading_day(n)) n = next_day(n);
  return n;
}

// The session at a date and a time of day in ET (minutes after midnight)
static Session session_at(const CDate &d, int minutes)
{
  if(!is_trading_day(d)) return CLOSED;
  bool early = in_list(EARLY_CLOSES_2026, d);
  const int pre_open = 4 * 60, open = 9 * 60 + 30;
  const int close = early ? 13 * 60 : 16 * 60;
  const int post_close = early ? 17 * 60 : 20 * 60;
  if(minutes < pre_open) return CLOSED;
  if(minutes < open) return PRE_MARKET;   // 9:30 itself is regular
  if(minutes < close) return REGULAR;     // 16:00 itself is post-market
  if(minutes < post_close) return POST_MARKET;
  return CLOSED;
}

static const char *day_name(int wd)
{
  static const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return names[wd % 7];
}

// Parse "HH:MM" into minutes, -1 if it is not a time
static int parse_hhmm(const std::string &s)
{
  auto f = split(s, ':');
  if(f.size() != 2) return -1;
  int h = to_int(f[0]), m = to_int(f[1]);
  if(h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

int main(int argc, char **argv)
{
  if(argc > 2) { // One date and time from the command line
    CDate d;
    int minutes = parse_hhmm(argv[2]);
    if(!d.SetDate(argv[1]) || minutes < 0) {
      std::cerr << "Usage: " << argv[0] << " [M/D/YYYY HH:MM]\n";
      return 1;
    }
    std::cout << d << " " << argv[2] << " ET (" << day_name(weekday(d)) << "): "
	      << session_name(session_at(d, minutes)) << "\n";
    return 0;
  }

  // 1. Sample times, with the session each one should be in
  struct Sample { const char *date, *time; Session expect; const char *why; };
  const Sample samples[] = {
    {"11/9/2026", "03:59", CLOSED, "one minute before the pre-market"},
    {"11/9/2026", "04:00", PRE_MARKET, "pre-market opens"},
    {"11/9/2026", "09:29", PRE_MARKET, "last pre-market minute"},
    {"11/9/2026", "09:30", REGULAR, "the opening bell"},
    {"11/9/2026", "15:59", REGULAR, "last regular minute"},
    {"11/9/2026", "16:00", POST_MARKET, "the closing bell starts post-market"},
    {"11/9/2026", "19:59", POST_MARKET, "last post-market minute"},
    {"11/9/2026", "20:00", CLOSED, "post-market is over"},
    {"11/7/2026", "10:00", CLOSED, "a Saturday"},
    {"11/8/2026", "10:00", CLOSED, "a Sunday"},
    {"11/26/2026", "10:00", CLOSED, "Thanksgiving"},
    {"11/27/2026", "12:59", REGULAR, "early close day, before 13:00"},
    {"11/27/2026", "13:00", POST_MARKET, "early close day, after the bell"},
    {"11/27/2026", "17:00", CLOSED, "early close day, post-market ends at 17:00"},
    {"7/3/2026", "10:00", CLOSED, "Independence Day, observed"},
  };
  int failures = 0;
  std::cout << "Sessions at sample times (ET):\n";
  for(const Sample &s : samples) {
    CDate d(s.date);
    Session got = session_at(d, parse_hhmm(s.time));
    bool ok = got == s.expect;
    if(!ok) failures++;
    std::cout << format("  %-10s %s %s  %-11s %-3s %s\n", d.str().c_str(),
			day_name(weekday(d)), s.time, session_name(got),
			ok ? "ok" : "BAD", s.why);
  }

  // 2. The trading days in a month, and the next trading day
  std::cout << "Trading days in November 2026:";
  int count = 0;
  for(CDate d(11, 1, 2026); d.GetMonth() == 11; d = next_day(d)) {
    if(is_trading_day(d)) {
      std::cout << (count % 10 == 0 ? "\n  " : " ") << (int)d.GetDay();
      count++;
    }
  }
  std::cout << "\n  " << count << " trading days\n";
  if(count != 20) failures++; // 21 weekdays, less Thanksgiving

  struct Next { const char *from, *expect; };
  const Next nexts[] = {
    {"11/6/2026", "11/9/2026"},    // Friday -> Monday
    {"11/25/2026", "11/27/2026"},  // Over Thanksgiving
    {"12/24/2026", "12/28/2026"},  // Over Christmas and a weekend
    {"7/2/2026", "7/6/2026"},      // Over the observed July 4
  };
  for(const Next &n : nexts) {
    CDate from(n.from), got = next_trading_day(from);
    bool ok = got == CDate(n.expect);
    if(!ok) failures++;
    std::cout << "  next trading day after " << from << " (" << day_name(weekday(from))
	      << ") is " << got << " (" << day_name(weekday(got)) << ")"
	      << (ok ? "" : " BAD") << "\n";
  }

  // 3. Right now in New York. The systime.h functions take the time
  // zone from the TZ setting, so set it for this process only.
#if !defined(_WIN32)
  setenv("TZ", "America/New_York", 1);
  tzset();
  std::time_t now = std::time(nullptr);
  std::tm et = local_tm(now);
  CDate today((__UBYTE__)(et.tm_mon + 1), (__UBYTE__)et.tm_mday, (gxUINT16)(et.tm_year + 1900));
  std::cout << "Now: " << time_string("%Y-%m-%d %H:%M:%S %Z", now) << " ("
	    << iso8601_utc() << "), session: "
	    << (today.GetYear() == 2026 ? session_name(session_at(today, et.tm_hour * 60 + et.tm_min))
		: "unknown (the holiday list is for 2026)") << "\n";
#endif

  std::cout << (failures == 0 ? "All samples match" : "Some samples differ") << "\n";
  return failures == 0 ? 0 : 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
