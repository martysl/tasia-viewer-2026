# DX11 port — complete remaining work inventory

Audited 2026-10-13 with 4 non-overlapping agents covering indra/ newview,
llrender, everything-else, and the UI render path. Nothing skipped.

## THE STRATEGIC PROBLEM (one sentence)

The donor DELETED the OpenGL half. We KEPT it and layered DX alongside.
So every class now exists in two versions (`LLFontGL`/`LLFontDX`,
`LLImageGL`/`LLImageDX`, `LLGLTexture`/`LLDXTexture`, `LLGLSLShader`/`LLHLSLShader`,
`gGL`/`gDX`) and **all UI code still calls the GL half**.

Proof: `llui/` has **0** `DX_RENDER` guards. `DXUIBatch.cpp` and
`DXRender2DUtils.cpp` are byte-identical to the donor, compiled, linked,
and referenced by **nothing**.

## The immediate crash: LLFontGL::sFontRegistry is NULL

`llrender/llfontgl.cpp:1231` — `return sFontRegistry->getFont(desc);`

`sFontRegistry` is created only in `LLFontGL::initClass()`, which we correctly
excluded under DX. But **275 call sites in llui/** still call
`LLFontGL::getFontSansSerif()` etc. — the default arguments of every control's
`Params` constructor, evaluated at XUI load, before login.

Reached from: `llui/lluictrl.cpp:83`, `llcontainerview.cpp:131`,
`lltextbase.cpp:1225`, `llbutton.cpp:1203`, `llviewerwindow.cpp:1065/3005/3125/3180`,
`lltracker.cpp:724`, `lloutfitgallery.cpp:1084`, `fsfloaterim.cpp:1047`.

This is NOT shimmable — a second registry cannot be made non-null cheaply.
The donor's answer is a tree-wide `LLFontGL` → `LLFontDX` rename.

## 4 COMPILE BLOCKERS (we never hit these — the last successful build was
## before these files were compiled into a DX=ON link)

| # | Where | Error |
|---|---|---|
| C1 | `llappearance/lltexlayer.h:31` + `lltexlayer.cpp` (19 uses) | `gAlphaMaskProgram` undeclared — `llglslshader.h` hides it under `#if !defined(DX_RENDER)`. Fix: include `llhlslshader.h` (which already declares it at `llhlslshader.h:310`) |
| C2 | `llui/CMakeLists.txt` | missing `include(DXRender)` + `include_directories(${DXRENDER_INCLUDE_DIRS})` + `target_link_libraries(... ${DXRENDER_LIBRARIES})` |
| C3 | `llappearance/CMakeLists.txt` | same three blocks missing |
| C4 | `llrender/llrendertarget.h:40` | `#include "DXRenderTarget.h"` is unguarded — should be `#ifdef DX_RENDER` like `llvertexbuffer.h:44` |

C2/C3 are why `llui/` and `llappearance/` **do not compile** under DX_RENDER today.

## Severity table

| severity | count | top items |
|---|---|---|
| COMPILE BLOCKER | 4 | C1–C4 above |
| CRASH | 1 | `LLFontGL::sFontRegistry` NULL (275 sites) |
| CRASH (structural) | 2 | `gAlphaMaskProgram`, `LLFontGL` in llfontvertexbuffer |
| CORRECTNESS | 8 | gGL/gDX matrix split, viewport never set, white-texture fallback, `LLRender2D` origin statics |
| LATENT | 14 | dead GL TUs in the build, `glScissor`, `color4ub` wrong static, thread_local gaps |
| DEAD | 6 | `llpostprocess.cpp`, `llrendersphere`, `llrendernavprim`, `gl_segmented_rect_2d_tex` |

## The 4 structural renames (the actual work)

| rename | scope | mechanical? |
|---|---|---|
| `LLFontGL` → `LLFontDX` | ~150 files | yes, `sed` — but LLFontGL must not be shimmed |
| `gl_*` → `dx_*` in llrender2dutils | ~110 files, 333 call sites | yes — bodies route through DXRender2DUtils + gDXUIBatch |
| `gGL` → `gDX` in llui/ | 147 sites, 27 files | yes |
| `LLGLTexture` → `LLDXTexture` | llappearance + llui | yes — but nothing derives from LLDXTexture yet |

## The one redesign

`LLTexture::getGLTexture()` returns `LLImageGL*`. The donor returns `LLImageDX*`.
Until that swap: **every texture binds as flat white.** Terrain, avatars,
textures, icons. The UI will render, just untextured.

`LLViewerTexture : LLGLTexture` → `LLDXTexture` also needs `LLImageDX` to gain
the statics LLImageGL has (`setManualImage`, `dirtyTexOptions`,
`allocateConversionBuffer`) which ~15 newview sites call.

## Recommended order

1. **C1–C4** — make llui/llappearance compile at all. Without this nothing else can be tested.
2. **LLFontGL null guard** (1 line) — un-crashes the viewer immediately, lets it reach login.
3. **viewport** — `llviewerwindow.cpp:6871/6889` → `llSetDXViewport()`. 2 lines. Without a viewport nothing is visible.
4. **gGL → gDX in llui/** + llrender2dutils context swap — makes UI geometry correct.
5. **LLFontGL → LLFontDX** rename — makes text actually draw.
6. **LLTexture type swap** — makes textures stop being white. Last; independent of the crash.

Steps 1–3 are small. Step 4–5 are large but mechanical. Step 6 is the redesign.

## Highest-value single experiment

Remove `OpenGL::GL OpenGL::GLU` from `indra/llrender/CMakeLists.txt`
`target_link_libraries` under `DX_RENDER AND WIN32`. The donor documents this
technique. The link error then enumerates every remaining GL symbol dependency
definitively, instead of us finding them one crash at a time.