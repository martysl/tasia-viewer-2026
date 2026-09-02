# -*- cmake -*-

# some webrtc headers require C++ 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(00-Common)
include(Linking)
include(WebRTC)

project(llwebrtc)

set(llwebrtc_SOURCE_FILES
    llwebrtc.cpp
    )

set(llwebrtc_HEADER_FILES
    CMakeLists.txt
    llwebrtc.h
    llwebrtc_impl.h
    )

# On Linux ARM64 the staged WebRTC library (crow-misia chromimum 139,
# identical to what Megapahit uses) exposes the m139 audio API. The default
# llwebrtc sources are written against the older m137 API and will not
# compile. Megapahit adapted llwebrtc to compile against the m139 API by
# selecting alternate code with a CM_WEBRTC=1 macro. We pull those adapted
# sources in for the ARM64 build only; all other platforms keep the m137 code.
if (LL_LINUX_ARM64)
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/arm64/llwebrtc.cpp")
        message(FATAL_ERROR "ARM64 llwebrtc sources missing; check the indra/llwebrtc/arm64 directory")
    endif ()
    set(llwebrtc_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/arm64/llwebrtc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/arm64/llwebrtc.h
        ${CMAKE_CURRENT_SOURCE_DIR}/arm64/llwebrtc_impl.h
        )
    add_compile_definitions(CM_WEBRTC=1)
endif ()

add_library (llwebrtc SHARED ${llwebrtc_SOURCE_FILES})

set_target_properties(llwebrtc PROPERTIES PUBLIC_HEADER llwebrtc.h)

if (WINDOWS)
    cmake_policy(SET CMP0091 NEW)
    set_target_properties(llwebrtc
        PROPERTIES
        LINK_FLAGS "/debug /LARGEADDRESSAWARE"
        )
	target_link_libraries(llwebrtc PRIVATE ll::webrtc 
                                       secur32
                                       winmm
                                       dmoguids
                                       wmcodecdspuuid
                                       msdmo
                                       strmiids
                                       iphlpapi
                                       libcmt)
    # as the webrtc libraries are release, build this binary as release as well.
    target_compile_options(llwebrtc PRIVATE "/MT" "/Zc:wchar_t")
    if (USE_BUGSPLAT)
        set_target_properties(llwebrtc PROPERTIES PDB_OUTPUT_DIRECTORY "${SYMBOLS_STAGING_DIR}")
    endif (USE_BUGSPLAT)
elseif (DARWIN)
    target_link_libraries(llwebrtc PRIVATE ll::webrtc)
    if (USE_BUGSPLAT)
        set_target_properties(llwebrtc PROPERTIES XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
                                                  XCODE_ATTRIBUTE_DWARF_DSYM_FOLDER_PATH "${SYMBOLS_STAGING_DIR}/dSYMs")
    endif (USE_BUGSPLAT)
elseif (LINUX)
    target_link_libraries(llwebrtc PRIVATE ll::webrtc)
endif (WINDOWS)
    
target_include_directories( llwebrtc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (WINDOWS)
    set_property(TARGET llwebrtc PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
else()
    target_compile_options(llwebrtc PRIVATE -Wno-deprecated-declarations) # webrtc::CreateAudioDeviceWithDataObserver is deprecated
endif (WINDOWS)

ADD_CUSTOM_COMMAND(TARGET llwebrtc POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       $<TARGET_FILE:llwebrtc>
                               ${SHARED_LIB_STAGING_DIR})
# Add tests
if (LL_TESTS)
endif (LL_TESTS)
