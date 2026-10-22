#!/usr/bin/env python3
"""Verify the legacy host-bake fetch policy against the real source.

Cloud-avatar investigation history, kept here so the wrong turn is not repeated:

1. FTT_HOST_BAKE was reported as "Unsupported fetch type 2" at runtime, which
   looked like an unimplemented legacy path.
2. A first attempt set mCanUseHTTP = false for FTT_HOST_BAKE, reasoning that
   baked textures should come from the region host over UDP.
3. That was WRONG. Kirsten has no UDP texture transfer path at all:
   lltexturefetch.cpp aborts any fetch whose mCanUseHTTP is false
   ("abort, nowhere to get data"), so the override disabled the only working
   route instead of enabling another one.
4. A live capability probe proved i-Grid advertises the HTTP ViewerAsset
   capability, so the correct policy is to leave HTTP enabled and use the
   opaque capability path with `?texture_id=<uuid>` on OpenSim. Only the
   Second Life route adds the legacy slash before the query.

So this test now asserts HTTP stays enabled for FTT_HOST_BAKE.
"""

import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
texture_cpp = (root / "llviewertexture.cpp").read_text()
texture_h = (root / "llviewertexture.h").read_text()
fetch_cpp = (root / "lltexturefetch.cpp").read_text()
avatar_cpp = (root / "llvoavatar.cpp").read_text()
net_h = (root / "llviewernetwork.h").read_text()

# 1. FTT_HOST_BAKE is a real enum value (not a fabricated type).
assert re.search(r"FTT_HOST_BAKE\s*,", texture_h), "FTT_HOST_BAKE missing from enum"

# 2. The constructor must NOT warn "unsupported" any more: the type is supported.
start = texture_cpp.index("LLViewerFetchedTexture::LLViewerFetchedTexture(const LLUUID& id, FTType f_type")
opening = texture_cpp.index("{", start)
depth, end = 1, opening + 1
while depth:
    depth += (texture_cpp[end] == "{") - (texture_cpp[end] == "}")
    end += 1
ctor = texture_cpp[start:end]

# Only executable logging counts, never the explanatory comment.
assert 'LL_WARNS() << "Unsupported fetch type "' not in ctor, \
    "host-bake still reported as unsupported at runtime"
assert not re.search(r"mCanUseHTTP\s*=\s*false", ctor), \
    "host-bake must NOT disable HTTP: Kirsten has no UDP texture path, so the " \
    "override would abort the fetch. i-Grid advertises HTTP ViewerAsset."

# 3. The flag actually reaches the fetch worker (otherwise the fix is cosmetic).
assert "mCanUseHTTP)" in fetch_cpp or "mCanUseHTTP," in fetch_cpp, \
    "can_use_http not forwarded to createRequest"
assert "worker->setCanUseHTTP(can_use_http)" in fetch_cpp, \
    "fetch worker never receives the HTTP capability flag"
call = texture_cpp[texture_cpp.index("createRequest(mFTType"):]
call = call[:call.index(";")]
assert "mCanUseHTTP" in call, "mCanUseHTTP not passed from texture into createRequest"

# 4. HTTP is enabled by default, which is exactly why it must stay that way.
assert re.search(r"mCanUseHTTP\s*\(\s*true\s*\)", fetch_cpp), \
    "worker default no longer enables HTTP; host-bake policy assumption changed"

# 4b. The ViewerAsset URL route that actually serves these textures must remain
#     intact, and the abort-on-no-HTTP branch must still be a hard abort.
assert 'region->getViewerAssetUrl()' in fetch_cpp, \
    "ViewerAsset URL lookup removed; OpenSim baked textures would have no route"
assert 'TasiaAssetCaps::assetURL(http_url, "texture", mID.asString(),' in fetch_cpp, \
    "ViewerAsset texture_id URL must preserve the grid-compatible capability path"
assert 'http_url + "/?texture_id="' not in fetch_cpp, \
    "Do not append the Second Life slash to OpenSim opaque capability paths"
assert "abort, nowhere to get data" in fetch_cpp, \
    "abort guard removed; do not assume a UDP fallback exists"

# 5. The legacy route is still OpenSim-only and host-based, not a fabricated URL.
fetch_fn = avatar_cpp[avatar_cpp.index("LLViewerFetchedTexture *LLVOAvatar::getBakedTextureImage("):]
fetch_fn = fetch_fn[:fetch_fn.index("\n}")]
assert "isInOpenSim()" in fetch_fn and "FTT_HOST_BAKE" in fetch_fn, \
    "legacy baked textures must stay OpenSim-only and host-fetched"
assert "getImageURL" in fetch_fn and fetch_fn.index("return LLViewerTextureManager::getFetchedTexture") \
    < fetch_fn.index("getImageURL"), \
    "host fetch must return before the Linden appearance URL path can overwrite it"

# 6. Agni/Aditi never enter the legacy branch.
assert "isInSecondLife() const { return mGrid == MAINGRID || mGrid == BETAGRID; }" in net_h
assert "isInOpenSim() const { return !isInSecondLife(); }" in net_h

# 7. No stray warning remains anywhere for this fetch type.
assert 'LL_WARNS() << "Unsupported fetch type "' not in texture_cpp

print("PASS: FTT_HOST_BAKE is supported, keeps HTTP enabled for the ViewerAsset "
      "route, never disables it (no UDP fallback exists), stays OpenSim-only, "
      "and no longer warns as unsupported")
