# Optional during compilation; mandatory in the distributable package checker.
set(TASIA_FFMPEG_DIR "${CMAKE_BINARY_DIR}/tasia-ffmpeg" CACHE PATH
    "Verified converter bundle from scripts/Stage-TasiaFFmpeg.ps1")
function(tasia_stage_ffmpeg target)
    if(NOT WINDOWS)
        return()
    endif()
    set(bundle_files tasia-ffmpeg.exe tasia-ffmpeg-license.txt tasia-ffmpeg-source.json)
    foreach(name IN LISTS bundle_files)
        if(NOT EXISTS "${TASIA_FFMPEG_DIR}/${name}")
            message(WARNING "Tasia FFmpeg bundle incomplete: stage with scripts/Stage-TasiaFFmpeg.ps1 before packaging")
            return()
        endif()
    endforeach()
    foreach(name IN LISTS bundle_files)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TASIA_FFMPEG_DIR}/${name}" "$<TARGET_FILE_DIR:${target}>/${name}"
            VERBATIM)
    endforeach()
endfunction()
