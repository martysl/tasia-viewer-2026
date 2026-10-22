# OpenSim compatibility audit and implementation checklist

Updated: 2026-10-22. Foundation: Kirsten DX11 (viewer version `26.9.4015`).
References:

- `/home/marty/opensim/tasia-kirsten` — active viewer source.
- `/home/marty/opensim/tasia-viewer-2026` — old viewer comparison reference.
- `/home/marty/opensim/telep-npc` — checked OpenSim server source (not proof of
  the deployed grid's exact revision/configuration).

### Renderer reality: DX11 only, with OpenGL-era names still present

Both trees render through **DirectX 11** (`DX_RENDER=ON` in
`indra/cmake/Variables.cmake`). Do not be misled by identifiers such as
`LLGLTexture`, `gDX.getTexUnit()`, `glReadPixels` or `FTT_*`: most are
compatibility shims retained in the DX11 code base. Therefore:

- Port the reference's **OpenSim behavior/protocol** (what to send and receive),
  but verify the **DX11 mechanism** rather than assuming either renderer.
- Before porting any readback/upload path, re-check whether the current
  reference already implements it in DX11. If it still uses a raw OpenGL call,
  the DX11 equivalent in Kirsten is `dxrender/resources/DXReadback`
  (staging texture, RowPitch, top-left vs bottom-left orientation, format).
- A successful compile is not proof: assert the actual pixel/J2C bytes or the
  actual D3D calls.

**Only Agni/Aditi are Second Life; all other grids select OpenSim compatibility.**
Capabilities still determine optional services; OpenSim mode must not fabricate
Linden server baking, require QUIC, or grant grid privileges.

This is a tracked audit, **not a declaration that the viewer supports every
OpenSim feature**. Reference code is evidence of required paths, not a drop-in
replacement. Each implementation group needs focused tests, a real Windows
build and Mom's interactive verification. Missing items stay explicit.

## Current deployment

**Age38 legacy fixes candidate installed at `E:\TasiaViewerLatest`.** Build exit
0, no compiler errors, independent matching EXE/PDB age38, installed/archive
hashes verified. Backup age37: `E:\TasiaViewerBackups\before-legacy-fix-20261018-225709`.
See BUILD_NOTES.md for artifact hashes and log. Runtime result pending; test
tattoo/alpha/universal layer wear, detach/reattach, BOM face rendering, and
attachment BOM texture updates.

Legacy fixes:
- `sendLegacyAppearance()` sends ALL `getNumBakes()` slots (6-11), not just 0-5.
  Extended tattoo/universal layers are now sent via AgentSetAppearance TE message.
- `LLOverrideBakedTextureUpdate` uses `getNumBakes()` instead of hardcoded 11.
- `removeFromAvatar()` clears textures to `IMG_DEFAULT_AVATAR` when detaching
  tattoos/alpha/universal layers (fixes stale textures / black faces).
- `LLViewerTexLayerSetBuffer::processUploadCompletion()` and
  `LLVOAvatarSelf::setNewBakedTexture()` call `refreshBakeTexture()` on all
  attachments after a baked texture is assigned. Attachments using baked texture
  magic IDs (IMG_USE_BAKED_*) rebind to the newly uploaded bake instead of
  staying black (fixes Genus Eyebrow Tintable A and similar BOM attachments).

### Previous age37 candidate (historical)

**Age35 COF link-target fetch candidate installed at `E:\TasiaViewerLatest`.**
Build exit 0, zero compiler errors, independent matching EXE/PDB age35,
installed/archive hashes verified. Age34 backup:
`E:\TasiaViewerBackups\before-cof-link-targets-20261018-214840`.
See BUILD_NOTES.md for hashes/log. Runtime hair/eyes/cloud outcome pending;
no inventory link deletion or fabricated wearables. Next user login: 45–60s.
Age35 runtime failed: both requested COF targets remained unresolved and
hair/eyes remained zero. Exact-target matching against real simulator worn-item
assets is now building; it must refuse nonmatching or unavailable assets rather
than fabricate parts. No newer deployment yet.

### Previous age34 candidate (historical)

**Age34 required-body-part repair candidate is installed at
`E:\TasiaViewerLatest`.** Build exit 0, no compiler errors, independent EXE/PDB
age34 match, installed/archive hashes verified. Backup age33 at
`E:\TasiaViewerBackups\before-required-bodyparts-20261018-212726`.
See BUILD_NOTES.md for full artifact evidence. Runtime outcome pending;
simulator-asset repair is not a readiness bypass or default-part creation.
Age34 live result: hair/eyes still absent; repair blocked by broken COF links,
so no simulator repair was requested. A bounded real link-target inventory fetch
is now building. Broken links are preserved, not removed or replaced.

### Previous age33 diagnostic (historical)

**Age33 cloud-readiness diagnostic is installed at `E:\TasiaViewerLatest`.**
Live diagnostics confirmed hair=0 and eyes=0 while shape/skin and baked GPU
textures were ready; this required-body-part gate causes cloud in this session.
Repair using real simulator worn-item assets only for parts absent from complete
COF is building; not yet deployed. No default-part creation or gate bypass.
Build exit 0, no compiler errors, matching EXE/PDB symbols and installed hashes.
This adds bounded numeric diagnostics only; clouding is NOT fixed. Mom's next
login should remain active at least 45s; inspect only sanitized CloudReadiness
fields. See BUILD_NOTES.md for hashes, build log, backup and archive.

### Previous age32 candidate (historical)

**Age32 BOM recovery is now installed at `E:\TasiaViewerLatest`.** Windows
**Mom reports cloud on age32; runtime compatibility has NOT passed.**
build exit code 0; zero compiler errors; changed avatar/bake units compiled;
independent EXE/PDB symbols match. Installed and archived hashes checked:
- EXE: `1A23171AFA5AEDB1127437AC82B2B3185D613E64DE836F7E0B02569F61ED3697`
- PDB: `59A1CC37EA6D3A10C60E89F30CBB9FC0551BDB8E9575B7246F22029929BED4D2`

Backup: `E:\TasiaViewerBackups\before-bom-recovery-20261018-202132`.
Build: `F:\tasia-kirsten\build-bom-recovery-20261018-201344.log`.
Ten focused offline suites passed. No viewer launched/stopped. Mom's actual
tattoo/BOM visual result is pending. This supersedes all deployment entries below.

### Pre-recovery state (historical)

**Pre-recovery inspection confirmed age31.** `E:\TasiaViewerLatest`
holds EXE `6B3E6C76417AEC1D738D8D4DC1F2EA38F5890656F774BD0F2A32DD7FF8532F48`,
PDB `FAA849072B9FDED914956E0F826ED3472F39FD3A8B21ECE036B16B11719C6CED`,
matching symbols age31. Full build log `F:\tasia-kirsten\build-opensim-compat-20261018-172834.log`,
Known link warnings remain. Backup of previous age28 pair at
`E:\TasiaViewerBackups\before-body-part-validation-20261018-174535`.

Age31 includes the invisible-alpha branch; the previous claim it was reverted
in this binary was incorrect. Archive of age29 at
`F:\tasia-kirsten\verified-builds\invisible-alpha-20261018-164319`.
Rollback safety backup at
`E:\TasiaViewerBackups\before-rollback-alpha-20261018-165429`.

Recovery source is uncommitted. It restores damaged picking declarations and
behavior, removes the uninitialized region limit, and uses advertised BOM to
enable eleven-slot uploads/completions rather than six. The universal creation
guard was pre-existing. See BUILD_NOTES.md for recovery build/deployment evidence.
Viewer NOT launched by agent. Tattoos/BOM visual correctness awaits Mom's test;
offline tests are not a substitute for the Windows build or runtime verification.

### Historical local-bake candidate

**Newest candidate, supersedes historical entries below:** local-bake + initial
worn-item fallback installed to `E:\TasiaViewerLatest`, EXE
`82CD04FBB7E7838E64738ABB836B5580902FB6443CE6E90324431347A1EE465F`, PDB
`818A554337C9891D20BB63B50FEC657335AE12F5C0A876977E73DCA26D75CAD2`,
matching symbols age28. Full build log 147 lines, 0 errors; known link warnings
remain. Nine offline suites PASS. Six LocalBake stage diagnostics verified in
binary. Archive `F:\tasia-kirsten\verified-builds\localbake-wearables-20261018-161859`;
backup `E:\TasiaViewerBackups\before-localbake-wearables-20261018-161859`.
Viewer not launched by agent. **Mom's visual/runtime result pending.**
Known initial-stage limits: six original bake slots; duplicate-asset worn-item
snapshots rejected (shared mutable wearable cache), no universal negotiation.

### Historical diagnostic deployment

**Latest installed build (supersedes the historical pair below):** texture HTTP
diagnostic, EXE `4E413C69A1794F4CDB5A7452709A08BDD29C91090F01C0D3B7DE4F17A0F24D2C`,
PDB `0C5443C62B915CFA0595C8F689F5FD553E27F4A7649AEDFB13600E1909B28381`,
matching symbols age 25. Backup of prior pair:
`E:\TasiaViewerBackups\before-texfetch-diag-20261018-132842`.
Mom reports avatar still cloud. Foundation edits transferred for a CONTROL
BUILD ONLY; no later installation. See latest entry in `BUILD_NOTES.md`.

Local-bake readback/upload/appearance-sender stage now also compiles on Windows:
full-log 0 errors, matching EXE/PDB age27, archived (NOT installed) at
`F:\tasia-kirsten\verified-builds\localbake-upload-20261018-160043`.
Eight offline suites pass, including compiled/sanitized pixel, callback,
upload-policy and appearance-sender fixtures. Real GPU/J2C/grid behavior remains
unverified. Initial worn-item acquisition via AgentWearablesRequest/Update is
still absent and is the next dependency to implement/verify.

**Evidence limits:** no HTTP failure diagnostics in a session does not establish
that every required texture was requested or fetched correctly. Neither missing
server baking nor an empty decode reason establishes asset absence/404.
Marker/identifier counts are not a functional-completeness test. The upload
state machine, J2C readback, wearable acquisition and appearance send still need
implementation/verification; the current foundation is not a finished bake port.

### Historical capability-diagnostic deployment

`E:\TasiaViewerLatest` has the **capability-diagnostic build**,
EXE SHA-256 `5E38B39FA090B206EB1DB90FA5DEDFA9BA0B47A5B44CB461E129D3A6EC30C2BB`,
PDB `26DC5B4AD4FDBB962213BA2667B80EB74B854219E962BA8B360D4FAD6F7B966D`
(EXE/PDB symbols match, GUID+age 23). Installed 2026-10-18 03:10 local.
Build log written 03:10:16, i.e. **after** the 02:58:51 source transfer, so it
is genuinely the diagnostic build; 0 errors, all changed units recompiled, and
the deploy script verified the probe string is present inside the EXE.
The known-good Group 1 pair (`3D5FCF24...` / `91F8AF5D...`) is backed up at
`E:\TasiaViewerBackups\before-opensim-capsdiag-20261018-031029`.
Source backup `F:\tasia-kirsten\source-backup-opensim-20261018-025851`.
Viewer log lives at `%APPDATA%\Tasia Viewer DX\logs\TasiaViewer-LOG.log`.

Previous deployment: **Group 1 region/parcel/terrain build**,
`3D5FCF24E9A5E8CA2B9FF42042DA8705B0681EE284017640ADF6DE46A67CE3CD` /
`91F8AF5DF3C3AA7FCFC3873651D1B6611801AF0FD3E392F0C315CADD2BD0D0D6`
(GUID+age 21), Mom-verified working on i-Grid.

### Diagnostic result (2026-10-18 03:11, Mom's live i-Grid session)

```
OpenSimAssets : ViewerAsset capability advertised: YES
```

Same session: 0× `Unsupported fetch type`, 0× `not available via HTTP`,
`Legacy self appearance accepted; visual parameter count: 253`, and 3× new
`Failed to deserialize material LLSD` (GLTF, see coverage matrix).
Open question for Mom: dressed or still cloud?

Earlier deployed build: `8E31026B...D753CB` (legacy-appearance group). Its
post-install logs no longer showed the old appearance-service URL warnings but
still rejected overlay chunks 4..15, which is what Group 1 targets.

### Mom's Group 1 test result (2026-10-18, ~02:43 local)

Mom reports **"i am cloud but it work"** — her avatar renders on i-Grid and the
viewer is usable. This is the first confirmation that avatar rendering works on
OpenSim after the region/parcel/terrain fix.

Stated conservatively:
- Accepting the valid parcel overlay chunks (previously rejecting
  `sequence_id 4..15`) plus per-region widths resolved the main
  presence/region problem behind "no avi, no body, no movement".
- **"Cloud"** is the expected legacy appearance: OpenSim delivers texture data
  through the legacy host route rather than a Linden appearance service, so some
  textures stay unloaded. Cloud is a known, acceptable limitation, **not** a
  failure and not a reason to redo Group 1. Group 2 would improve it.
- Walking is expected to follow from the same region/interpolation correction but
  must still be confirmed in plain words.
- Teleport works and world load is instant; only the avatar's textures lag.

**Instrumented confirmation** (read-only inspection of
`%APPDATA%\Tasia Viewer DX\logs\TasiaViewer-LOG.log`, 445 lines, deployed EXE
`3D5FCF24...7CE3CD`; no credentials, UUIDs or URLs exported):

| Signal | Before | Now |
|---|---|---|
| `Ignoring parcel overlay with sequence_id 4..15` | 12 per session | **0** |
| `AgentAppearanceServiceURL not set` / `unable to determine URL` | 40 + 40 | **0** |
| forced disconnect | – | **0** |

First-event traces confirm the path end to end:
- `First OpenSim region allocation: 512m, 512 terrain grid points per edge` —
  the live region really is 512×512, which is why 16 overlay chunks were sent.
- `Accepted extended parcel overlay chunk within 16384-byte ownership allocation`.
- `Decoding extended OpenSim terrain patch IDs` — OpenSim LandExtended type `M`.
- `AgentMovementComplete accepted for current identity; self avatar valid: 0`
  at login, then `... self avatar valid: 1` after teleport — the self avatar
  object exists and survives a region change.
- `Self avatar object initialized from ObjectUpdate`.
- `Legacy self appearance accepted; visual parameter count: 253; baked texture
  source: simulator host` — full visual parameters applied via the legacy route.
- A later teleport allocated a **256m** region, so mixed-size regions are handled.

Result: avatar renders, movement works, teleport works, world loads instantly,
and Agni/Aditi keep their untouched SL paths. The only remaining known gap is
the **cloud avatar** (missing baked textures), because OpenSim performs no
server baking and the local bake/worn-items pipeline (Group 2) is not yet
ported. Remaining log noise is benign: repeated unknown simulator statistics,
date/timezone, audio engine and translation lookups.

## Coverage matrix

| Area | Evidence / required behavior | Status / next verification |
|---|---|---|
| Grid identity and login | `LLGridManager::isInSecondLife/isInOpenSim`; custom HTTP(S) endpoints; Agni/Aditi protected. | Implemented; classifier, bounded grid-catalog import and metadata URL policy have source-fixture coverage. OpenSim search resolves simulator feature → login response → validated per-grid `search` metadata → `SearchURLOpenSim`; the Destinations guide resolves simulator feature → requested login response. Neither inherits SL endpoints. Legacy unbound `UserLoginInfo` is now reused only on Second Life system grids; custom grids require a per-grid credential or manual entry. Hypergrid URI variants, live search/destinations and live login remain unverified. |
| Accounts | SQLite/DPAPI per-grid accounts, encrypted recoverable passwords and same-user import. | Current login UI saves after successful auth; grid switching now preserves only a manually typed username and clears password/remember-password state; selected accounts continue to load from the new grid. Old viewer's MD5 authenticator placeholder remains protocol-only and is not treated as a real password. Windows UI/DPAPI behavior remains unverified. Non-Windows explicitly disables the raw-password SQLite/import extension and relies on the existing `LLSecAPIHandler` credential path; recoverable-password/import parity is not implemented there. |
| UDP message definitions | Both templates have 483 messages; no message IDs differ; 26 critical avatar/movement/connection definitions match exactly. `MapBlockReply.Size` is restored; `RezScript.NewScriptInfo` remains current-only. | Comparator passes 12,000 synthetic packets. Stepwise protocol details/status in `OPENSIM_PROTOCOL_AUDIT.md`. Do not replace templates wholesale. Live delivery still separate. |
| Login region dimensions | i-Grid `LLLoginResponse.cs:543-544` supplies `region_size_x/y`. | Group 1 now carries both through initial region construction; SL ignores OpenSim size extensions. |
| Simulator enable, teleport, crossing | i-Grid HTTP event queue supplies `RegionSizeX/Y`; UDP definitions need not contain these LLSD extensions. | Group 1 reads optional sizes before construction. Missing sizes retain existing regions instead of shrinking them. |
| Region model and interpolation | Per-region width drives containment, terrain lookup and interpolation. Global width changes would corrupt mixed-region behavior. | Group 1 constructs bounded square regions with independent widths; updates quantized object position ranges, start-location/camera bounds and terrain composition edges. No live movement success claimed. |
| Parcel overlay | i-Grid emits fixed 1024-byte chunks, one byte per 4m cell; original guard allowed only 4 chunks. | Group 1 derives count from real allocation; keeps negative/overflow/length bounds checks. Tests cover 256..4096, all chunks, out-of-order delivery and invalid IDs. |
| Parcel properties/bitmaps | Agent, selected and collision bitmaps use region-dependent sizes/strides. Reference's fixed maximum buffers alone do not isolate maps. | Group 1 grows bounded buffers on demand, retains independent map strides, bounds point queries and rejects short/oversized bitmaps rather than reading beyond data. Requires Windows/UI verification. |
| Extended terrain | i-Grid `LLClientView::SendLayerData` selects LandExtended for regions above 256m. Old Kirsten discards type `M` and only decodes 10-bit patch IDs. | Group 1 accepts `M` only on OpenSim, uses 32-bit IDs (16 per axis), retains SL 10-bit format. Actual header/bit-packer tests pass. GPU terrain rendering not tested. |
| Neighbor seams / region shapes | Checked OpenSim server caps region dimensions at `MaximumRegionSize=4096`; renderer/composition remains square/power-of-two based. | **Partial:** square power-of-two sizes 256..4096; other shapes explicitly warned/fallback, not advertised as supported. Region-level object reachability uses geometric bounds over all loaded regions. Mixed-width terrain maps shared cardinal patch spans, corner-only links and edge-span endpoint diagonals; all surface links are tracked for teardown while legacy `mNeighbors[8]` exposes one representative per direction. Stub harness covers all orientations, endpoint diagonals, multiple neighbors, all corner directions, duplicate/conflicting links and teardown. Arbitrary multi-region topology, Windows build, rendered seams and live crossing remain unverified. Pathfinding still chooses one region per direction; late size renegotiation remains unsupported. A 4096m region still allocates substantially more terrain memory than a standard region and needs runtime stress testing. |
| Presence / movement | `CompleteAgentMovement` → server root transition → `AgentMovementComplete` → self ObjectUpdate → AgentUpdate. Texture completion is not itself a normal-walking gate. | Source sequence exists. A session/agent mismatch now returns before the movement-complete startup flags are set; source-contract test covers that ordering. Group 1 adds bounded first-event traces for accepted movement-complete, self-object creation and accepted legacy appearance. Actual root/movement state still needs verification. |
| Legacy appearance receive | i-Grid omits COF version; old Kirsten dropped self `-1 >= -1` and assumed Linden bake URLs. | First deployed group accepts legacy self messages, preserves server COF sequencing for modern messages and routes legacy baked textures to host exclusively. Source-level tests pass. |
| Cloud avatar / baked texture fetch | `FTT_HOST_BAKE` is a real fetch type; the live grid advertises `ViewerAsset`. | **Source fix implemented:** host-bake fetches are supported and retain the HTTP `ViewerAsset/?texture_id=<uuid>` route; OpenSim/source checks pass. The earlier `mCanUseHTTP = false` workaround is explicitly rejected. Visual/live fetch verification remains pending. |
| i-Grid asset capability | Diagnostic probe in `llviewerregion.cpp:3556`, deployed and run on Mom's live grid. | **`ViewerAsset capability advertised: YES`** — i-Grid serves assets over HTTP `ViewerAsset`; the large UDP port is not needed for textures. ViewerAsset URL construction remains in `lltexturefetch.cpp`. Candidate HTTP 404s still need endpoint-by-endpoint investigation; they do not implicate QUIC by themselves. |
| GLTF material deserialization (new) | 3× `Failed to deserialize material LLSD` at `llgltfmateriallist.cpp:596`. | Not investigated. Prime suspect for attachments rendering wrong even when the body is textured. Also seen: `LLDate` `date 1970-01-08 failed to parse`. |
| Worn items and local baking | Wearable asset transport, initial-wearable snapshot, bake readback/upload policy and appearance sender have focused production-source harnesses. | **Source implemented, runtime incomplete:** harnesses pass for bounded OpenSim legacy wearable transfer, initial-wearable guards, AIS outfit-fetch failure/mismatched-ID fail-closed handling, DX11 readback buffer/pitch/format logic, upload policy and appearance packet shape. Previously observed solid-black `BAKED_HEAD` is contained by rejecting all-black/nontransparent uploads, but its root cause and visual result are unresolved. GPU/J2C and live clothing/appearance results are not verified. Do not label the complete appearance pipeline runtime-proven. |
| DX11 bake upload / BoM | Current source has `DXReadback` bake readback and upload/lifetime/retry policy with focused harnesses. | **Source/harness coverage exists:** pixel channel order, padded pitch, alpha/five-channel packing, bounds, retries and appearance packet policy are tested. The black-head upload guard preserves the existing bake and retries; it does not fix the reported black face. Real GPU/J2C upload and BoM end-to-end behavior remain unverified. |
| Attachments / HUDs | Server independently rezes attachments; viewer needs a valid parent avatar, inventory links, updates and working rendering. | Existing basic paths; no proof of complete visibility. Audit COF sync, missing parent/order, attach/detach and HUD state with appearance pipeline. |
| Non-texture assets | `llviewerassetstorage.cpp` refreshes ViewerAsset and has a bounded legacy UDP fallback for supported bodypart/clothing asset types. | **Partial:** source harness covers OpenSim gating, missing-route/404/410 handoff, callback/session guards and transfer queue ownership. Other asset types (notably mesh/texture), stale capability behavior, and live asset delivery remain separate and unverified. |
| Inventory | Old viewer uses legacy `FetchInventory`/`FetchInventoryDescendents` plus `FetchInventoryReply`/`InventoryDescendents` when AIS/capabilities are unavailable; checked OpenSim registers these UDP handlers. | **Read path restored in current source:** OpenSim-only item/folder fallback sends and response callbacks are back; reply handlers validate agent ID and are covered by compiled production-method fixtures. HTTP/AIS stays preferred, SL behavior unchanged. Inventory delete/purge/move legacy fallback remains a separate gap; no live inventory session tested. |
| Event queue / HTTP | i-Grid long-polls 58s and returns 502 when empty; reference sets a 60s OpenSim transfer timeout. Both recognize 502 generally. | Audit effective timeout/retry/cancellation. Reference's extra OpenSim 502 `else if` is unreachable; do not copy it as a fix. Actual disconnect trigger remains unproven. |
| Limits / SimulatorFeatures | Checked server advertises per-region prim/physics scale limits and height/heightmap values in `OpenSimExtras`; legacy `lfsimfeaturehandler.cpp` consumes scale and other optional keys. Current source stores finite OpenSim `MinSimHeight` and `say-range` per region. | **Partial:** world-map Z permits the advertised negative floor; local speaker proximity follows the region say range. SL preserves 0..4096 and 20m defaults. Prim-scale edit paths still use fixed limits. Chat fade keeps the current user `NearChatRadius` preference. Whisper/shout presentation and other extras (FPS, export, URLs/currency, max height/heightmap) need consumer-by-consumer audit. Focused tests pass; Windows/live verification pending. |
| Mesh / materials / uploads | Reference has extensive OpenSim paths in meshrepository, including optional services/limits. | Needs method-by-method audit of download, rigging, upload/cost/physics paths; no blanket portability claim. DX11 rendering remains intact. |
| Map / minimap / URLs | Reference and checked server use optional `MapBlockReply.Size` pairs for larger regions; reference SLURL place-details flow carries region origin so parcel coordinates remain local in varregions. | **Source fixes:** bounded OpenSim map sizes flow through tracking/drawing; place details use SLURL origin or loaded region; landmark-to-pick uses loaded target origin; profile/classified text, group-land/holdings lists, and selected-parcel details use loaded-region coordinates. Unknown/unloaded positions retain prior fallback. Standard 256m behavior is unchanged. Focused checks pass; live map/place UI tests pending. Hypergrid routing remains separate. |
| World-map altitude | OpenSim advertises `OpenSimExtras.MinSimHeight` (`-100` in checked server constants); legacy stores it per region and permits negative world-map Z. | **Source fix:** current viewer stores a finite OpenSim-only per-region minimum and applies it when displaying coordinates and tracking typed world-map Z; SL remains clamped at 0..4096. Focused region policy tests pass. Windows build and live negative-Z teleport remain unverified. |
| Profiles / role branding | Current processor reads OpenSim profiles over AgentProfile or legacy UDP; old reference sends `AvatarPropertiesUpdate` when editing the self profile without AgentProfile. | **Source change + offline contract checked:** self-only legacy edits merge one changed field into the latest self `AvatarPropertiesReply` and send the full legacy packet, preserving profile URL, both image IDs, both descriptions and publish state. The cached reply is tagged at receipt with the current session; `AvatarPropertiesReply` has no SessionID, so a delayed prior-session reply is not independently excluded. `FLAboutText` is Variable 1 and the shared serializer clamps it to 255 bytes including NUL (254 payload bytes), despite the UI allowing 65,000 characters; no multibyte-boundary or live save/reload test was run. `hide_age` remains cap-only because the legacy packet has no such field. Live OpenSim save/UI behavior and Windows build remain unverified. Keep real UUID/grid identity and privileges distinct from cosmetic role labels. en/pl and DX11 icon loading need verification. |
| Groups / chat / money / presence lists | Mom previously confirmed chat, money and online status; group/session extensions and grid currency vary. | Preserve working paths; audit optional caps, legacy fallback, limits and currency presentation separately. No all-features pass implied. |
| Media / external integrations | CEF is packaged; VLC preference, GIPHY/Klipy picker and snapshot destinations remain unfinished/partially wired. | Independent backlog, not evidence of UDP presence correctness. Do not label placeholder upload buttons functional. |
| QUIC | Old reference origin matches requested GitHub repo; local `main` is `93a5c1db`, observed remote `main` is newer (`fee60c9e`). Server sends `[00 00 00 00]` control-stream keepalives every 30s by default and consumes them as markers. | **Viewer parser fix:** current viewer now consumes zero-length frames; compiled production parser fixture covers isolated/fragmented keepalive + packet. Old parser stayed in PAYLOAD after an isolated zero-length frame. Current authenticates `quicready` on QUIC and has 5s timeout/recovery; old lacks both. Current falls back over UDP after established failure but does not reconnect QUIC on that circuit. Legacy retry preference is registered (default 1), consumed by the shared endpoint helper, and clamped to 1–10 synchronous start attempts; it does not retry asynchronous handshakes. A compiled harness exercises the production retry method; endpoint-order contracts cover login, EnableSimulator, TeleportFinish and CrossedRegion. Removed the inert fallback checkbox and replaced it with localized information that LLUDP recovery is automatic when possible; this preserves current recovery behavior rather than reviving old strict no-fallback semantics. Full QUIC suite and settings/UI XML checks pass; Windows build, live 300s idle, crossing/reconnect, and final Farm review are pending. |
| Diagnostics | `llerror.h` hardcodes LL_DISABLE_INFOS=1. Old logs cannot prove missing INFO-level events. | Group 1 adds only first-event diagnostics via LL_INFOS_ONCE; no global logging-header change or full-logging rebuild. Never print credentials/cap URLs/session IDs. |

## Group 1 source verification

Commands (Linux, offline):

```sh
python3 indra/newview/tests/tasia_region_compat_test.py
CXX='c++ -fsanitize=address,undefined -fno-omit-frame-pointer' python3 indra/newview/tests/tasia_region_compat_test.py
python3 indra/newview/tests/tasia_opensim_compat_test.py
python3 indra/newview/tests/tasia_protocol_compare_test.py /home/marty/opensim/tasia-viewer-2026-work
git diff --check
```

The region test compiles actual extracted production functions, actual bit-packer
and patch header with minimal dependency stubs. It is not a full viewer build,
packet capture replay, GPU test or successful OpenSim login. The sanitizer pass
applies only to that isolated native harness, not the Windows viewer.

Before deployment: review all changed files, transfer/hash-check changed headers
and source, run one normal Windows build, require actual compilation and changed
EXE, verify/archive matching EXE/PDB, back up the installed pair and deploy only
with approval. Mom alone performs interactive login tests.

## Next implementation groups

1. Verify Group 1 in the actual Windows build before adding unrelated fixes.
2. Restore legacy assets/inventory negotiation and the coordinated worn-item /
   local bake / upload / appearance-send pipeline, including DX11-specific tests.
3. Complete region neighbors/shapes/maps/limits and HTTP lifecycle behavior.
4. Complete remaining profiles/groups/mesh/media/branding/optional transport,
   using this matrix to avoid losing requested work.

Order may change based on concrete test evidence; a warning disappearing or a
successful build never substitutes for working avatar, movement and teleport.
