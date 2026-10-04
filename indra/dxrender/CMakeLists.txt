# -*- cmake -*-
#
# Direct3D 11 renderer backend.
#
# Added from the Kirstens Viewer DX_RENDER work. Only added to the build
# when DX_RENDER is ON and the host is Windows (guarded by the caller in
# indra/CMakeLists.txt), because this target links d3d11/dxgi/d3dcompiler/
# dcomp unconditionally.
#
# This library deliberately depends on nothing from llrender: the renderer
# binding itself lives in llrender/newview, so dxrender stays a standalone
# resource/device wrapper layer.

project(dxrender)

include(00-Common)
include(LLCommon)
include(LLMath)

set(dxrender_CORE
    core/DXDevice.cpp
    core/DXContext.cpp
    core/DXPipelineState.cpp
    core/DXSwapChain.cpp
    core/DXStateCache.cpp
    core/DXWorkerDevice.cpp
    core/DXSharedResource.cpp
    core/DXCompositionSurface.cpp
)

set(dxrender_RESOURCES
    resources/DXBuffer.cpp
    resources/DXTexture.cpp
    resources/DXCubeTexture.cpp
    resources/DXCubeArrayTexture.cpp
    resources/DXCubeMapFaces.cpp
    resources/DXShader.cpp
    resources/DXRenderTarget.cpp
    resources/DXSampler.cpp
    resources/DXVertexLayout.cpp
    resources/DXUIBatch.cpp
    resources/DXReadback.cpp
    resources/DXRender2DUtils.cpp
    resources/DXQuery.cpp
    resources/DXOcclusionQuery.cpp
)

set(dxrender_HEADER_FILES
    core/DXDevice.h
    core/DXContext.h
    core/DXPipelineState.h
    core/DXSwapChain.h
    core/DXStateCache.h
    core/DXWorkerDevice.h
    core/DXSharedResource.h
    core/DXCompositionSurface.h

    resources/DXBuffer.h
    resources/DXTexture.h
    resources/DXCubeTexture.h
    resources/DXCubeArrayTexture.h
    resources/DXCubeMapFaces.h
    resources/DXShader.h
    resources/DXRenderTarget.h
    resources/DXSampler.h
    resources/DXVertexLayout.h
    resources/DXUIBatch.h
    resources/DXReadback.h
    resources/DXRender2DUtils.h
    resources/DXQuery.h
    resources/DXOcclusionQuery.h
)

set_source_files_properties(${dxrender_HEADER_FILES}
    PROPERTIES HEADER_FILE_ONLY TRUE
)

set(dxrender_SOURCE_FILES
    ${dxrender_CORE}
    ${dxrender_RESOURCES}
    ${dxrender_HEADER_FILES}
)

add_library(dxrender ${dxrender_SOURCE_FILES})

# Sort by high-level to low-level, matching llrender.
# llcommon/llmath/llfilesystem supply the headers through their INTERFACE
# include directories, so no include_directories() is needed here.
target_link_libraries(dxrender
    llcommon
    llmath
    llfilesystem

    # Direct3D 11 system libraries
    d3d11
    dxgi
    d3dcompiler
    dcomp
)

# core/ and resources/ include each other (e.g. DXShader.cpp -> DXDevice.h).
# PUBLIC, not INTERFACE: INTERFACE only applies to consumers of this target,
# not to dxrender's own sources, which then cannot resolve "DXDevice.h".
target_include_directories(dxrender PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

set_property(TARGET dxrender PROPERTY POSITION_INDEPENDENT_CODE ON)

if (MSVC)
  target_compile_options(dxrender PRIVATE "/Zc:wchar_t-")
endif()
