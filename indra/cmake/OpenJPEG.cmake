# -*- cmake -*-
include_guard()

include(Prebuilt)
include(Linking)

add_library( ll::openjpeg INTERFACE IMPORTED )

use_system_binary(openjpeg)
if (LL_LINUX_ARM64)
  find_library(OPENJPEG_LIBRARY NAMES openjp2 REQUIRED)
  find_path(OPENJPEG_INCLUDE_DIR openjpeg.h PATH_SUFFIXES openjpeg-2.5 openjpeg-2.4 openjpeg-2.3 openjpeg REQUIRED)
  target_link_libraries(ll::openjpeg INTERFACE ${OPENJPEG_LIBRARY})
  target_include_directories(ll::openjpeg SYSTEM INTERFACE ${OPENJPEG_INCLUDE_DIR})
  return()
endif ()

use_prebuilt_binary(openjpeg)

find_library(OPENJPEG_LIBRARY
    NAMES
    openjp2
    openjp2.lib
    libopenjp2.a
    libopenjp2.so
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

target_link_libraries(ll::openjpeg INTERFACE ${OPENJPEG_LIBRARY})

target_include_directories(ll::openjpeg SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/openjpeg)
