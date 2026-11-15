# dr7features/inner/CMakeLists.txt - DataReel 6.4: builds the DataReel
# 7.x library from a 7.x source tree (DATAREEL7_DIR) and the plain
# interface dr7inner (dr7inner.cpp, 7.x headers only). The DataReel 6.x
# CMakeLists.txt runs this project as an ExternalProject when
# DATAREEL7_FEATURES_DIR is set; it is not meant to be used directly.
cmake_minimum_required(VERSION 3.16)
project(dr7inner LANGUAGES CXX)

if(NOT DATAREEL7_DIR OR NOT EXISTS "${DATAREEL7_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "DATAREEL7_DIR must be a DataReel 7.x source tree")
endif()
if(NOT EXISTS "${DATAREEL7_DIR}/include/gxvindex.h" OR
   NOT EXISTS "${DATAREEL7_DIR}/include/gxvset.h")
  message(FATAL_ERROR "${DATAREEL7_DIR} has no vector index (gxvindex.h): "
    "DataReel 7.4 or later is required")
endif()
file(STRINGS "${DATAREEL7_DIR}/version.txt" _dr7_version LIMIT_COUNT 1)
if(_dr7_version VERSION_LESS "7.4")
  message(FATAL_ERROR "DataReel ${_dr7_version} in ${DATAREEL7_DIR}: "
    "DataReel 7.4 or later is required")
endif()
message(STATUS "DataReel 7.x features from ${DATAREEL7_DIR} (DataReel ${_dr7_version})")

set(DATAREEL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${DATAREEL7_DIR}" datareel7)

add_library(dr7inner STATIC dr7inner.cpp)
target_include_directories(dr7inner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(dr7inner PRIVATE DataReel::datareel)
set_target_properties(dr7inner PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Where the 6.x build finds the two libraries (one line each)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/dr7inner_files.txt"
  CONTENT "$<TARGET_FILE:dr7inner>\n$<TARGET_FILE:datareel>\n")
