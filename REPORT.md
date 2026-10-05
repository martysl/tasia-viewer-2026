# REPORT.md — D3D11 backend progress log

Branch `tasia-dx11-merge`. Newest first. Verification evidence only, no claims
without a command behind them.

## R11 — THE DX VIEWER LINKS (`ae98e5e`, CI run `37357859954`)

| check | result |
|---|---|
| MSVC, `DX_RENDER=ON`, builds **and links** `firestorm-bin.exe` | **SUCCESS**, 1h02m |
| `viewer linked` assertion | passed |
| `LNK1120` unresolved externals | **0** (was 3) |
| mingw gates | 8/8, 26/26, 10/10 |
| native Linux | `BUILD_EXIT=0` |

`firestorm-bin.exe` now exists under `newview/Release/`. This is the first
moment the DX backend could actually be executed; three earlier states in this
project compiled and rendered nothing.

The last three link errors:
- `gEXRImage` — a real type mismatch across the shared/DX boundary.
  `llreflectionmapmanager.cpp`, compiled in both builds, defined it as
  `LLPointer<LLImageGL>` while `dxdrawpoolwlsky.cpp` declared
  `LLPointer<LLImageDX>`. Different instantiations, different mangled names.
  Fixed with an alias following the existing `LLVOSkyCubeMap` pattern, applied
  to all four declaration sites rather than only the definition.
- `gLastCompositedPostTarget` — no reader in this tree at all. Defined in the
  DX-only `dxpipeline.cpp`.
- `FTM_RENDER_WATER_OPAQUE` — a real omission; this tree's
  `lldrawpoolwater.cpp` contains no `LLTrace` at all.

**The checker review found two gates that were lying and one certain crash:**
- `glregress-alias.sh` compared against `HEAD`, which by then *was* the commit
  containing the change, so it would have passed while comparing the tree with
  itself. Re-anchored to `b6f6360^`.
- `dxcheck.sh` continued past a missing input file without setting its failure
  flag, and exited 0 having checked nothing when the list came back empty.
- `dxcheckshared.sh` silently covered 15 of 18 shared files. Now 26.
- **`LLGLSLShader::sCurBoundShaderPtr` is permanently NULL under DX** — assigned
  only inside `LLGLSLShader::bind()`, which the DX path never calls. Eleven
  files dereferenced it unguarded, `lldrawpool.cpp:675` worst. Substituted the
  alias at 23 sites. This compiled, linked, and would have crashed on the first
  draw.

Also fixed: `is_little_endian()` had two external definitions once both
`llimagegl.cpp` and `llimagedx.cpp` were in the build. The donor never hits it
because `llimagegl.cpp` is not in its build; here the DX copy became `static`,
leaving exactly one external definition and clearing MSVC's LNK4006 plus the
LNK4088 warning that the image may not run.

## R10 — T2 + T5 + depth state (`21bb380`)

| check | result |
|---|---|
| `dxcheck.sh` | 6/6 OK |
| deletions in `indra/llrender` | **0** |
| deletions in `indra/newview` not containing `LLGLSLShader` | **0** |
| Linux full build | `BUILD_EXIT=0` |

**The blocker is cleared.** Two agents independently established that no
`LLHLSLShader` was ever constructed, so `D3DCompile` could never run and nothing
could render regardless of what else was finished.

Solved with one guarded typedef in `llviewershadermgr.h:33-44`:
`LLViewerShaderProgram` is `LLHLSLShader` under `DX_RENDER` and `LLGLSLShader`
otherwise, substituted at all 336 program uses. The file text stays
backend-agnostic, so no `#if` appears at any of those sites and the GL
preprocessed output is byte-identical to HEAD.

`#define LLGLSLShader LLHLSLShader` was rejected on evidence: `llenvironment.h`
and the test stub legitimately still mean the GL class in the same translation
unit, so the define would silently retype the GL override.

Depth state: no `OMSetDepthStencilState` was ever issued, because `LLGLDepthTest`
was pure unguarded GL. `DXStateCache` already had a matching factory, so no
`dxrender` change was needed. Added `glDepthFuncToDX` and
`applyDXDepthStencilState` to the existing anonymous namespace and hooked them
into `LLGLDepthTest`'s ctor and dtor. GL preprocessed output byte-identical.

**Two things the agents flagged that are still wrong, deliberately not forced:**
- `syncMatrices()` has no DX branch, so the depth convention does not line up:
  `dxrender` clears depth to `0.0` (reversed-Z) while the target still uploads
  GL-convention depth. The agent used the donor's reversed-Z comparison funcs and
  flagged the mismatch rather than flipping them to contradict `dxrender`.
