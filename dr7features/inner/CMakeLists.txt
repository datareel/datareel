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
# 11/17/2026: object replication (gxv7ObjRepl) needs the 7.4 tree with
# gxObjRepl and the canonical gxStream block header (7.4 beta 5, at or
# after the objrepl-lib merge 41b6e15): gxobjrepl.h, no gxrepl, and the
# copy's lag age from 846245d (gxobjrepl.cpp, "oldest > behind").
set(_dr7_min "DataReel 7.4 at or after 41b6e15 (gxObjRepl, objrepl-lib 846245d) is required")
if(NOT EXISTS "${DATAREEL7_DIR}/include/gxobjrepl.h" OR
   NOT EXISTS "${DATAREEL7_DIR}/src/gxobjrepl.cpp")
  message(FATAL_ERROR "${DATAREEL7_DIR} has no gxObjRepl (gxobjrepl.h): ${_dr7_min}")
endif()
if(EXISTS "${DATAREEL7_DIR}/utils/gxrepl")
  message(FATAL_ERROR "${DATAREEL7_DIR} still has gxrepl (a 7.4 beta before beta 5): ${_dr7_min}")
endif()
file(STRINGS "${DATAREEL7_DIR}/src/gxobjrepl.cpp" _dr7_lag REGEX "oldest > behind")
if(NOT _dr7_lag)
  message(FATAL_ERROR "${DATAREEL7_DIR}: gxobjrepl.cpp is older than 846245d: ${_dr7_min}")
endif()
message(STATUS "DataReel 7.x features from ${DATAREEL7_DIR} (DataReel ${_dr7_version})")

set(DATAREEL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${DATAREEL7_DIR}" datareel7)

add_library(dr7inner STATIC dr7inner.cpp)
target_include_directories(dr7inner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(dr7inner PRIVATE DataReel::datareel)
set_target_properties(dr7inner PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The DataReel 7.x programs a 6.x installation uses as separate
# processes (DR7_TOOLS, on by default), built from the 7.x tree as the
# 7.x build makes them, into <build>/dr7inner/tools:
#   objrepl, objrepl_sim, test_gxobjrepl_api  object replication (gxObjRepl), every platform
#   vix_rebuild                               every platform
#   object_server, object_client              the database object server (POSIX)
#   vector_server                             6.4: the object server with a vector set (POSIX, below)
# 11/17/2026: gxrepl went from DataReel 7.4 (beta 5): replication is gxObjRepl.
option(DR7_TOOLS "Build the DataReel 7.x tools (objrepl, object server)" ON)
if(DR7_TOOLS)
  set(_u "${DATAREEL7_DIR}/utils")
  add_executable(vix_rebuild "${_u}/vix_rebuild/vix_rebuild.cpp")
  add_executable(objrepl "${_u}/objrepl/objrepl.cpp")
  add_executable(objrepl_sim "${_u}/objrepl/objrepl_sim.cpp")
  add_executable(test_gxobjrepl_api "${_u}/objrepl/test_gxobjrepl_api.cpp")
  set(_dr7_tools vix_rebuild objrepl objrepl_sim test_gxobjrepl_api)
  if(WIN32)
    target_link_libraries(test_gxobjrepl_api PRIVATE ws2_32)
  else()
    add_executable(object_server "${_u}/dbserver/object_server.cpp")
    add_executable(object_client "${_u}/dbserver/object_client.cpp")
    # The 7.4 object_server serves one database without vectors; the
    # 6.x vector_client test (examples_socket/vector_client) needs one
    # with a vector set: vector_server.cpp (7.x headers only)
    add_executable(vector_server vector_server.cpp)
    list(APPEND _dr7_tools object_server object_client vector_server)
  endif()
  foreach(_t ${_dr7_tools})
    target_link_libraries(${_t} PRIVATE DataReel::datareel)
  endforeach()
  set_target_properties(${_dr7_tools} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
endif()
