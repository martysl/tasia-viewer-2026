# -*- cmake -*-
include_guard()

include(Linking)
include(Prebuilt)

add_library( ll::webrtc INTERFACE IMPORTED )
if (LL_LINUX_ARM64)
    target_include_directories( ll::webrtc SYSTEM INTERFACE "/usr/include" "${LIBS_PREBUILT_DIR}/include/webrtc" "${LIBS_PREBUILT_DIR}/include/webrtc/third_party/abseil-cpp")
else ()
    target_include_directories( ll::webrtc SYSTEM INTERFACE "${LIBS_PREBUILT_DIR}/include/webrtc" "${LIBS_PREBUILT_DIR}/include/webrtc/third_party/abseil-cpp")
    use_prebuilt_binary(webrtc)
endif ()

if (LL_LINUX_ARM64)
    find_library(WEBRTC_LIBRARY NAMES webrtc REQUIRED)
else ()
    find_library(WEBRTC_LIBRARY
        NAMES
        webrtc
        PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)
endif ()

target_link_libraries( ll::webrtc INTERFACE ${WEBRTC_LIBRARY} )

if (DARWIN)
    target_link_libraries( ll::webrtc INTERFACE ll::oslibraries )
elseif (LINUX)
    target_link_libraries( ll::webrtc INTERFACE X11 )
endif ()


