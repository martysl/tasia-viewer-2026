# -*- cmake -*-

# <FS:Ansariel> Prefer FS-specific Discord implementation
#include(Prebuilt)
#
#include_guard()
#
#add_library(ll::discord_sdk INTERFACE IMPORTED)
#target_compile_definitions(ll::discord_sdk INTERFACE LL_DISCORD=1)
#
#use_prebuilt_binary(discord_sdk)
#
#target_include_directories(ll::discord_sdk SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/discord_sdk)
#target_link_libraries(ll::discord_sdk INTERFACE discord_partner_sdk)
# </FS:Ansariel>

include_guard()
add_library(fs::discord INTERFACE IMPORTED)

include(Prebuilt)

# LL_DISCORD is never defined (the define is commented out above), so no
# viewer source references the Discord SDK. However fsdiscordconnect.cpp calls
# the Discord_* API unconditionally (no LL_DISCORD guard) and includes
# <discord-rpc/discord_rpc.h>, so discord-rpc (header + lib) is required.
# On x86_64/Windows the autobuild prebuilt is used; on ARM64 the x86 prebuilt
# is EM: 62 so it is built from source and staged by prepare-linux-arm64-deps.sh.
if (NOT LL_LINUX_ARM64)
  use_prebuilt_binary(discord-rpc)

  find_library(DISCORD_LIBRARY
    NAMES
    discord-rpc.lib
    libdiscord-rpc.a
    PATHS "${ARCH_PREBUILT_DIRS_RELEASE}" REQUIRED NO_DEFAULT_PATH)

  target_link_libraries(fs::discord INTERFACE ${DISCORD_LIBRARY})

  target_include_directories(fs::discord SYSTEM INTERFACE
          ${AUTOBUILD_INSTALL_DIR}/include/discord-rpc
          )
else ()
  # ARM64: use the source-built discord-rpc staged by the prep script.
  find_library(DISCORD_LIBRARY
    NAMES libdiscord-rpc.a
    PATHS "${LIBS_PREBUILT_DIR}/lib/release" REQUIRED NO_DEFAULT_PATH)

  target_link_libraries(fs::discord INTERFACE ${DISCORD_LIBRARY})

  target_include_directories(fs::discord SYSTEM INTERFACE
          ${LIBS_PREBUILT_DIR}/include/discord-rpc
          )
endif ()
