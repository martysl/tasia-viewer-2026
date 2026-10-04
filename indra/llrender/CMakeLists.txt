# -*- cmake -*-

project(llrender)

include(00-Common)
include(OpenGL)
include(FreeType)
include(LLCommon)
include(LLImage)
include(LLWindow)
# S24: DX_RENDER backend. include() is guarded on the option alone (matching
# the donor): DXRender.cmake only sets two variables and has an include guard,
# so it is inert and harmless on a non-Windows host.
if(DX_RENDER)
  include(DXRender)
endif()

set(llrender_SOURCE_FILES
    llcubemap.cpp
    llcubemaparray.cpp
    llfontbitmapcache.cpp
    llfontfreetype.cpp
    llfontfreetypesvg.cpp
    llfontgl.cpp
    llfontvertexbuffer.cpp
    llfontregistry.cpp
    llgl.cpp
    llglslshader.cpp
    llgltexture.cpp
    llimagegl.cpp
    llpostprocess.cpp
    llrender.cpp
    llrender2dutils.cpp
    llrendernavprim.cpp
    llrendersphere.cpp
    llrendertarget.cpp
    llshadermgr.cpp
    lltexture.cpp
    lltexturemanagerbridge.cpp
    lluiimage.cpp
    llvertexbuffer.cpp
    llglcommonfunc.cpp
    )
    
set(llrender_HEADER_FILES
    CMakeLists.txt

    llcubemap.h
    llcubemaparray.h
    llfontgl.h
    llfontvertexbuffer.h
    llfontfreetype.h
    llfontfreetypesvg.h
    llfontbitmapcache.h
    llfontregistry.h
    llgl.h
    llglheaders.h
    llglslshader.h
    llglstates.h
    llgltexture.h
    llgltypes.h
    llimagegl.h
    llpostprocess.h
    llrender.h
    llrender2dutils.h
    llrendernavprim.h
    llrendersphere.h
    llshadermgr.h
    lltexture.h
    lltexturemanagerbridge.h
    lluiimage.h
    lluiimage.inl
    llvertexbuffer.h
    llglcommonfunc.h
    )

list(APPEND llrender_SOURCE_FILES ${llrender_HEADER_FILES})

# S24: the 12 DX-only llrender translation units. Deliberately appended from a
# separate list rather than inlined into the two lists above, so that with
# DX_RENDER=OFF these files are not part of the build at all - not compiled,
# not listed, no ${DXRENDER_INCLUDE_DIRS} on the include path. The generated
# DX_RENDER=OFF build is therefore identical to today's.
# (add_library() and its include_directories() context follow below, so this
# must stay above them.)
# Guarded on WIN32 as well as DX_RENDER, matching DXRender.cmake's own stated
# contract ("only reachable when DX_RENDER is ON *and* the host is Windows")
# and indra/CMakeLists.txt's add_subdirectory(dxrender) guard. The DX_RENDER=1
# preprocessor macro is likewise only defined in the Windows block of
# 00-Common.cmake, so on a non-Windows host the option stays completely inert
# instead of compiling six sources that can never see it.
if(DX_RENDER AND WIN32)
  list(APPEND llrender_SOURCE_FILES
      DXCubeMap.cpp
      DXCubeMapArray.cpp
      lldxtexture.cpp
      llfontdx.cpp
      llhlslshader.cpp
      llimagedx.cpp
      DXCubeMap.h
      DXCubeMapArray.h
      lldxtexture.h
      llfontdx.h
      llhlslshader.h
      llimagedx.h
      )

  # DXTexture.h / DXShader.h / DXDevice.h / DXUIBatch.h etc. live in dxrender.
  include_directories(${DXRENDER_INCLUDE_DIRS})
endif()

if (BUILD_HEADLESS)
  add_library (llrenderheadless
    ${llrender_SOURCE_FILES}
    )

  set_property(TARGET llrenderheadless
    PROPERTY COMPILE_DEFINITIONS LL_MESA=1 LL_MESA_HEADLESS=1
      )

  target_link_libraries(llrenderheadless
          llcommon
          llimage
          llmath
          llrender
          llxml
          llfilesystem
          )
endif (BUILD_HEADLESS)

add_library (llrender ${llrender_SOURCE_FILES})
target_include_directories( llrender  INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET llrender PROPERTY POSITION_INDEPENDENT_CODE ON)

# Libraries on which this library depends, needed for Linux builds
# Sort by high-level to low-level
target_link_libraries(llrender
        llcommon
        llimage
        llmath
        llfilesystem
        llxml
        llwindow
        ll::freetype
        OpenGL::GL
        OpenGL::GLU
        )

# S24: additive on top of the OpenGL link above, not an either/or. The donor
# drops OpenGL entirely when DX_RENDER is on, but that only works there because
# its llgl.cpp/llgltexture.cpp/llglslshader.cpp/llimagegl.cpp were rewritten to
# contain no raw gl*() calls. Those four files are still the untouched GL
# originals here and still call gl*(), so OpenGL has to stay linked or the
# final viewer link would break with unresolved externals. Guarded on WIN32 as
# well, because dxrender is only added as a subdirectory when DX_RENDER AND
# WIN32 (see indra/CMakeLists.txt) - linking a target that was never defined
# is a hard configure error.
if(DX_RENDER AND WIN32)
  target_link_libraries(llrender ${DXRENDER_LIBRARIES})
endif()
