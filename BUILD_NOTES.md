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