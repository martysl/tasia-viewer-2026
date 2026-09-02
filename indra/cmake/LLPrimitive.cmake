# -*- cmake -*-

# these should be moved to their own cmake file
include(Prebuilt)
include(Linking)
include(Boost)

include_guard()

add_library( ll::minizip-ng INTERFACE IMPORTED )
add_library( ll::libxml INTERFACE IMPORTED )
add_library( ll::colladadom INTERFACE IMPORTED )

# ND, needs fixup in collada conan pkg
if( USE_CONAN )
  target_include_directories( ll::colladadom SYSTEM INTERFACE
    "${CONAN_INCLUDE_DIRS_COLLADADOM}/collada-dom/"
    "${CONAN_INCLUDE_DIRS_COLLADADOM}/collada-dom/1.4/" )
endif()

use_system_binary( colladadom )

if (LL_LINUX_ARM64)
  # colladadom uses the old minizip API (unzip.h/zip.h) from zlib contrib;
  # system libminizip-dev provides both headers and libminizip.so (old ABI).
  find_library(MINIZIP_LIBRARY NAMES minizip
    PATHS /usr/lib/aarch64-linux-gnu /usr/lib REQUIRED)
  target_link_libraries(ll::minizip-ng INTERFACE ${MINIZIP_LIBRARY})
  target_include_directories(ll::minizip-ng SYSTEM INTERFACE
    /usr/include /usr/include/minizip)

  find_library(LIBXML2_LIBRARY NAMES xml2 REQUIRED)
  target_link_libraries(ll::libxml INTERFACE ${LIBXML2_LIBRARY})

  find_library(COLLADADOM_LIBRARY NAMES
    libcollada14dom23 libcollada14dom collada14dom
    PATHS "${LIBS_PREBUILT_DIR}/lib/release" "${LIBS_PREBUILT_DIR}/lib" NO_DEFAULT_PATH REQUIRED)
  target_link_libraries(ll::colladadom INTERFACE ${COLLADADOM_LIBRARY} ll::boost ll::libxml ll::minizip-ng)
  target_include_directories(ll::colladadom SYSTEM INTERFACE
    ${LIBS_PREBUILT_DIR}/include/collada
    ${LIBS_PREBUILT_DIR}/include/collada/1.4
    /usr/include)

  if (LINUX)
    add_library( ll::pcre INTERFACE IMPORTED )
    find_library(PCRE_LIBRARY NAMES pcrecpp pcre
      PATHS /usr/lib/aarch64-linux-gnu /usr/lib REQUIRED)
    target_link_libraries( ll::pcre INTERFACE ${PCRE_LIBRARY} )
  endif ()
  return()
endif ()

use_prebuilt_binary(colladadom)
use_prebuilt_binary(minizip-ng) # needed for colladadom
use_prebuilt_binary(libxml2)

find_library(MINIZIPNG_LIBRARY
    NAMES
    minizip.lib
    libminizip.a
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

target_link_libraries(ll::minizip-ng INTERFACE ${MINIZIPNG_LIBRARY})

find_library(LIBXML2_LIBRARY
    NAMES
    libxml2.lib
    libxml2.a
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

target_link_libraries(ll::libxml INTERFACE ${LIBXML2_LIBRARY})

if (WINDOWS)
    target_link_libraries( ll::libxml INTERFACE Bcrypt.lib)
endif()

target_include_directories( ll::colladadom SYSTEM INTERFACE
        ${LIBS_PREBUILT_DIR}/include/collada
        ${LIBS_PREBUILT_DIR}/include/collada/1.4
        )

find_library(COLLADADOM_LIBRARY
    NAMES
    libcollada14dom23-s.lib
    collada14dom
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

target_link_libraries(ll::colladadom INTERFACE ${COLLADADOM_LIBRARY} ll::boost ll::libxml ll::minizip-ng)

# <FS> GLIB uses pcre, so we need to keep it for Linux builds
if (LINUX)
   add_library( ll::pcre INTERFACE IMPORTED )
   use_prebuilt_binary(pcre)
   target_link_libraries( ll::pcre INTERFACE pcrecpp pcre )
endif ()
# </FS>