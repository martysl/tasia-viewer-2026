# -*- cmake -*-
include(Linking)
include(Prebuilt)

include_guard()

# ND: Turn this off by default, the openal code in the viewer isn't very well maintained, seems
# to have memory leaks, has no option to play music streams
# It probably makes sense to to completely remove it

set(USE_OPENAL ON CACHE BOOL "Enable OpenAL")

# <FS:Zi> Always download the packaged OpenAL runtime for the legacy x86
# SLVoice helper. Native ARM builds use the distribution OpenAL/ALUT libraries.
if (LINUX AND NOT LL_LINUX_ARM64)
  use_prebuilt_binary(openal)
endif ()

# ND: To streamline arguments passed, switch from OPENAL to USE_OPENAL
# To not break all old build scripts convert old arguments but warn about it
if(OPENAL)
  message( WARNING "Use of the OPENAL argument is deprecated, please switch to USE_OPENAL")
  set(USE_OPENAL ${OPENAL})
endif()

if (USE_OPENAL)
  add_library( ll::openal INTERFACE IMPORTED )
  if (LL_LINUX_ARM64)
    find_path(OPENAL_INCLUDE_DIR AL/al.h REQUIRED)
    target_include_directories(ll::openal SYSTEM INTERFACE "${OPENAL_INCLUDE_DIR}")
  else ()
    target_include_directories(ll::openal SYSTEM INTERFACE "${LIBS_PREBUILT_DIR}/include/AL")
    use_prebuilt_binary(openal)
  endif ()
  target_compile_definitions( ll::openal INTERFACE LL_OPENAL=1)

  if (LL_LINUX_ARM64)
    find_library(OPENAL_LIBRARY
        NAMES OpenAL32 openal libopenal.dylib libopenal.so
        REQUIRED)
    find_library(ALUT_LIBRARY
        NAMES alut libalut.dylib libalut.so
        REQUIRED)
  else ()
    find_library(OPENAL_LIBRARY
        NAMES OpenAL32 openal libopenal.dylib libopenal.so
        PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)
    find_library(ALUT_LIBRARY
        NAMES alut libalut.dylib libalut.so
        PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)
  endif ()

  target_link_libraries(ll::openal INTERFACE ${OPENAL_LIBRARY} ${ALUT_LIBRARY})

endif ()
