# OpenSim protocol differences and misses

- Updated: 2026-10-21
- Active viewer: `/home/marty/opensim/tasia-kirsten`
- Old viewer reference: `/home/marty/opensim/tasia-viewer-2026`
- Checked OpenSim server source: `/home/marty/opensim/telep-npc`

This is the step-by-step protocol delta ledger, not a blanket compatibility
claim. “Implemented” means source/tests only unless a row explicitly says live
verified. Server-source evidence does not prove the deployed grid runs that exact
revision.

## Rules for this audit

- Keep OpenSim behavior gated separately; do not regress Second Life.
- Compare actual request/reply fields and handlers, not just message names.
- Port only behavior supported by the target server; preserve safer current
  guards unless evidence shows they reject valid traffic.
- Keep HTTP asset/mesh 404s separate from simulator UDP/QUIC diagnosis.
- Keep login passwords in the current per-grid SQLite/DPAPI store. Never revive
  legacy credential encoding; the MD5 authenticator is only the login protocol.
- Make one focused change per step, add a focused regression test, then update
  this ledger and `OPENSIM_COMPATIBILITY.md`.

## Step status

| Step | Area | Current result | State / next action |
|---:|---|---|---|
| 0 | Message-template baseline | Both templates have 483 messages; no message IDs are current-only/reference-only. 26 critical avatar/movement/connection definitions match. 12,000 synthetic packets pass. `MapBlockReply.Size` has now been restored; `RezScript.NewScriptInfo` remains current-only. | **Done, rechecked 2026-10-20.** Do not replace templates wholesale. Review the remaining field delta individually. |
| 1 | Legacy inventory fetch/read | Old no-cap `FetchInventory` / `FetchInventoryDescendents` requests and `FetchInventoryReply` / `InventoryDescendents` responses were missing. Current source now sends these only on OpenSim when HTTP/AIS is unavailable and checks reply AgentID. | **Restored in source; 2/2 focused fixture tests pass.** Full build/live request still needed. |
| 2 | World-map region sizes | Optional `MapBlockReply.Size` and `SizeX/SizeY` are wired through template, prehash, OpenSim-only reply parsing, bounded `LLSimInfo` extents, tracking bounds and map drawing. | **Source fix, focused contracts and Windows Release build pass.** SL and old replies retain 256m defaults. Live SL/OpenSim map checks remain pending; Hypergrid/SLURL audit remains separate. |
| 3 | Inventory delete/purge | Both protocols define remove/purge messages, and checked server handlers delete items/folders and purge asynchronously. Old viewer sends UDP `RemoveInventoryItem`, `RemoveInventoryFolder`, and `PurgeInventoryDescendents`; active inventory helpers are AIS-only and warn otherwise. The old client mutates local state and fires callbacks optimistically after sending. The checked server's purge handler queues `PurgeFolderAsync` via `FireAndForget`, logs service failure, and sends no application-level completion reply. | **Verified miss; deliberately not ported.** A fresh empty listing is a possible observation, not yet a safe completion signal: the async purge has no ack, legacy replies have no request ID, and current reply handling does not reconcile absent cached children. Do not delete the parent on a timeout, malformed/stale reply, or unresolved callback failure. See confirmation contract below; server/version semantics and reply correlation remain blockers. |
| 4 | Legacy profile UDP | Current source now decodes supported Groups/Picks/Notes replies and requests Picks/Notes on OpenSim when `AgentProfile` is absent. Unsupported standalone groups request and unsafe full-record profile update were not copied. | **Source port complete; focused tests pass; live tab test pending.** See `plan.md`. |
| 5 | Wearable assets and appearance | Current source keeps `ViewerAsset` HTTP and adds a guarded OpenSim transfer fallback only for bodypart/clothing when cap is absent or HTTP returns 404/410. Wearable acquisition, local bake/upload and appearance send have source fixtures. AIS outfit fetch now passes completion IDs and refuses a failed/mismatched `FetchCategoryLinks` result before applying a cached/partial outfit. | **Source/fixture coverage; live avatar/bake incomplete.** No stale or partial outfit mutation on the audited AIS error path. Do not broaden to mesh/arbitrary assets or HTTP permission failures without evidence. |
| 6 | Region dimensions, terrain and parcel data | Login/event-queue region dimensions, extended terrain IDs and larger parcel overlays have OpenSim source handling and focused tests. Checked server `MaximumRegionSize` is 4096m; viewer allocation/map/parcel bounds now match it. | **Mostly implemented; source/live evidence varies.** Square power-of-two regions 256..4096 only; non-square/non-power-of-two rendering and some unequal-size seams remain unsupported/partial. Known terrain/composition arrays at 4096m total about 320MiB before patch/renderer resources; full allocation/crossing stress still needs Windows runtime evidence. |

### Neighbor discovery versus terrain connection (2026-10-21)

`LLWorld::addRegion()`'s eight width-offset handle probes are **surface-neighbor
connection probes**, not the mechanism that discovers/enables simulators. The
OpenSim server's EventQueue `EnableSimulator` includes `RegionSizeX/Y`; the
viewer dispatches that event as LLSD and its handler reads those fields before
calling `addRegion()`. TeleportFinish and CrossedRegion likewise carry the
dimensions in their LLSD event bodies, and their handlers read them. The
message-template declarations are legacy UDP schemas and do not list these
extra LLSD body keys; `app_settings/message.xml` selects the LLSD flavor for
these message names.

