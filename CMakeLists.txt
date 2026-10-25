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
  btstack btcache btnode dbasekey gpersist gxbtree pod gxint16 gxuint16
  gxfloat dbfcache dbugmgr
  # RDBMS
  gxrdbdef gxrdbhdr gxrdbms gxrdbsql
  # Sockets
  gxsocket gxshttp gxshttpc gxsmtp gxsping gxspop3 gxsftp gxshtml gxsurl
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

message(STATUS "DataReel ${PROJECT_VERSION}: GUI=${DATAREEL_GUI} SSL=${DATAREEL_SSL} "
  "64-bit database=${DATAREEL_64BIT_DATABASE}")
