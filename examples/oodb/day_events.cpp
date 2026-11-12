// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: day_events.cpp
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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

A day-organized event database.

Many programs collect events that are only ever looked at by time: a
trade log, sensor readings, alerts, news items. A good layout for them
is one database file per day, in a folder per year and month:

  day_events/2026/11/09/events.gxd
  day_events/2026/11/10/events.gxd
  ...

This keeps every file small, makes "today" the only file being written,
and lets old days be backed up, compressed, copied to another machine
or deleted one folder at a time without touching the rest. A query for
a time range opens only the days in the range.

The program:
 1. Appends events for several days (including a gap day with no
    events). Each event is a fixed-size record: a time stamp (seconds
    since 1970, UTC), a type, a ticker and a short text. Events are
    appended in time order, so a day file is already sorted by time.
 2. Queries a time range that starts in the middle of one day and
    ends in the middle of a later one. It walks the day folders in
    the range, skips days with no file, reads each day's records in
    order and stops reading a day as soon as it passes the end time.
 3. Checks the query against the same filter run over the list of
    events it wrote.

For lookups by key (all events for one ticker, say) add a gxBtree
index file next to each day's data file; see examples/database/btree
and docs/ai/samples/indexed_records.cpp.

Usage: day_events [folder]

  folder  Where to put the day folders (default ./day_events). The
          program removes and rebuilds it, so do not point it at
          anything else.

Exits with 0 when the query returns exactly the expected events.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "gxdbase.h"
#include "strutil.h"
#include "systime.h"

using namespace datareel;
namespace fs = std::filesystem;

struct Event {
  std::int64_t when;  // Seconds since 1970-01-01 UTC
  std::int32_t type;  // 1 trade, 2 alert, 3 news
  char ticker[12];
  char text[44];
};

// Seconds since 1970 for a UTC date and time. timegm() is not in the
// C++ standard, so count the days from the civil date directly
// (the algorithm from Howard Hinnant's date library).
static std::int64_t utc_seconds(int y, int m, int d, int hh, int mm, int ss)
{
  y -= m <= 2;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  std::int64_t days = era * 146097 + (std::int64_t)doe - 719468;
  return days * 86400 + hh * 3600 + mm * 60 + ss;
}

// The day file that holds events at time t: <root>/YYYY/MM/DD/events.gxd
static fs::path day_file(const fs::path &root, std::int64_t t)
{
  std::string ymd = time_string("%Y/%m/%d", (std::time_t)t, true);
  return root / fs::path(ymd) / "events.gxd";
}

// Append one event to its day's file, creating the folders and the
// file the first time a day is written
static bool append(const fs::path &root, const Event &e)
{
  fs::path f = day_file(root, e.when);
  std::error_code ec;
  fs::create_directories(f.parent_path(), ec);
  gxDatabase db;
  std::string name = f.string();
  if(db.Open(name.c_str()) != gxDBASE_NO_ERROR &&
     db.Create(name.c_str()) != gxDBASE_NO_ERROR) {
    std::cerr << name << ": " << db.DatabaseExceptionMessage() << "\n";
    return false;
  }
  FAU_t addr = db.Alloc(sizeof(e));
  bool ok = addr != (FAU_t)0 && db.Write(&e, sizeof(e), addr) == gxDBASE_NO_ERROR;
  db.Close();
  return ok;
}

// Every event with from <= when < to, across all the day files in
// between. "days_opened" counts the day files that were read.
static std::vector<Event> query(const fs::path &root, std::int64_t from, std::int64_t to,
				int &days_opened)
{
  std::vector<Event> out;
  days_opened = 0;
  // Start at midnight UTC of the first day and step one day at a time
  for(std::int64_t day = from - from % 86400; day < to; day += 86400) {
    fs::path f = day_file(root, day);
    if(!fs::exists(f)) continue; // No events that day
    gxDatabase db;
    std::string name = f.string();
    if(db.Open(name.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) continue;
    days_opened++;
    for(FAU_t addr = db.FindFirstObject(); addr != (FAU_t)0; addr = db.FindNextObject(addr)) {
      Event e;
      if(db.ObjectLength(addr) != sizeof(e) || db.Read(&e, sizeof(e), addr) != gxDBASE_NO_ERROR)
	continue;
      if(e.when >= to) break;      // The file is in time order: done with this day
      if(e.when >= from) out.push_back(e);
    }
    db.Close();
  }
  return out;
}

static Event make_event(std::int64_t when, int type, const char *ticker, const char *text)
{
  Event e;
  memset(&e, 0, sizeof(e));
  e.when = when;
  e.type = type;
  copy_to(e.ticker, sizeof(e.ticker), ticker);
  copy_to(e.text, sizeof(e.text), text);
  return e;
}

static std::string show(const Event &e)
{
  static const char *types[] = {"?", "trade", "alert", "news"};
  return format("%s  %-5s %-5s %s", time_string("%Y-%m-%d %H:%M:%S", (std::time_t)e.when, true).c_str(),
		types[e.type >= 1 && e.type <= 3 ? e.type : 0], e.ticker, e.text);
}

int main(int argc, char **argv)
{
  fs::path root = argc > 1 ? argv[1] : "day_events";
  std::error_code ec;
  fs::remove_all(root, ec); // Start from an empty folder each run

  // 1. Events over five days: Nov 9, 10, 11, (nothing on the 12th), 13.
  // Times are UTC; 14:30 UTC is 9:30 AM in New York in November.
  std::vector<Event> written;
  const char *tickers[] = {"AMTX", "SOUN", "BTCS"};
  for(int d : {9, 10, 11, 13}) {
    for(int i = 0; i < 12; i++) {
      int minutes = 13 * 60 + i * 37; // 13:00 to about 19:47 UTC
      Event e = make_event(utc_seconds(2026, 11, d, minutes / 60, minutes % 60, 0),
			   1 + i % 3, tickers[i % 3], format("event %d of Nov %d", i, d).c_str());
      if(!append(root, e)) return 1;
      written.push_back(e);
    }
  }
  std::cout << "Appended " << written.size() << " events into day folders under "
	    << root.string() << ":\n";
  for(auto &p : fs::recursive_directory_iterator(root)) {
    if(p.is_regular_file()) std::cout << "  " << p.path().generic_string() << "\n";
  }

  // 2. A range from the middle of Nov 10 to the middle of Nov 13
  std::int64_t from = utc_seconds(2026, 11, 10, 16, 0, 0);
  std::int64_t to = utc_seconds(2026, 11, 13, 15, 0, 0);
  int days_opened = 0;
  std::vector<Event> got = query(root, from, to, days_opened);
  std::cout << "Query " << time_string("%Y-%m-%d %H:%M", (std::time_t)from, true) << " to "
	    << time_string("%Y-%m-%d %H:%M", (std::time_t)to, true) << " UTC: " << got.size()
	    << " events from " << days_opened << " day files (Nov 12 has none)\n";
  for(const Event &e : got) std::cout << "  " << show(e) << "\n";

  // 3. The same filter over what was written
  std::vector<Event> expect;
  for(const Event &e : written) if(e.when >= from && e.when < to) expect.push_back(e);
  bool ok = got.size() == expect.size() && days_opened == 3;
  for(std::size_t i = 0; ok && i < got.size(); i++) {
    ok = memcmp(&got[i], &expect[i], sizeof(Event)) == 0;
  }
  std::cout << (ok ? "The query matches the events written" : "The query is WRONG") << "\n";
  return ok ? 0 : 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