The width-based probes therefore do not have to locate a 256m region hidden
partway along a 512m edge for crossing/region lookup: simulator enable events
add those regions independently, and `getRegionFromPosGlobal()` scans the full
loaded region list by each region's origin and width. Confirmed by source trace,
not live crossing. The limitation is specifically terrain adjacency: the
surface has one legacy neighbor slot per direction and the original
`LLSurface::connectNeighbor()` maps equal patch counts one-to-one. A separate
partial implementation now maps shared patch intervals for unequal-width
square regions and tracks every surface link for teardown. Its compiled stub
harness covers cardinal spans, endpoint diagonals, multiple smaller neighbors,
all corner orientations, duplicate/conflicting links and cleanup. This does not establish complete topology across arbitrary
multi-region mosaics, nor pathfinding support for multiple neighbors in one
direction. Treat the seam implementation as partial pending independent review,
a full viewer build, and terrain-render/live-crossing verification.

Region-level object reachability had a separate dependency on the surface links:
`LLViewerObject::isReachable()` breadth-first traversed
`getNeighboringRegions()`, which omits unequal-width neighbors because those
surfaces are not connected. It now uses a loaded-region geometry scan that
includes edge- and corner-touching regions and rejects gaps/overlaps. This is
covered by the compiled bounds-helper tests but is not a live crossing or
object-permission verification. Partial mixed-size surface seam links are now
implemented as described above; pathfinding's one-region-per-direction
selection remains unchanged.
| 7 | Event queue and HTTP lifecycle | Checked server long-polls EventQueue for about 58s; reference has an OpenSim 60s transfer timeout. The reference’s extra OpenSim 502 branch is unreachable. | **Not yet reconciled against effective current timeout/retry/cancel paths.** Trace the concrete request lifecycle before editing; circuit timeout is not yet attributed to this. |
| 8 | QUIC handshake/routing | Viewer and checked server agree on ALPN, circuit binding, and `quicready`; active per-region simulator ports are distinct from the 9001 proxy. Fixed zero-length server keepalive framing; restored bounded synchronous connection-start retries; documented automatic LLUDP fallback. | **Viewer source fixes + compiled parser/routing/retry fixtures pass.** No live 300-second idle test or Windows build yet. Tasia_Magic_School's absent 9036 socket remains unrelated/unresolved runtime evidence. |
| 9 | SimulatorFeatures and limits | Checked server emits per-simulator scale limits, `MinSimHeight`, `MaxSimHeight`, terrain-heightmap limits, grid/search/guide/currency URLs, and chat ranges. Current viewer now consumes finite OpenSim `MinSimHeight` for world-map Z and `say-range` for local-speaker proximity. | **Partial source fixes:** negative-Z input/display and per-region speaker-list radius are restored; SL retains its prior 0..4096 Z and 20m speaker radius. Prim/physics scale, whisper/shout presentation, and remaining extras are still unconsumed or under audit. Do not apply per-region scale values to global `LLWorld`; audit region-aware edit paths separately. Focused source/helper tests pass; no full build or live map/chat test. |
| 10 | Mesh/material/upload | Current source has grid-aware mesh capability routing, while historical logs include mesh/asset HTTP 404s. Old viewer has additional OpenSim-specific mesh/upload paths. | **Method-by-method audit pending.** Classify endpoint, status and returned body first; 404s do not establish a QUIC cause. GLTF material parse failures also need their own reproduction. |
| 11 | Map URLs/Hypergrid | Old viewer contains Hypergrid map-name handling and variable region-map display; current map block size extension is restored (Step 2). Reference also preserves SLURL region origins for non-teleport place details; current source now carries `ox`/`oy` into region-local parcel-coordinate calculation. | **Partial.** Focused source checks pass; live SL/OpenSim SLURL, place-detail and teleport behavior remain unverified. Continue exact-name lookup, Hypergrid parsing/routing and grid-authority audits. |
| 12 | Login, grid metadata, saved accounts | Current tree adds bounded grid catalog/metadata discovery absent from the old Tasia UI. Windows account-history passwords use per-grid SQLite/DPAPI; only the login authenticator uses MD5. | **Phase 1 source fixes + offline contracts:** typed username survives grid changes without password/remember-password carry-over; unbound legacy credentials are SL-only; movement completion validates agent/session before changing startup state; identity/circuit values are not emitted by the touched diagnostics. The raw-password SQLite/import extension explicitly reports unavailable off Windows and leaves the existing `LLSecAPIHandler` remembered-credential path in use; raw-password recovery/import parity is unsupported there. Windows UI/DPAPI and live login remain unverified. |

### 2026-10-22 login failure-stage evidence and Phase 1 corrections

The reported candidate is recorded in `BUILD_NOTES.md` as the isolated Windows
Release package `self-avatar-cof-fallback-20261022-01`, EXE SHA-256
`bd104eecfa892c88561d95da53364864908a3329048907a2b3546b212c1c218b`. Its
portable log records QUIC establishment without simulator-session confirmation,
fallback to UDP, `CompleteAgentMovement` reliable-delivery timeout `-23016`, then
no circuit ping for 103.17 seconds and circuit drop. This establishes the observed
failure at **stage D (simulator circuit/movement confirmation)** for that
candidate; it does not establish whether the server failed to receive/process
`UseCircuitCode` or `CompleteAgentMovement`, or whether the server's reply/ACK was
lost. The exact candidate's complete source-tree SHA and server-side event
correlation are unavailable. No login credentials, account, or live grid were
used in this audit.

| Stage | Evidence in candidate record | Assessment |
|---|---|---|
| A — login UI / credential selection | No recorded UI/credential-selection error in the inspected connection lines. | Not observed; no reproduction with Mom's account. |
| B — XML-RPC authentication | Candidate reached world initialization and attempted simulator connection; authenticated response was accepted. | Authentication succeeded for the recorded attempt. |
| C — response parsing / benefits | `process_login_success_response()` advanced into world initialization and enabled the first simulator circuit. | No parsing/benefits failure observed. |
| D — region seed / circuit / movement | QUIC session confirmation absent, UDP fallback; `CompleteAgentMovement` delivery timed out; later circuit ping timeout; no verified `AgentMovementComplete`. | Failure stage established; root cause unresolved without server-side correlation of UseCircuitCode, CompleteAgentMovement, quicready, ACK and AgentMovementComplete. Do not alter credentials, disable ViewerAsset, or infer TCP failure from legacy error name `LL_ERR_TCP_TIMEOUT`. |
| E — inventory / COF / appearance | The candidate did not reach usable-world validation; outfit/COF result was not tested in this attempt. | Not reached / not verified. |

