# CMake build for DataReel 7.
#
# Builds the DataReel library ("datareel", alias DataReel::datareel),
# and optionally the examples and utilities. The configuration macros
# are PUBLIC because the DataReel headers change layout based on them,
# so programs that link the library are compiled with the same set.
#
# Build and install:
#   cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
#   cmake --build cmake-build -j
#   cmake --install cmake-build --prefix /usr/local
#
# Use an installed DataReel from another CMake project:
#   find_package(DataReel 7 REQUIRED)
#   target_link_libraries(myapp PRIVATE DataReel::datareel)
#
# Or build it as part of another project:
#   add_subdirectory(path/to/datareel datareel)
#   target_link_libraries(myapp PRIVATE DataReel::datareel)
#
# Options:
#   DATAREEL_SSL             OPENSSL (default on Linux) or SCHANNEL
#                            (default on Windows). TLS is required.
#   DATAREEL_64BIT_DATABASE  64-bit database engine (64-bit file offsets).
#                            Must match the files you open: a 32-bit
#                            build cannot read 64-bit files and the reverse.
#   DATAREEL_NATIVE_TYPES    Native integer and float types (default ON).
#                            OFF uses the portable gxINT/gxFLOAT byte
#                            classes; only needed for files written that way.
#   DATAREEL_GUI             CONSOLE or WX (wxWidgets 3.x message boxes)
#   BUILD_SHARED_LIBS        Build a shared library instead of a static one
#   DATAREEL_BUILD_EXAMPLES  Build the programs in examples/ and utils/

cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" _dr_version LIMIT_COUNT 1)
project(DataReel VERSION ${_dr_version} LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if(WIN32)
  set(_dr_ssl_default SCHANNEL)
else()
  set(_dr_ssl_default OPENSSL)
endif()
set(DATAREEL_SSL ${_dr_ssl_default} CACHE STRING
  "TLS library: OPENSSL or SCHANNEL (Windows only)")
set_property(CACHE DATAREEL_SSL PROPERTY STRINGS OPENSSL SCHANNEL)

set(DATAREEL_GUI CONSOLE CACHE STRING
  "Message display: CONSOLE or WX (wxWidgets 3.x message boxes)")
set_property(CACHE DATAREEL_GUI PROPERTY STRINGS CONSOLE WX)

option(DATAREEL_64BIT_DATABASE
  "Build the 64-bit database engine with large file support" OFF)
option(DATAREEL_NATIVE_TYPES
  "Use native integer and float types instead of the portable gxINT/gxFLOAT classes" ON)
option(DATAREEL_BUILD_EXAMPLES "Build the examples and utilities"
  ${PROJECT_IS_TOP_LEVEL})

# Every source file except the Schannel code, which gxssl.cpp includes,
# and the Windows DLL entry point
file(GLOB DATAREEL_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER DATAREEL_SOURCES EXCLUDE REGEX "/(gxsschan|stdafx|gxdlcode)\\.cpp$")
file(GLOB DATAREEL_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
list(FILTER DATAREEL_HEADERS EXCLUDE REGEX "/(stdafx|gxdlincs)\\.h$")

add_library(datareel ${DATAREEL_SOURCES})
add_library(DataReel::datareel ALIAS datareel)
set_target_properties(datareel PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  POSITION_INDEPENDENT_CODE ON)
target_include_directories(datareel PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/datareel>)
target_compile_features(datareel PUBLIC cxx_std_17)

target_compile_definitions(datareel PUBLIC
  __USE_BINARY_SEARCH__ __USE_SINGLE_COMPARE__
  __USE_DATABASE_CACHE__ __USE_BTREE_CACHE__
  $<$<NOT:$<CONFIG:Debug>>:__NO_DEBUG_OPTIONS__>)

if(DATAREEL_NATIVE_TYPES)
  target_compile_definitions(datareel PUBLIC
    __USE_NATIVE_INT_TYPES__ __USE_NATIVE_FLOAT_TYPES__)
else()
  target_compile_definitions(datareel PUBLIC __X86__)
endif()

if(DATAREEL_64BIT_DATABASE)
  target_compile_definitions(datareel PUBLIC
    __64_BIT_DATABASE_ENGINE__ __ENABLE_64_BIT_INTEGERS__)
  if(NOT WIN32)
    target_compile_definitions(datareel PUBLIC
      _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64 _LARGE_FILES)
  endif()
elseif(NOT WIN32)
  target_compile_definitions(datareel PUBLIC _FILE_OFFSET_BITS=64)
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

# TLS
if(DATAREEL_SSL STREQUAL "OPENSSL")
  find_package(OpenSSL 1.1.1 REQUIRED)
  target_link_libraries(datareel PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  if(WIN32)
    target_link_libraries(datareel PUBLIC crypt32)
  endif()
elseif(DATAREEL_SSL STREQUAL "SCHANNEL")
  if(NOT WIN32)
    message(FATAL_ERROR "DATAREEL_SSL=SCHANNEL is only available on Windows")
  endif()
  target_compile_definitions(datareel PUBLIC __USE_GX_SCHANNEL__)
  target_link_libraries(datareel PUBLIC secur32 crypt32 ncrypt)
else()
  message(FATAL_ERROR "DATAREEL_SSL must be OPENSSL or SCHANNEL, not '${DATAREEL_SSL}'")
endif()

# Platform
if(WIN32)
  target_compile_definitions(datareel PUBLIC
    __WIN32__ __NTFS__ WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_definitions(datareel PUBLIC
      __MSVC__
      _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
      _HAS_STD_BYTE=0)  # Avoid std::byte and Windows byte conflicts
    target_compile_options(datareel PUBLIC /EHsc /GR /utf-8)
  endif()
  target_link_libraries(datareel PUBLIC ws2_32 iphlpapi)
  if(BUILD_SHARED_LIBS)
    target_compile_definitions(datareel PUBLIC __MSVC_DLL__)
    target_compile_definitions(datareel PRIVATE GXDLCODE_EXPORTS)
  endif()
else()
  find_package(Threads REQUIRED)
  target_compile_definitions(datareel PUBLIC
    __UNIX__ __POSIX__ __LINUX__ GNU
    __LINUX_BAUD_RATE_38400_SUPPORT__ __LINUX_BAUD_RATE_57600_SUPPORT__
    __LINUX_BAUD_RATE_115200_SUPPORT__ __LINUX_BAUD_RATE_230400_SUPPORT__
    __LINUX_BAUD_RATE_460800_SUPPORT__)
  target_link_libraries(datareel PUBLIC Threads::Threads)
  # GCC 8 keeps std::filesystem in a separate library
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
     CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(datareel PUBLIC stdc++fs)
  endif()
endif()

# Install the library, headers and a CMake package
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS datareel EXPORT DataReelTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${DATAREEL_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/datareel)
install(EXPORT DataReelTargets NAMESPACE DataReel::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DataReel)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/DataReelConfig.cmake.in" [=[
@PACKAGE_INIT@
include(CMakeFindDependencyMacro)
if("@DATAREEL_SSL@" STREQUAL "OPENSSL")
  find_dependency(OpenSSL 1.1.1)
endif()
if(NOT WIN32)
  find_dependency(Threads)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/DataReelTargets.cmake")
]=])
configure_package_config_file(
  "${CMAKE_CURRENT_BINARY_DIR}/DataReelConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/DataReelConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DataReel)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/DataReelConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/DataReelConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/DataReelConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DataReel)

if(DATAREEL_BUILD_EXAMPLES)
  add_subdirectory(examples)
  add_subdirectory(utils)
endif()

message(STATUS "DataReel ${PROJECT_VERSION}: SSL=${DATAREEL_SSL} "
  "64-bit database=${DATAREEL_64BIT_DATABASE} "
  "native types=${DATAREEL_NATIVE_TYPES} GUI=${DATAREEL_GUI}")
