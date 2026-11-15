DataReel 6.4 example: vector search for a DataReel 6.x program
================================================================

This program keeps its records in a DataReel 6.x gxDatabase file and
finds them by vector similarity with the DataReel 7.x vector index.
The vector index id of each record is the record's 6.x block address,
so a search result is read straight from the 6.x database.

DataReel 6.4 has no vector database code of its own. The vector index
(gxv7VectorIndex) and vector set (gxv7VectorSet) classes in
dr7features/include/dr7features.h call the DataReel 7.x classes
(gxVectorIndex, gxVectorSet), built from a DataReel 7.4 or later
source tree by the 6.x CMake build:

  cmake -S datareel-6.4 -B build -DCMAKE_BUILD_TYPE=Release \
        -DDATAREEL7_FEATURES_DIR=/path/to/datareel-7.4
  cmake --build build -j2
  cd some_empty_folder && /path/to/build/vector_index

Add -DDATAREEL_64BIT_DATABASE=ON for the 64-bit database engine; the
7.x library is built with the same engine.

What the program shows and checks:
- 2000 records in a 6.x database, their vectors in a .vix file
- Search() (HNSW, approximate) and Exact(); exact results against a
  brute force search; HNSW recall
- tag filters, Get(), Delete(), Update(), duplicate and infinite
  vectors refused, read only open, spec check, Verify()
- float16 vectors with the cosine metric, int8 vectors with the inner
  product
- a vector set (.vixset) of two day files, searched as one, with a
  day range

  vector_index --phase-c DIR

also opens the DataReel 7.4 phase C test files (DataReel 7.4 source:
utils/vix_test/data/fau64 for the 64-bit engine, fau32 for the 32-bit
engine; use a copy of the folder) and compares 28 results with the
ones DataReel 7.4 wrote in phase_c_expected.txt: a 6.x program reads
the 7.x .vix files and gets the same answers.

Using the classes in your own program:

  #include "dr7features.h"           // after or instead of 6.x headers
  gxv7VectorIndex vix;
  vix.Create("items.vix", 16);        // 16 floats a vector, L2
  vix.Insert(record_address, vec);    // id = your 6.x block address
  gxv7Result r[10]; unsigned n;
  vix.Search(query, 10, r, n);        // closest first; r[i].id

Link the program with the CMake target DataReel::dr7features (it
brings the 6.x library, the 7.x library and their system libraries).
A C++17 compiler is needed for programs that use these classes, the
same compiler for both libraries. Do not include DataReel 7.x headers
in a 6.x source file: the two versions share header names and include
guards.