This branch additionally restores typed-username convenience on a grid switch
while clearing password state, prevents unbound legacy `UserLoginInfo` from being
rebound to custom grids, checks the AgentID/SessionID before advancing movement
state, and removes username/host/circuit identifiers from the touched log lines.
These are code/source-contract checks, not proof of successful SL or OpenSim
login. `tasia_login_flow_test.py` passes; see `BUILD_NOTES.md` for environment and
review limits. The `tasia` remote is the configured alias in this checkout; no
`origin` remote is configured.

## Protocol-definition details

### `MapBlockReply.Size` — source fix completed; runtime verification pending

- Both templates now define the variable `Size` block with `SizeX` and `SizeY`;
  active prehash declarations/definitions are present.
- Active handler reads the extension only on OpenSim and carries bounded sizes
  into map region storage, tracking intersection and displayed extents.
- Checked server `LLClientView.SendMapBlock()` emits size pairs for map blocks
  whose X or Y size exceeds 256m. It budgets the extra bytes per map result.
- Scope remains this optional extension. Preserve 256m defaults for SL and when
  the block is absent or has a mismatched entry count; malformed dimensions fall
  back to 256m. Focused policy and source-contract checks pass, including drawing
  extents. Windows x64 Release build completed with exit 0 and zero compiler/linker
  error markers; five warnings are recorded in `BUILD_NOTES.md`. Live packet
  capture and SL/OpenSim map behavior are not verified.

### `RezScript.NewScriptInfo` — current-only field, not yet called a miss

The active template and `LLViewerObject::saveScript()` support a conditional
`NewScriptInfo.TemplateID` block; the old template lacks it. The checked server
source's decoded `RezScript` handler does not visibly consume that field. Do not
remove the current feature or label it OpenSim-incompatible until packet decoding
and the deployed server’s script-template behavior are verified.

## Verification record

- 2026-10-21: Step 9 initial source comparison found no `OpenSimExtras` consumer
  in the current tree. The follow-up adds a narrow `MinSimHeight` consumer only:
  each region stores the finite OpenSim extra and world-map coordinate paths use
  it; this is not a general extras handler. The old `lfsimfeaturehandler.cpp` reads
  `MinPrimScale`, `MaxPrimScale`, and `MaxPhysPrimScale` from extras and applies
  them through global `LLWorld` setters. Checked OpenSim
  `Scene.AllModulesLoaded()` advertises per-simulator scale limits, and
  `SimulatorFeaturesModule` returns them under `OpenSimExtras`. Current
  `get_default_max_prim_scale()` instead returns fixed 64m/10m constants;
  `LLPanelObject::refresh()` uses that fixed result, while min scale remains
  `MIN_PRIM_SCALE`. Additional fixed-limit paths include multi-object and face
  scaling in `llmanipscale.cpp`, clipboard size paste in `llpanelobject.cpp`, and
  mesh upload preview scale in `llmodelpreview.cpp`. Object property `sendScale()`
  sends the entered size directly; the UI spinner maximum is its main client-side
  guard. `LLSelectMgr::selectGetSameRegion()` exists but its checked caller guards
  linking, not scale manipulation; therefore mixed-region scale selections cannot
  be assumed impossible. This confirms the viewer-side parity gap, not a server
  defect. A safe scale port still needs limits resolved per object/region and
  applied consistently through manipulation, properties, clipboard paste and
  upload preview. Do not apply a last-response-wins global callback.

- 2026-10-21: Continued the read-only extras inventory against checked server
  emitters and legacy `lfsimfeaturehandler.cpp`. Core server extras include
  `SimulatorFPS*`, per-region prim/physics scale limits, grid identity/alias,
  search/destination/currency URLs, `MinSimHeight`, `MaxSimHeight`,
  `MinHeightmap`, and `MaxHeightmap`; `ChatModule` emits say/shout/whisper ranges.
  Legacy consumes several further optional keys (export, map server, grid status,
  avatar picker, currency symbol). Current login/grid code handles some URL
  services independently, so an unreferenced extra alone does not prove behavior
  loss. Confirmed world-map parity gap: OpenSim advertises `MinSimHeight` (`-100`
  in checked constants), legacy stores it per region and accepts negative world-map
  Z, while current map paths imposed zero. Current source stores a finite
  OpenSim-only region minimum and applies it to world-map display/typed Z; Second
  Life remains 0..4096. Focused region/compatibility tests pass; full build and live
  SL/OpenSim map behavior remain unverified.

- 2026-10-21: `ChatModule.RegionLoaded()` advertises the configured integer
  `say-range` per region (default 20). Legacy `LLLocalSpeakerMgr::updateSpeakerList()`
  uses that value both when collecting nearby text speakers and when removing
  speakers that move out of range. Current code used fixed `CHAT_NORMAL_RADIUS`
  (20m) in those two operations. Current source now stores a bounded/validated
  per-region say range and uses it for that list only; Second Life's default stays
  20m. Current chat-color fading uses the user-controlled `NearChatRadius`, unlike
  legacy's say range; that preference remains untouched. `whisper-range` and
  `shout-range` have no corresponding current UI consumer identified in this pass.
  Helper/feature-wiring checks pass; speaker method is not compiled in isolation,
  and Windows/live chat verification remain pending.

Remaining extra-key disposition from this pass (source audit, not blanket parity):

