# -*- cmake -*-
include(Prebuilt)
include(GLIB) # <FS:Beq/> add glib includes
if (NOT LINUX)
  return()
endif()

add_library( ll::gstreamer INTERFACE IMPORTED )
target_compile_definitions( ll::gstreamer INTERFACE LL_GSTREAMER010_ENABLED=1)
use_system_binary(gstreamer10)

if (LL_LINUX_ARM64)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GSTREAMER10 REQUIRED gstreamer-1.0 gstreamer-base-1.0)
  target_include_directories( ll::gstreamer SYSTEM INTERFACE ${GSTREAMER10_INCLUDE_DIRS})
  target_link_libraries( ll::gstreamer INTERFACE ${GSTREAMER10_LIBRARIES})
  return()
endif ()

use_prebuilt_binary(gstreamer10)
use_prebuilt_binary(libxml2)
# <FS:Beq> make sure glib is found
set(_gstreamer10_include_dirs
    ${LIBS_PREBUILT_DIR}/include/gstreamer-1.0
    ${GLIB_INCLUDE_DIRS}
)
list(REMOVE_ITEM _gstreamer10_include_dirs "")
# </FS:Beq>

target_include_directories( ll::gstreamer SYSTEM INTERFACE ${_gstreamer10_include_dirs})
