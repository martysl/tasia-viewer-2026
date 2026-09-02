# -*- cmake -*-
include(Prebuilt)

if (LINUX AND NOT SYSTEMLIBS )
  set(USE_JEMALLOC ON)
endif ()

if( USE_JEMALLOC )
  if (USESYSTEMLIBS)
    message( WARNING "Not implemented" )
  else (USESYSTEMLIBS)
    if (NOT LL_LINUX_ARM64)
      use_prebuilt_binary(jemalloc)
    endif ()
  endif (USESYSTEMLIBS)
endif()
