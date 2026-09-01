# Build Notes

## 2026-05-25: Profile remote badge image loading

- Branch/worktree: `feature/tasia-tag-badge-fix` at `/mnt/a/2026/tasia-tag-badge-fix`.
- Changed profile remote badge loading away from `LLViewerTextureManager::getFetchedTextureFromUrl(...)`.
- New flow:
  - coroutine downloads raw image bytes with redirects enabled
  - timeout/transfer timeout: 60 seconds
  - retries: 0
  - range request/max accepted body: 10 MiB
  - content type is cleaned from response headers, with URL extension fallback
  - image is decoded with `LLViewerTextureManager::getFetchedTextureFromMemory(...)`
  - `LLThumbnailCtrl::setTexture(...)` displays the decoded in-memory texture
- Fallback remains `Profile_Badge_Team` if fetch/decode fails.
- Profile badge hover text no longer includes the badge image URL.
- Fallback team badge now shows immediately while the remote badge image is fetched/decoded.
- Added `TasiaProfile` log lines for fetch/decode/display status.
- `badge_name` may now be a built-in profile badge texture name (`Profile_Badge_Linden`, `Profile_Badge_Premium_Lifetime`, etc.); remote `badge_icon` URL still wins when provided.

### 2026-05-26 release
- Linux release: `https://github.com/martysl/tasia-viewer/releases/tag/v8.0.1-20`
- Linux direct asset: `https://github.com/martysl/tasia-viewer/releases/download/v8.0.1-20/Phoenix-FirestormOSTasia-Releasex64_LEGACY-8-0-1-78266.tar.xz`
- Windows release: `https://github.com/martysl/tasia-viewer/releases/tag/v8.0.1-47-windows`
- Windows direct asset: `https://github.com/martysl/tasia-viewer/releases/download/v8.0.1-47-windows/Tasia-Viewer-Windows-FMOD.zip`
- Older visible releases/runs cleaned, keeping the latest Linux + Windows runs visible.
- Discord announcement posted successfully with Mom-provided webhook: HTTP 204.

### Focused checks
- Initial CI failed because `LLViewerTextureList::getImageFromMemory(...)` is private.
- Follow-up fix switches to public `LLViewerTextureManager::getFetchedTextureFromMemory(...)`.

## 2026-05-24: Remote Tasia user config

- New files:
  - `indra/newview/lltasia_user_config.h`
  - `indra/newview/lltasia_user_config.cpp`
- Settings:
  - `TasiaRemoteUserConfigEnabled` default true
  - `TasiaRemoteUserConfigURL` default `https://i.let-us.cyou/hg/config.json`
  - `TasiaRemoteUserConfigTimeout` default 5 seconds
  - `TasiaRemoteUserConfigMaxBytes` default 262144
- Startup behavior:
  - Fetch is non-blocking via coroutine.
  - Fetch is one-shot per viewer run.
  - Failures only log a warning and do not block login.
- Rendering behavior:
  - Nametag adds custom title line from `custom_title`, `title`, `rank_title`, or fallback `badge_name`.
  - Nametag title color accepts `tag_color` or `color` as `#RRGGBB`/`#RRGGBBAA`.
  - Profile account area shows badge/profile text and tooltip; profile uses existing local badge icon slot.
- Welcome behavior:
  - Default changed from `welcome.txt` to `welcome.php`.
  - Existing persisted old URL is mapped to `welcome.php` at runtime.
  - First usable line is used; no randomization.
  - Fetch resets per visible login/teleport progress screen.

### Focused checks
- `git diff --check`: passed.
- `indra/newview/app_settings/settings.xml` XML parse: passed.
- Strict conflict-marker scan in `indra/newview` cpp/h/xml: passed.

## Build Environment
- **Platform**: Ubuntu 22.04 (GitHub Actions runner)
- **Compiler**: GCC 11+ 
- **Build system**: CMake + autobuild + Ninja
- **FMOD**: Required (2.03.07), private package in martysl/tasia-private-deps

## Known Build Issues

### GCC -Wmaybe-uninitialized (FIXED)
- **File**: `indra/newview/llvisualeffect.h:130` - `LLTweenableValueLerp<LLVector4>::m_StartTime`
- **Fix**: Added member initializers in constructor (m_StartTime(0.0), m_Duration(0.0), m_StartValue(), m_EndValue())
- **Status**: ✅ Fixed in commit e466846140

## Configuration
- Channel: `Tasia-Releasex64`
- `--fmodstudio` flag required
- No KDU

