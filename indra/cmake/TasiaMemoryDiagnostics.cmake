# Opt-in, selective MSVC instrumentation for the OpenSim world-loading crash.
# Does not change normal Release builds or enable system-wide heap diagnostics.
option(TASIA_MEMORY_DIAGNOSTICS "Instrument image/volume/texture memory accesses with MSVC AddressSanitizer" OFF)

if(TASIA_MEMORY_DIAGNOSTICS)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR "TASIA_MEMORY_DIAGNOSTICS currently requires Windows/MSVC")
    endif()

    set(_tasia_memory_sources
        llimage/llimage.cpp
        llmath/llvolume.cpp
        llrender/llimagedx.cpp
        dxrender/resources/DXTexture.cpp
        newview/lldrawpoolbump.cpp
    )
    foreach(_relative IN LISTS _tasia_memory_sources)
        get_filename_component(_directory "${_relative}" DIRECTORY)
        # DXTexture.cpp belongs to the dxrender target's directory, not its resources folder.
        if(_directory STREQUAL "dxrender/resources")
            set(_directory "dxrender")
        endif()
        set_property(SOURCE "${CMAKE_SOURCE_DIR}/${_relative}"
            DIRECTORY "${CMAKE_SOURCE_DIR}/${_directory}"
            APPEND PROPERTY COMPILE_OPTIONS
                /fsanitize=address /Y-
                /D_DISABLE_VECTOR_ANNOTATION /D_DISABLE_STRING_ANNOTATION)
    endforeach()
    # MSVC object directives infer the matching ASan runtime automatically.
    # Disable STL container annotations because other/prebuilt translation units
    # are deliberately not instrumented. Heap access checking remains enabled.
    message(STATUS "Tasia memory diagnostics: selective MSVC AddressSanitizer enabled")
endif()
