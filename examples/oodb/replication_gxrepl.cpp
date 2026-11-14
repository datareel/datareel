// ----------------------------------------------------------- //
// C++ Source Code File Name: replication_gxrepl.cpp
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

The replication example's own gxrepl (replication_gxrepl): the gxrepl
program (utils/gxrepl/gxrepl.cpp, built with GXREPL_EXTRA_EXTRACTORS)
plus this file, which registers the example's extractor, so a set can say
"derived_extractor = example_records". An application builds its gxrepl
the same way with its own extractors: built in, never loaded from a
file, so the extractor and gxrepl never drift apart between nodes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"
#include "gxrepl.h"
#include "replication_records.h"

void gxrepl_register_extractors()
{
  datareel::gxrRegisterExtractor("example_records", example::extractor());
}
