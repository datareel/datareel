// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: replication_extractor_so.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/15/2026
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

The replication example's extractor (replication_records.h) as a shared
library, for an unchanged gxrepl (DataReel 7.4, gxrextract.h): a set's
"derived_extractor = /path/to/example_records.so" makes gxrepl dlopen it
and call gxr_extractor() for the table of calls. The library is built
without DataReel (CMake MODULE, no link): it uses gxrepl's own copy of the
library, which gxrepl exports to it (ENABLE_EXPORTS), so the index pages it
writes go through gxrepl's staged scope like a built-in extractor's.

The same records and indexes as the replication example; the walkthrough
runs with it as:
  EXTRACTOR=BUILD/examples/example_records.so GXREPL=BUILD/utils/gxrepl \
    examples/oodb/replication_walkthrough.sh BUILD/examples
POSIX only.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"
#include "replication_records.h"

extern "C" const datareel::gxrExtractorOps *gxr_extractor(void)
{
  return example::extractor();
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