- `map-server-url` already arrives through the login response and populates
  `CurrentMapServerURL`; no current consumer of the simulator extra was found.
- `search-server-url` and `destination-guide-url` are read by the legacy handler;
  current `llfloatersearch.cpp`/`llpaneldirweb.cpp` use `SearchURL`, and
  `llfloaterdestinations.cpp` uses `DestinationGuideURL`. The grid metadata
  validator accepts `search`, but no consumer of that stored metadata field was
  found. Thus these are concrete configuration/feature-routing gaps, not proof
  that the server's endpoint is safe to apply to every grid/region.
- `currency-base-uri` can be region-specific via optional `SampleMoneyModule`,
  while current currency transactions resolve a selected-grid `currency_uri` or
  validated `helper_uri` and verify it with `gridURL()`. Do not accept a
  simulator-supplied transaction URL without an explicit trust/host policy;
  different region endpoints are a potential gap, not authorization to weaken
  the current URL guard.
- `GridStatus`/`GridStatusRSS` can be advertised by checked GridService config;
  current status polling reads a single global setting, and its HTTP request
  disables peer verification. Per-region routing would need a grid-scoped,
  TLS-verified design before consuming those URLs.
- Legacy `GridURL` affects its Hypergrid prefix; checked `GridURLAlias` is
  converted into server-side `SceneGridInfo` aliases rather than directly read
  by the legacy feature handler. Current viewer has no direct OpenSimExtras
  consumer for either; audit exact SLURL authority/teleport flows separately.
- Legacy caches `SimulatorFPS*`, but no call sites consuming those getters were
  found in the reference checkout. `ExportSupported` gates legacy export UI,
  while current has no `PERM_EXPORT` consumer; the checked server advertises the
  flag but no matching permission implementation was found in this source tree.
  Verify deployed server/version and permission-bit semantics before porting.
- Checked `MaxSimHeight` is 50000, but legacy world-map Z still clamps its upper
  bound to 4096; `MinHeightmap`/`MaxHeightmap` need terrain-tool/server behavior
  traced before use. Do not infer a viewer bug from an advertised limit alone.

Next safe action: design a grid-scoped search/destination configuration path and
an explicit trust policy for any economy-feature override; then audit Hypergrid
authority/SLURL behavior. Keep the larger prim-scale port blocked until
region-specific constraints are designed across all edit paths; no global
last-response-wins values.

## Variable-region allocation ceiling correction

2026-10-21 source trace: checked server `indra/telep-npc/OpenSim/Framework/Constants.cs`
defines `MaximumRegionSize = 4096`, `MinRegionSize = 256`, and terrain patch size
16. Current `LLSurface::create()` allocates `(width+1)^2` `F32` heights and the
same number of `LLVector3` normals (12 bytes from its three-F32 representation),
before patch objects and composition data. At 4096m those two arrays total
268,566,544 bytes (~256MiB); at 8192m they would total ~1GiB, even though the
checked server rejects dimensions above 4096. The old viewer policy incorrectly
accepted 8192m. Set the shared region/map/parcel ceiling to 4096 and test that
8192 is rejected before terrain creation. This prevents a malformed/non-server
size from triggering the oversized allocation; it does **not** make a legal
4096m allocation cheap or prove crossings crash-free. Keep variable-size support
marked partial until mixed-size/shape and high-memory crossing tests run.

Follow-up allocation trace: `LLVLComposition` also inherits `LLViewerLayer`,
whose constructor allocates and zeroes `width*width` `F32` values. At width 4096
that adds another 67,108,864 bytes (64MiB); the two known surface/composition
arrays therefore total about 320MiB before patch objects and renderer resources.
`LLPatchVertexArray` uses small LOD lookup tables and is not the dominant
allocation. No region-allocation API currently reports recoverable failure to
all startup/teleport/crossing callers; a speculative low-memory fallback to a
fake 256m square would misrepresent a valid region. A safe runtime budget guard
needs an explicit user-visible unsupported/low-memory path across those callers,
not just another dimension clamp.

2026-10-21 region-local place coordinate parity: the reference
`llurldispatcher.cpp::regionHandleCallback()` adds the parsed SLURL's local
position to the resolved region origin and includes that origin as `ox`/`oy` in
the non-teleport `remote_place` key. Reference `LLPanelPlaces` passes the handle
to a `LLPanelPlaceInfo::displayParcelInfo()` overload, which computes local
coordinates by subtracting that origin. Current source previously dropped
`ox`/`oy` and used `fmod(global,256)` for all regions. Restored the origin key,
conditional dispatch, and overload. Standard 256m SL coordinates produce the
same local X/Y; larger OpenSim region coordinates no longer wrap at 256m. The
focused region harness includes source-wiring assertions; no Windows build or
live SLURL/place-details test has been run.

Follow-up in `LLLandmarksPanel::doCreatePick()`: the landmark already supplies
its `region_id`, but the current path used `fmod(global,256)` for the
`RemoteParcelRequest` local coordinate. It now resolves that UUID through
`LLWorld::getRegionFromID()` and uses `LLViewerRegion::getPosRegionFromGlobal()`
when loaded, retaining the legacy modulo fallback for an unloaded/unknown
region. Focused source wiring assertions pass; no live landmark/pick request or
Windows build has verified the endpoint behavior.

Related local-coordinate displays fixed in the same audit: profile-pick and
classified location text now resolve the loaded region containing the global
position and format its region-local X/Y. They retain the former 256m modulo
when that position is outside all known regions. This avoids changing behavior
for standard SL regions while preventing a loaded 512m+ region's coordinates
from wrapping in these UI strings. Tests assert production-source wiring only;
the live UI was not rendered/tested.

