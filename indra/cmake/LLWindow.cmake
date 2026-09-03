# -*- cmake -*-

include(Variables)
include(GLEXT)
include(Prebuilt)

include_guard()
add_library( ll::SDL INTERFACE IMPORTED )


if (LINUX)
  #Must come first as use_system_binary can exit this file early
  #target_compile_definitions( ll::SDL INTERFACE LL_SDL=1)

  #use_system_binary(SDL)
  #use_prebuilt_binary(SDL)
  
  target_include_directories( ll::SDL SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include)

  if( USE_SDL1 )
    target_compile_definitions( ll::SDL INTERFACE LL_SDL=1 )

    use_system_binary(SDL)
    use_prebuilt_binary(SDL)
    set (SDL_FOUND TRUE)

    target_link_libraries (ll::SDL INTERFACE SDL directfb fusion direct X11)

  else()
    target_compile_definitions( ll::SDL INTERFACE LL_SDL2=1 LL_SDL=1 )

    if (ARCH STREQUAL "aarch64")
      find_path(SDL2_INCLUDE_ROOT
        NAMES SDL2/SDL.h
        PATHS /usr/include
        REQUIRED NO_DEFAULT_PATH)
      find_library(SDL2_LIBRARY
        NAMES SDL2
        PATHS /usr/lib/${DPKG_ARCH}
        REQUIRED NO_DEFAULT_PATH)
      target_include_directories(ll::SDL SYSTEM INTERFACE ${SDL2_INCLUDE_ROOT})
      target_link_libraries(ll::SDL INTERFACE ${SDL2_LIBRARY} X11)
    else()
      use_system_binary(SDL2)
      use_prebuilt_binary(SDL2)
      target_link_libraries(ll::SDL INTERFACE SDL2 X11)
    endif()
    set (SDL2_FOUND TRUE)
  endif()
endif (LINUX)


