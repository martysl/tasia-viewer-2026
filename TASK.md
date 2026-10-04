# TASK.md — D3D11 backend for Tasia Viewer

Branch: `tasia-dx11-merge` (never merged to `main`)
Worktree: `/home/marty/opensim/tasia-viewer-2026-work`
Donor (read-only reference): `/home/marty/Kirstensviewer`
Artifacts and tooling: `/home/marty/opensim/tasia-dx11-notes/`

## Definition of done

A Windows build with `DX_RENDER=ON` that **starts and renders a region**, verified
by screenshot, with the native Linux OpenGL build still green.

"Compiles" is explicitly **not** done. Three earlier states all compiled and
rendered nothing.

## Non-negotiable rules

1. **Additive only.** Never remove, rename or reorder OpenGL code. Never change GL
   behaviour. The donor's GL deletions are out of scope. Every change to a
   pre-existing file is either a new declaration inside `#ifdef DX_RENDER` or an
   addition that provably cannot affect GL.
2. **Verify, don't assume.** Every claim needs a command output or file:line.
3. **Never claim it renders without rendering it.** Screenshot or it did not happen.
4. **Linux stays green.** `BUILD_EXIT=0` on the native build after every step.
5. Do not modify the donor. Do not commit unless told. Do not push.

## Tooling

| tool | purpose |
|---|---|
| `/home/marty/opensim/tasia-dx11-notes/dxcheck.sh` | local mingw semantic check of DX sources, seconds. `-DDX_RENDER=1 -DWIN32 -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -std=c++20` |
| `/home/marty/opensim/tasia-dx11-notes/mingwshim/` | APR/shlobj stubs; the packaged APR headers are Linux-flavoured and unusable from mingw |
| `/home/marty/opensim/tasia-dx11-notes/env.sh` | autobuild environment |
| `/home/marty/opensim/tasia-dx11-notes/reconf_build.sh` | Linux reconfigure + full build |
| CI `build-windows-dxrender.yml` | MSVC authority, ~4 min, builds `dxrender` and `llrender` |
| CI `build-windows.yml` | full Windows portable build, input `dx_render` |

`dxcheck.sh` does **not** see MSVC-only warnings (C4005, C2220). CI is the
authority. Never put scratch files in `/tmp` — it is tmpfs and is wiped on
suspend/reboot.

---

## Completed

- [x] XML/UI repairs (16 files). Linux `BUILD_EXIT=0`.
- [x] `indra/dxrender` — 45 files, compiles, `dxrender.lib`.
- [x] `indra/llrender` DX surface — 12 files, 7077 lines, additive declarations.
- [x] llrender DX compile errors — 5 groups, incl. `llshaderfeatures.h` extraction.
- [x] llrender DX definitions — link closure resolves all 12 task symbols.

## Remaining work

### T1 — llshadermgr DX branch of `loadShaderFile`
**Why:** `llhlslshader.cpp:531` always returns false, so **no HLSL is ever compiled**.
Without this, DX renders nothing even once linked.
**Do:** port the DX branch from donor `llshadermgr.cpp:751+`, guarded. Populate
`mVertexShaderSourceText` / `mFragmentShaderSourceText`. Honour `attaches_deferred_util`.
**Done when:** a runtime log line proves a shader was compiled via the DX path.

### T2 — HLSL shader set
**Why:** 281 files the DX backend loads at runtime; none exist in the target.
**Do:** copy `indra/newview/app_settings/shaders/**/*.hlsl` and `varying/*.hlsli`
from the donor. Ensure the shader include path resolves (`DXShader.h:99-108`
documents `#include "varying/uiVarying.hlsli"` handling). Do **not** port GLSL over
the existing GLSL — the target already has 224 working `.glsl`.
**Done when:** dxcheck clean, Linux untouched, and the include path is proven.

### T3 — `LLGLState::setEnabled` DX branch
**Why:** **blocking for correctness.** The donor routes `GL_BLEND` →
`applyDXBlendState()` and `GL_CULL_FACE`/`GL_SCISSOR_TEST`/`GL_DEPTH_CLAMP` →
`applyDXRasterizerState()`. Without it, enabling blending or toggling cull never
reaches D3D11 — only `LLRender::blendFunc()` pushes a blend state.
**Also:** `applyDXRasterizerState` currently has no caller, so `sCullFace` and the
polygon-offset fields are dead. Port the donor's `applyDXState()` helper into
`llgl.cpp` and wire the callers. `LLRender::setColorMask` must also reach
`applyDXBlendState()` or the D3D11 write mask lags.
**Authorisation:** this edits shared GL code. Additions and guards only.
**Done when:** toggling cull and blend in a DX build provably changes D3D11 state.

### T4 — newview DX pipeline
**Why:** the renderer itself. Largest single item.
**Do:** port 20 files from the donor `indra/newview/dx*`: `dxpipeline.cpp` (1938
lines), `dxbc7compressor.*` (438), `dxbc7uploadmanager.*` (210) and six
`dxdrawpool*` (~2800). Plus `indra/newview/CMakeLists.txt` wiring behind
`if(DX_RENDER)`, and any DX settings the code reads.
**Depends on:** T1, T2, T3.
**Done when:** `viewer` compiles and links with `DX_RENDER=ON`.