The generic `LLPanelPlaceInfo::displayParcelInfo(region_id, pos_global)` path
now also resolves the loaded containing region before falling back to modulo
256. This covers remote-place and history entries that do not carry the
SLURL-specific `ox`/`oy` origin. Unknown/unloaded positions preserve the old
fallback; source assertions pass, while endpoint/UI runtime behavior remains
unverified.

The same loaded-region conversion now covers classified profile text, group-land
and land-holdings lists, and selected-parcel place-profile data. The two land
lists retain the modulo fallback when no loaded region contains the global
position; selected-parcel data already has an authoritative `LLViewerRegion*`
and now uses it directly. These are UI coordinate corrections, not terrain
crossing fixes; source assertions pass, but visual/runtime verification remains
open.

- 2026-10-20: Map-size source fix verification: region compatibility and SL
  isolation test passed; world-map size contracts 4/4 passed; protocol comparator
  passed 12,000 synthetic packets and reports only current-only
  `RezScript.NewScriptInfo`; `git diff --check` passed. Windows x64 Release build
  then completed with exit 0, zero compiler/linker errors and five warnings;
  binary hashes and backup paths are recorded in `BUILD_NOTES.md`. No live test.
- 2026-10-20: `tasia_protocol_compare_test.py
  /home/marty/opensim/tasia-viewer-2026` passed: 12,000 synthetic valid packets,
  483 messages in each template, 26 critical definitions identical, and no
  message IDs present in only one tree. It reports only the two field-level
  differences listed above.
- Legacy inventory fetch fixture: 2/2 passed; OpenSim services/profile 15/15;
  identity/grid 4/4; `git diff --check` passed in the same session.
- These are source/fixture checks, not a full viewer build, live packet capture,
  or live grid test.

### 2026-10-22 AIS outfit-fetch failure guard and redacted diagnostics

The legacy `CallAfterCategoryFetch` route already fails closed when its observer
invokes the callback after failure. The AIS route had a separate gap: its
nullary callback ran regardless of the `FetchCategoryLinks` result, and AIS
completion IDs were extracted from error responses. A previously complete cached
category could therefore pass `isCategoryComplete()` after a failed refresh and
be applied as though the request had succeeded. The new completion-aware overload
retains the old nullary overload for existing callers, but lets the outfit path
require a non-null ID matching the requested category before `wearCategoryFinal`.
AIS update application and completion-ID extraction now require a successful
HTTP status and an LLSD map. Failure still settles the outfit loading indicator
and stops the matching diagnostic phase before returning without copy/apply. The
nearby background-fetch warning no longer prints the category UUID. This is
source-verified, not live AIS proof.

Also removed AIS diagnostics that printed inventory capability URLs, full request
or response LLSD, and inventory UUIDs. Generic request-size, operation and HTTP
status messages remain. The AIS source-contract test checks callback ID/status
gating, failure-before-mutation ordering and absence of URL/result logging.
`tasia_wear_category_fetch_test.py`, `tasia_initial_wearables_test.py` (including
its ASan/UBSan fixture), and `tasia_cof_library_fetch_test.py` pass. The Farm
review `job_mvjmn03u_909a6a3154` agreed with the ID/status guard and legacy
wrapper, and requested verifying loading-indicator settlement; the failure path
now also stops the diagnostic phase. This follow-up is code/test checked, not
live AIS proof. Follow-up review `job_mvjmpr5f_9bceab0be6` found the settlement
delta correct and requested a callback-order check. Source tracing confirms one
AIS completion callback followed by background-fetch bookkeeping, with no later
consumer requiring this diagnostic phase to remain active; its sole start and
error/normal stops are the three occurrences in `llappearancemgr.cpp`. No Windows
build, live AIS failure injection, or outfit runtime test was run.

### 2026-10-22 legacy bake / BoM parity audit

Compared the reference `indra/newview/llviewertexlayer.cpp::doUpload()` with
the current `indra/newview/llviewertexlayer.cpp::doUpload()` and
`processUploadCompletion()`. The reference's `glReadPixels(GL_RGBA)` path is not
copied: the active viewer is DX11, and current `LLBake::readColor()` uses
`DXReadback::readPixels()` with bounds/sample/format validation, row-pitch
compaction, and BGRA-to-RGBA conversion. The current bake still gathers the
morph mask, packs RGBA plus mask into five `RGBHM` channels, validates encoded
J2C dimensions/components, uploads through `UploadBakedTexture` when available,
and otherwise uses asset storage. Legacy baking is gated to OpenSim regions with
central bake version 0 and valid baked slots; Second Life/server-bake paths remain
separate. Upload completion uses revision/ticket checks and bounded retries before
assigning the baked texture, and accepted assignments refresh attachment bake
bindings. Fully invisible layers assign `IMG_INVISIBLE` without manufacturing an
empty encoded texture.

The recorded live failure remains specifically `BAKED_HEAD`: Mom reported that
the visual retest still failed, and the read-only package-log inspection recorded
under `BUILD_NOTES.md` → “BAKED_HEAD remained solid black after the readiness fix”
found a later all-RGB-black/nontransparent composite uploaded after readiness
changed from false to true. The current source guard rejects that summary before
encoding and lets the existing bounded retry policy run; this is containment, not
a root-cause repair. That candidate log recorded a 542-byte payload, but its
decoded inputs could not be attributed because texture IDs were redacted. I did
not independently observe the viewer image; the unresolved visual status is
Mom's reported result plus the inspected log, not a new runtime test. No source
evidence identifies which wearable/texture or compositor layer produced the black
pixels. Do not change texture routing, bypass readiness, or weaken the head-bake
guard without a new sanitized per-input failure trace.