## Workflow
- Manual trigger only via GitHub Actions (`workflow_dispatch`)
- Single platform per run
- Build order: Linux → Windows → macOS

## 2026-05-18: GIPHY/welcome/loading feature branch

### Generated GIPHY key support
- Branch: `feature/tasia-giphy-welcome-loading-linux`
- Build-time environment variable: `TASIA_GIPHY_API_KEY`
- Generator: `scripts/generate_tasia_giphy_key.py`
- Generated ignored files:
  - `indra/newview/lltasia_giphy_key.generated.h`
  - `indra/newview/lltasia_giphy_key.generated.cpp`
- Runtime accessor:
  - `indra/newview/lltasia_giphy_key.h`
  - `indra/newview/lltasia_giphy_key.cpp`

### Runtime key priority
1. `TasiaGiphyAPIKey` user setting
2. Generated obfuscated build-time fallback
3. Empty/not configured

### Verification run
- `env -u TASIA_GIPHY_API_KEY python3 scripts/generate_tasia_giphy_key.py --header indra/newview/lltasia_giphy_key.generated.h --source indra/newview/lltasia_giphy_key.generated.cpp`
- Fake-key generator test under `/tmp/opencode`: passed, plaintext fake key not present in generated source.
- `python3 -m py_compile scripts/generate_tasia_giphy_key.py`: passed.
- XML parse of `indra/newview/app_settings/settings.xml`: passed.
- `git diff --check`: passed.

### Full build status
- No full Linux build has been run yet after these feature edits.

### Welcome text client
- Files added:
  - `indra/newview/lltasia_welcome_client.h`
  - `indra/newview/lltasia_welcome_client.cpp`
- `LLProgressView` now requests one random usable line during startup loading.
- Fetch settings:
  - `TasiaWelcomeURL`
  - `TasiaWelcomeURLTimeout`
  - `TasiaWelcomeMaxBytes`
- Failure, timeout, empty body, or no valid line leaves the existing server/grid progress message unchanged.
- Late responses after startup completion are ignored.

### GIPHY API client
- Files added:
  - `indra/newview/llgiphyclient.h`
  - `indra/newview/llgiphyclient.cpp`
- Supports:
  - search endpoint
  - trending endpoint
  - rating from `TasiaGiphyRating`
  - result parsing for page URL, preview GIF, fixed-width GIF, downsized GIF, and original GIF
  - `GIPHY is not configured.` fallback when no runtime or generated key is available
- The client does not log request URLs, because those contain the API key query parameter.

### GIPHY picker floater
- Files added:
  - `indra/newview/llfloatergiphypicker.h`
  - `indra/newview/llfloatergiphypicker.cpp`
  - `indra/newview/skins/default/xui/en/floater_giphy_picker.xml`
- Registered floater name: `giphy_picker`
- Current behavior:
  - loads trending GIFs on open
  - supports search button and trending button
  - lists result title and normal GIPHY page URL
  - `Use Selected` invokes an optional callback or copies the selected URL to clipboard
  - includes `Powered by GIPHY` text
- Nearby chat wiring:
  - Added `GIF` button to `indra/newview/skins/default/xui/en/floater_fs_nearby_chat.xml`
  - Button opens `LLFloaterGiphyPicker`
  - Selected GIF sends the normal GIPHY page URL through `FSFloaterNearbyChat::sendChatFromViewer(...)`
  - Current whisper/say/shout selection is respected

### GIPHY chat previews
- Implemented in `indra/newview/fschathistory.cpp`.
- Controlled by `TasiaAnimatedGifChatPreview`.
- Current preview is a safe local card:
  - detects supported `giphy.com`, `media.giphy.com`, and `*.giphy.com` URL forms
  - shows `GIF preview`, canonical GIPHY page URL, `Powered by GIPHY`, and an `Open GIF` button
  - does not alter the sent chat payload
  - skips plain-text chat history and loaded chat logs to avoid mass widget creation
- Full inline animated thumbnail rendering is not implemented yet.
- Direct image URL previews are also implemented in `FSChatHistory`.
- Controlled by `TasiaImageChatPreview`.
- Supported direct image extensions:
  - `.png`
  - `.jpg`
  - `.jpeg`
  - `.gif`
  - `.webp`
  - `.bmp`
  - `.apng`
- Active nearby chat and Firestorm IM both use `FSChatHistory`; legacy `LLChatHistory` is currently disabled by `#if 0`.
- YouTube embeds are implemented in `FSChatHistory`.
- Controlled by `TasiaYouTubeChatPreview` (default true).
- Supported URL forms include `youtube.com/watch?v=...`, `youtube.com/embed/...`, `youtube.com/shorts/...`, `youtube.com/live/...`, and `youtu.be/...`.
- Chat sends and stores the normal URL; Tasia Viewer renders the local embed card.

