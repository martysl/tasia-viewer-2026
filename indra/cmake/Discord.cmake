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
# viewer source references the Discord SDK. On ARM64 the libdiscord-rpc.a
# autobuild prebuilt is x86-only (EM: 62), so make fs::discord a no-op there.
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
endif ()