Focused fixtures pass in this worktree (command output summarized in
`BUILD_NOTES.md` → “Phase 3 legacy local-bake / BoM parity audit”):
`tasia_bake_readback_test.py` (production pixel helper,
synthetic DX format/pitch, ASan/UBSan), `tasia_bake_upload_test.py` (upload
policy/appearance sender fixtures, ASan/UBSan), `tasia_bake_refresh_test.py`,
`tasia_legacy_bake_port_test.py`, and `tasia_bom_limits_test.py`. These do not
exercise the real D3D device, J2C codec/server, upload endpoint, avatar layers or
live BoM attachments. The current Linux environment has no configured build tree,
`msbuild`, `devenv`, or `cl`; no Windows build was run in this worktree. A real
Windows DX11 build and Mom's visual test remain required. The prior candidate's
black face is **not fixed** by source guards.

### Step 3 — legacy purge confirmation contract (design only)

Source facts verified in the current viewer, historical viewer, and checked
OpenSim source:

- `PurgeInventoryDescendents` is sent reliably, but the checked server schedules
  `PurgeFolderAsync` with `FireAndForget`; it emits no application-level success
  or failure reply. Reliable transport delivery is not purge completion.
- The historical viewer clears local descendants and invokes its callback
  immediately after sending purge. Its category-destroy callback then decides
  whether to send `RemoveInventoryFolder` from that optimistic local state.
- `categoryHasChildren()` can return `CHILDREN_MAYBE`; `CHILDREN_NO` can also
  reflect only local/server count state. Protected categories are rejected by
  `remove_inventory_category()`, but it only warns on incomplete categories.
- `processInventoryDescendents()` checks OpenSim and AgentID and validates
  folder/version/count fields, then upserts received children and updates the
  parent count/version. It neither removes absent cached children nor exposes a
  purge-request correlation token. The legacy reply contains no request ID, so a
  delayed reply cannot be unambiguously tied to a particular poll/purge attempt.
- The bounded Pawan review independently echoed these facts, but its response was
  truncated at the proposal heading by the Farm output-token cap; no proposal from
  that response is treated as evidence.

Candidate state machine and safety boundary (not implemented):

1. **Guard:** only enter for an existing, non-protected category on an OpenSim
   no-AIS route. Keep the category object and callback alive. Unknown/incomplete
   metadata is not proof of emptiness; never use the old destructor's optimistic
   local purge as confirmation.
2. **Purge sent:** send one reliable purge request. Do not clear local descendants,
   issue parent removal, or fire a success callback yet. Never automatically
   resend the destructive purge: timeout does not prove it failed, and a retry
   could erase children added after the first purge.
3. **Bounded read-back:** after a delay, request that folder's legacy contents;
   permit at most one confirmation fetch in flight per folder and poll only up to
   a fixed attempt/deadline bound. Validate AgentID, exact FolderID/owner, known
   version, nonnegative count, and consistency between the advertised direct
   descendant count and returned folder/item blocks before treating a response as
   a snapshot. Nonempty results may be reconciled and retried; timeout, malformed
   data, or unresolved state must stop without parent deletion.
4. **Empty snapshot:** only after a validated empty response may local descendants
   be reconciled, then may the existing parent-removal path be considered. The
   current handler cannot safely do this yet: it needs a completion hook and
   explicit snapshot reconciliation, and absence from a reply must remove stale
   local children rather than merely upserting received objects.
5. **Hard blocker:** because the server provides no purge completion ack and the
   legacy reply has no request correlation, even a post-request empty response
   cannot be proven to belong to that request (it could be delayed/out-of-order).
   Repeated empty polls reduce ambiguity but do not establish a strict guarantee.
   Do not ship automatic parent deletion on this evidence alone unless server
   purge/version ordering is verified or the protocol gains a correlated completion
   signal. Until then, safest behavior is to leave the parent intact and surface a
   clear incomplete/timeout outcome. The generic `LLInventoryCallback::fire()` has
   no failure parameter, so callback settlement/error UX must also be designed;
   do not fire it as success on timeout.

Focused regression matrix required before any implementation: state `YES`,
`MAYBE`, unknown and stale-zero; protected and incomplete categories; purge send
failure; delayed server purge; nonempty then empty replies; permanently nonempty
and timeout; wrong agent/folder/owner, invalid version/count, count/block mismatch;
stale cached children absent from an empty snapshot; duplicate, late and
out-of-order replies; concurrent folder mutation; exactly-once callback on success
and a defined failure settlement; assert no parent removal/local purge/callback
success on every ambiguous or failed path. Test message routing with production
methods plus service doubles, not only source substring assertions.

### 2026-10-21 supplemental read-only source comparisons

- Inventory: current and old message templates retain the same inventory message
  IDs; the existing no-cap fetch fallback is present in current source. A separate
  mutation difference needs future review: old `LLViewerInventoryItem::updateServer()`
  sends `UpdateInventoryItem` when AIS is unavailable, while current calls
  `AISAPI::UpdateItem()` unconditionally; `AISAPI::UpdateItem()` returns when its
  inventory capability is absent. The checked server registers the legacy item
  update handler. This is a source-level compatibility risk, not a runtime-tested
  regression; do not change it as part of the purge task.
- QUIC: current `LLCircuitData::ackReliablePacket()` rejects bootstrap ACKs before
  authenticated `quicready`; the old implementation has no equivalent guard.
  Current source also has explicit UDP recovery on QUIC failure/readiness timeout,
  while the old source says “NO LLUDP fallback per spec.” Both viewer trees skip
  LLUDP retransmission for QUIC circuits, so retransmission is not a difference.
- Reviewed the curated 2026-10-21 snapshot only. No viewer source, live viewer,
  or cache was changed. These comparisons do not establish deployed behavior.

### 2026-10-21 QUIC source/config audit — read-only

