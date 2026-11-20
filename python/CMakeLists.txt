# DataReel 7.5 beta: the datareel Python module's extension (_datareel), built by setup.py. It builds the DataReel
# library from DATAREEL_SOURCE (the repository top) as a static library with the 64-bit database engine and links it
# into the extension. OpenSSL is linked from the system (libssl.so.3 on RHEL 9 and 10).
cmake_minimum_required(VERSION 3.16)
project(datareel_python LANGUAGES CXX)
if(NOT DATAREEL_SOURCE)
  get_filename_component(DATAREEL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()
set(BUILD_SHARED_LIBS OFF)
set(DATAREEL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(DATAREEL_64BIT_DATABASE ON CACHE BOOL "" FORCE)
set(DATAREEL_ZLIB OFF CACHE BOOL "" FORCE)
add_subdirectory("${DATAREEL_SOURCE}" datareel EXCLUDE_FROM_ALL)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
Python3_add_library(_datareel MODULE WITH_SOABI src/_datareel.cpp)
target_link_libraries(_datareel PRIVATE DataReel::datareel)
set_target_properties(_datareel PROPERTIES CXX_VISIBILITY_PRESET hidden)
