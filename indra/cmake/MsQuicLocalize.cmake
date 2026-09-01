# -*- cmake -*-
# Script-mode CMake helper invoked from MsQuic.cmake.
cmake_policy(SET CMP0057 NEW)
#
# Renames symbols originating from MsQuic's bundled quictls (its OpenSSL fork)
# inside libmsquic.a so they cannot collide with the viewer's own libcrypto.a
# at static link time. Every defined extern symbol in the OpenSSL-derived
# object files is rewritten to "__msquic_<name>", and the same rename is
# applied to every other object file in the archive so internal references
# stay consistent.
#
# Required -D variables:
#   MSQUIC_AR_PATH  absolute path to libmsquic.a (rewritten in place)
#   AR              path to ar
#   NM              path to nm
#   OBJCOPY         path to objcopy
#   MARKER          path to stamp file written on success

foreach(_v MSQUIC_AR_PATH AR NM OBJCOPY MARKER)
    if (NOT DEFINED ${_v} OR ${_v} STREQUAL "")
        message(FATAL_ERROR "MsQuicLocalize: -D${_v} is required")
    endif()
endforeach()

if (NOT EXISTS "${MSQUIC_AR_PATH}")
    message(FATAL_ERROR "MsQuicLocalize: archive not found: ${MSQUIC_AR_PATH}")
endif()

if (EXISTS "${MARKER}")
    file(TIMESTAMP "${MARKER}"               _marker_ts "%s" UTC)
    file(TIMESTAMP "${MSQUIC_AR_PATH}"        _ar_ts     "%s" UTC)
    file(TIMESTAMP "${CMAKE_CURRENT_LIST_FILE}" _self_ts "%s" UTC)
    if (NOT _marker_ts STREQUAL ""
        AND NOT _ar_ts STREQUAL ""
        AND NOT _self_ts STREQUAL ""
        AND _marker_ts GREATER_EQUAL _ar_ts
        AND _marker_ts GREATER_EQUAL _self_ts)
        return()
    endif()
endif()

get_filename_component(_ar_dir "${MSQUIC_AR_PATH}" DIRECTORY)
set(_work "${_ar_dir}/.msquic_localize")
file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}")

execute_process(
    COMMAND "${AR}" x "${MSQUIC_AR_PATH}"
    WORKING_DIRECTORY "${_work}"
    RESULT_VARIABLE _r)
if (NOT _r EQUAL 0)
    message(FATAL_ERROR "MsQuicLocalize: 'ar x' failed (${_r}) on ${MSQUIC_AR_PATH}")
endif()

file(GLOB _all_objs "${_work}/*.o" "${_work}/*.obj")
if (NOT _all_objs)
    message(FATAL_ERROR "MsQuicLocalize: no object files extracted from ${MSQUIC_AR_PATH}")
endif()

file(GLOB _ssl_objs
    "${_work}/lib*-lib-*.o"
    "${_work}/lib*-shlib-*.o")

if (NOT _ssl_objs)
    message(STATUS "MsQuicLocalize: no quictls/OpenSSL object files found; nothing to do")
    file(WRITE "${MARKER}" "")
    return()
endif()

set(_rename_file "${_work}/redefine.txt")
file(WRITE "${_rename_file}" "")
set(_seen "")

foreach(_obj IN LISTS _ssl_objs)
    execute_process(
        COMMAND "${NM}" --defined-only --extern-only --format=posix "${_obj}"
        OUTPUT_VARIABLE _nm_out
        RESULT_VARIABLE _r)
    if (NOT _r EQUAL 0)
        message(FATAL_ERROR "MsQuicLocalize: nm failed on ${_obj}")
    endif()
    string(REPLACE "\n" ";" _lines "${_nm_out}")
    foreach(_line IN LISTS _lines)
        if (_line MATCHES "^([^ \t]+)[ \t]+[A-Za-z][ \t]")
            set(_sym "${CMAKE_MATCH_1}")
            if (NOT _sym MATCHES "^__msquic_" AND NOT _sym IN_LIST _seen)
                list(APPEND _seen "${_sym}")
                file(APPEND "${_rename_file}" "${_sym} __msquic_${_sym}\n")
            endif()
        endif()
    endforeach()
endforeach()

list(LENGTH _seen _n)
message(STATUS "MsQuicLocalize: renaming ${_n} bundled OpenSSL/quictls symbols")

foreach(_obj IN LISTS _all_objs)
    execute_process(
        COMMAND "${OBJCOPY}" --redefine-syms=${_rename_file} "${_obj}"
        RESULT_VARIABLE _r)
    if (NOT _r EQUAL 0)
        message(FATAL_ERROR "MsQuicLocalize: objcopy failed on ${_obj}")
    endif()
endforeach()

file(REMOVE "${MSQUIC_AR_PATH}")
execute_process(
    COMMAND "${AR}" qcs "${MSQUIC_AR_PATH}" ${_all_objs}
    RESULT_VARIABLE _r)
if (NOT _r EQUAL 0)
    message(FATAL_ERROR "MsQuicLocalize: 'ar qcs' failed (${_r})")
endif()

file(WRITE "${MARKER}" "")
file(REMOVE_RECURSE "${_work}")