**Legacy reference provenance:** `/home/marty/opensim/tasia-viewer-2026` has
`origin=https://github.com/martysl/tasia-viewer-2026.git`, matching the requested
candidate, and is on local `main` at `93a5c1db`. Remote `main` was observed at
`fee60c9e`; therefore the local comparison checkout is the correct repository
but is behind the observed remote head. Its pre-existing `autobuild.xml` edit was
left untouched, and no fetch or reference edit was made.

**Viewer contract (current source):** QUIC is OpenSim-only and opt-in through
`TasiaQuicEnabled`; startup initializes MsQuic with ALPN `opensim-ll/1`. Login
uses `sim_quic_host`/`sim_quic_port`; event-queue negotiation reads the simulator
endpoint variants emitted by OpenSim. The viewer opens a bidirectional stream and
frames LL packets as a 4-byte big-endian length plus payload. It sends
`UseCircuitCode` first, holds other traffic until a `quicready` message arrives
from the same authenticated QUIC circuit, and recovers over UDP on transport or
readiness failure (five-second readiness timeout). Relevant sources:
`indra/newview/lltasia_quic.h`, `indra/newview/llstartup.cpp`,
`indra/llquic/llquicglobal.cpp`, `indra/llquic/llquicstream.cpp`,
`indra/llmessage/llcircuit.cpp`, and `indra/newview/llviewergenericmessage.cpp`.

Negotiation call sites cover login, `EnableSimulator`, `TeleportFinish`, and
`CrossedRegion`. A fresh crossing route sends `UseCircuitCode` before
`CompleteAgentMovement`; teleport sends `UseCircuitCode` before movement. Existing
circuit routes are deliberately reused rather than silently converted from UDP
to QUIC. On an established QUIC failure the current circuit replays its bootstrap
and pending, unaccepted packets over LLUDP, then remains on UDP; it does not
transparently reconnect to QUIC on that same circuit. A later fresh circuit can
negotiate again. This differs from a retrying reconnect and should be tested as
the intended recovery behavior.

**Legacy retry/fallback preference status:** the old reference registers
`TasiaQuicConnectionRetries` (default 1) and
`TasiaQuicFallbackLLUDP` (default false) in `settings.xml`; login,
`EnableSimulator`, teleport and crossing use the retry setting. Current retry
control is now registered and consumed by the shared endpoint helper; the message
layer clamps it to 1–10 and creates a fresh connection object for each synchronous
connection-start failure. The default remains one attempt. TLS/QUIC handshake is
asynchronous, so this setting does not retry a handshake/readiness timeout; the
existing readiness deadline and LLUDP recovery still handle those. The old
`fallback=false` semantic is not restored because current routing intentionally
recovers over LLUDP. Removed the inert fallback checkbox from English/Polish
Preferences and replaced its contradictory description with localized
information that LLUDP recovery is automatic when the grid permits it.

**Checked OpenSim source contract:** `[ClientStack.Quic]` defaults to disabled;
when enabled, the listener uses ALPN `opensim-ll/1`, accepts the viewer's framed
bidirectional control stream, unwraps packets into the normal LLUDP dispatcher,
authorizes `UseCircuitCode`, associates the transport, and sends `quicready`
before the normal region handshake. The server source is stream-only; the viewer
uses QUIC datagrams only if the peer negotiates a send limit, otherwise it falls
back to its stream path. The stream packet formats therefore interoperate at
ordinary LL packet sizes. There is a theoretical size-boundary inconsistency:
viewer stream framing accepts payloads through 65,535 bytes, while server framing
accepts 65,536 bytes but assembles into a 65,536-byte buffer including the
4-byte header. A 65,533–65,536-byte server-side frame cannot fit; the viewer's
actual `NET_BUFFER_SIZE` is 8,192, so this boundary is not reachable by normal
viewer LL packets. It remains a source-level hardening/test item, not a demonstrated
cause of the reported session failure.

**Idle keepalive interoperability finding and viewer fix:** checked server
`QuicServerConfig` defaults `KeepaliveMs=30000`; its `QuicClientConnection`
writes `PacketFraming.KeepaliveFrame`, the four zero bytes `[00 00 00 00]`, on
the control stream at that interval. Server `PacketFraming.TryReadFrame()` treats
a zero payload length as a keepalive marker and consumes it without delivering a
packet. Before this patch, current viewer `LLQuicStream::feedRx()` rejected
lengths below 6 as invalid, called the connection failure path, and consequently
fell back to UDP. The viewer now consumes zero-length frames in place and
continues parsing coalesced/fragmented stream data. A compiled production-parser
fixture covers a byte-fragmented keepalive, a valid LL frame, and a following
keepalive. The legacy reference parser did not implement the marker: after an
isolated zero-length frame it remained in its payload phase, and later traffic
could be emitted as an empty frame. The behavior was adapted to the current
bounded/error-checked parser rather than copied blindly.
This establishes the source-level cause/fix for periodic idle keepalives, but does
not yet prove a live connection survives 300+ seconds.

**Live i-Grid INIs checked read-only:** SSH connected to `i.let-us.cyou` on its
documented non-default SSH port. The active ROBUST container bind-mounts
`/home/marty/opensim/Robust.HG.ini`; its `[QuicProxy]` section says
`Enabled=true`, `Port=9001`, `ALPN=opensim-ll/1`. The shared
`/home/marty/opensim/igrid-bin/bin/OpenSim.ini` has a 9001 default, but it is not
the per-region file selected by the running simulators. Inspected simulator
command lines use `./Regions/<region>/Opensim.ini`; the mounted per-region files
are under `/home/marty/opensim/regiongen/Regions/<region>/Opensim.ini`. Those
files enable QUIC, advertise `i.let-us.cyou:9001`, use the matching ALPN, and set
distinct simulator backend ports (for example `Blue=9010`, `Grid_Welcome=9033`,
`Tasia_Magic_School=9036`). Only selected QUIC/network settings were extracted;
credentials and unrelated INI values were not read into the audit.