### Loading panel branding / YouTube
- `panel_progress.xml` now says `Tasia Viewer uses` and includes `Powered by GIPHY`.
- Optional YouTube loading media is implemented in `LLProgressView`.
- Controlled by:
  - `TasiaLoadingYouTubeEnabled` (default false)
  - `TasiaLoadingYouTubeURL` (default empty)
- Accepted URL forms include `youtube.com/watch?v=...`, `youtube.com/embed/...`, `youtube.com/shorts/...`, `youtube.com/live/...`, and `youtu.be/...`.
- The URL is converted to a muted autoplay embed URL locally; the configured URL is not logged.
- Loading YouTube is not the default behavior; chat/IM YouTube embeds are the default behavior.

### GitHub Actions readiness
- Linux workflow `.github/workflows/build-tasia.yml` passes `secrets.TASIA_GIPHY_API_KEY` into configure.
- Build should still configure if `TASIA_GIPHY_API_KEY` is missing; generated fallback key will be empty and runtime GIPHY picker will report `GIPHY is not configured.` unless `TasiaGiphyAPIKey` is set by the user.
- Existing `FMOD_DEPS_TOKEN` secret remains required for FMOD dependency download.
- Generated GIPHY files are ignored and must not be committed.

## 2026-05-17: TasiaFeed upload fixes

### Fix 1: Wrong HTTP method (postAndSuspend with string → implicit LLSD)
- **Symptom**: Viewer sends wrapped XML instead of raw JSON
- **Root cause**: `postAndSuspend` has no `std::string` overload. String body was implicitly converted to LLSD and sent as `application/llsd+xml` XML via `requestPostWithLLSD` → server received `<llsd><string>{json}</string></llsd>`
- **Fix**: Changed to `postJsonAndSuspend` which sends raw JSON via `HttpCoroJSONHandler`
- **Commits**: linux: 75d0d60276, windows-build-test: 43f6446404

### Fix 2: Wrong response handler (expected HTTP_RESULTS_RAW)
- **Symptom**: Even with proper JSON sending, "No response from server" still shows
- **Root cause**: `TasiaFeedUploadResponse` checked for `HTTP_RESULTS_RAW` ("raw") key, which is **only** set by `HttpCoroRawHandler`. But `postJsonAndSuspend` uses `HttpCoroJSONHandler` which parses JSON and returns keys directly in `aData` (no "raw" key). So every response hit the error path.
- **Fix**: Rewrote `TasiaFeedUploadResponse` to read the already-parsed JSON keys (`success`, `post_url`, `message`) directly from `aData` instead of going through `HTTP_RESULTS_RAW`.
- **Commits**: linux: 85493dbca7, windows-build-test: 9cb3609280

### Files changed
- `indra/newview/tasiafeedconnect.cpp`

## 2026-05-16: BugSplat removal / crash reporter reconfiguration

### Summary
Replaced the BugSplat crash reporting pipeline with a generic HTTP crash endpoint. All BugSplat-specific validation, fallback URL construction, and DB-name logic have been removed or relaxed.

### Changes

1. **`indra/linux_crash_logger/linux_crash_logger.cpp`** — Removed BugSplat fallback URL construction. The endpoint string is now used directly as-is (`std::string url = strEndpoint;`).

2. **`indra/newview/llappviewerlinux.cpp`** — Renamed `gBugsplatDB` → `gCrashReportURL`. Changed all `"BUGSPLAT"` log tags to `"CRASHREPORT"`. The validation block now reads `"CrashReportURL"` from `build_data.json`, accepts any non-empty URL (no domain/prefix check), and rejects empty values instead.

3. **`indra/newview/viewer_manifest.py`** — Changed `build_data.json` key from `"BugSplat DB"` to `"CrashReportURL"`. Validation now requires an `http://` or `https://` URL prefix instead of the old `tasia_` / specific-domain check.

4. **`scripts/configure_firestorm.sh`** — Hardcoded the crash endpoint to `https://apps.easierit.org/igrid/bugs/api/v1/report` instead of constructing a dynamic `tasia_<channel>` BugSplat DB name.

### Rationale
Eliminates the dependency on BugSplat's proprietary SDK/API and lets the crash reporter send dumps to any standard HTTP endpoint. The viewer-side code no longer hardcodes assumptions about which SaaS service processes the crashes.
