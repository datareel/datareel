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

# The DataReel 7.x programs a 6.x installation uses as separate
# processes (DR7_TOOLS, on by default): gxrepl (database replication)
# and the database object server and client on POSIX, vix_rebuild
# everywhere. They are the 7.x programs, built from the 7.x tree as the
# 7.x build makes them, into <build>/dr7inner/tools.
option(DR7_TOOLS "Build the DataReel 7.x tools (gxrepl, object server)" ON)
if(DR7_TOOLS)
  set(_u "${DATAREEL7_DIR}/utils")
  add_executable(vix_rebuild "${_u}/vix_rebuild/vix_rebuild.cpp")
  target_link_libraries(vix_rebuild PRIVATE DataReel::datareel)
  set(_dr7_tools vix_rebuild)
  if(NOT WIN32)
    add_executable(gxrepl "${_u}/gxrepl/gxrepl.cpp")
    target_link_libraries(gxrepl PRIVATE DataReel::datareel)
    set_target_properties(gxrepl PROPERTIES ENABLE_EXPORTS ON)
    include("${_u}/gxrepl/gxrepl_version.cmake")
    gxrepl_stamp(gxrepl)
    add_executable(object_server "${_u}/dbserver/object_server.cpp")
    target_link_libraries(object_server PRIVATE DataReel::datareel)
    add_executable(object_client "${_u}/dbserver/object_client.cpp")
    target_link_libraries(object_client PRIVATE DataReel::datareel)
    list(APPEND _dr7_tools gxrepl object_server object_client)
  endif()
  set_target_properties(${_dr7_tools} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
endif()

# Where the 6.x build finds the two libraries (one line each)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/dr7inner_files.txt"
  CONTENT "$<TARGET_FILE:dr7inner>\n$<TARGET_FILE:datareel>\n")
