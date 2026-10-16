# Windows build notes

Practical notes for building this viewer on a VS2022 machine. Everything here was
found the hard way; each item is a step the build fails on without it.

## Prerequisites

| Requirement | Version | Notes |
|---|---|---|
| Visual Studio | **2022, toolset v143** (14.44) | The triplets must match, see below |
| CMake | 3.20+ | Bundled with the VS Build Tools |
| vcpkg | **tag `2025.06.13`** or older | Newer vcpkg requires toolset v145 |
| OpenMP | any | Required by the top-level CMakeLists |

```powershell
git clone --depth 1 --branch 2025.06.13 https://github.com/microsoft/vcpkg F:\vcpkg
& F:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

Then run the build with `F:\vcpkg` on `PATH`, otherwise `prepare` aborts with
"vcpkg.exe not found in PATH".

## Toolset pinning (v143, not v145)

Every triplet in `triplets/` sets `VCPKG_PLATFORM_TOOLSET`. These are set to
**v143**, which is Visual Studio 2022. They were originally v145, which is
Visual Studio 2026 and does not exist on a VS2022 install.

With a mismatched value vcpkg does not report a useful error - it fails every
port with the same unhelpful line:

```
error: in triplet x64-windows-static-extreme: Unable to find a valid Visual Studio instance
  with toolset version v145
```

If you see that, check the triplet toolset against your installed compiler
before anything else.

## Packages

84 packages: 22 tarballs from the S3 bucket, 62 vcpkg ports.

The tarballs unpack into the repository root, producing `include/`, `lib/release/`,
`bin/`, `fonts/`, `etc/`. `LIBS_PREBUILT_DIR` points at the repository root
(`indra/cmake/Variables.cmake`).

`installed-packages.json` is the ledger that records what is already unpacked.
Deleting an entry forces a reinstall of that package.

### OpenSSL

OpenSSL ships as a prebuilt tarball rather than a vcpkg port. The manifest
originally requested vcpkg version `1.1.1n#1`, a port that only exists in a
2020-era registry; current vcpkg has dropped every 1.1.1 release, so that
override could never resolve and the whole prepare stage aborted.

The tarball carries 1.1.1w, matching the 1.1.1 series that `libcurl` in
`installS24.xml` is linked against. If `libcurl` is ever updated to OpenSSL 3.x,
this tarball has to be rebuilt to match.

## PowerShell 5.1 scripts

`indra/newview/Copy-ViewerManifest.ps1` is invoked by the build through
`powershell.exe`, which is 5.1 on Windows. Two constraints follow:

- It must be stored as **UTF-8 with BOM and CRLF**. Without the BOM, Windows
  PowerShell reads the file as ANSI, and the non-ASCII progress markers corrupt
  the parse - the build dies with a misleading "Missing closing `}`".
- `Join-Path` takes only two positional arguments (`-Path`, `-ChildPath`). A
  third string aborts the step with "A positional parameter cannot be found".

The other `*.ps1` files in the repository (`Invoke-Configure.ps1`,
`AutoBuild-S24.ps1`, and friends) are interactive scripts, not build steps, and
still have the BOM/LF problem. They need the same treatment before anyone runs
them on Windows.

## Build

```powershell
cmake -S F:\tasia-kirsten\indra -B F:\tasia-kirsten\build -G "Visual Studio 17 2022" -A x64
cmake --build F:\tasia-kirsten\build --config Release --target Kirstens-S24 -- /m:8 /v:minimal
```

## Runtime verification (Acer, 2026-10-16)

The Release package was tested from `F:\tasia-kirsten\indra\S24-Packaged`
in the active Windows desktop session, not over SSH (an SSH session has no
usable DX11 swapchain). The tested package contained 11,144 files and includes
`llplugin\media_plugin_cef.dll`.

After 120 seconds the viewer remained running on the NVIDIA GeForce RTX 4050
Laptop GPU, with the startup log confirming both the rendering adapter and
audio-engine initialization. The initial failure was a stack overflow in
MSVC's `std::regex` engine while assembling HLSL. `DXShader::resolveIncludes()`
and texture-index guard detection now use line parsers instead.

`Kirstens-S24` explicitly depends on `media_plugin_cef`; without that
dependency, building only the viewer executable omitted the CEF plugin and the
login splash could not load HTML content.

