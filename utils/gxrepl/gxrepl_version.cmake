# 11/10/2026: gxrepl_stamp(TARGET): "gxrepl version" prints the git head of
# the DataReel sources (with +dirty when they have uncommitted changes) and
# the build time, taken when CMake configures (automate/gxrepl_build.sh
# configures on every run). Outside a git checkout both read "unknown".
function(gxrepl_stamp target)
  set(_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
  set(_head "unknown")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${_dir}" rev-parse --short=12 HEAD
                    OUTPUT_VARIABLE _h OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _rc)
    if(_rc EQUAL 0 AND NOT _h STREQUAL "")
      set(_head "${_h}")
      execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${_dir}" status --porcelain --untracked-files=no -- ../..
                      OUTPUT_VARIABLE _s OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(NOT _s STREQUAL "")
        set(_head "${_head}+dirty")
      endif()
    endif()
  endif()
  string(TIMESTAMP _when "%Y-%m-%d %H:%M:%S UTC" UTC)
  target_compile_definitions(${target} PRIVATE GXREPL_GIT_HEAD="${_head}" GXREPL_BUILD_TIME="${_when}")
endfunction()
