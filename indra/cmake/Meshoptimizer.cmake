# -*- cmake -*-

include(Linking)
include(Prebuilt)

include_guard()
add_library( ll::meshoptimizer INTERFACE IMPORTED )

if (LL_LINUX_ARM64)
  # On ARM64, use the system libmeshoptimizer-dev package.
  # The prebuilt .a is x86-only.
  target_link_libraries(ll::meshoptimizer INTERFACE meshoptimizer)
  # /usr/include provides "meshoptimizer.h"; the shim in LIBS_PREBUILT_DIR
  # provides "meshoptimizer/meshoptimizer.h" for sources using that pattern.
  target_include_directories(ll::meshoptimizer SYSTEM INTERFACE
      /usr/include
      ${LIBS_PREBUILT_DIR}/include)
else ()
  use_system_binary(meshoptimizer)
  use_prebuilt_binary(meshoptimizer)

  find_library(MESHOPTIMIZER_LIBRARY
      NAMES
      meshoptimizer.lib
      libmeshoptimizer.a
      PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

  target_link_libraries(ll::meshoptimizer INTERFACE ${MESHOPTIMIZER_LIBRARY})
  target_include_directories(ll::meshoptimizer SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/meshoptimizer)
endif ()