The linker still emits the upstream `/FORCE:MULTIPLE` warnings for duplicate
libxml2/llprimitive UTF conversion symbols. The verified package starts despite
them, but they remain technical debt and should be removed in a dedicated
linkage cleanup rather than masked further.

## Custom grid manager: data layer

`LLGridManager::addCustomGrid()` accepts an explicit HTTP(S) login endpoint and
an optional label. It preserves the endpoint path and port, uses first/last-name
credentials, and does not invent a Second Life CGI path or splash page. Current
URI parsing supports DNS/IPv4 hosts, not IPv6 literals. Credentials in URIs,
queries, fragments, invalid ports and duplicate authorities are rejected.

`removeCustomGrid()` protects built-in grids and the currently selected grid.
Both operations persist only custom entries to the existing user `grids.xml`,
using a temporary file and replacement rename. On write failure the in-memory
change is rolled back. Agni/Aditi initialization and selection remain unchanged.

Regression cases were added to `tests/llviewernetwork_test.cpp`; this branch has
no CMake unit-test targets, so these cases have not been executed. Windows
Release compilation of the data layer passed (`build-grid-data.log`,
`EXITCODE=0` on Acer).

`LLFloaterGridManager` adds a login-screen "Manage Grids..." button, a saved
grid list, label/URI inputs and add/remove actions. List changes refresh the
login selector without changing the current grid or touching credential fields.
The selector becomes visible automatically when custom entries exist. English
and Polish floater XML parses successfully; Windows UI compilation is pending.

Next: verify the management window in the viewer, adding/removing a grid,
persistence across restart, and actual
OpenSim authentication. A grid list alone is not full OpenSim compatibility.

## Custom grid manager: data layer + UI

`LLGridManager::addCustomGrid()` accepts an explicit HTTP(S) login endpoint and
an optional label. It preserves the endpoint path and port, uses first/last-name
credentials, and does not invent a Second Life CGI path or splash page. Current
URI parsing supports DNS/IPv4 hosts, not IPv6 literals. Credentials in URIs,
queries, fragments, invalid ports and duplicate authorities are rejected.

`removeCustomGrid()` protects built-in grids and the currently selected grid.
Both operations persist only custom entries to the existing user `grids.xml`,
using a temporary file and replacement rename. On write failure the in-memory
change is rolled back. Agni/Aditi initialization and selection remain unchanged.

`LLFloaterGridManager` adds a login-screen "Manage Grids..." button, a saved
grid list, label/URI inputs and add/remove actions. List changes refresh the
login selector without changing the current grid or touching credential fields.
The selector becomes visible automatically when custom entries exist. English
and Polish floater XML parses successfully.

Regression cases were added to `tests/llviewernetwork_test.cpp`; this branch has
no CMake unit-test targets, so these cases have not been executed. Windows
compilation of data layer + UI passed (`EXITCODE=0` on Acer).

Verified end-to-end on Acer 2026-10-16: package contains 11,146 files including
`llplugin\media_plugin_cef.dll` and both `floater_grid_manager.xml` skins;
viewer remained running after 120 s in the active GPU session with RTX 4050
detected and audio initialized; startup log contains no fatal/critical errors.
Runtime interaction with the Manage Grids window and OpenSim login itself has
not yet been exercised — that requires an actual OpenSim grid to test against.
A grid list alone is not full OpenSim compatibility.

## QUIC (MsQuic) - OpenSim grid transport

Added `indra/llquic/` module (api, registration, configuration, stream,
connection, global) wrapping Microsoft MsQuic v2.4.10 for QUIC client
connections to OpenSim simulators.

- `indra/cmake/MsQuic.cmake`: downloads Microsoft-signed msquic.dll
  prebuilt from GitHub releases (v2.4.10). The signed DLL avoids Windows
  Defender false positives on unsigned DLLs, and ships with its own
  quictls/OpenSSL isolated from the viewer's libcurl OpenSSL.
- `indra/CMakeLists.txt`: `add_subdirectory(llquic)` guarded by WIN32.
- `indra/newview/CMakeLists.txt`: links `Kirstens-S24` to `llquic` and
  copies `msquic.dll` next to the viewer EXE after build (imported
  targets don't support POST_BUILD).

Build verified: `EXITCODE=0` on Acer VS2022/v143, `msquic.dll` (2.6 MB)
copied to `build/newview/Release/`. OpenSim login over QUIC itself
requires an actual OpenSim grid to test against; the library compiles,
links, and the viewer starts with it present.
