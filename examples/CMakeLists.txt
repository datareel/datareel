# DataReel 7 examples. Each program is one source file.

function(datareel_example name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE DataReel::datareel)
  set_target_properties(${name} PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
endfunction()

# Object database
datareel_example(db_simple         database/simple/testprog.cpp)
datareel_example(db_database       database/database/testprog.cpp)
datareel_example(db_btree          database/btree/testprog.cpp)
datareel_example(db_btree_cached   database/btree_cached/testprog.cpp)
datareel_example(db_btree_node     database/btree_node/testprog.cpp)
datareel_example(db_database_cache database/database_cache/testprog.cpp)
datareel_example(db_defrag         database/defrag/testprog.cpp)
datareel_example(db_pod            database/pod/testprog.cpp)
datareel_example(db_infohog        database/infohog/infohog.cpp)

# Object database patterns (examples/oodb). journal_recovery uses
# fork() and kill(), so it is built on POSIX systems only.
datareel_example(typed_objects     oodb/typed_objects.cpp)
datareel_example(day_events        oodb/day_events.cpp)
# 11/14/2026 (7.4): a database folder copied by a plain file copy and checked with digests, with the index
# rebuild fallback, on Linux and Windows (11/16/2026: the gxrepl examples went with gxrepl)
datareel_example(replication_copy  oodb/replication_copy.cpp)
# 11/17/2026: gxObjRepl, a primary and a copy, then a planned switch (Linux and Windows)
datareel_example(object_replication oodb/object_replication.cpp)
# 11/15/2026 (7.4): the vector index, records with vector members, a vector set across day files
datareel_example(vector_basics     oodb/vector_basics.cpp)
datareel_example(vector_records    oodb/vector_records.cpp)
datareel_example(vector_days       oodb/vector_days.cpp)
if(NOT WIN32)
  datareel_example(journal_recovery oodb/journal_recovery.cpp)
  # 11/15/2026 (7.4): vector searches through the object server (POSIX only, like the object server)
  datareel_example(vector_search_service oodb/vector_search_service.cpp)
endif()

# TLS networking
datareel_example(tls_echo_server   tls/tls_echo_server.cpp)
datareel_example(tls_echo_client   tls/tls_echo_client.cpp)
datareel_example(https_get         tls/https_get.cpp)
datareel_example(https_server      tls/https_server.cpp)
# 11/16/2026 (7.4): web site checks, based on the 2007 http_ping and http_header_util programs
datareel_example(https_check       tls/https_check.cpp)
datareel_example(smtp_send         tls/smtp_send.cpp)
datareel_example(pop3_list         tls/pop3_list.cpp)
# 11/16/2026 (7.4): IMAP, based on the 2007 IMAP client program
datareel_example(imap_fetch        tls/imap_fetch.cpp)
datareel_example(ftps_client       tls/ftps_client.cpp)
datareel_example(dir_upload        tls/dir_upload.cpp)   # 11/17/2026: watch a folder, FTPS upload, delete (from dirscanf)
datareel_example(stream_server     tls/stream_server.cpp)
datareel_example(stream_client     tls/stream_client.cpp)

# General purpose
datareel_example(config_file       general/config_file.cpp)
datareel_example(logfile           general/logfile.cpp)
datareel_example(thread_pool       general/thread_pool.cpp)
datareel_example(trading_sessions  general/trading_sessions.cpp)
datareel_example(string_utils      general/string_utils.cpp)

# Plain text diagnostics (ICMP and telnet cannot use TLS)
datareel_example(ping              diagnostics/ping.cpp)
datareel_example(telnet_client     diagnostics/telnet_client.cpp)
# 11/16/2026 (7.4): plain UDP broadcast and multicast (trusted local networks only), based on the 2001
# broadcast and multicast programs
datareel_example(udp_broadcast     diagnostics/udp_broadcast.cpp)
datareel_example(udp_multicast     diagnostics/udp_multicast.cpp)

# Sample programs from the AI guides in docs/ai, built so they stay correct
foreach(sample tls_client tls_server https_fetch send_mail 
    record_store indexed_records vector_search)
  datareel_example(ai_${sample} ${PROJECT_SOURCE_DIR}/docs/ai/samples/${sample}.cpp)
endforeach()
