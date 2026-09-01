# -*- cmake -*-
include_guard(GLOBAL)

include(FetchContent)

set(MSQUIC_GIT_TAG "v2.4.10" CACHE STRING "MsQuic release tag for pre-built signed DLLs")

# ===========================================================================
# WINDOWS: Use Microsoft's pre-built, Microsoft-signed MsQuic DLL.
# This avoids unsigned msquic.dll triggering Windows Defender false positives.
# The DLL ships with its own quictls/OpenSSL — isolated from the viewer's
# OpenSSL used by libcurl.
# ===========================================================================
if (WIN32)
    set(_msquic_archive_url
        "https://github.com/microsoft/msquic/releases/download/${MSQUIC_GIT_TAG}/msquic_windows_x64_Release_openssl.zip")
    set(_msquic_install_dir "${CMAKE_BINARY_DIR}/msquic-prebuilt")
    set(_msquic_marker "${_msquic_install_dir}/.msquic-downloaded")

    # Download and extract Microsoft's signed binaries
    if (NOT EXISTS "${_msquic_marker}")
        message(STATUS "MsQuic: Downloading pre-built signed DLL from Microsoft (v${MSQUIC_GIT_TAG})...")
        file(DOWNLOAD "${_msquic_archive_url}" "${_msquic_install_dir}/msquic.zip"
             STATUS _dl_status)
        list(GET _dl_status 0 _dl_code)
        if (NOT _dl_code EQUAL 0)
            message(FATAL_ERROR "MsQuic: Failed to download pre-built binaries (HTTP ${_dl_code})")
        endif()
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${_msquic_install_dir}/msquic.zip"
            WORKING_DIRECTORY "${_msquic_install_dir}")
        file(WRITE "${_msquic_marker}" "ok")
        message(STATUS "MsQuic: Pre-built signed binaries ready.")
    endif()

    # Paths from the release zip: bin/msquic.dll, lib/msquic.lib, include/msquic.h
    set(_msquic_dll   "${_msquic_install_dir}/bin/msquic.dll")
    set(_msquic_lib   "${_msquic_install_dir}/lib/msquic.lib")
    set(_msquic_inc   "${_msquic_install_dir}/include")

    if (NOT EXISTS "${_msquic_dll}" OR NOT EXISTS "${_msquic_lib}")
        message(FATAL_ERROR "MsQuic: Could not find msquic.dll or msquic.lib in pre-built archive")
    endif()

    # Import the pre-built DLL as a library
    add_library(msquic SHARED IMPORTED GLOBAL)
    set_target_properties(msquic PROPERTIES
        IMPORTED_LOCATION "${_msquic_dll}"
        IMPORTED_IMPLIB   "${_msquic_lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${_msquic_inc}"
    )

    # Note: msquic.dll is copied to the output directory in the build workflow
    # (cannot use POST_BUILD on imported targets)

    message(STATUS "MsQuic: Using pre-built Microsoft-signed DLL (v${MSQUIC_GIT_TAG})")
    message(STATUS "  DLL:  ${_msquic_dll}")
    message(STATUS "  LIB:  ${_msquic_lib}")
    message(STATUS "  INC:  ${_msquic_inc}")

# ===========================================================================
# LINUX: Build from source (static) with symbol localization.
# No Defender issues on Linux — static linking avoids shipping extra .so files.
# ===========================================================================
else ()
    set(MSQUIC_GIT_TAG_SRC "v2.5.7" CACHE STRING "MsQuic git tag for Linux source build")

    set(QUIC_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
    set(QUIC_BUILD_TEST     OFF CACHE BOOL "" FORCE)
    set(QUIC_BUILD_PERF     OFF CACHE BOOL "" FORCE)
    set(QUIC_ENABLE_LOGGING OFF CACHE BOOL "" FORCE)
    set(QUIC_BUILD_SHARED   OFF CACHE BOOL "" FORCE)

    set(QUIC_TLS_LIB "quictls" CACHE STRING "" FORCE)

    FetchContent_Declare(
      msquic
      GIT_REPOSITORY https://github.com/microsoft/msquic.git
      GIT_TAG        ${MSQUIC_GIT_TAG_SRC}
      GIT_SUBMODULES_RECURSE TRUE
      GIT_SHALLOW    FALSE
    )

    set(_msquic_saved_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}")
    set(_msquic_saved_C_FLAGS_MINSIZEREL     "${CMAKE_C_FLAGS_MINSIZEREL}")
    set(_msquic_saved_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set(_msquic_saved_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}")
    set(_msquic_saved_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG}")
    set(_msquic_saved_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL}")
    set(_msquic_saved_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    set(_msquic_saved_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}")
    set(_msquic_saved_BUILD_SHARED_LIBS        "${BUILD_SHARED_LIBS}")

    FetchContent_MakeAvailable(msquic)

    set(CMAKE_C_FLAGS_DEBUG          "${_msquic_saved_C_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_MINSIZEREL     "${_msquic_saved_C_FLAGS_MINSIZEREL}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${_msquic_saved_C_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_C_FLAGS_RELEASE        "${_msquic_saved_C_FLAGS_RELEASE}")
    set(CMAKE_CXX_FLAGS_DEBUG          "${_msquic_saved_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "${_msquic_saved_CXX_FLAGS_MINSIZEREL}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${_msquic_saved_CXX_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_CXX_FLAGS_RELEASE        "${_msquic_saved_CXX_FLAGS_RELEASE}")
    set(BUILD_SHARED_LIBS              "${_msquic_saved_BUILD_SHARED_LIBS}")

    find_program(MSQUIC_NM_EXE nm)
    if (NOT MSQUIC_NM_EXE)
        message(FATAL_ERROR "MsQuic: 'nm' is required to localize bundled OpenSSL symbols")
    endif()

    set(_msquic_marker  "${msquic_BINARY_DIR}/msquic_localized.stamp")
    set(_msquic_archive "${msquic_BINARY_DIR}/bin/$<IF:$<CONFIG:Debug>,Debug,Release>/libmsquic.a")

    add_custom_command(
        OUTPUT  "${_msquic_marker}"
        COMMAND ${CMAKE_COMMAND}
                -DMSQUIC_AR_PATH=${_msquic_archive}
                -DAR=${CMAKE_AR}
                -DNM=${MSQUIC_NM_EXE}
                -DOBJCOPY=${CMAKE_OBJCOPY}
                -DMARKER=${_msquic_marker}
                -P "${CMAKE_CURRENT_LIST_DIR}/MsQuicLocalize.cmake"
        DEPENDS msquic_lib "${CMAKE_CURRENT_LIST_DIR}/MsQuicLocalize.cmake"
        COMMENT "Renaming bundled quictls/OpenSSL symbols inside libmsquic.a"
        VERBATIM)

    add_custom_target(msquic_localized ALL DEPENDS "${_msquic_marker}")
    add_dependencies(msquic msquic_localized)

    message(STATUS "MsQuic: Building from source (static, v${MSQUIC_GIT_TAG_SRC})")
endif ()

# Common: create the fs::msquic interface target
add_library(fs::msquic INTERFACE IMPORTED)
if (WIN32)
    target_link_libraries(fs::msquic INTERFACE msquic)
    target_include_directories(fs::msquic SYSTEM INTERFACE "${_msquic_inc}")
else ()
    target_link_libraries(fs::msquic INTERFACE msquic msquic::base_link)
    target_include_directories(fs::msquic SYSTEM INTERFACE
        ${msquic_SOURCE_DIR}/src/inc)
endif ()
