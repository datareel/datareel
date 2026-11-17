# DataReel 7 tests that are programs. run_tests.sh runs the rest.
add_executable(damaged_files damaged_files.cpp)
target_link_libraries(damaged_files PRIVATE DataReel::datareel)
set_target_properties(damaged_files PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026: journal salts distinct across forked processes (POSIX: fork)
if(NOT WIN32)
  add_executable(journal_salt_test journal_salt_test.cpp)
  target_link_libraries(journal_salt_test PRIVATE DataReel::datareel)
  set_target_properties(journal_salt_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

# 11/16/2026: block reuse, best fit and first fit (ReclaimBestFit() reused only exact fits)
add_executable(reclaim_test reclaim_test.cpp)
target_link_libraries(reclaim_test PRIVATE DataReel::datareel)
set_target_properties(reclaim_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(journal_test journal_test.cpp)
target_link_libraries(journal_test PRIVATE DataReel::datareel)
set_target_properties(journal_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026 (7.4): the engine signature and the layout detection (layout_detect_test.cpp)
add_executable(layout_detect_test layout_detect_test.cpp)
target_link_libraries(layout_detect_test PRIVATE DataReel::datareel)
set_target_properties(layout_detect_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026 (7.4): the database digests (gxddigest.h); derived_apply_test went with gxrepl
add_executable(digest_test digest_test.cpp)
target_link_libraries(digest_test PRIVATE DataReel::datareel)
set_target_properties(digest_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(stream_security_test stream_security_test.cpp)
target_link_libraries(stream_security_test PRIVATE DataReel::datareel)
set_target_properties(stream_security_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(proto_security_test proto_security_test.cpp)
target_link_libraries(proto_security_test PRIVATE DataReel::datareel)
set_target_properties(proto_security_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(support_security_test support_security_test.cpp)
target_link_libraries(support_security_test PRIVATE DataReel::datareel)
set_target_properties(support_security_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/16/2026 (7.4 beta5, G4-2): a plain text IMAP login needs AllowPlainLogin(); run_tests runs it on the mock server
add_executable(imap_login_test imap_login_test.cpp)
target_link_libraries(imap_login_test PRIVATE DataReel::datareel)
set_target_properties(imap_login_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# 11/17/2026 (7.4): TLS with PEM keys where no user is signed in (SSH, a Windows service): tls_key_test
add_executable(tls_key_test tls_key_test.cpp)
target_link_libraries(tls_key_test PRIVATE DataReel::datareel)
set_target_properties(tls_key_test PROPERTIES
# 11/17/2026 (7.4, from 6.4.1): sockets are not passed to programs this process runs (close on exec / not inherited)
add_executable(socket_cloexec_test socket_cloexec_test.cpp)
target_link_libraries(socket_cloexec_test PRIVATE DataReel::datareel)
set_target_properties(socket_cloexec_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
# 11/17/2026: the serial_read example on a pseudo terminal (POSIX)
if(NOT WIN32)
  add_executable(serial_read_test serial_read_test.cpp)
  set_target_properties(serial_read_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()
# 11/17/2026: strip_ansi() and AnsiStripper (strutil.h): terminal control sequences out of text
add_executable(ansi_strip_test ansi_strip_test.cpp)
target_link_libraries(ansi_strip_test PRIVATE DataReel::datareel)
set_target_properties(ansi_strip_test PROPERTIES
# 11/17/2026: gxCSVReader and gxCSVWriter (gxcsv.h)
add_executable(csv_test csv_test.cpp)
target_link_libraries(csv_test PRIVATE DataReel::datareel)
set_target_properties(csv_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