**Corrected source/config comparison:** the checked server source rejects a
backend `quicPort` only when it equals the proxy listener port; the login
pre-registration path likewise omits only a simulator port equal to the proxy
port. The actual per-region INIs use distinct ports, so the earlier same-port
diagnosis based on the shared base INI was wrong and is withdrawn. Host UDP socket
inspection agrees for several live regions: port 9001 is owned by `Robust.dll`,
while ports 9010 and 9033 are owned by the running `Blue` and `Grid_Welcome`
simulator processes. The `Tasia_Magic_School` process was running with its
per-region INI set to 9036, but no 9036 socket was present in the same snapshot.
This is a narrower runtime lead, not yet tied to the reported viewer session.
Available Docker, file, and tmux-captured logs had no matching QUIC startup or
registration lines. The deployed `OpenSim.Region.ClientStack.LindenUDP.dll` was
timestamped 2026-10-07 10:52 UTC (SHA-256
`1c4ebc443157a684c2efff6f435fd78c2610628b05a3fbf4fe9493f4532039fd`); its exact
source revision remains unverified.

**Verification boundary:** `python3 indra/newview/tests/tasia_quic_transport_test.py`
passes 6/6, including compiled production framing, routing, synchronous retry
fixtures and localized QUIC Preferences checks;
`git diff --check` passes for the focused source/test changes. No live viewer
session, packet capture, Windows build, or 300+ second idle test was run. The
Linux baseline CMake configure is blocked because `indra/CMakeLists.txt` hardcodes
the Windows `x64` generator platform; Ninja rejects that platform and no Windows
compiler/MSBuild is available in this environment. No server build, service
restart, INI change, or cache operation was performed.

## Next exact action

The source-snippet-grounded review `job_mvi6x8mx_7de7ddcc9e` is complete. Its lead
verified the helper/BFS behavior and expanded tests; the Pawan peer review was
empty. The first review's claims about a second BFS area filter and integer
overlap math were disproved by source. Added negative-width and partial-overlap
test cases. Region-list mutations are reached from simulator message handlers in
the per-frame `idleNetwork()` loop; `isReachable()` is called from UI/menu/chat
callbacks. This supports, but does not runtime-prove, same-main-loop access; no
concurrency stress test was run.

**Windows build update (2026-10-21):** Mom authorized an isolated build only.
Created `F:\tasia-kirsten\build-candidates\mixed-size-reachability-20261021-01`
without changing the original Acer tree. All 10,388 source snapshot files were
SHA-256 verified; CMake configuration is VS 2022 x64 Release. After resolving
candidate-only dependency/cache path issues and fixing the world-map spinner
type/direct include errors, attempt 4 built target `Kirstens-S24` with exit 0 and
zero error markers. Seven warnings remain, including linker duplicate-symbol and
`/FORCE` warnings; the message-template compatibility check fell back to
syntax-only because the upstream template download failed. EXE SHA-256 is
`fb10b60a525ce74b554f0a7a7a9f14510ddd98eff194a01ed3eff5d52f34b63d`; PDB
SHA-256 is `1994f956a333965852d5a8516e794b10186ae402849bf63d57e26be167bae0e6`.
No install, launch, profile/cache access, or modification of the original Acer
tree occurred.

**Next:** after Mom's manual login attempt, verify QUIC idle for >300s, reconnect,
region crossing, and LLUDP fallback; a successful compile is not runtime evidence.
Continue Step 9 by inventorying scale mutation paths and selected-object-to-region mapping before designing per-region limits.
Audit advertised heightmap/altitude keys separately. Do not change production
configuration or restart services without Mom's explicit request. Keep Steps 3
and 7–12 separate; the inventory purge safety blocker remains unchanged.

**Package/startup update (2026-10-21):** Mom authorized portable packaging and
isolated launch using a package-local temporary profile/cache. Package hash
verification passed (7,620/7,620 entries); startup process remained responsive
for 45 seconds in desktop session 1 and portable data was created beside the
candidate only. Mom reports login and QUIC work, with the avatar/objects loading
quickly; no stored login credential was used. Do not use the localhost-only saved
test account against the public i-Grid endpoint. Live >300-second idle, reconnect,
crossing, and LLUDP-fallback checks remain unevidenced.

**Appearance/system-layer log diagnosis (2026-10-21):** read-only scan of the
isolated candidate log shows asset-store “not found” errors and an outfit fetch
shortfall of one `WT_ALPHA` and one `WT_TATTOO` wearable. COF link targets were
later reported resolved/reloaded; that does not establish successful wearable
payload fetch. Required shape/skin/hair/eyes later reached 1, `previous_loaded=1`,
and no pending bake uploads; local bake uploads and legacy appearance send were
logged. `te=19` is `TEX_SKIRT_BAKED`, an optional skirt composite, and was
GPU-ready despite being undefined. Separate HTTP texture 404s/decode failure
lack safe asset correlation, so they cannot be attributed to the missing layers.
Mom identifies this as a viewer bug; the wearable/texture errors are observations,
not evidence that the server is the root cause. Work history records viewer-side
fixes in this exact bug family: age29 restored the DX11 invisible-alpha bake
semantic; age38 corrected legacy bake-slot appearance propagation, detach-time
tattoo/alpha/universal texture clearing, and attachment refresh after bake
assignment. The historical visual validation was pending, so the fixes are not
proof that the current candidate renders layers correctly. Current focused
`tasia_bake_upload_test.py`, `tasia_legacy_bake_port_test.py`, and
`tasia_cloud_readiness_test.py` pass, but do not exercise the live Windows
GPU/grid path. Candidate process is stopped; portable logs/data were preserved.
No production code changes or cache clearing occurred. Next investigation should
trace the viewer's current legacy local-bake/composition/texture assignment path;
do not change the OpenSim server based on these logs.