- `GL_DEPTH_TEST` has no branch in `applyDXState`, so `LLGLDisable
  depth(GL_DEPTH_TEST)` at `newview/pipeline.cpp:4603` silently no-ops, and
  `LLGLDepthTest::sWriteEnabled`/`sDepthFunc` are private with no accessor.

---

## R9 — llrender DX definitions (`f899525`)

**Task:** T-none; make `llrender.lib` link.

| check | result |
|---|---|
| `dxcheck.sh` all six DX sources | OK |
| `llrender.cpp` + `llgl.cpp`, `-DDX_RENDER=1` | 0 errors |
| link closure, llrender + dxrender | all 12 task symbols resolve |
| deletions in `git diff` | none outside pre-existing `autobuild.xml` noise |

Subagent additionally compared preprocessed output with `DX_RENDER` off:
`llgl.cpp` byte-identical to HEAD, `llrender.cpp` differs by exactly the
4-line `isRecording()` body. I re-ran `dxcheck.sh` myself and confirmed the
diff contains no removed lines.

**Found, deliberately not fixed:** `applyDXRasterizerState()` links but has no
caller — the donor calls it from `LLRender::cullFace` and `setPolygonOffset`,
neither of which exists in this tree. So `DXState::sCullFace` and
`mCurrPolygonOffsetFactor/Units` are dead. Escalated as **T3**.

---

## R8 — llrender DX compile errors (`5b36358`, `1f0fee8`)

| check | result |
|---|---|
| MSVC errors | 157 → 1 |
| remaining | `llimagegl.h(49) C4005 MEGA_BYTES_TO_BYTES macro redefinition` |

`llimagedx.h` already guarded the macro; `llimagegl.h` did not, so a TU seeing
both headers redefinition-fails under `/WX`. Guarded.

Five groups cleared:
1. `LLImageDX` never declared — `lldxtexture.h` relied on include order.
   Included `llimagedx.h` (no cycle). Exposed a second fault the forward
   declaration masked: `LLDXTexture::getGLTexture()` collided with virtual
   `LLTexture::getGLTexture()`, a covariant-return violation → renamed
   `getDXTexture()`.
2. `LLTexUnit::bind` had no DX overloads → declared under `#ifdef DX_RENDER`.
3. `llfontdx` needed `EFontHinting`, different `loadFace` arity,
   `LLFontBitmapCache::getImageDX`, `LLFontRegistry` DX fonts. Two ported
   additively, the rest adapted to this tree's font code.
4. `llhlslshader.cpp` and `llglslshader.h` both defined `LLShaderFeatures`,
   `LLShaderUniforms` and three program globals, because the donor deleted
   `llglslshader.*`. Extracted the two backend-independent types into new
   `llshaderfeatures.h`; made the three globals mutually exclusive via
   `#if defined(DX_RENDER)` / `#if !defined(DX_RENDER)`.
5. Wrong pointer type → added siblings `attachShaderFeaturesDX` and **non-pure**
   `updateShaderUniformsDX`. Retyping the existing pure virtual would have made
   `LLShaderMgr` abstract and broken three GL overrides.

---

## R7 — llrender DX surface (`131e01d`)

12 DX files copied (7077 lines) plus declarations in 4 shared headers.

Critical discovery by the subagent: **`DX_RENDER` was never a preprocessor
macro**, only a CMake cache variable, so every `#ifdef DX_RENDER` in the donor
evaluated false. Added the donor's own generator expression to `00-Common.cmake`.
Without it the entire port compiles to nothing.

`DXState` became `typedef LLGLState DXState;` rather than a rename, because the
donor renamed `LLGLState`→`DXState` and `gGLActive`→`gDXActive`; reproducing
those renames would break every GL reference in the tree.

---

## R6 — Wine and D3D11 feasibility (no code)

Verified on this machine, not assumed:

- `wine 10.0` + `xvfb` ran a Windows portable build to
  `STATE_LOGIN_SHOW`, with `llwindowwin32.cpp:1953 Created OpenGL 4.5
  compatibility context` and a live frame loop.
- A purpose-built D3D11 probe (`mingw`-compiled, run under Wine):
  `D3D11CreateDevice hr=0x00000000`, `feature_level=0xB100`,
  `adapter="NVIDIA GeForce GTX 470"`, and `D3DCompile hr=0` producing 584 bytes
  of bytecode.

The second result matters most: the DX backend compiles all 250+ shaders at
runtime through `D3DCompile`, so that call is load-bearing.

Caveat: wined3d advertises feature level 11.1 regardless of hardware; the real
card is a GTX 470, which is 11_0 max. This proves the API works under Wine, not
that the DX renderer looks good on this GPU.

