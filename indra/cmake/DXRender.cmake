# -*- cmake -*-
# Direct3D 11 renderer backend (Windows only).
#
# The viewer keeps OpenGL as the portable backend. This module is only
# reachable when DX_RENDER is ON *and* the host is Windows, because the
# D3D11/3D12/DXGI/DirectComposition headers and import libraries do not
# exist elsewhere. On every other platform the option is inert and the
# build is byte-for-byte the OpenGL one.
if (DXRENDER_CMAKE_INCLUDED)
  return()
endif (DXRENDER_CMAKE_INCLUDED)
set (DXRENDER_CMAKE_INCLUDED TRUE)

include(Variables)

set(DXRENDER_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/dxrender/core
    ${LIBS_OPEN_DIR}/dxrender/resources
    )

set(DXRENDER_LIBRARIES
    dxrender
    )