### T5 — call-site renames in llappearance / llui / llwindow
**Partly done:** the ~336 shader program sites now use `LLViewerShaderProgram`,
which resolves to `LLHLSLShader` under DX_RENDER, so programs are constructed and
`D3DCompile` runs. What remains is the surrounding subsystems.
**Still to do:** `llappearance` `gGL`→`gDX`; `llwindow` loses
`wglCreateContextAttribsARB` negotiation; `llui` DX font path; and the
`LLEnvironment::updateShaderUniformsDX` declaration (`llenvironment.h:138`) —
without it sky/water uniforms never reach the DX shaders.
**Done when:** no DX build references an uninitialised program or a dead uniform path.

### T7a — `LLRenderTarget` DX forwarders  ← new, blocking
**Why:** 11 errors. `dxpipeline` calls `LLRenderTarget::getColorSRV`,
`bindTarget(bool,bool)`, `clearColor`, `rebindWithDepth`, which exist only in the
donor. All five are one-line inline forwarders to `mDXRenderTarget`.
**Do:** port additively under `#ifdef DX_RENDER` in `llrender/llrendertarget.h`.
**Done when:** those five resolve.

### T7b — `gDXViewport` is never written  ← new, correctness
**Why:** it is defined but no DX code writes it, and `dxdrawpoolwlsky.cpp:362-363`
reads it, so the WLSky pass will read 0 for origin and size. The donor's
`llrendertarget.cpp:329-336` does the GL bottom-left → DX top-left flip
(`TopLeftY = height - (y + h)`). The target's `llrendertarget.cpp` has zero
`DX_RENDER` occurrences, so `glViewport` reaches nothing under DX.
**Do:** port the viewport flip, writing `gDXViewport`.
**Done when:** `gDXViewport` is written where `glViewport` is called.

### T7c — `pipeline.h` DX signatures  ← new, the real gate
**Why:** 26 errors, the largest single group. The donor renamed
`LLGLSLShader`→`LLHLSLShader` in shared headers; we deliberately kept the GL
names and typedef'd instead, so `LLPipeline::bindDeferredShader`,
`bindDeferredShaderFast`, `unbindDeferredShader`, `setEnvMat`,
`bindReflectionProbes`, `unbindReflectionProbes`, `bindShadowMaps`,
`bindLightFunc`, `setupSpotLight` and `LLRenderPass::uploadMatrixPalette` still
take `LLGLSLShader&`. Donor refs: `pipeline.h:330,331,334`, `lldrawpool.h:392`.
**Do:** take `LLViewerShaderProgram&` under DX_RENDER, plus `getPools()`,
`mStereoEyeL/R`, `mSSAOHistory`, `getNearbyLights()`, `getMaskMode`,
`updateUniformsPerFrame()`, `getLightScale()`, `getDetailTexture`.
**Done when:** dxpipeline compiles.

### T6 — gate: viewer links
Extend the CI gate to build the `viewer` target with `DX_RENDER=ON`. A static
library building is **not** sufficient — unresolved externals only surface at link.
**Done when:** `viewer.exe` produced.

### T7 — full Windows portable build + visual verification
Run `build-windows.yml` with `dx_render=true`. Then run the result under Wine on
this machine (`wine64`, Vulkan 1.4 + NVIDIA already verified working) and screenshot.
Compare against the native GL Linux build.
**Done when:** a screenshot shows a rendered region through the DX path.

### T7d — small accessor gaps  ← new
`llreflectionmapmanager.h` `updateUniformsPerFrame`/`getLightScale`;
`llvlcomposition.h` `getDetailTexture`/`getDetailRenderMaterials` (members exist
but are `protected`); `lldrawpoolwater.h:43` needs `friend class DXDrawPoolWater`
for `mWaterNormp`; `llvosky.h` `getCubeMap()` return type; `llspatialpartition.h`
`LLDrawInfo::mAttachedToAvatar`.

### T7e — donor-only features to decide on
`kveffects`/OpenCL post-fx (already excluded — no OpenCL in this tree);
WLSky galactic band, constellation lines and shooting stars (donor-only sky
feature, 7 errors); `DXBC7UploadManager` (46 errors, needs the excluded
OpenCL block). All three are donor extras, not port regressions.

## Known accepted regressions

- Tabular figures in `LLFontDX` — needs `EFontHinting` in shared GL font classes.
  DX text uses proportional digit advances, same as this tree's GL path.
- `LLTexUnit::bind(DXTexture&,…)` and `syncDXBindState()` not ported; no call site
  needs them yet.
- **BC7 texture compression unavailable** — needs OpenCL, which this tree lacks.
- **WLSky galactic band / constellations / shooting stars absent** — donor-only.
- **Scissor unimplemented under DX in both trees** — no `RSSetScissorRects`
  exists in donor or target; `DXStateCache.cpp` is byte-identical between them.
- **Polygon-offset sign is a shared donor limitation** — `DXStateCache.cpp:179`
  passes GL's positive offset straight through, which is wrong under reversed-Z.
  Left matching the donor deliberately rather than diverging on a shadow-path
  value that cannot be tested here.

## Review protocol

Each task: a subagent implements, then I independently re-run `dxcheck.sh`, inspect
`git diff` for any deletion, and re-read the claim. A subagent's own verification is
evidence, not proof. Anything touching shared GL code gets the preprocessed-output
comparison against HEAD.
