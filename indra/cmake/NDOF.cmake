# -*- cmake -*-
include(Prebuilt)

set(NDOF ON CACHE BOOL "Use NDOF space navigator joystick library.")

include_guard()
add_library( ll::ndof INTERFACE IMPORTED )

# No ARM64 build of the proprietary 3Dconnexion libndofdev prebuilt exists
# (the autobuild package is x86-only). NDOF is optional space-navigator
# joystick support, so disable it on ARM64 rather than link the wrong arch.
if (LL_LINUX_ARM64)
  set(NDOF OFF)
endif ()

if (NDOF)
  if (WINDOWS OR DARWIN)
    use_prebuilt_binary(libndofdev)
  elseif (LINUX)
    use_prebuilt_binary(open-libndofdev)
  endif (WINDOWS OR DARWIN)

  find_library(NDOF_LIBRARY
      NAMES
      libndofdev
      ndofdev
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

  target_link_libraries(ll::ndof INTERFACE ${NDOF_LIBRARY})

  target_compile_definitions(ll::ndof INTERFACE LIB_NDOF=1)
endif (NDOF)
