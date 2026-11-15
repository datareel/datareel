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
# rebuild fallback: the replication example that also builds on Windows (gxrepl is POSIX only)
datareel_example(replication_copy  oodb/replication_copy.cpp)
# 11/15/2026 (7.4): the vector index, records with vector members, a vector set across day files
datareel_example(vector_basics     oodb/vector_basics.cpp)
datareel_example(vector_records    oodb/vector_records.cpp)
datareel_example(vector_days       oodb/vector_days.cpp)
if(NOT WIN32)
  datareel_example(journal_recovery oodb/journal_recovery.cpp)
  # 11/14/2026 (7.4): replication with gxrepl, object by object: the writer / rebuild / digest program, and the
  # example's own gxrepl with its extractor built in (POSIX only, like gxrepl)
  # 11/15/2026 (7.4): vector searches through the object server (POSIX only, like the object server)
  datareel_example(vector_search_service oodb/vector_search_service.cpp)
  datareel_example(replication     oodb/replication.cpp)
  add_executable(replication_gxrepl ${CMAKE_CURRENT_SOURCE_DIR}/../utils/gxrepl/gxrepl.cpp oodb/replication_gxrepl.cpp)
  target_include_directories(replication_gxrepl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oodb)
  target_compile_definitions(replication_gxrepl PRIVATE GXREPL_EXTRA_EXTRACTORS=1)
  target_link_libraries(replication_gxrepl PRIVATE DataReel::datareel)
  set_target_properties(replication_gxrepl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
endif()

# TLS networking
datareel_example(tls_echo_server   tls/tls_echo_server.cpp)
datareel_example(tls_echo_client   tls/tls_echo_client.cpp)
datareel_example(https_get         tls/https_get.cpp)
datareel_example(https_server      tls/https_server.cpp)
datareel_example(smtp_send         tls/smtp_send.cpp)
datareel_example(pop3_list         tls/pop3_list.cpp)
datareel_example(ftps_client       tls/ftps_client.cpp)
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

# Sample programs from the AI guides in docs/ai, built so they stay correct
foreach(sample tls_client tls_server https_fetch send_mail 
    record_store indexed_records vector_search)
  datareel_example(ai_${sample} ${PROJECT_SOURCE_DIR}/docs/ai/samples/${sample}.cpp)
endforeach()