---

## R5 — Windows CI unblocked (`8eb0746`, `35acf58`, `31f4101`)

| check | result |
|---|---|
| `DX11 Compile Check` | SUCCESS, 4m26s, 22 objects, `dxrender.lib` |
| Windows portable build | was failing since 2026-09-11 |

Two independent faults. `gyan.dev/ffmpeg-release-essentials.zip` is a **rolling**
URL, so its pinned hash went stale — every build died at "Stage verified FFmpeg
converter". Re-pinning that URL would only postpone it and the versioned paths
404, so switched to a dated `BtbN/FFmpeg-Builds` tag whose assets are fixed once
published, pinned to the asset digest from the GitHub API. Its archive names the
licence `LICENSE.txt`, not `LICENSE`. Hash check kept strict.

Added a `dx_render` input to `build-windows.yml` (default false) — it previously
never passed `DX_RENDER`, so it could not verify the backend at all. Added
`build-windows-dxrender.yml`, a ~4 minute gate that builds only `dxrender` and
`llrender`, skipping FMOD, ffmpeg, voice and packaging.

Four compile bugs found this way: `INTERFACE` include dirs apply only to a
target's consumers, not its own sources; `_WIN32_WINNT=0x0601` pinned tree-wide
made `dcomp.h` compile to nothing (112 of 128 errors); `lldir.h` had zero
includes and leaned on precompiled headers; and `std::regex::multiline` is C++17
and absent from MSVC 14.44 even under `/std:c++20`.

---

## R4 — Linux baseline established

| check | result |
|---|---|
| configure | `CONFIGURE_EXIT=0` |
| build | `BUILD_EXIT=0`, 974 targets |

`gcc 15` here vs `gcc 11/12` in CI produced a false `-Warray-bounds` failure
inside libstdc++. Proven pre-existing by reproducing it on a stashed tree;
waived with `-Wno-error=array-bounds`.

---

## R3 — XML/UI repairs (`6f4c8f2`)

`floater_post_process.xml` declared all 16 controls with a bogus `wmi` prefix
while the code requested bare names, and `wmi` exists in no C++ file. `getChild`
fabricates a dummy on a miss, so the whole Post Processing floater was dead UI
plus log spam. XUI language files are overlays that update by name and never
append, so every Polish label was silently discarded too.

`menu_gltf.xml` was referenced with a literal `TODO : create this` and did not
exist. `lltasiaguard.cpp` POSTed an empty `ExternalIP`. Two real null-derefs
found that the first audit missed.

Audit corrections worth remembering: of 105 "missing widgets", **91 were false
positives** — commented-out code, or names built at runtime like
`"lod_source_" + lod_name[lod]`. And the 87 files that lost elements versus
upstream phoenix-firestorm were **deliberate feature strips**, consistent in code
and XML; restoring them would only resurrect UI for code that no longer exists.

---

## R2 — Kirstens viewer analysis

Established the donor is a Linden Lab r4015 fork, not a Tasia project. Its GL
backend was never actually deleted: `llgl.cpp`, `llgl.h`, `llglstates.h`,
`llglheaders.h` all still exist and `llrender/CMakeLists.txt` still builds them.
Commit `c36dde9 "retire gl context"` removed exactly one thing —
`LLWindowWin32::createSharedContext`. Only 4 GL sources were dropped from the
build list. This is why the additive strategy is viable.

Protocol findings that corrected earlier notes: avatar `.llm` files contain **no**
`bind_shape_matrix`; bind pose is implicit identity and offsets come from
skeleton skin offsets summed along the parent chain
(`llavatarjointmesh.cpp:62,105`). `bind_shape_matrix` exists only for
object/`.lslm`/DAE meshes (`llmodel.cpp:1662`), confirming the row-vector
convention there. The LL client requests `UpdateAvatarAppearance`
(`llappearancemgr.cpp:3884`) gated on `region_protocols & 1`
(`llviewerregion.cpp:3189`), with **no** client-side bake fallback — and i-grid
serves neither that nor `ViewerAsset`, which is why the `igridext` plugin exists.

---

## Environment notes

`/tmp` is tmpfs and is wiped on every suspend/reboot. It destroyed the first XML
audit reports and the autobuild venv overnight. Everything durable now lives in
`/home/marty/opensim/tasia-dx11-notes/`.

`autobuild.xml` was already dirty before this work began and strips SDL2's
copyright and licence metadata. Not ours, not committed, worth reverting
separately.

Wine, mingw-w64, libfontconfig1-dev, libfreetype6-dev and mesa-common-dev were
installed on this machine.
