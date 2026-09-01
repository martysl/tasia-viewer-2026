# Project Status

## Current Phase

Linux viewer builds and runs with FMOD.

Current branch: `feature/tasia-giphy-welcome-loading-linux`.

Current feature work: GIPHY/welcome/loading improvements are being implemented on Linux first, then ported to Windows after Linux success.

Next work:
1. **Grid lock**: remove/block Second Life and add I-Grid Beta.
2. **Version/build identity**: internal 8.0.1, display 8.0.1.<GitHub run number>, show short commit SHA.
3. **Snapshot system**: replace Flickr/Primfeed with real TasiaFeed upload.
4. **TasiaFeed backend**: PHP + DB + WebDAV storage under apps.easierit.org/igrid/feed/.
5. **Bug reporting**: adapt BugSplat/crash reporter into real TasiaBugReport/TasiaCrash.
6. **Bug backend**: PHP + DB/admin under apps.easierit.org/igrid/bugs/.
7. **Branding cleanup** while keeping legal credits intact.
8. **Manual Linux build** and test.

## 2026-05-18: GIPHY/welcome/loading branch status

### What is done
- Added build-time generated obfuscated GIPHY API key fallback support.
- Added generated-file ignores for:
  - `indra/newview/lltasia_giphy_key.generated.h`
  - `indra/newview/lltasia_giphy_key.generated.cpp`
- Added runtime key accessor files:
  - `indra/newview/lltasia_giphy_key.h`
  - `indra/newview/lltasia_giphy_key.cpp`
- Added generator script:
  - `scripts/generate_tasia_giphy_key.py`
- Wired Linux workflow configure step to pass `TASIA_GIPHY_API_KEY` from GitHub Secrets without printing it.
- Added Tasia settings for welcome text, GIPHY, animated chat preview, and optional loading YouTube embed.
- Added async welcome text client:
  - `indra/newview/lltasia_welcome_client.h`
  - `indra/newview/lltasia_welcome_client.cpp`
- Hooked `LLProgressView` to request one random usable line during startup loading and ignore late responses after startup completes.
- Added standalone GIPHY API client:
  - `indra/newview/llgiphyclient.h`
  - `indra/newview/llgiphyclient.cpp`
- `LLGiphyClient` supports search/trending, safe key lookup, rating setting, JSON parsing, and `GIPHY is not configured.` fallback.
- Added registered GIPHY picker floater shell:
  - `indra/newview/llfloatergiphypicker.h`
  - `indra/newview/llfloatergiphypicker.cpp`
  - `indra/newview/skins/default/xui/en/floater_giphy_picker.xml`
- Registered floater as `giphy_picker` in `indra/newview/llviewerfloaterreg.cpp`.
- Added nearby chat `GIF` button in `indra/newview/skins/default/xui/en/floater_fs_nearby_chat.xml`.
- Wired nearby chat button to `LLFloaterGiphyPicker`; selected GIF sends the normal GIPHY page URL through existing nearby chat send path.
- Added local GIPHY URL preview cards in `indra/newview/fschathistory.cpp`, gated by `TasiaAnimatedGifChatPreview`.
- Added direct image URL previews in `indra/newview/fschathistory.cpp`, gated by `TasiaImageChatPreview`.
- Added inline YouTube embeds in `indra/newview/fschathistory.cpp`, gated by `TasiaYouTubeChatPreview` and enabled by default.
- Confirmed active nearby chat and Firestorm IM use `FSChatHistory`; legacy `LLChatHistory` path is currently disabled by `#if 0`.
- Added loading panel branding text and `Powered by GIPHY` credit in `indra/newview/skins/default/xui/en/panel_progress.xml`.
- Added optional loading YouTube embed behavior in `LLProgressView`, gated by `TasiaLoadingYouTubeEnabled` and `TasiaLoadingYouTubeURL`; loading media is disabled by default.

### What is broken
- No known breakage from the current edits.
- Full animated thumbnail rendering inside chat is still not implemented; current chat preview is a safe local GIPHY card with an external open button.
- No full Linux build has been run for this feature branch yet.

### What was last attempted
- Generated empty local fallback key files with no secret present.
- Tested the generator with a fake key under `/tmp/opencode` and verified plaintext is not written.
- Parsed `indra/newview/app_settings/settings.xml` successfully.
- Ran `git diff --check` successfully.
- Added welcome client/progress hook and reran focused XML/whitespace checks successfully.
- Added `LLGiphyClient` and reran focused XML/whitespace/script checks successfully.
- Added `LLFloaterGiphyPicker`, registered it, added XUI, and reran focused XML/whitespace/script checks successfully.
- Wired nearby chat GIPHY button/selection send path and reran focused XML/whitespace/script checks successfully.
- Added GIPHY chat preview cards, loading branding/GIPHY credit, optional YouTube loading media, and reran focused XML/whitespace/script checks successfully.
- Enabled loading YouTube by default, added image URL chat previews, verified active IM coverage through `FSChatHistory`, and reran focused XML/whitespace/script checks successfully.
- Corrected YouTube behavior: chat/IM YouTube embeds are now enabled by default via `TasiaYouTubeChatPreview`; loading YouTube is optional/off by default. Focused checks passed again.

