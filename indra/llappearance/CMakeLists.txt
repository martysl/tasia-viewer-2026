# -*- cmake -*-

project(llappearance)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLCoreHttp)
include(LLWindow)
include(Linking)

set(llappearance_SOURCE_FILES
    llavatarappearance.cpp
    llavatarjoint.cpp
    llavatarjointmesh.cpp
    lldriverparam.cpp
    lljointdata.h
    lllocaltextureobject.cpp
    llpolyskeletaldistortion.cpp
    llpolymesh.cpp
    llpolymorph.cpp
    lltexglobalcolor.cpp
    lltexlayer.cpp
    lltexlayerparams.cpp
    llwearable.cpp
    llwearabledata.cpp
    llwearabletype.cpp
    llviewervisualparam.cpp
    llavatarappearancedefines.cpp
    )
    
set(llappearance_HEADER_FILES
    CMakeLists.txt

    llavatarappearance.h
    llavatarjoint.h
    llavatarjointmesh.h
    lldriverparam.h
    lljointpickname.h
    lllocaltextureobject.h
    llpolyskeletaldistortion.h
    llpolymesh.h
    llpolymorph.h
    lltexglobalcolor.h
    lltexlayer.h
    lltexlayerparams.h
    llwearable.h
    llwearabledata.h
    llwearabletype.h
    llviewervisualparam.h
    llavatarappearancedefines.h
    )

list(APPEND llappearance_SOURCE_FILES ${llappearance_HEADER_FILES})

# Same need as llui: lltexlayer.cpp includes llrender2dutils.h (via llui.h) and
# llrendertarget.h, both of which pull in DX headers under DX_RENDER.
if(DX_RENDER)
  include(DXRender)
endif()

if(DX_RENDER AND WIN32)
  include_directories(${DXRENDER_INCLUDE_DIRS})
endif()

add_library (llappearance ${llappearance_SOURCE_FILES})

target_link_libraries(llappearance
  ${DXRENDER_LIBRARIES}
        llcharacter
        llinventory
        llimage
        llrender
        llfilesystem
        llmath
        llxml
        llmessage
        llcorehttp
        llcommon
    )
target_include_directories( llappearance  INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR})

if (BUILD_HEADLESS)
  add_library (llappearanceheadless ${llappearance_SOURCE_FILES})
  target_include_directories( llappearanceheadless  INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR})

  target_link_libraries(llappearanceheadless
          llcharacter
          llinventory
          llimage
          llfilesystem
          llmath
          llxml
          llmessage
          llcorehttp
          llcommon
      )
endif (BUILD_HEADLESS)
