# -*- cmake -*-
include_guard()
include(Prebuilt)

add_library(ll::zlib INTERFACE IMPORTED)

use_prebuilt_binary(zlib)

# The sources include <zlib.h> and call the plain zlib API
# (llsdserialize.cpp, llreflectionmapmanager.cpp), and the matching header at
# include/zlib.h is stock zlib 1.3.1 - not the zlib-ng compat header.
#
# This used to name "zs.lib"/"zsd.lib", which are not what the zlib port in
# installS24.xml produces under the x64-windows-static-viewer triplet; that
# port emits zlib.lib. Every consumer then failed the link with LNK1181 on
# the missing zs.lib. The zlib-ng port also installed alongside it as
# zlibstatic-ng.lib, but linking that against the stock header would mix two
# implementations of the same symbols.
set(ZLIB_LIBRARIES optimized zlib.lib debug zlib.lib)
set(ZLIB_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)

target_link_libraries(ll::zlib INTERFACE ${ZLIB_LIBRARIES})
target_include_directories(ll::zlib SYSTEM INTERFACE ${ZLIB_INCLUDE_DIRS})