### Exact last failing step
- None in this session.

### What must not be changed
- Do not hardcode, print, commit, or document the real GIPHY API key.
- Do not commit generated GIPHY key files.
- Do not disturb the known-good Windows build path while Linux feature work is incomplete.
- Keep Linux feature work on `feature/tasia-giphy-welcome-loading-linux` until Linux build succeeds.

### Next exact action
- For GitHub Linux build: ensure existing `FMOD_DEPS_TOKEN` secret is present. Add `TASIA_GIPHY_API_KEY` repo secret if the packaged GIPHY picker should work without users manually setting `TasiaGiphyAPIKey`.

## Build Status

| Platform | Build | Runtime | TasiaFeed upload |
|----------|-------|---------|------------------|
| Linux    | ✅ v0.1.0 | ✅ (basic login) | ❌ HTTP bug (postJsonAndSuspend fix applied, new build needed) |
| Windows  | ⏳ blocked on Linux first | - | - |
| macOS    | ⏳ blocked on Windows first | - | - |

## Completed Milestones

<<<<<<< HEAD
- v0.1.0 release (tagged) - Linux builds and runs with FMOD
- FMOD integration working (private deps pipeline)
- KDU removed
- Second Life grids removed from bundled defaults
- release branch created
- GitHub Actions: manual-only workflow, single-platform
- GCC -Wmaybe-uninitialized fixed (real fix, not suppression)
- **Grid Lock added**: I-Grid Beta included, SL grids blocked programmatically at all entry points, startup purge of existing SL grids
- **Version bumped to 8.0.1**: Display version 8.0.1.<GitHub run number>, commit SHA visible in About window
=======
## What is broken
- Full viewer build has not been run for this badge image loading patch yet.

## What was last attempted
- Focused code review/static sanity pass on profile badge image loading patch.
- CI builds were triggered after commit `4ed7ac0c21`.
- Removed profile badge URL from hover tooltip; hover now uses configured tooltip/profile text only.

## Exact last failing step
- Linux run `26417570550` and Windows run `26417571261` failed compiling `indra/newview/llpanelprofile.cpp` because `LLViewerTextureList::getImageFromMemory(...)` is private.

## What must not be changed
- Existing GIPHY/welcome/chat preview behavior.
- Existing remote config JSON schema and fallback badge behavior.

## Next exact action
- Commit and push fix to use public `LLViewerTextureManager::getFetchedTextureFromMemory(...)`, then rerun focused Linux/Windows CI builds.

## 2026-05-26 Badge release

## What is done
- Latest badge fallback builds passed:
  - Linux run `26422421054` / commit `fbec75918c`
  - Windows run `26422421559` / commit `bef3638071`
- Published prereleases:
  - Linux `v8.0.1-20`: `https://github.com/martysl/tasia-viewer/releases/tag/v8.0.1-20`
  - Windows `v8.0.1-47-windows`: `https://github.com/martysl/tasia-viewer/releases/tag/v8.0.1-47-windows`
- Deleted older visible releases `v8.0.1-17` and `v8.0.1-44-windows`.
- Deleted older current-viewer Actions runs, keeping latest Linux/Windows release runs visible.

## What is broken
- Nothing release-blocking currently known.

## What was last attempted
- Posted formatted Discord message with separator lines using Mom-provided webhook.

## Exact last failing step
- Earlier local webhook from `~/.config/opencode/token.txt` returned HTTP 403; Mom-provided webhook succeeded with HTTP 204 when sent with a User-Agent header.

## What must not be changed
- Published release tags/assets unless replacing with a new build.

## Next exact action
- Runtime-test the new release builds, especially profile badge fallback/loading behavior.

## 2026-05-26 Built-in profile badge names

## What is done
- Added support for using built-in profile badge texture names directly in `badge_name`.
- Supported names: `Profile_Badge_Beta`, `Profile_Badge_Beta_Lifetime`, `Profile_Badge_Lifetime`, `Profile_Badge_Linden`, `Profile_Badge_Pplus_Lifetime`, `Profile_Badge_Premium_Lifetime`, `Profile_Badge_Team`.
- Remote `badge_icon` URL still takes priority; built-in `badge_name` is used as fallback while remote image loads or if no URL is provided.

## What is broken
- Not built yet after the built-in badge-name change.

## What was last attempted
- Static patch and whitespace checks.

## Exact last failing step
- None yet for this change.

## What must not be changed
- Existing URL-based remote badge behavior and fallback team badge behavior.

## Next exact action
- Commit/push Linux and Windows badge branches and run CI.
>>>>>>> 4b7e6eedcca (Support built-in profile badge names)
