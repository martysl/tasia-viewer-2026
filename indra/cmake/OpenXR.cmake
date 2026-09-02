# -*- cmake -*-

include(Prebuilt)

include_guard()
add_library( ll::openxr INTERFACE IMPORTED )

if(USE_CONAN )
  target_link_libraries( ll::openxr INTERFACE CONAN_PKG::openxr )
  return()
endif()

if (LL_LINUX_ARM64)
  find_library(OPENXR_LIBRARY NAMES openxr_loader REQUIRED)
  target_link_libraries( ll::openxr INTERFACE ${OPENXR_LIBRARY} )
  return()
endif ()

use_prebuilt_binary(openxr)
if (WINDOWS)
  target_link_libraries( ll::openxr INTERFACE ${ARCH_PREBUILT_DIRS_RELEASE}/openxr_loader.lib )
else()
  target_link_libraries( ll::openxr INTERFACE ${ARCH_PREBUILT_DIRS_RELEASE}/libopenxr_loader.a )
endif (WINDOWS)

if( NOT LINUX )
  target_include_directories( ll::openxr SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include)
endif()
