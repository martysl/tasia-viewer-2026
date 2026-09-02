# -*- cmake -*-

include(Prebuilt)

add_library(ll::sse2neon INTERFACE IMPORTED)

if (DARWIN OR (LINUX AND ARCH STREQUAL "aarch64"))
    use_system_binary(sse2neon)
    use_prebuilt_binary(sse2neon)

    target_include_directories( ll::sse2neon SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/sse2neon)
endif()
