# CMake build for the DataReel library.
#
# Builds the same objects as build/makefile (Linux) and winslib/msvc.mak
# (Windows) into a static library target named "datareel" (alias
# DataReel::datareel). The configuration macros are PUBLIC because the
# DataReel headers change layout based on them, so programs that link the
# library are compiled with the same set.
#
# Stand-alone:
#   cmake -S . -B cmake-build
#   cmake --build cmake-build -j
#
# From another CMake project:
#   set(DATAREEL_GUI WX)               # optional, before add_subdirectory
#   add_subdirectory(path/to/datareel datareel)
#   target_link_libraries(myapp PRIVATE DataReel::datareel)
#
# The makefiles in build/, env/ and winslib/ are unchanged.

cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" _dr_version LIMIT_COUNT 1)
project(DataReel VERSION ${_dr_version} LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(WIN32)
  set(_dr_ssl_default SCHANNEL)
else()
  set(_dr_ssl_default OPENSSL)
endif()
set(DATAREEL_SSL ${_dr_ssl_default} CACHE STRING
  "SSL support: OPENSSL, SCHANNEL (Windows only) or NONE")
set_property(CACHE DATAREEL_SSL PROPERTY STRINGS OPENSSL SCHANNEL NONE)

set(DATAREEL_GUI CONSOLE CACHE STRING
  "Message display: CONSOLE or WX (wxWidgets 3.x message boxes and wxString support)")
set_property(CACHE DATAREEL_GUI PROPERTY STRINGS CONSOLE WX)

option(DATAREEL_64BIT_DATABASE
  "Build the 64-bit database engine with large file support" OFF)
option(DATAREEL_NATIVE_TYPES
  "Use native integer and float types instead of the portable gxINT/gxFLOAT classes" ON)

set(DATAREEL_SOURCES
  # Database core and extensions
  gxint32 gxint64 gxuint32 gxuint64 gxcrc32 gxdbase gxderror gxdfp64 gxdfptr
  gxdjrnl gxdpcache
  btstack btcache btnode dbasekey gpersist gxbtree pod gxint16 gxuint16
  gxfloat dbfcache dbugmgr
  # RDBMS
  gxrdbdef gxrdbhdr gxrdbms gxrdbsql
  # Sockets
  gxsocket gxshttp gxshttpc gxsmtp gxsping gxspop3 gxsimap gxsftp gxshtml gxsurl
  gxsutils gxs_b64 gxtelnet wserror gxsrss gxsxml gxssl gxdatagm gxstream
  # Serial comm
  gxscomm scomserv
  # Threads
  gthreadt gxmutex thelpers thrapiw gxthread gxsema gxcond thrpool
  # General purpose
  asprint bstreei cdate devcache dfileb fstring futils gxconfig gxip32
  gxlistb gxmac48 htmldrv logfile memblock membuf ostrbase pscript strutil
  systime ustring
  # Messages and debug
  terminal ehandler gxdstats leaktest)
list(TRANSFORM DATAREEL_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/")
list(TRANSFORM DATAREEL_SOURCES APPEND ".cpp")

add_library(datareel STATIC ${DATAREEL_SOURCES})
add_library(DataReel::datareel ALIAS datareel)
target_include_directories(datareel PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_definitions(datareel PUBLIC
  __X86__
  __USE_ANSI_CPP__
  __USE_CPP_IOSTREAM__
  __USE_BINARY_SEARCH__ __USE_SINGLE_COMPARE__
  __USE_DATABASE_CACHE__ __USE_BTREE_CACHE__
  __VMEMORY_BUFFER__
  __HAS_UNICODE__
  __USE_POSTSCRIPT_PRINTING__ __USE_HTM_PRINTING__ __USE_TEXT_PRINTING__
  $<$<NOT:$<CONFIG:Debug>>:__NO_DEBUG_OPTIONS__>)

if(DATAREEL_NATIVE_TYPES)
  target_compile_definitions(datareel PUBLIC
    __USE_NATIVE_INT_TYPES__ __USE_NATIVE_FLOAT_TYPES__)
endif()

if(DATAREEL_64BIT_DATABASE)
  target_compile_definitions(datareel PUBLIC
    __64_BIT_DATABASE_ENGINE__ __ENABLE_64_BIT_INTEGERS__)
  if(NOT WIN32)
    target_compile_definitions(datareel PUBLIC
      _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64 _LARGE_FILES)
  endif()
endif()

# Message display
if(DATAREEL_GUI STREQUAL "WX")
  find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
  target_compile_definitions(datareel PUBLIC __wxWIN3__ ${wxWidgets_DEFINITIONS})
  target_include_directories(datareel PUBLIC ${wxWidgets_INCLUDE_DIRS})
  target_compile_options(datareel PUBLIC ${wxWidgets_CXX_FLAGS})
  target_link_libraries(datareel PUBLIC ${wxWidgets_LIBRARIES})
elseif(DATAREEL_GUI STREQUAL "CONSOLE")
  target_compile_definitions(datareel PUBLIC __CONSOLE__)
else()
  message(FATAL_ERROR "DATAREEL_GUI must be CONSOLE or WX, not '${DATAREEL_GUI}'")
endif()

# SSL
if(DATAREEL_SSL STREQUAL "OPENSSL")
  find_package(OpenSSL 3 REQUIRED)
  target_compile_definitions(datareel PUBLIC __USE_GX_SSL_EXTENSIONS__)
  target_link_libraries(datareel PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  if(WIN32)
    target_link_libraries(datareel PUBLIC crypt32)
  endif()
elseif(DATAREEL_SSL STREQUAL "SCHANNEL")
  if(NOT WIN32)
    message(FATAL_ERROR "DATAREEL_SSL=SCHANNEL is only available on Windows")
  endif()
  target_compile_definitions(datareel PUBLIC
    __USE_GX_SSL_EXTENSIONS__ __USE_GX_SCHANNEL__)
  target_link_libraries(datareel PUBLIC secur32 crypt32 ncrypt)
elseif(NOT DATAREEL_SSL STREQUAL "NONE")
  message(FATAL_ERROR "DATAREEL_SSL must be OPENSSL, SCHANNEL or NONE, not '${DATAREEL_SSL}'")
endif()

# Platform
if(WIN32)
  target_compile_definitions(datareel PUBLIC
    __WIN32__ __HAS__BEGINTHREADEX__ __NTFS__
    __USE_CRC32_TABLE_FUNCTIONS__ __USE_EDS_TEST_FUNCTIONS__)
  if(MSVC)
    target_compile_definitions(datareel PUBLIC
      __MSVC__
      _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
      _WINSOCK_DEPRECATED_NO_WARNINGS
      _HAS_STD_BYTE=0)  # Avoid std::byte and Windows byte conflicts
    target_compile_options(datareel PUBLIC /EHsc /GR)
  endif()
  target_link_libraries(datareel PUBLIC ws2_32 wsock32)
else()
  find_package(Threads REQUIRED)
  target_compile_definitions(datareel PUBLIC
    __UNIX__ __POSIX__ __LINUX__ GNU
    __REENTRANT__ _REENTRANT _POSIX_PTHREAD_SEMANTICS
    __USE_SIGNAL_IO__
    __LINUX_BAUD_RATE_38400_SUPPORT__ __LINUX_BAUD_RATE_57600_SUPPORT__
    __LINUX_BAUD_RATE_115200_SUPPORT__ __LINUX_BAUD_RATE_230400_SUPPORT__
    __LINUX_BAUD_RATE_460800_SUPPORT__)
  target_link_libraries(datareel PUBLIC Threads::Threads)
endif()

# 11/15/2026: DataReel 6.4: the DataReel 7.x vector database features
# for 6.x programs, built from a DataReel 7.4 or later source tree.
# 11/17/2026: and object replication (gxObjRepl): the 7.4 tree at or
# after 41b6e15 (checked in dr7features/inner/CMakeLists.txt).
# DataReel 6.4 has no copy of the 7.x code: this builds the 7.x library
# (its names are in namespace datareel, so it links next to the 6.x
# library), the plain interface dr7inner (dr7features/inner, 7.x headers
# only) and the 6.x classes in dr7features (dr7features.h, 6.x headers
# only). Link a program with DataReel::dr7features. Off by default: the
# 6.x library is then the same as without this option.
set(DATAREEL7_FEATURES_DIR "" CACHE PATH
  "DataReel 7.4 or later source tree for the vector database features (empty: off)")
option(DATAREEL7_TOOLS
  "With DATAREEL7_FEATURES_DIR: also build the 7.x objrepl, object server/client (POSIX) and vix_rebuild" ON)
if(DATAREEL7_FEATURES_DIR)
  include(ExternalProject)
  get_filename_component(_dr7_dir "${DATAREEL7_FEATURES_DIR}" ABSOLUTE)
  set(_dr7_bin "${CMAKE_CURRENT_BINARY_DIR}/dr7inner")
  if(CMAKE_CONFIGURATION_TYPES)
    set(_dr7_cfg "$<CONFIG>/")
  else()
    set(_dr7_cfg "")
  endif()
  set(_dr7_inner_lib
    "${_dr7_bin}/${_dr7_cfg}${CMAKE_STATIC_LIBRARY_PREFIX}dr7inner${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(_dr7_lib
    "${_dr7_bin}/datareel7/${_dr7_cfg}${CMAKE_STATIC_LIBRARY_PREFIX}datareel${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(_dr7_args
    -DDATAREEL7_DIR=${_dr7_dir}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DDATAREEL_64BIT_DATABASE=${DATAREEL_64BIT_DATABASE}
    -DDR7_TOOLS=${DATAREEL7_TOOLS})
  if(NOT DATAREEL_SSL STREQUAL "NONE")
    list(APPEND _dr7_args -DDATAREEL_SSL=${DATAREEL_SSL})
  endif()
  # Both projects need the same MSVC runtime. With none given, both use
  # the CMake default (/MD); an empty value would give the 7.x side /MT.
  if(MSVC AND DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
    list(APPEND _dr7_args "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}")
  endif()
  ExternalProject_Add(dr7inner_build
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dr7features/inner"
    BINARY_DIR "${_dr7_bin}"
    CMAKE_ARGS ${_dr7_args}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
    INSTALL_COMMAND ""
    BUILD_ALWAYS ON
    BUILD_BYPRODUCTS "${_dr7_inner_lib}" "${_dr7_lib}")
  add_library(dr7inner STATIC IMPORTED GLOBAL)
  set_target_properties(dr7inner PROPERTIES IMPORTED_LOCATION "${_dr7_inner_lib}")
  add_library(datareel7 STATIC IMPORTED GLOBAL)
  set_target_properties(datareel7 PROPERTIES IMPORTED_LOCATION "${_dr7_lib}")
  if(CMAKE_CONFIGURATION_TYPES)
    foreach(_c ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER ${_c} _cu)
      string(REPLACE "$<CONFIG>" "${_c}" _l1 "${_dr7_inner_lib}")
      string(REPLACE "$<CONFIG>" "${_c}" _l2 "${_dr7_lib}")
      set_target_properties(dr7inner PROPERTIES IMPORTED_LOCATION_${_cu} "${_l1}")
      set_target_properties(datareel7 PROPERTIES IMPORTED_LOCATION_${_cu} "${_l2}")
    endforeach()
  endif()
  add_dependencies(dr7inner dr7inner_build)
  add_dependencies(datareel7 dr7inner_build)
  # The libraries the 7.x library needs
  set(_dr7_link Threads::Threads)
  if(NOT TARGET Threads::Threads)
    find_package(Threads REQUIRED)
  endif()
  if(NOT DATAREEL_SSL STREQUAL "NONE" AND NOT WIN32)
    find_package(OpenSSL REQUIRED)
    list(APPEND _dr7_link OpenSSL::SSL OpenSSL::Crypto)
  endif()
  find_package(ZLIB QUIET)
  if(ZLIB_FOUND)
    list(APPEND _dr7_link ZLIB::ZLIB)
  endif()
  if(WIN32)
    list(APPEND _dr7_link secur32 crypt32 ncrypt ws2_32 iphlpapi)
  endif()
  add_library(dr7features STATIC dr7features/src/dr7features.cpp)
  add_library(DataReel::dr7features ALIAS dr7features)
  target_include_directories(dr7features PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/dr7features/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/dr7features/inner")
  target_link_libraries(dr7features PUBLIC datareel dr7inner datareel7 ${_dr7_link})
  target_compile_features(dr7features PUBLIC cxx_std_17)
  add_dependencies(dr7features dr7inner_build)
  # The example and test program (examples_database/vector_index)
  add_executable(vector_index examples_database/vector_index/testprog.cpp)
  target_link_libraries(vector_index PRIVATE DataReel::dr7features)
  # The object server client example (examples_socket/vector_client).
  # POSIX only: the 7.4 object client (multiplex session) is not built
  # on Windows, and gxv7ObjectClient returns an error there.
  if(NOT WIN32)
    add_executable(vector_client examples_socket/vector_client/testprog.cpp)
    target_link_libraries(vector_client PRIVATE DataReel::dr7features)
  endif()
  # 11/17/2026: object replication with the 7.x gxObjRepl (gxv7ObjRepl):
  # examples_database/objrepl_6x, every platform. DataReel 7.4 beta 5
  # removed gxrepl, so the replication_6x example went with it.
  add_executable(objrepl6 examples_database/objrepl_6x/objrepl6.cpp)
  target_link_libraries(objrepl6 PRIVATE DataReel::dr7features)
  message(STATUS "DataReel 7.x vector database features from ${_dr7_dir}")
  if(DATAREEL7_TOOLS)
    message(STATUS "DataReel 7.x tools (objrepl, object_server, vector_server, vix_rebuild): ${_dr7_bin}/tools")
  endif()
endif()

message(STATUS "DataReel ${PROJECT_VERSION}: GUI=${DATAREEL_GUI} SSL=${DATAREEL_SSL} "
  "64-bit database=${DATAREEL_64BIT_DATABASE}")
