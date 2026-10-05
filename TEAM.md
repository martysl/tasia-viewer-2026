# TEAM.md — how the D3D11 port is worked on

Worktree `/home/marty/opensim/tasia-viewer-2026-work`, branch `tasia-dx11-merge`.
Donor, read-only: `/home/marty/Kirstensviewer`. Notes and tooling:
`/home/marty/opensim/tasia-dx11-notes/`. **Never write to `/tmp`** — it is tmpfs and
is wiped on suspend and reboot; a crash already destroyed one set of audit reports.

## Roles

| role | count | owns | may edit |
|---|---|---|---|
| **Writer** | 4 | one subsystem each | only the files in its own brief |
| **Checker** | 1 | cross-cutting correctness | nothing — reviews, never writes |
| **Me** | 1 | control | verifies every claim independently, owns commits and CI |

A writer never touches a file outside its brief. If it needs one, it stops and
reports. Collisions are the main cause of wasted work here, so they are prevented by
ownership rather than detected afterwards.

## The gates

Run all of these; they are fast and they are the feedback loop.

| gate | covers | state |
|---|---|---|
| `dxcheck.sh` | `dxrender` + `llrender` DX sources | 8/8 |
| `dxchecknewview.sh` | the ten `dx*` files in newview | 10/10 |
| `dxcheckshared.sh` | fifteen shared newview files under `DX_RENDER=1` | 15/15 |
| `glregress*.sh` | proves the OpenGL build is unchanged | see below |
| `glcompile.sh` | native `-fsyntax-only` of touched files, DX off | 21/21 |
| CI `build-windows-dxrender.yml` | MSVC, builds **and links** `firestorm-bin` | authority |
| `reconf_build.sh` | native Linux configure + full build | must stay `BUILD_EXIT=0` |

MSVC is the only authority. mingw does not see MSVC-only warnings — `C4005` and
`C2220` both reached a green-looking tree and were caught only by CI.

## Rules that exist because breaking them cost real time

1. **Additive only.** Never remove, rename or reorder OpenGL code. With
   `DX_RENDER` off the OpenGL build must be byte-identical, not merely similar.
2. **One alias, not `#define`.** `LLViewerShaderProgram` is `LLHLSLShader` under
   DX and `LLGLSLShader` otherwise. `#define LLGLSLShader LLHLSLShader` was
   rejected: `llenvironment.h` and the test stub still mean the GL class in the same
   translation unit.
3. **Never retype a virtual.** `updateShaderUniforms` overrides a pure virtual in
   `llrender/llshadermgr.h`. Retyping it would make the class abstract and break
   three overrides. The DX path is the sibling `updateShaderUniformsDX`.
4. **Compile is not done.** Three states in this project compiled and rendered
   nothing. A gate that builds a static library cannot detect an unresolved
   external — link the executable.
5. **Never claim it renders without rendering it.** Screenshot or it did not happen.

## Handover format

Every writer ends with: what changed and where; which gates it ran and the numbers;
`git diff --numstat` per file; and anything it could not do without touching a file
it did not own, with the donor file:line. A writer's own verification is evidence,
not proof — I re-run the gates and read the diff.

## Current position

All three mingw gates green, native Linux `BUILD_EXIT=0`. The open question is
whether MSVC links `firestorm-bin`, which is the first moment the DX path could
actually be executed. After that: run it under Wine (verified working on this
machine) and screenshot.