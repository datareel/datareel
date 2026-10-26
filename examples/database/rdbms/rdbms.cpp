// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: rdbms.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
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

gxRDBMS relational table demo. Creates an employee table with a
primary key on the employee name and a secondary key on the
department, adds records, finds one by key, changes it, searches the
table and lists it in key order. The table is stored in employees.gxd
with index files employees.gix1 and employees.gix2.

Usage: rdbms [directory]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include "gxrdbms.h"

static void print_record(gxrdDatabaseRecord &r)
{
  gxINT32 id = 0;
  gxFLOAT64 salary = 0;
  CDate hired;
  r.GetField(&id, sizeof(id), 0);
  r.GetField(&salary, sizeof(salary), 3);
  r.GetField(&hired, sizeof(hired), 4);
  std::cout << "  " << (long)id << "  " << (const char *)r.GetField(1)
	    << ", " << (const char *)r.GetField(2) << ", "
	    << (double)salary << ", hired " << hired.str() << "\n";
}

static int add_employee(gxRDBMS &db, long id, const char *name,
			const char *dept, double salary, const char *hired)
{
  gxrdDatabaseRecord rec(db.RecordDef());
  gxINT32 i = (gxINT32)id;
  gxFLOAT64 s = salary;
  CDate d(hired);
  rec.SetField(&i, sizeof(i), 0);
  rec.SetField(name, (__ULWORD__)strlen(name) + 1, 1); // Variable length
  rec.SetField(dept, (__ULWORD__)strlen(dept) + 1, 2);
  rec.SetField(&s, sizeof(s), 3);
  rec.SetField(&d, sizeof(d), 4);
  if(!db.AddRecord(&rec)) {
    std::cout << "Not added: " << name << " ("
	      << db.GetDatabase()->DataFileExceptionMessage() << ")\n";
    return -1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  // Columns: ID, Name (primary key), Department (secondary key),
  // Salary, Hire date. {n} is the display width and [n] the size.
  std::string tdef = "employees, ID{6}INT32, Name{30}VCHAR[64], "
    "Department{20}CHAR[32], Salary{10}FLOAT, Hired{10}DATE";
  // ALLOWDUP lets several records share a department key value
  std::string kdef = "PRI: COL 2, SEC: COL 3, ATTRIB: NOCASE, ATTRIB: ALLOWDUP";

  gxRDBMS db;
  gxDatabaseError err = argc > 1 ? db.CreateTable(tdef, kdef, argv[1]) : 
    db.CreateTable(tdef, kdef);
  if(err != gxDBASE_NO_ERROR) {
    std::cerr << gxDatabaseExceptionMessage(err) << "\n";
    return 1;
  }
  std::cout << "Record definition: " << db.RecordDef() << "\n";

  if(db.IsEmpty()) {
    add_employee(db, 101, "Alice Jones", "Engineering", 125000.0, "3/14/2019");
    add_employee(db, 102, "Bob Brown", "Support", 72000.0, "6/1/2021");
    add_employee(db, 103, "Carol Smith", "Sales", 88000.0, "11/20/2017");
    add_employee(db, 104, "Dave Lee", "Engineering", 118000.0, "1/9/2023");
    add_employee(db, 105, "Erin Diaz", "Support", 69000.0, "8/30/2024");
  }
  std::cout << db.NumRecords() << " records\n";

  // Find by primary key. Only the key field is needed and NOCASE
  // makes the match ignore case.
  gxrdDatabaseRecord found(db.RecordDef());
  found.SetField("BOB BROWN", 10, 1);
  FAU_t addr;
  if(!db.FindRecord(&found, addr)) {
    std::cerr << "Bob Brown not found\n";
    return 1;
  }
  std::cout << "Found:\n";
  print_record(found);

  // Change the salary
  gxrdDatabaseRecord changed(found);
  gxFLOAT64 raise = 76000.0;
  changed.SetField(&raise, sizeof(raise), 3);
  if(!db.ChangeRecord(&found, &changed)) {
    std::cerr << "Change failed\n";
    return 1;
  }
  std::cout << "Changed Bob Brown's salary\n";

  // Search the department index (one-based field 3) for Engineering
  std::set<gxrdDatabaseRecord> matches;
  std::string dept = "engineering";
  // Arguments: index 1 (the secondary key), one-based field 3, no case
  // compare, no alphanumeric compare, exact match, at most every record
  err = db.Search(matches, dept, 1, 3, 0, 0, 0, db.NumRecords());
  if(err != gxDBASE_NO_ERROR) {
    std::cerr << "Search failed: " << gxDatabaseExceptionMessage(err) << "\n";
    return 1;
  }
  std::cout << "Engineering (" << matches.size() << "):\n";
  for(const auto &m : matches) {
    gxrdDatabaseRecord r(m);
    print_record(r);
  }

  // List the whole table in primary key order
  std::set<gxrdDatabaseRecord> all;
  FAU_t n = db.NumRecords();
  db.SortOrderList(all, n, 1, n); // Limit, first and last record
  std::cout << "All employees:\n";
  for(const auto &m : all) {
    gxrdDatabaseRecord r(m);
    print_record(r);
  }
  db.Close();
  return matches.size() == 2 ? 0 : 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
