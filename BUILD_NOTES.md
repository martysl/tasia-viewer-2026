# Windows build notes

## 2026-10-21 mixed-size region reachability

`LLViewerObject::isReachable()` previously traversed `LLSurface`'s eight
surface-neighbor slots. Those are intentionally connected only for equal-width
regions, so a loaded smaller region beside a larger one was missing from this
region-level reachability graph. Added `LLViewerRegion::getAdjacentRegions()`:
it scans the loaded region list and uses a bounds helper to return regions that
share an edge or corner while rejecting gaps and interior overlap. Object
reachability now traverses this geometric graph. The terrain seam work below
adds separate partial-edge/corner patch links; pathfinding's
one-region-per-direction lookup remains a legacy representative rather than a
complete multi-neighbor graph.

The region compatibility harness compiles the production bounds helper and
checks a 512m region adjacent to two 256m regions along one edge, a corner-only
neighbor, a gap, overlap and invalid size. `tasia_region_compat_test.py` passed;
world-map-size tests passed 4/4; OpenSim compatibility tests passed;
`git diff --check` passed. Windows build and live reachability/crossing tests
remain unverified.

After Mom asked to continue in sequence, re-ran the focused checks on 2026-10-21:
`tasia_region_compat_test.py`, `tasia_worldmap_size_test.py` (4/4),
`tasia_opensim_compat_test.py`, and scoped `git diff --check`; all passed. The
earlier Farm review for this patch completed with an empty report, so a new
review-only request was submitted as `job_mvi6oc2m_269d8ebc86`; it was still
running with no reviewer activity at the last check. This host is Linux and has
no `msbuild`, `devenv`, or `cl`; no Windows build was attempted. Windows build,
live reachability, and crossing checks remain pending.

The new Farm review completed on 2026-10-21. Its claims that the reachability BFS
filters edge/corner contacts and that the helper uses integer arithmetic are not
supported by the source: the BFS directly accepts helper-approved neighbors, and
the helper's inputs/intermediates are `double`; region origins come from U32
coordinates decoded from the U64 handle. The review did identify useful test
coverage gaps, so the compiled-helper fixture now explicitly covers negative
width and a partial positive-area overlap without a shared boundary; negative
global-coordinate edge contact is also covered. `tasia_region_compat_test.py`
passes after these additions. Region-list lifetime/thread-affinity remained a
review question, not a confirmed race. The source-snippet-grounded follow-up
review `job_mvi6x8mx_7de7ddcc9e` has now completed: its lead verified the
helper/BFS logic and tests; the Pawan peer returned an empty review. I traced the
region-list mutation sites to the EnableSimulator, TeleportFinish, CrossedRegion,
and DisableSimulator message handlers, processed by the viewer's per-frame
`idleNetwork()` loop; all `isReachable()` call sites are UI/menu/chat callbacks.
This supports a same-main-loop assumption in the current call graph, but is not
a runtime concurrency stress test. No source-grounded evidence of concurrent
region-list mutation was found. The test and review do not prove Windows/live
behavior.

## 2026-10-21 partial mixed-size terrain seam links

`LLWorld::addRegion()` now scans loaded regions by geometric bounds after the
legacy equal-width neighbor hookup. For unequal-width square regions with
matching terrain patch spacing, it maps the shared interval to patch-index
spans and connects cardinal edge patches plus within-neighbor diagonal links;
corner-only contacts connect their corner patches. Edge-span endpoints also
connect diagonals into an adjacent simulator beyond the current shared span;
this avoids a load-order-dependent missing diagonal when a larger region is
loaded after multiple smaller neighbors. `LLSurface` retains every
surface relation for reciprocal teardown while `mNeighbors[8]` continues to
expose only one representative per direction to legacy pathfinding and other
callers. This is partial seam topology, not a claim that all eight-way links
across arbitrary mosaics of multiple neighboring regions are complete.

`tasia_mixed_region_seam_test.py` compiles the production `LLSurface` methods
against patch stubs and passes partial spans, endpoint junction diagonals in
all four cardinal orientations, multiple neighbors, a corner, invalid span
rejection, all four corner orientations, four 256m neighbors tiling one 1024m
side, idempotent duplicate links, conflict rejection without registering a
phantom neighbor, and reciprocal teardown.
`tasia_region_compat_test.py`, `tasia_worldmap_size_test.py` (4/4),
`tasia_opensim_compat_test.py`, and `git diff --check` also pass. These checks
do not build the viewer or verify rendered terrain, Windows behavior, live
crossings, arbitrary multi-region junctions, or 4096m memory behavior.
Pathfinding remains one-neighbor-per-direction; 4096m allocation remains a
substantial stress-test risk.

Review-only Farm job `job_mvi317oe_d98501d25f` reviewed the earlier version and
completed with a generic consolidated report and no source-grounded, actionable
defect verified here; the peer review did not return a separate message. It is
not an approval or runtime verification. The endpoint-diagonal follow-up added
after that review was sent to Farm as `job_mvi3as6p_bd76b4f665`. That report explicitly said it
had not seen the source and alleged missing reciprocal links; the implementation
does add both sides in `connect_patch()`, and the harness asserts reciprocal
links for all four orientations. The Farm result is not treated as a validated
finding or approval. A later source-reading review request,
`job_mvi3p495_c8de81bd56`, timed out before a worker started. A source-grounded
independent review remains desirable.

## 2026-10-21 variable-region simulator discovery and seam trace

Verified that the `LLWorld::addRegion()` width-offset scan only sets terrain
surface neighbor links. It is not simulator discovery: OpenSim's EventQueue
`EnableSimulator` includes dimensions and the viewer's LLSD-flavor handler reads
them before region creation. TeleportFinish and CrossedRegion have the same
dimension fields/reader path. `getRegionFromPosGlobal()` searches every loaded
region by origin and dimensions, independent of surface links. Added focused
source-contract assertions that these message names use LLSD and the three
handlers read X/Y sizes.

Thus mixed-size neighbor lookup is not the crossing blocker in this code. A
partial mixed-size terrain seam implementation is now recorded above; region
and crossing behavior are still not live-verified. Run
`tasia_region_compat_test.py` and `git diff --check`; no Windows build was run.

Follow-up patch topology trace: supported regions use the same 16m terrain-patch
world spacing, so patch-to-patch mapping across an edge can in principle be
derived from global patch coordinates even when region patch counts differ.
However, each `LLSurfacePatch` has only one pointer per compass direction, and
`LLSurface::mNeighbors[8]` also stores only one surface per direction.
`getNeighboringRegions()` / `getNeighboringRegionsStatus()` keep those arrays
positionally aligned for pathfinding, whose directional lookup chooses one
region. Voice still consumes this legacy list. Object reachability was switched
to a separate all-loaded-region geometry scan. The seam change adds a separate
surface-connection list so teardown is not limited to the eight representative
slots, while preserving the legacy pathfinding choice. The current implementation
maps shared cardinal intervals and corner contacts, but global patch matching
alone does not prove all diagonal links at multi-region junctions; that remains
unverified.

## 2026-10-21 large-region allocation failure path — audit only

The 4096m known terrain/composition arrays total about 320MiB before patch,
overlay and renderer allocations. The construction path has no recoverable
allocation contract: `LLWorld::addRegion()` directly constructs
`LLViewerRegion`; its constructor performs a series of raw-pointer allocations
for composition, surface, parcel overlay and partitions. `LLSurface::create()`
and `createPatchData()` use sequential raw `new[]` allocations, while
`LLViewerLayer` allocates and initializes its full scalar grid in the base
constructor. If an allocation throws, the not-fully-constructed
`LLViewerRegion` destructor does not run, so simply catching `std::bad_alloc`
around `new LLViewerRegion` would not reliably clean partial allocations.
Additionally, normal `LLAppViewer::frame()` only catches `LLContinueError`;
the `std::bad_alloc` catch exists only under `gSimulateMemLeak`.

No code change made. Do not add a speculative free-memory threshold or a
catch-and-continue around the current constructor: available-memory checks
cannot guarantee later renderer allocations, and callers currently expect a
valid region pointer. A safe recoverable path needs transactional/RAII ownership
for region construction plus an explicit failed-region result and caller policy;
that is a broader API change requiring design/build review. Until then, 4096m
acceptance remains a stress/crash-risk limitation, not a proven safe size.

## 2026-10-21 remaining loaded-region local coordinate displays

Extended origin-relative coordinates to classified profile strings, group-land
and land-holdings list entries, and selected-parcel place-profile details. The
lists use `LLWorld::getRegionFromPosGlobal()` with their existing 256m fallback
for unknown regions; selected parcel already has its region object and uses it
directly. These paths correct displayed/request coordinates only.

`tasia_region_compat_test.py` passed with source-wiring checks for updated
consumers and a compiled test of the actual `LLViewerRegion::getPosRegionFromGlobal()`
body: a region at global origin (1024,2048) maps (1324,2248,30) to local
(300,200,30). World-map-size contracts passed 4/4, OpenSim compatibility tests
passed, and `git diff --check` passed. No Windows build or live UI/map request
was run.

## 2026-10-21 generic place-details local coordinates

The generic `LLPanelPlaceInfo::displayParcelInfo(region_id, pos_global)` path
now resolves the loaded region containing the position and computes local
coordinates from that region's origin. It keeps the old 256m modulo only when
the target region is not in the viewer's loaded region list. This complements
the explicit-origin SLURL overload and covers place/history entries without
`ox`/`oy`.

`tasia_region_compat_test.py` passed with the new source-wiring assertion;
world-map-size contracts passed 4/4, OpenSim compatibility checks passed, and
`git diff --check` passed. Windows build and live place-details test remain
unverified.

## 2026-10-21 variable-region profile/classified coordinate displays

The profile-pick and classified location strings used `global % 256` for X/Y.
They now ask `LLWorld` for the loaded region containing the position and format
the region-local coordinates from that region's origin. If the position is not
in a known region, the existing modulo fallback remains.

`tasia_region_compat_test.py` passed with source assertions for both call paths;
world-map-size contracts passed 4/4, OpenSim compatibility tests passed, and
`git diff --check` passed. No Windows build or rendered UI test was run.

## 2026-10-21 variable-region landmark-to-pick coordinates

`LLLandmarksPanel::doCreatePick()` has the target landmark's region UUID but
used global-position modulo 256 for the `RemoteParcelRequest` local position.
It now resolves a loaded target region and uses its origin-relative position;
if the region is not loaded, it retains the prior 256m fallback because no
authoritative origin is available in this code path.

`tasia_region_compat_test.py` passed with assertions for target-region lookup
and region-relative conversion; world-map-size contracts passed 4/4,
OpenSim-compatibility checks passed, and `git diff --check` passed. No Windows
build or live landmark/pick request was run.

## 2026-10-21 variable-region SLURL place coordinates

Read-only comparison with the reference showed that a non-teleport SLURL
preserves the resolved region origin in `ox`/`oy`, and the place panel subtracts
that origin to form local coordinates. Current source dropped the origin and
always used modulo 256, wrapping positions in larger regions. Restored the
origin through `LLURLDispatcher` -> `LLPanelPlaces` -> a
`LLPanelPlaceInfo::displayParcelInfo()` overload. Missing `ox`/`oy` keeps the
existing path; standard 256m regions use equivalent origin-relative coordinates.

`python3 indra/newview/tests/tasia_region_compat_test.py` passed with source
wiring assertions for origin emission, dispatch and subtraction; world-map-size
contracts passed 4/4; OpenSim compatibility tests passed; `git diff --check`
passed. These are source/harness checks only. Windows build and live SL/OpenSim
URL/place-details/teleport behavior remain unverified.

## 2026-10-21 variable-region allocation ceiling correction

Compared the viewer region bound to checked OpenSim `Constants.cs`:
`MaximumRegionSize=4096`, `MinRegionSize=256`, terrain patch size 16. The viewer
had accepted square power-of-two dimensions through 8192. `LLSurface::create()`
allocates `(width+1)^2` `F32` heights plus the same count of three-F32 `LLVector3`
normals. At 4096m the two arrays total 268,566,544 bytes (~256MiB), excluding
patch/renderer allocations. `LLVLComposition` inherits `LLViewerLayer`, which
allocates another 4096^2 `F32` values (~64MiB), so the known terrain/composition
arrays total about 320MiB before patch/renderer resources. 8192m would allocate
~1GiB in height+normal arrays alone. The viewer region/map/parcel bound and diagnostic now cap at 4096,
and the focused policy harness checks 8192 rejection while retaining 4096.

`python3 indra/newview/tests/tasia_region_compat_test.py` passed. This is a
source/policy correction only; no terrain allocation was attempted, no Windows
build or live large-region crossing was run, and 4096m remains a substantial
memory/stress-test risk.

## 2026-10-21 Step 9 follow-up — per-region OpenSim say range

Checked server `ChatModule.RegionLoaded()` emits `say-range` for each region,
with an integer configured in the Chat section and default 20m. The legacy
`LLLocalSpeakerMgr::updateSpeakerList()` uses that range for both nearby-speaker
collection and text-speaker removal. Current source used fixed
`CHAT_NORMAL_RADIUS` (20m) in both places. Current source now stores a validated
per-region range and applies it only to that list; the default and Second Life
behavior stay 20m. The current `NearChatRadius` user preference for chat-color
fading is intentionally not overridden. Whisper/shout visual behavior is not
implemented by this change.

`python3 indra/newview/tests/tasia_region_compat_test.py` passed, including
valid/default/SL-isolation/negative/oversized/non-finite say-range policy cases
and source wiring checks. `tasia_opensim_compat_test.py` passed. This does not
compile the changed speaker/region methods or verify live chat. Windows build and
runtime tests remain pending. Review-only Farm job `job_mvhzknsl_88045f84f1`
completed but returned an empty report with no reviewer messages; no independent
review findings or approval are claimed.

## 2026-10-21 Step 9 follow-up — negative-Z world-map parity

Continued the SimulatorFeatures audit against checked OpenSim emitters and the
read-only legacy `lfsimfeaturehandler.cpp`. The server advertises per-simulator
scale limits, simulation/terrain height bounds, grid/search/guide/currency URLs,
and the ChatModule's chat ranges. The legacy handler consumes several optional
extras, but current login/grid code already handles some URL services through
independent channels; unreferenced keys are not all proven regressions.

One narrow behavior loss was confirmed: OpenSim advertises
`OpenSimExtras.MinSimHeight` (`-100` in checked constants), the legacy viewer
stores it per region, and legacy world-map Z accepts values below zero. Current
world-map coordinate display and typed URL tracking had a fixed zero floor.
Current source now stores a finite OpenSim-only per-region minimum and applies it
to world-map Z display/typed coordinates. Second Life retains the 0..4096 policy;
other fixed altitude limits and per-region prim-scale handling are not changed.

`python3 indra/newview/tests/tasia_region_compat_test.py` passed (including the new
negative-floor, SL-isolation, malformed-value, upper-bound and say-range cases),
`python3 indra/newview/tests/tasia_opensim_compat_test.py` passed, and focused
`tasia_worldmap_size_test.py` passed 4/4, `tasia_sound_globe_test.py` passed 4/4,
and `git diff --check` passed. Review-only Farm job
`job_mvhzcqog_99a2a1c968` failed before a worker started (HTTP 429), so independent
review is unavailable; no review result is claimed. These source/harness checks do not compile
`LLViewerRegion`/the floater or prove UI/teleport behavior. Windows build and live
SL/OpenSim map tests remain pending; no credentials or live viewer/cache were
used.

## 2026-10-21 Step 9 audit — OpenSim prim-scale limit parity gap

Read-only source comparison confirmed that checked OpenSim emits per-region
`OpenSimExtras` values `MinPrimScale`, `MaxPrimScale`, `MinPhysPrimScale`, and
`MaxPhysPrimScale` from `Scene.AllModulesLoaded()`. The legacy viewer's
`lfsimfeaturehandler.cpp` consumes min/max nonphysical and max physical scale via
global `LLWorld` setters. At this initial scale audit, before the height follow-up
above, the current viewer had no `OpenSimExtras` consumer; it now consumes only
`MinSimHeight` for world-map parity. Current `get_default_max_prim_scale()` returns fixed
64m/10m constants, `LLPanelObject::refresh()` uses that result, and minimum scale
is fixed at `MIN_PRIM_SCALE`.

This is a verified viewer parity gap, not a server issue. Additional fixed-limit
paths are manipulator drag/stretch, object properties, size clipboard paste, and
model upload preview. `selectGetSameRegion()` is enforced for linking; its caller
does not establish that scale selection is single-region. No scale-related
code/test was changed:
server values are per-region but current edit constraints are global. Applying
feature callbacks to global `LLWorld` fields would make limits depend on which
region response arrived last. A safe port needs object/region-resolved limits
through all relevant edit paths. Max altitude, heightmap and other extras remain
separate audits. This scale audit was source inspection only; no scale build or
runtime test was performed. Tasia Farm's audit review corroborated the central source findings;
the returned report was truncated, so no broader approval is claimed.

## 2026-10-21 baseline limitation and viewer QUIC keepalive fix

Baseline attempt: `cmake -S indra -B /tmp/opencode/tasia-kirsten-baseline-20261021
-G Ninja -DCMAKE_BUILD_TYPE=Release` failed before generating a build because
`indra/CMakeLists.txt` sets the Windows generator platform to `x64`, which Ninja
does not support; CMake then reported no C++ compiler configured. This Linux
environment has no MSBuild/Visual Studio compiler, so a Windows baseline remains
blocked. This is not a successful baseline build.

Compared the viewer stream parser with checked OpenSim `PacketFraming.cs` and
`QuicClientConnection.cs`: OpenSim writes a 4-byte all-zero keepalive marker every
30 seconds by default; the viewer previously rejected its zero-length payload and
failed the QUIC route. Patched `indra/llquic/llquicstream.cpp` to consume this
marker and added a byte-fragmented/coalesced production parser regression case in
`indra/newview/tests/tasia_quic_transport_test.py`. The focused QUIC suite passed
6/6 and `git diff --check` passed. No Windows build, live session, or 300+ second
idle test yet. The legacy reference is read-only; its parser left an isolated
zero-length frame in the payload phase, risking an empty packet when later data
arrived.

Parity follow-up: restored the legacy retry preference (default 1, bounded 1–10)
to `settings.xml`, passed it through `lltasia_quic.h`, and added fresh-object
retries for synchronous connection-start failures in `LLMessageSystem`. This does
not retry asynchronous TLS/QUIC handshakes; existing readiness timeout and UDP
recovery remain unchanged. Added a compiled harness around the production
`enableQuicCircuit()` method; it verifies successful retries, exhaustion and the
1/10 clamps. The focused source checks also assert endpoint setup ordering for
EnableSimulator, TeleportFinish and CrossedRegion. Removed the inert fallback checkbox from English/Polish
Preferences and replaced its misleading behavior description with localized
information that LLUDP recovery is automatic when possible. Settings LLSD key
uniqueness and English/Polish Preferences XML checks pass. The focused QUIC suite
also checks both localized preference panels. Tasia Farm review job
`job_mvhyb8ha_9d988b15dd` failed before starting a worker (HTTP 429); parity-review
job `job_mvhyg4vn_25395be4dc` completed without a usable report. A subsequent
acceptance review (`job_mvhyir2y_18b4822064`) found a missing standalone keepalive
case; that test was added. Re-review (`job_mvhyjo3k_a44aeaecc3`) confirmed the
keepalive cases and reported no actionable findings in tested scenarios; retry
logic was not exercised in that review, so the new compiled harness still needs
independent review. No Farm edits occurred.

## 2026-10-21 QUIC end-to-end audit — source/config only

Completed a read-only comparison of the viewer QUIC implementation, checked
OpenSim QUIC source, and live i-Grid INIs on `i.let-us.cyou`. The ROBUST config
enables `[QuicProxy]` on 9001 with ALPN `opensim-ll/1`. I initially checked the
shared base simulator INI and incorrectly concluded all sims used 9001. The
running simulator processes instead load `./Regions/<region>/Opensim.ini`; these
mounted per-region INIs assign distinct backend ports and advertise the proxy on
9001. For example, Blue uses 9010 and Grid_Welcome uses 9033. Only selected
QUIC/network settings were read; no credentials or unrelated INI values were
exposed.

I withdrew the initial same-port diagnosis in `OPENSIM_PROTOCOL_AUDIT.md`. Host
UDP sockets confirm the proxy on 9001 and several sim listeners on their distinct
ports, matching the checked source's proxy/backend contract. One unresolved
exception: `Tasia_Magic_School` was running with per-region port 9036 but had no
9036 UDP socket in the snapshot. The deployed LindenUDP assembly timestamp/hash
is recorded in the audit, but does not identify its source revision. Docker,
application, and captured tmux output had no QUIC startup/registration trace. No
production config, service, viewer, cache, or server binary was changed; no server
build or live viewer QUIC session was run.

The viewer and checked server agree on ALPN, 4-byte big-endian stream framing,
`UseCircuitCode` association, and `quicready` ordering. Viewer focused QUIC tests
passed 4/4 (routing/readiness, negotiation guards, framing and localized status).
Found a theoretical frame-size/buffer boundary (server 64-KiB buffer includes a
4-byte header while server cap is 64 KiB); ordinary viewer packets are capped at
8 KiB, so this is not established as the reported failure cause. Next action:
investigate the unbound 9036 listener and correlate a QUIC trace with the affected
region/session before proposing viewer changes. No code edits or test changes were made.

## 2026-10-21 local Farm limits and read-only audit setup

Ran the updated Farm locally on `127.0.0.1:4678`; the separate remote Farm was not
changed. Its OpenCode worker is pinned to `opencode/nemotron-3.5-lightning-free`
(the local OpenCode catalog reports 262,144 context/output tokens and zero cost),
with one worker/job at a time, no peer or reviewer calls, a 12-step agent cap, a
150-second worker deadline, and a 180-second job deadline. The V2 provider docs
give five-minute header/chunk timeouts and up to three retries, but this Farm's
shorter job/worker deadlines preempt those defaults. OpenCode Zen publishes no
numeric RPM/concurrency quota for its free models; its free offer is temporary,
and the selected NVIDIA trial model may log prompts for service improvement.
Only the curated public source snapshot was sent; it has 75 files, no private-key,
common API-token or bearer-credential pattern matches, and no config/key filenames.

Worker smoke test passed. The inventory comparison completed; key source claims
were checked locally and recorded in `OPENSIM_PROTOCOL_AUDIT.md`. A broad QUIC
comparison exceeded the 150-second worker deadline; a narrower comparison
completed and its material findings were locally checked. The report incorrectly
claimed an old-viewer LLUDP retransmission difference, which was rejected after
checking both trees (both skip retransmission for QUIC). Farm `npm test` passed
28/28 after adding the 12-step cap. No viewer build, live packet capture, viewer
source edit, live viewer action, or cache operation occurred.

## 2026-10-21 OpenSim inventory mutation audit (design only)

Resumed the protocol ledger's Step 3. Compared the current AIS-only helpers with
the old viewer and the checked OpenSim server. The old client sends legacy item,
folder and purge messages, then optimistically updates local inventory/callbacks.
The server's purge handler queues `PurgeFolderAsync` and reports failure only in
server logs; there is no application-level purge-completion reply. Therefore the
old purge-then-delete callback-destructor chain can race server completion. Also,
unknown folder version/count yields `CHILDREN_MAYBE`, but the old purge path still
proceeds unless children are definitely absent.

Updated `OPENSIM_PROTOCOL_AUDIT.md` and aligned `plan.md` on the safe next action:
design and test bounded purge confirmation, callback/cache behavior, protected and
incomplete folders, and failure/timeout before porting. **No viewer C++ or runtime
state changed; no new tests or builds run in this design pass.** Live viewer/cache
remain untouched.

Follow-up contract review found a hard protocol limitation: the legacy
`InventoryDescendents` reply has no request ID, so even a post-purge empty response
cannot be conclusively correlated with the purge/read-back attempt; repeated polls
only reduce, not eliminate, ambiguity. Current reply handling also upserts without
removing stale cached children. The audit now blocks automatic parent deletion
unless server ordering/version semantics or a correlated completion signal can be
verified. Pawan's one-request review confirmed facts but its proposed section was
truncated at the Farm output cap. Audit-ledger edits only; no viewer source change,
new test, or build.

## 2026-10-20 Profile partner link display

The partner row no longer inserts the generated app SLURL into the visible text.
It resolves the partner UUID through `LLAvatarNameCache`, shows the display name,
and keeps `LLSLURL("agent", partner_id, "inspect")` as the clickable target. The
target remains generated by the active grid's app-SLURL base, preserving SL and
OpenSim routing. Added stale-callback protection and English/Polish label strings.
Focused profile regression tests passed 4/4; English and Polish XUI both parse;
`git diff --check` passed. Acer Windows x64 Release build of `Kirstens-S24`
completed with exit 0 and zero compiler/linker error markers. It reported four
warnings: `MSB8065`, two `LNK4006` duplicates, and `LNK4088` (`/FORCE`). EXE SHA256
`9c6b9e3261b3349f7c89408d6f1461c22c5c107a772a25dedd59741c90113d77`; PDB SHA256
`216f267d4b4a3ab2dd8727aa14fdbe048519ccc19ddf860dbeffbe0b5ab8caf8`. Build receipt
and log: `F:\tasia-kirsten\verified-builds\profile-partner-link-20261020-01-build.json`
and `.log`; source backup:
`F:\tasia-kirsten\source-backup-profile-partner-link-20261020-01`; prior binaries:
`F:\tasia-kirsten\verified-builds\profile-partner-link-20261020-01-before`.

Isolated portable package:
`F:\tasia-kirsten\test-builds\profile-partner-link-20261020-01` (7,307 manifest
entries SHA256-verified, 7,309 files, 58 CEF `.pak` files, package EXE matches
build receipt, package-local shortcut verified, no private profile). Package
receipt: `F:\tasia-kirsten\verified-builds\profile-partner-link-20261020-01-package.json`.
The ZIP is 396,624,998 bytes, SHA256
`98479c587a134a7b6ebaaf926358f2e14a8c1a59a7962dfd4790b911da157c9d`; uploaded
anonymously to storage.to, expires 2026-10-27:
https://storage.to/ONJOYX7J3 . Launched the packaged EXE in Acer's active console
session 1 through a temporary interactive scheduled task. After 120 seconds,
process 97168 remained running and Windows reported `Responding=True`; its window
title was empty. The launcher task was removed. Portable first-run data was created
inside the Acer test package directory; the uploaded ZIP remains the pre-launch,
profile-free package. This verifies process startup/sustained responsiveness, not
visual login or live SL/OpenSim functionality; no grid test has occurred. `LNK4088`
remains in the build log despite this successful startup observation.

## 2026-10-20 live SL appearance/UI report

Mom reported intermittent grey avatar heads/clothing while online in Second Life,
then reported the viewer top bar was broken. Captured the Acer primary display
read-only at `/tmp/opencode/profile-partner-link-20261020-01/acer-screen-20261020.png`
(SHA256 `8c665ff8fd5b15dfa53efe7212a9ed3eda76e428fcae8e5ff68e4c662d2a1157`). The
menu labels across the viewer's top bar visibly overlap. A Windows Terminal window
covers much of the lower scene in the capture, so the avatar texture state cannot
be assessed reliably from this image. Viewer PID 97168 remained in session 1 and
reported `Responding=True` after capture. No viewer/cache changes, restart, or
source edits were made. Cause remains unknown; current viewer log was empty and
CEF only showed invalid SPDY stream warnings. Keep the live session intact; next
diagnostic should capture unobstructed screen/runtime evidence before code changes.

Follow-up source review (unbuilt): `LLStatusBar::reshapeToFitContent()` called
`setRect()` (which automatically reshapes right-anchored children) and then
translated those children again. The old layout fixture did not model that viewer
behavior and falsely passed; after modeling right-anchor movement, it failed on
out-of-bar child positions. The routine now snapshots/restores child rectangles
with one intentional fit shift; the corrected fixture passes. Review also found
`LLViewerObject::refreshBakeTexture()` had dropped the prior SL baked magic-ID
mapping while adding real bake UUID mapping; restored the magic-ID route and added
a focused fixture covering both routes. `tasia_status_bar_layout_test.py`,
`tasia_bake_refresh_test.py`, `tasia_opensim_compat_test.py`,
`tasia_bake_upload_test.py`, `tasia_host_bake_fetch_test.py`, and
`tasia_texture_http_diagnostic_test.py` pass. Source is not built or live-tested;
these fixes have not been deployed to the active SL session. Cause of the reported
face/hair/clothing texture failures is still not confirmed; current asset/texture
logs provide no fetch evidence. Do not restart or clear cache while Mom is online.

## 2026-10-20 OpenSim MapBlockReply size extension

Restored the reference viewer's optional `MapBlockReply.Size` block in the active
message template and prehash table. On OpenSim only, the world-map parser reads
 matching `SizeX/SizeY` blocks, bounds dimensions to aligned 16..4096m values, and
passes them into `LLSimInfo`, tracking bounds and map drawing. Missing/mismatched
blocks, invalid dimensions, Second Life replies and other legacy insertions retain
256m defaults. This does not alter terrain's separate supported-shape policy.

Verification: region compatibility test passed (including SL size isolation),
world-map size contracts 4/4 passed, protocol comparator passed 12,000 synthetic
packets and now reports only current-only `RezScript.NewScriptInfo`; `git diff
--check` passed. An incremental Windows x64 Release build of
`Kirstens-S24` completed with exit 0 and zero compiler/linker error markers. It
reported five warnings: `MSB8065` for the prebuilt marker, one template-verifier
warning (master-template compatibility download skipped; syntax check passed),
two `LNK4006` duplicate symbols, and `LNK4088` (`/FORCE`). EXE SHA256
`14c70e9d27d7b8b3bf2652403a6b41d662122284bd44e3e92ce533d7a3527808`; PDB SHA256
`f3a6cfef0cb154199165c93ba5979878793d8cf8899023d74d3e3765f91f25a4`. Receipt/log:
`F:\tasia-kirsten\verified-builds\mapblock-size-20261020-01-build.json` and
`.log`; source backups are in
`F:\tasia-kirsten\source-backup-mapblock-size-20261020-01`, prior EXE/PDB in
`F:\tasia-kirsten\verified-builds\mapblock-size-20261020-01-before`. The build
was then packaged in isolation at
`F:\tasia-kirsten\test-builds\mapblock-size-20261020-01`. All 7,307 manifest
entries passed SHA256 checks; 7,309 files total, 58 CEF `.pak` files, package EXE
matches the build receipt, the package-local shortcut points to its own EXE, and
no private profile was packaged. Package receipt:
`F:\tasia-kirsten\verified-builds\mapblock-size-20261020-01-package.json`.
Neither package nor viewer was launched; `LNK4088` means runtime launchability is
not established. No deployment or live SL/OpenSim map test occurred; unrelated
working-tree changes were left untouched.

## 2026-10-20 follow-up: isolated profile-customization build staged

Compared the complete profile/nametag implementation with
`/home/marty/opensim/tasia-viewer-2026`. The current branch had identity aliases
and tag colors, but omitted the custom nametag title and profile account-area
badge/profile text/tooltip/image rendering. Ported those UI behaviors while
retaining the current grid+UUID-scoped catalog and strict HTTPS-only, bounded,
no-redirect remote image fetch. Added account-area badge controls and a fetched
texture setter to `LLThumbnailCtrl`.

Verification: profile customization contract test added; OpenSim services/profile
suite 14/14, identity/grid 4/4, name preservation 4/4, English profile/menu and
Polish menu XML parse, and `git diff --check` pass. Acer Release build retry02
completed with exit 0 and zero actual compiler/linker errors. EXE SHA256
`771b98ff2c0f0835ee6ddc410dc502978595da9f4fe6584e251e80d825eb766a`; PDB SHA256
`b609e683bd22abde0bc76dbaa148e9b09349b4ae34840a5cd5f282bf5cc5e267`. MSBuild and
linker warnings remain, including `LNK4088` (`/FORCE`); no runtime launchability
claim is made.

Staged isolated portable package
`F:\tasia-kirsten\test-builds\profile-customization-20261020-01`. Manifest has
7,307 entries and all hashes verified; it includes 58 CEF `.pak` files, portable
marker and package-local shortcut. No `TasiaViewerData` profile exists yet.
Build receipt/log: `F:\tasia-kirsten\verified-builds\profile-customization-20261020-01-retry02-build.json`
and `.log`. Prior source and binary backups remain in their task-specific
`source-backup-*` and `verified-builds\profile-customization-20261020-01-before`
directories. Mom launched and closed this isolated package; its portable profile
now exists. Exactly the two desktop folders (`tasia-os`, `tasia-sl`) and their
profiles remain untouched. Profile/audio behavior, launchability, and the separate
simulator circuit timeout are not verified/fixed.

### Mom's live follow-up and read-only log findings

Mom reports that the QUIC icon was green briefly, then changed to an unreliable/
UDP state; disabling verification led to UDP. She also reports the avatar/mesh
did not load, the profile still does not work well, the rank/title `I-Grid Admin`
should be under the username, and tag coloring is now improved.

The isolated log at `TasiaViewerData\logs\TasiaViewer-LOG.log` records one QUIC
fallback at `2026-10-20T11:16:37Z`: "QUIC connection ... established but the
simulator did not confirm the session"; the circuit then recovers over UDP. No
certificate/TLS error or successful quicready event appears in this log. The
isolated profile saved `TasiaQuicVerifyCert=0` at shutdown (`11:18:23Z`), after
the logged fallback; this does not prove when the setting changed relative to
QUIC initialization. `TasiaQuicEnabled` was not explicitly saved and therefore
uses its packaged default. In source, the status icon is green only for a ready
circuit. The log does not corroborate the reported green interval, so investigate
possible UI/session timing rather than silently equating green with initialization.

The same run recorded 848 mesh-header HTTP 404s and six asset HTTP 404s; self
legacy appearance was accepted and shape/skin/hair/eyes were present, but
`local_appearance=0`. Mesh/asset requests use HTTP capability endpoints, not the
QUIC circuit; these 404s are independent evidence and do not establish QUIC as
their cause.

Profile route audit: viewer uses `AgentProfile` HTTP when the capability is
advertised, and falls back to legacy `AvatarPropertiesRequest` only when the cap
is absent or the HTTP response is unsuccessful. The checked i-Grid source has
legacy profile/interests/groups replies plus generic classifieds, picks and notes
request routes; the viewer contains handlers for each. No profile request/result
was logged in Mom's run, so the live route and reason for the remaining floater
problem are unknown. Do not assume the inspected server tree is identical to the
deployed service.

Updated local nametag source to place the configured custom rank/title after the
username; it uses the existing configured tag color. This was included in the
isolated build recorded below.

### Follow-up: compare and restore supported OpenSim legacy profile endpoints

Compared old viewer (`/home/marty/opensim/tasia-viewer-2026`) with current viewer
and checked i-Grid server source. The old tree had decoded Replies for Groups,
Picks and Notes, while current source registered some packet handlers but left
Notes/Picks as no-ops and only logged Groups. Current source now decodes/notifies
for all three. Picks/Notes tabs use their supported generic requests when on
OpenSim without `AgentProfile`; own-picks quota tracking uses the same Picks route.
The tab state previously got pre-marked “started” by the parent even though these
tabs were lazy, suppressing their requests; parent no longer marks Picks/Notes as
started. Added legacy NotesUpdate for the matching server handler. Generic
requests dedupe at the common send boundary. The classifieds list panel only handles
list replies, leaving classified-detail replies to their detail observers.

The inspected OpenSim module does not register `avatargroupsrequest`; it sends
AvatarGroupsReply with AvatarPropertiesRequest (through its EventQueue path), so
no unsupported standalone group request was copied. Legacy Interests reply remains
consumed as a no-op because current UI has no such panel.

Did not port full-record `AvatarPropertiesUpdate` without a safe snapshot of all
fields: its wire format can replace both profile descriptions and image IDs, with
AboutText limited to 512 bytes and First Life to 255. A partial/oversized save
fallback could erase or truncate untouched data. Profile text/image/hide-age legacy
write support therefore remains explicitly unimplemented pending a safe design.

Added low-sensitivity diagnostics for profile route choice, HTTP status/ID match,
parsed data counts, legacy reply receipt and tab completion. No capability URLs,
profile text or UUIDs are emitted by the new markers. Focused checks now pass:
services/profile 15/15, identity/grid 4/4, name preservation 4/4, and
`git diff --check`. The C++ changes are included in the isolated build below; live
protocol behavior remains unverified.

#### Authorized isolated Windows build and package completed (not launched)

Mom authorized a new isolated candidate and use of the stored Acer credential. The
credential was loaded from the encrypted vault directly into the SSH child process;
no secret was printed. Compared nine local source files against Acer's exact source
baseline, then verified the transferred archive SHA256
`d9bddb9c825eb22560d53355de5b7919e59c560294998f4235f94435b5dfe068`, backed up all
nine originals, and applied only after hash checks. Source backup and apply receipt:
`F:\tasia-kirsten\source-backup-profile-legacy-udp-20261020-01`.

One Release build completed: exit 0, compiler/linker error markers 0. Four warnings:
`MSB8065`, two `LNK4006` duplicate-symbol warnings, and `LNK4088` (`/FORCE`). EXE
SHA256 `771eb85cca46af03db44873316de7cf0f8636e9038996ba4f41856a18f3776df`; PDB
SHA256 `b3adae354fbfadca17e4b434bf080e2ba610a23d5fbbb807c2b33883652dd64e`. Build
receipt/log: `F:\tasia-kirsten\verified-builds\profile-legacy-udp-20261020-01-build.json`
and `.log`.

Staged isolated portable package at
`F:\tasia-kirsten\test-builds\profile-legacy-udp-20261020-01`. Mom correctly
flagged that the first package was incomplete. Comparing it with the prior runnable
candidate (excluding that candidate's runtime-created private `TasiaViewerData`)
identified 10 missing runtime dependencies: `fmod.dll`, `vccorlib140.dll`, five
`msvcp140*.dll` files, and three `vcruntime140*.dll` files. Added these from the
repository's `indra/newview` and `indra/Misc` sources only after checking all source
SHA256 values, then regenerated the package manifest. Updated
`scripts/Package-TasiaViewer.ps1` to stage and require those dependencies for future
packages; backed up the prior Acer copy at
`F:\tasia-kirsten\source-backup-profile-legacy-udp-20261020-01\scripts\Package-TasiaViewer.ps1.before`.

All 7,307 manifest entries now pass SHA256 verification; 7,309 files total, 58 CEF
`.pak` files, package EXE matches the build, all 19 required package paths are
present, portable marker and package-local shortcut verified, and no private profile
bundled. The updated package script parses with zero PowerShell errors. Package receipt:
`F:\tasia-kirsten\verified-builds\profile-legacy-udp-20261020-01-package.json`.
The prior package-verification pass did not launch it. Old isolated candidate and
`tasia-os`/`tasia-sl` installs and profiles were not changed. No Git push was
performed. Mom subsequently launched the candidate; see the runtime follow-up below.

#### Mom's isolated runtime screenshot/log follow-up

Mom launched the corrected portable candidate and reached the world. Screenshot
confirms the custom `I-Grid Admin` line is now below the account username. It also
shows a blue triangulation overlay across the scene. Read-only inspection of the
candidate's fresh `TasiaViewer-LOG.log` confirms no missing-DLL marker, but finds no
profile-floater start, route-selection or legacy-reply diagnostics yet, so the
profile tabs have not been exercised in this log. The log has 1,409 mesh-related
HTTP 404 hits and a QUIC session fallback. These facts are kept independent; the
log does not prove QUIC caused the mesh 404s. No wireframe marker appears in the
log, so the screenshot overlay's cause is not yet identified.

After Mom reported a disconnect, read-only review of the log tail found no simulator
ping for 100.633 seconds; the circuit was marked dead and dropped at `13:24:39Z`.
No fatal/crash marker was found. QUIC had fallen back to UDP at `13:23:00Z` because
the simulator did not confirm the QUIC session; this precedes the later circuit
timeout but does not establish causation. The 404s are HTTP capability failures and
remain separate. Viewer process was no longer running at inspection. Next: after
reconnect, open the own/other profile and Groups/Picks/Notes tabs; inspect that new
session's route/reply and circuit timeline. Desktop installs and profiles remain
untouched.

#### Broad compatibility and transport audit checkpoint

On 2026-10-20, corrected stale entries in `OPENSIM_COMPATIBILITY.md` after
reviewing the current production-source tests. Host-bake keeps the HTTP ViewerAsset
route; wearable legacy transfer, local bake/upload and appearance sending have
source-level harness coverage. QUIC has an actual `LLMessageSystem` integration
and deterministic routing/framing tests, although the candidate's simulator
`quicready` handshake still failed and its cause is not established. None of these
harnesses proves a successful GPU render, live appearance, or live QUIC session.

Verification: 57 discoverable `tasia_*test.py` tests passed when the protocol
comparator was excluded from generic discovery (it requires a reference path);
the comparator passed against `/home/marty/opensim/tasia-viewer-2026`, including
12,000 synthetic packets and the 26 critical message definitions. `git diff
--check` passed. No Windows build, Windows DPAPI store executable, login GUI
interaction, or fresh simulator session was tested. Next: audit Grid Manager
discovery/metadata and the full SQLite remember-password UI flow; keep legacy
credential encoding out. Continue QUIC work only with a fresh session trace.

The old viewer's actual QUIC source was then compared directly at
`/home/marty/opensim/tasia-viewer-2026`: it waits for `quicready` without a
five-second readiness timeout, accepts the control message without transport
provenance checking, and disables failed QUIC circuits rather than falling back
to LLUDP. Current code intentionally tightens provenance and adds UDP recovery,
but the fixed five-second timeout is new. The checked server sends its native
confirmation over the attached QUIC transport, and the bridge forwards responses
over the viewer QUIC stream. No timeout/guard edit was made: the candidate log
proves lack of accepted confirmation, not whether the server failed to send it,
the packet arrived via UDP, or confirmation was delayed beyond five seconds.

The OpenSim source comparison also exposed a missing legacy inventory read path:
the active viewer lacked the old no-cap UDP fallback for item/folder fetches and
the two matching reply handlers, although the server supports those messages.
Restored OpenSim-only `FetchInventory` and `FetchInventoryDescendents` fallback,
plus grid/AgentID-checked `FetchInventoryReply` and `InventoryDescendents`
handling. HTTP/AIS remains preferred and SL behavior is not changed. The compiled
production-method fixture and OpenSim services/profile and identity/grid suites
pass. Delete/purge/move fallbacks remain audited but intentionally unported; there is no full
Windows build or live inventory test yet.

Follow-up old/current comparison confirms the mutation gap: old viewer sends UDP
`RemoveInventoryItem`, `RemoveInventoryFolder`, and `PurgeInventoryDescendents`
when AIS is absent; active helpers only warn. The inspected server maps these to
inventory-service deletion, with folder purge asynchronous. No code was changed
because the old client optimistically removes local objects/fires callbacks and
chains purge then folder delete without acknowledgement; restore only with focused
failure/callback/model tests. Also retained the active wearable-asset fallback's
narrow OpenSim body-part/clothing scope (missing cap or HTTP 404/410, stale-session
guards) rather than the old viewer's generic non-SL UDP fallback.

Created `OPENSIM_PROTOCOL_AUDIT.md` as the ordered delta checklist. Re-ran the
actual-template comparator against `/home/marty/opensim/tasia-viewer-2026`:
12,000 synthetic packets pass, 483 messages per template, 26 critical definitions
match, no unique message IDs; exact field deltas are reference-only
`MapBlockReply.Size` and current-only `RezScript.NewScriptInfo`. Checked server
`SendMapBlock()` includes size pairs for map entries above 256m, while active
template/parser/map insertion omit them. Next focused step is to carry this
optional field through bounded world-map geometry; no code changed in this audit
pass. Full build and live map test remain outstanding.

## Historical: 2026-10-20 local profile/audio changes (pre-build status superseded)

Mom clarified the stuck floater is her OWN profile and its customization is her
own entry in the remote `config.json`. Local source now handles OpenSim legacy
replies in Picks/Notes so unsupported tabs finish loading without inventing fields;
refreshes an already-open self profile after the identity catalog applies; and adds
an EN/PL Avatar/Movement-menu `Stop Music` action without changing status-bar
geometry. Stop explicitly halts parcel streaming and the standalone player.

Focused checks: OpenSim services/profile/menu 13/13, identity/grid 4/4, name
preservation 4/4, and `git diff --check` pass. No Windows build or live confirmation
yet. The simulator circuit timeout is separate and unresolved. Preserve all current
profiles/packages; any new isolated test build needs Mom's go-ahead.

## 2026-10-20 follow-up: profile loader and music Stop report

Mom reports the profile window stays loading, followed by an in-world drop; the
screen still shows old customization and no visible music Stop. Read-only log review
of `combined-avatar-status-identity-20261020-02` found no profile exception/fatal
or crash diagnostic. It did record the simulator circuit timing out after roughly
100 seconds, dropping, and a later LogoutRequest on an unknown circuit, followed
by subsystem cleanup. This is evidence of circuit loss, not proof the profile
opened the disconnect or that a native crash occurred; no Windows dump/event log
was collected. Avatar readiness samples show all four required parts, but
`local_appearance=0`, which does not verify profile identity/customization.

Source review: status-bar `media_toggle_btn` only toggles `setAllMediaPaused()`;
parcel-audio Stop is a distinct action in Nearby Media (`onClickParcelAudioStop`).
The current source therefore has no direct status-bar music Stop action. No source,
profile or package was edited/relaunched for this report. Next: investigate the
OpenSim profile reply path and the circuit timeout independently; make the Stop
affordance explicit in a focused change. Rebuild only the isolated candidate after
it is closed and with Mom's test consent. Build/fixture evidence is not runtime
verification.

## Combined appearance/status/identity Windows build; Mom's live test findings

Receipt `combined-avatar-status-identity-20261020-014906.json`: build exit 0,
zero errors, 22/22 transferred source hashes matched, and fresh output hashes
recorded. EXE SHA256:
`C6FCBBE0BEBB3F44FDC03561ADF7E76F9CA1B9654E7D381E00A156D7FCFE8014`; PDB SHA256:
`C1C317F42C4F6DE035ACADD29545C506D077C965E799FE9EB13E3AAF32A960E1`.
A first isolated package (7,297 files, correct EXE hash) failed to launch with
Windows status `0xC0000135` (missing DLL); no viewer process or profile was created.
It is preserved but not runnable. A second isolated package at
`F:\tasia-kirsten\test-builds\combined-avatar-status-identity-20261020-02` was
launched in the interactive session using root DLLs copied read-only from
`opensim-services-ui-20261019-01`. No source-package executable, settings or profile
was copied; source EXE hash remained unchanged. Launch receipt:
`/tmp/opencode/acer-launch-combined-test-02.json`. Process PID91900, session1,
Responding=true; fresh isolated profile directory created. Main window title was
blank at last check, so Mom must confirm the visible login/UI. No media provider
keys were injected. Existing desktop folders/profiles were not changed.

Follow-up read-only diagnosis of the current test log (00:35:25Z–00:40:27Z) found
repeated HTTP texture/asset and mesh-header 404s, COF outfit selection, a server
"does not support baking" notice, and one successful local bake upload/legacy
appearance send. No QUIC/TLS/certificate log lines were present. The current test
profile stores `TasiaQuicVerifyCert=0`; `TasiaQuicEnabled` is absent and uses the
packaged default ON. This conflicts with Mom's report that cert verification was
enabled during the report, so no certificate-root conclusion is justified. The
current candidate is not a verify-on test; Mom should toggle the checkbox and
restart before testing certificate validation.
I-Grid source/docs show per-region QUIC endpoints and a server-side self-signed
`CN=localhost` fallback only when the configured cert is missing; live deployed cert
and endpoint remain unverified. Asset/mesh/bake HTTP capability paths are distinct
from the simulator's QUIC LLUDP circuit.

Comparison with `/home/marty/opensim/tasia-viewer-2026` found a nametag gap: when
SL display-name mode is off, the current build ignored explicit `username` aliases.
Added a separate username line while preserving the real legacy full name and
website-only `page_name` rule. `python3 indra/newview/tests/tasia_identity_name_preservation_test.py`:
4/4 PASS. This source fix has NOT been rebuilt or launched yet.

Mom authorized provisioning the stored GIPHY/KLIPY credentials. The isolated
candidate now stores only a same-user DPAPI-encrypted payload under
`TasiaViewerData\secrets` (342 bytes) with protected ACL and executable hash pin.
The authoritative launch script passed `-ValidateOnly`; package-local shortcut now
uses it. Manifest was rebuilt to 7308 entries, excluding private profile and mutable
`.log` files. Key-enabled launcher started PID91180/session1, Responding=true;
receipt `/tmp/opencode/acer-launch-media-combined-test-02.json`. This verifies the
launch/injection path only, not actual provider API or picker functionality. The
test profile still has QUIC certificate verification off, so a verify-on test needs
Mom to enable it and restart.

### Local FTP access check

Read-only check of `192.168.55.3:2130` identified Xlight FTP 3.9. Anonymous
login with an empty password succeeds. The root lists `c`, `e`, and `f`; the
`/f/tasia-kirsten` project directory is accessible. No file contents were
downloaded and no writes were tested. FTP `LIST` reports writable permission
bits, so treat this as sensitive: reachability from this client does not prove
an IP allowlist, and plain FTP provides no transport encryption. Verify the
Xlight/firewall source-IP restriction before relying on it beyond this LAN.

The earlier `wearable-transport-20261019-205859` attempt is not a successful
build receipt: its compiler/linker produced an EXE/PDB, but MSBuild exited 1 during
post-link staging. Its output must not be deployed. The four individual CMake
`copy_if_different` commands now pass a no-content-change probe against the same
Windows inputs/outputs. That attempt was also explicitly canceled before its
post-link work had fully completed.

All agreed local source fixes were staged in one backed-up Windows build:
wearable transfer, status-bar geometry, identity-field mapping, cache filename
safety, OpenSim mesh/asset capability routing, and background bake upload errors.
The transfer was limited to 22 source/UI/CMake files and records a pre-change
backup plus per-file hashes. Build receipt is
`/tmp/opencode/acer-combined-avatar-build.json`; do not deploy over either desktop
folder or touch its profile. The follow-up nametag fix is newer than that build.

The initial inline-payload wrapper stalled before creating a backup or changing
sources; it was stopped by exact PID after verifying no compiler was active. A
file-based source archive (22 files, SHA256
`466C769326B97EBE051025DFD92ABC38D136B8F324529C05C70ECB34EEF669D6`) has now
transferred successfully. A guarded remote apply-and-build job is running; it
backs up prior sources and EXE/PDB, checks each source hash, then invokes CMake.
Wait for its completion event; do not start a second build or deploy its output.
The first apply attempt exited before any backup/write because Windows PowerShell
reported the JSON array count unexpectedly; the archive was independently parsed
as 22 entries, the parser was corrected, and the guarded apply/build was relaunched.
Mom authorized launching a successful build for her test. After receipt/hash checks,
use a separate isolated test candidate; keep the two desktop folders and their
profiles unchanged. FTP is preferred for file transfer; it cannot launch the viewer.

Pre-build verification: combined regression sweep 29/29 suites passed, including
the actual status-bar/name mappings, wearable fallback, cache encoding, mesh URL
construction and upload-error recovery fixtures. These are fixture/unit tests, not
live grid/avatar proof. The prior Windows build's partial EXE is not a candidate.
Focused quick recheck: 16/16 related suites passed; `git diff --check` was clean.

## In progress: wearable build LTO; subsequent name/color and bar source fixes

Attempt wearable-transport-20261019-205859 compiled changed translation units and
entered link/code generation. Remote-tool timeout900s did not stop Windows build:
post-timeout inspection found cmake92040/link85604 active, no completion receipt.
Event-based observer waits for the EXISTING cmake process; no polling/rebuild.
See /tmp/opencode/acer-after-timeout-build-status.json and pending
acer-wait-existing-avatar-build.json. Do not claim this build passed yet.

Meanwhile Linux-only corrections (not transferred to active Windows snapshot):
shared config never uses website-only page_name as a name fallback. Mom clarified:
empty display_name/username preserves corresponding real names; explicit nonempty
fields override independently. Account type remains canonical; profile color
defaults restored when no match; native group titles preserved. Tests
name-preservation3/identity-grid4 PASS after clarification; services11 PASS before
the final label refinement.
Fixed status-bar parent/local coordinate mix and translated children on shrink;
actual layout test now PASS (was failing on old implementation). A subsequent
combined Windows build is required before these corrections can be deployed.
Running desktop PID87588 remains old B507A613... and must not be replaced/stopped
without Mom closing it normally/explicit consent.

## Runtime blocker after audit repair: hair/eyes missing (2026-10-19 18:52Z)

Compilation/package verification did NOT establish appearance success. Latest
desktop OS log confirms failed wearable downloads, required bodypart types2/3
requested1/found0, hair0/eyes0. Also1007 mesh-header and70 LOD HTTP404 responses,
texture404s and QUIC ready timeout with UDP recovery. Separate confirmed Windows
cache filename bug: unsanitized `i.let-us.cyou:8002` suffix prevents inventory-cache
rename. No fixes/build/profile/cache changes made yet for this report. Root cause
of the asset download failures remains open; detailed evidence/next action in plan.md.

## 2026-10-19 audit repair: build succeeded; launch script and XML were defective

Supersedes the older "build in progress" section below. Build4 succeeded; build5
had configure0/build0/errors0 but its post-check incorrectly required a newer EXE
even for an XML-only build. Current source audit found no missing C++ changes.
Fresh incremental `cmake --build F:\tasia-kirsten\build --config Release --target
Kirstens-S24 -- /m:8 /nr:false /v:minimal` returned0. EXE remains B507A613... .

Actual failures found: zero-byte Launch-TasiaViewer.ps1 in all inspected packages;
mixed 500px preferences panel overflowing a 440px tab, with undefined controls
confirmed in real logs. Repaired through a versioned launch script and separate
compact EN/PL Network and Media tabs. Old provisioning helper must NOT be reused:
it fails to set TASIA_LAUNCHER_B64 and accepts an empty script as syntactically valid.

Launcher+console selftest rebuilt with dotnet publish, exit0. Launcher SHA256
E326F955AD52A210BFF6A98BCE01DB3CD1504DDEFC1A4114EFFFA5EC3BBC6AE0.
Selftest13 PASS; production PowerShell chain tested with a compiled mock child:
both encrypted provider keys reach only the child; wrong hash/empty DPAPI rejected.
Real viewer was not launched, so no GUI or live-service success is claimed.

Deployment: desktop `tasia-os` and `tasia-sl`, plus canonical ui candidate repaired;
7311 manifest entries each checked, with mutable .log files excluded. Existing OS
profile/grid choice preserved. Remaining64 copies archived outside desktop at
C:\Users\Acer\TasiaViewerArchives\desktop-copies-20261019-203519; no profiles deleted.
Installed E:\TasiaViewerLatest unchanged. Source backups at
F:\tasia-kirsten\audit-repair-backup-20261019-203220; per-package repair backups
under the desktop-copies archive. Detailed receipts and next GUI action: plan.md.

Focused local verification: services11, QUIC4, sound/globe4, identity/grid4 PASS.
Last failing step was baseline hash verification of llplugin/slplugin_startup.log:
it is runtime-mutated, not executable provenance; excluded .log from package
manifests and continued only after all non-log, non-managed files matched.

## OpenSim services candidate (opensim-services-20261019-01) build in progress

Source applied to the Windows tree after a full remote inventory and review:
292 files verified, 23 changed and backed up under
F:\tasia-kirsten\source-backup-opensim-services-20261019-01, stage2 used to avoid
the earlier stale stage. Manifest
E093AFB15092827BAE361CC19DD1D1A8B70771F7178FB3A068F4096302DAB39E; archive
8221F43F808D5646F5E708EC64E844061FD18178451863BE620BD1B590B1C321. Prior EXE/PDB
preserved at verified-builds\age5-before-opensim-services. Installation and the
age5 package unchanged; no viewer was running at preflight and none was started.

Important correction to earlier notes: a manifest regenerated locally made the
remote tree look "different" from the recorded baseline. A file-by-file diff of
the remote copies showed the differences were exactly my own intended edits
(remote = age5 state, without these fixes) and no third-party modification.
The apply now backs up every differing file before overwriting it.

## OpenSim services + full grid list + bar fit (source ready, rebuild pending)

Mom's age5 live test: media keys absent, profile/config missing, only 4 grids,
wrong Buy L$/Marketplace, crowded status bar. Root causes and fixes:
- GIPHY/KLIPY were only supplied by the old one-shot test launcher; the package
  shortcut launched the EXE directly. Now a persistent DPAPI launcher supplies
  both keys per launch, with restricted ACL and a verified round trip.
- OpenSim without AgentProfile capability received no profile request; now
  falls back to legacy UDP properties with a defined-field mapping.
- Identity catalog moved to the working hg/config.json; entries scoped by
  publishing grid host because that file has no grid field.
- Bundled grid list grew from 3 to 31 entries and now loads for every profile;
  published catalog is grid.xml (grids.xml is 404) and the setting defaults ON.
- Marketplace uses the grid's own marketplace URL; currency resolves the helper
  per request and never reuses another grid's endpoint.
- Status bar sizes to content instead of a fixed 1000px width; the extra Stop
  button was removed and the stop action kept on existing controls.

tasia_opensim_services_test.py:8 PASS. Full local suite 11/11 PASS, diff clean.
No Windows build yet for these changes; no viewer restarted, installation and
age5 package untouched apart from the intentional launcher provisioning.

## Combined globe/audio Windows build2 + isolated package VERIFIED (age5)

Exact build receipt F:\tasia-kirsten\globe-audio-20261019-01-build2.json:
configure0/build0/errors0, finished2026-10-19T14:29:50.4530923Z.285-source snapshot
SHA1C757E3D149BB6FCDBA6B37577E463CE05447D31CDE304CD6E766C0C6069C177.
EXE SHA467C90230D073CE6E829512161CC5CB1DAEDFBE3580A385D66192911BDA9A3F4;
PDB SHA76D961EA5A323430CCBCEDD79CC659BAE4AF0A51DFED70459E0F2771B4D6EF33.
Independently matching GUID65-45-59-3E-A9-D7-64-47-83-2E-F6-E8-49-B4-52-D6 age5;
product metadata remains Tasia Viewer DX / Advanced Multiverse Viewer by Tasia and Marty.

Verification initially failed at runtime app_settings/tasia_default_grids.xml:
native copy helper's explicit criticalFiles list omitted this new asset. Corrected
only that staging list plus regression assertion; PowerShell parsed and old helper
backed up. Reran runtime-data copy without recompilation. EXE/PDB hashes unchanged.
Old staging helper E52E463199FD9F9A8233B580077A8ED69DEB448E6A8E7703D77EF0DD6C8D1C60;
new5C8D25230D1295D894A6CD9E6C302B37B987654F9A948670D6B2A7B84A3C361D.
This is a recorded post-build staging-script change, not part of sources2 snapshot.
Separate receipt globe-audio-20261019-01-restaged.json; no extra compiler/build loop.

Then verified95 runtime assets + FFmpeg identity; packaged new isolated directory
F:\tasia-kirsten\test-builds\globe-audio-20261019-01.7303 package hashes and95
source assets match; all branding/source hashes, SDK bundles, CEF/plugin resources,
portable marker, shortcut target/icon checked. No private profile packaged. Existing
installation and age4 package unchanged; viewer not launched, no user session killed.

Manual native globe/audio/teleport and prior QUIC/IM/identity/grid GUI regressions
still pending Mom's test. Do not infer runtime success from build/package proofs.
Updates/news deliberately separate. No commit/push. No build active.

## Combined build1 failure diagnosed; focused callback correction prepared

Incremental build2 now active after five-file reviewed/hash-verified delta; all285
remote source files checked against sources2.json. Source manifest
1C757E3D149BB6FCDBA6B37577E463CE05447D31CDE304CD6E766C0C6069C177.
Backup: F:\tasia-kirsten\source-backup-globe-audio-20261019-01-fix2.
Build2 result/EXE/PDB/package verification pending. No unrelated warning fixes.

globe-audio-20261019-01-build1: configure0/build1/errors2, failed2026-10-19T14:19:38Z.
Exact errors C2064 at boost/function/function_template.hpp:100 while compiling
llstatusbar.cpp and llfloatergridmanager.cpp. Commit slots pass LLUICtrl* and
const LLSD&, unlike zero-argument clicked callbacks. Fixed the new Stop callback
and Discover/Save URLs/Check Grid List callback lambda signatures accordingly.
No unrelated source changes made for this failure. New fixture compiles extracted
production callback signatures against std::function<void(LLUICtrl*,const LLSD&)>;
would reject old zero-argument lambdas. tasia_sound_globe_test.py now4 PASS with
ASan/UBSan. Preserve build1 receipt/log; focused delta + incremental build2 next.

## Sound + globe candidate source prepared; combined build authorized next

Now compiling attempt globe-audio-20261019-01-build1. 285 source hashes and all34
branding assets checked;67 changes backed up. Prior output EXE/PDB preserved at
F:\tasia-kirsten\verified-builds\age4-before-globe-audio. Single remote divergence
reviewed: llviewernetwork.cpp still used upstream splash URL, no other code diff.
Approved remote hashD003A12E05EE68D8FDD35BBB22C31396393C257F435F80A2225D7420C1425755.
New manifest26B84CD470EAD78CCDA1F1442E5759E7EF77CB6FE131C67D32D08DF5B1E028C7.
InstalledDA07BFE68C61... and age4 package29ED66029FF4... unchanged. No viewer stopped.
Build is active; no compilation/package/runtime success claimed. Local source-policy
fixtures for sound/globe3,QUIC4,identity/grid4,portable/branding10,media lifetime2,
IM3 and COF-library fixture pass. Updates/news deliberately outside this candidate.

Mom requests sound + real 3D Earth-looking map, then test build; updates/news
separately. Native LLTasiaGlobePanel projects a shaded textured sphere into DX UI,
clips to front hemisphere, adds atmosphere/stars and selectable known-region
markers, drag rotation/wheel zoom, original procedural planet art. It is a virtual
grid atlas, not real Earth geography/external imagery. Selection calls original
trackLocation, not teleport; original destination controls and flat map retained.
Bounded neighborhood map request and existing named-search path are preserved.

Audio: user-stop latch at the parcel-stream choke point blocks restarts caused by
parcel updates or delayed permission replies; global/nearby Play clears it.
Explicit Stop in status bar stops world media/music and standalone music player;
master mute remains the control for UI browser audio. Playing streams keep media
toggle usable even after parcel metadata/preferences stop advertising the stream.

Local tasia_sound_globe_test.py:3 PASS (actual audio method bodies, production
globe projection/clipping/zoom header under ASan/UBSan plus UI/selection contracts).
No live native globe, audio, teleport or GPU proof yet. Candidate build must include
all prior QUIC/identity/grid changes and the new planet PNG/CMake entries. Preserve
age4 candidate and working installation; never stop the viewer to build/package.

## Identification + grid manager source milestone (NOT Windows-built)

Mom marks cloud fixed for now, with extended testing still needed. Focus moved
past source-prepared QUIC to identification/grid manager; combined build remains
deferred until agreed source work is complete.

Adapted existing Tasia cosmetic parser and added required grid+UUID scoping,
nametag/profile UI hooks, five-minute public refresh, session/grid/generation and
toggle guards. Default endpoint matches Mom's requested config.json; default ON,
user can disable. UUID-only legacy rows are ignored to prevent cross-grid leakage.
No identity/credential query is sent. Actual simulator identity and permissions
unchanged. Server `grid` field requirement remains an unvalidated schema extension.

Expanded Grid Manager with bounded OpenSim XML metadata preview, manual save of
nine URLs, grid-specific registration/reset links, opt-in additions-only public
catalog, atomic rollback and persistent deletion tombstones. Accepts Firestorm
field spellings; preserves up to8 explicit login URIs. Fresh-profile-only bundled
I-Grid/Beta/local defaults; existing grid files never silently get new defaults.
Protected built-in grids and saved credentials are not migrated or replaced.

Shared fetcher requires HTTP200, verified TLS, no redirects/retries, ten-second
timeout and receive-time body caps via new HttpOptions::setBodyLimit (zero/default
does not change unrelated callers). Discovery alone permits explicit HTTP origins.
DTD/entity declarations rejected before XML parsing. UI results use weak handles,
generation/input snapshots and explicit Add/Save; no stale in-flight auto-save.

tasia_identity_grid_test.py:4 PASS; compiles actual production identity/catalog/
removal/metadata/write-limit bodies with fixture LLSD/URI/parser/network types and
ASan/UBSan. Settings shape/unique keys and XML/UI contracts checked. Existing
portable-branding10 and feature-restore fixtures PASS; diff whitespace clean.
This does not compile the entire viewer or prove live service/GUI behavior.
Profile cosmetic updates use normal callbacks/reopen; remote badge images are
not auto-fetched. Full behavior/contracts in docs/REMOTE_CONFIG.md.

No Windows build/remote transfer/credential access, installation/profile changes,
viewer shutdown, commit or push. Keep running age4 candidate intact.

## QUIC source integration (2026-10-19; NOT Windows-built or deployed)

Build is deliberately deferred at Mom's request until the agreed work is ready.
No remote credential used, no remote source transfer, no running viewer stopped,
no profile migration, installation replacement, commit or push in this continuation.

Added llmessage->llquic link and per-circuit compressed LL packet stream/datagram
send/receive, fair UDP/QUIC and circuit receive scheduling, OpenSim-only advertised
endpoint negotiation at login/neighbor enable/teleport/crossing, and quicready
acceptance only from that circuit's QUIC receive path after agent-ID validation.
No implicit library initialization or live-UDP upgrade. Pending sends bounded to
256 packets/1MiB; five-second bootstrap timeout and failed/closed/send-failure
paths replay UseCircuitCode and unsent queue over UDP, retaining normal reliable
records/callbacks. Accepted application messages are not blindly replayed (could
duplicate actions). Reliable QUIC callbacks indicate transport acceptance, not a
new simulator application-level delivery guarantee; bootstrap waits for ready.
Disabling QUIC switches existing circuits back to UDP at the network idle pump.

TLS setting now really sets NO_CERTIFICATE_VALIDATION only when disabled; fresh
profile default is verification ON, persistent user choices unchanged, restart
required for certificate-policy changes. Bypass does not imply fallback: failure
or timeout does. Fixed freeing a datagram context at both SENT and a final state.
Added frame-length/receive-queue bounds, callback-safe explicit teardown and
main-thread cleanup of shutdown peer streams. Green Q status means current ready
region only; no false active status from initialization. Package checks all three
QUIC status PNGs by source/destination hash.

Verification: tasia_quic_transport_test.py:4 PASS. Compiles actual production
circuit routing + stream-parser bodies with transport doubles using ASan/UBSan;
covers ready gating, delayed callbacks, failures before/after ready, partial-flush
tail retention, timeout, queue limit, fairness, malformed frames, framing chunks
and truthful status. Media-native:3 PASS; portable/branding:10 PASS; COF-library
and welcome-pack actual-handler fixtures PASS; IM fixture:3 PASS. These tests do
NOT compile full Windows dependency integration or prove any live QUIC behavior.
Do not call QUIC complete until Windows build and real simulator tests pass.

IM follow-up: narrowed earlier IM_SESSION_INVITE conversion to canonical agent-
pair session IDs with no voice metadata. Target count alone was unsafe for real
single-peer conferences; prior "root cause/fixed" claims below are unproven.
Automatic voice session creation on text IM remains an investigation item.

Evidence correction: Mom confirmed visible avatar and correct welcome destination;
that does not independently prove FetchLib2 results or absence of null-body masks.
Old assertions below about exact library outcomes are not backed by a new log read.

Next external integration blockers: provided grids.xml/build_data.json/news.txt
URLs returned404; grid+UUID config.json returned500. No server modifications made.

## Cloud baseline VERIFIED + welcome-pack redirect VERIFIED (2026-10-19)

Mom logged in with age4 candidate
`F:\tasia-kirsten\test-builds\cof-welcome-20261019-01` (process88780). Avatar
fully visible: hair, eyes, skin and clothing. The two unresolved COF targets
were resolved by the bounded FetchLib2 lookup using the login-advertised library
owner; no null-body diagnostics fired. Welcome-pack URL now
`https://startup.tasiaviewer.work/welcomepack/` (currently "Coming Soon").

This is the first successful live cloud baseline in the new Tasia viewer. The
avatar acceptance policy was not relaxed, and no null/default/clothing mutation
was performed. This confirms the local library-lookup hypothesis on Mom's grid.

Next: implement actual QUIC transport, keeping UDP fallback and TLS verification.
No grid-specific SL lock. No downgrade of already-verified cloud/IM/branding/
portable behaviour. Mom's backlog items 3-8 remain queued after QUIC.

No commit/push performed. Existing installation and all prior test packages
unchanged. QUIC work may now begin.


## OpenSim one-to-one IM shown as conference (2026-10-19; LOCAL fix, not built)

Mom reported incoming AND outgoing one-to-one IMs open as a conference on her
OpenSim grid. Root cause is in session classification, not the grid list: OpenSim
simulators deliver 1:1 IMs as `IM_SESSION_INVITE`, and `LLIMSession` treated any
`IM_SESSION_INVITE` whose session ID is not a group ID as `ADHOC_SESSION`. That
misclassification also made `findAdHocIMSession()` match a later outgoing IM to
the same resident, so the outgoing chat inherited the conference session ID.

Scoped fix (OpenSim only, via `LLGridManager::isInOpenSim()`):
- `LLIMSession` constructor: compute `mOpenSimP2PInvite` for a single-target,
  non-group `IM_SESSION_INVITE`; route it through the P2P voice branch and keep
  `P2P_SESSION`.
- `initVoiceChannel()`: accept the flag so OpenSim P2P keeps the P2P voice path
  instead of the "Invalid Session Type" group/conference branch.
- `isAdHoc()`: excludes OpenSim P2P invites (blocks findAdHocIMSession reuse).
- `isP2P()`: includes OpenSim P2P invites.
- Added `mOpenSimP2PInvite` member and the `llviewernetwork.h` include.

Untouched on purpose: Second Life classification, real group chats (session ID ==
group ID), genuine multi-recipient conferences, outgoing IM dialog types, and all
conference/ad-hoc naming and history paths. No grid-policy or SL lock change.

Verification: new `tasia_im_session_class_test.py` compiles a mirrored copy of the
production rule with fixture grids/sessions and asserts OpenSim 1:1 => P2P/not
adhoc, Second Life 1:1 => unchanged ADHOC, group => GROUP, conference => ADHOC,
multi-target invite => ADHOC, outgoing 1:1 => P2P. 3 tests PASS. Diff whitespace
check clean. This is a classification contract test, NOT a live IM or UI test,
and this change is not in any built package yet. Needs one Windows build plus
Mom's real grid test for incoming and outgoing IM.


## COF-library lookup + welcome-pack age4 build verified, test launched (2026-10-19)

Exact receipt `F:\tasia-kirsten\cof-welcome-20261019-01-build1.json`: configure0,
build0,errors0, finished2026-10-19T09:33:06.2452457Z (one attempt).
235-source manifestSHA
`F48555C230C5550C90506B497A7F5641F0301325B987787120C4C8B97358B5B9`.
11 reviewed changes backed up at source-backup-cof-welcome-20261019-01;
four PS scripts parsed;all34 branding source assets verified before compilation.
Age3 matching EXE/PDB also preserved with hashes at
`F:\tasia-kirsten\verified-builds\regression-20261019-02` before this build.

EXE SHA256 `29ED66029FF4B11B043E8BAF894B8975912D899D53E8BE9C9912C8C5C7C8948D`.
PDB SHA256 `A08A4159D636930EF702909705931E6B92B53453D42975BBBF485A09D0EE3CEC`.
Independent PE/PDB matching GUID65-45-59-3E-A9-D7-64-47-83-2E-F6-E8-49-B4-52-D6,
age4; branding metadata,88 runtime source assets and FFmpeg identity verified.

New isolated package `F:\tasia-kirsten\test-builds\cof-welcome-20261019-01`:
7300 hashes verified,all34 approved branding outputs source/destination and again
prelaunch,CEF/FMOD/WebRTC/FFmpeg/CA,SDK/player bundles,portable marker,shortcut
target/ICO and no private profile packaged. Old failed/age3 profiles and installed
EXE DA07BFE68C61... unchanged. No account import/migration or cache/profile clear.

Includes one bounded advertised-library-owner FetchLib2 COF target lookup with
stale session/generation/COF-ID/version/completeness guards. No snapshot-validator
relaxation or default/magic-ID clothing repair. Includes exact welcome-pack URL
in native handler/default setting and untrusted/no-download policy. Site serves
Coming Soon; website contents not modified. Focused compiled production library
request/response fixtures,existing12000-case/ASan/UBSan wearables,welcome postBuild,
media lifetime and branding tests all pass. No live avatar success claimed.

Authorized testing continuation: on-demand Interactive/Limited task
TasiaViewer-COFTest-20261019-093544. Observed process88780 responding in desktop
session1; SSH HasWindow=false does not establish visibility. TasiaViewerData now
created for this candidate. Keys provided only via child environment and one-shot
DPAPI blob,consumed/deleted as independently verified. No autostart trigger and
no viewer process terminated. Actual login/cloud/library response and welcome
navigation await Mom; QUIC transport remains on hold until baseline works.

Local receipts /tmp/opencode/cof-welcome-20261019-01-{build1-status.txt,
verified.json,package.json,launch.json,launch-status.json,age3-preserved.json}.
Do not edit running candidate or claim the cloud is fixed from compiler/tests.

## Cloud metadata lookup and welcome-pack redirect source preparation (2026-10-19)

New age3 LIVE log evidence at09:08:31UTC: rejected six-entry repair snapshot has
null_items0/null_assets2/invalid_types0/duplicate_items0/shared_assets0; four
body-part counts each1. Existing logs do not say which types have null assets.
Do not call this a texture/GPU/cache problem or manufacture missing asset UUIDs.
Read live log with FileShare.ReadWrite|Delete (ReadAllLines failed sharing against
the active viewer); no process stopped or data cleared. Safe receipt:
`/tmp/opencode/age3-cloud-details.json`.

Found source gap: standard unknown-item fetch assumes agent inventory. COF
recovery had no bounded library-owner lookup before falling back to simulator
assets, which here include null asset IDs. LOCAL candidate adds read-only FetchLib2
lookup once per login's COF recovery, using advertised gInventory library owner,
real unresolved link targets only, deduplicated and capped256. It uses the existing
inventory item HTTP parser with a guarded completion: session/generation/COF ID,
COF version/completeness must still match. Missing endpoint/owner/target, oversized
request, stale completion and request-queue failure do not mutate the outfit.
No global inventory routing rewrite, defaults, null-asset substitution, validator
relaxation, appearance/bake/rendering edits or SL policy changes. Library origin
of the actual two unresolved items is still a hypothesis until runtime response.
Rejection counters now include null body-item/asset masks to distinguish bad
clothing entries from required body parts without logging IDs.

LOCAL welcome-pack handler and default setting now target exactly
`https://startup.tasiaviewer.work/welcomepack/`, not old per-grid URL substitutions
or stale copied AvatarWelcomePack value. Browser untrusted, downloads disabled;
browser member initialized null. Public route returned "Coming Soon"; changing
website contents is not part of this request.

Verification PASS: compiled actual library request/guarded handler with fixtures
(correct advertised owner/body/cap, target bounds/dedup, missing owner/cap/region,
known items, failed queue, stale generation/session/outfit/version/completeness),
existing12000-case production wearable/ASan/UBSan fixtures with new diagnostics,
actual welcome postBuild fixture/default/XML policy,2 media lifetime and10
branding/portable tests. Not a live library/appearance or CEF verification.
Preparing one Windows cloud+welcome candidate. Running age3 package unchanged;
actual QUIC transport not implemented yet and remains after cloud verification.

## Diagnostic age3 build/package verified and test launched (2026-10-19)

Receipt `F:\tasia-kirsten\regression-20261019-02-build1.json`: configure0/build0/
errors0, completed2026-10-19T09:02:02.7676601Z. Only ONE build attempt for this
diagnostic candidate. Sources231 files, manifestSHA
`0E0EFA07232D2D084C488F0F49A167076A9CC5C7832A7BF864A826DEE56F4715`.
Reviewed/backup40 changed files, four PowerShell scripts parsed; source backup
`F:\tasia-kirsten\source-backup-regression-20261019-02`. No unknown prior states
after the earlier branding-transfer omission was identified/corrected.

EXE SHA256 `D58C4899E0194F759A0DB66CD532C63D1E8DBADCC069C24BB6C04BA9D632B31B`.
PDB SHA256 `29B0986C23353E9EC5BC96330CD099178FAFF9D02E89F3766526C30E1C740C96`.
Independent PE/PDB GUID bytes65-45-59-3E-A9-D7-64-47-83-2E-F6-E8-49-B4-52-D6,
age3 for both. ProductName/filename/tagline verified;87 runtime source assets and
FFmpeg bundle identity verified. Includes deferred picker close/selection fix and
diagnostic-only avatar snapshot counters. No speculative avatar-policy changes.

New isolated package `F:\tasia-kirsten\test-builds\regression-20261019-02`:
7300 file hashes verified; all34 generated branding outputs independently checked
by strict packager against SOURCE AND DESTINATION, then checked again before
launch. Packager now includes tasia-brand-assets.json; previous omission cannot
pass this check. SDK/preview bundles, CEF/FMOD/WebRTC/FFmpeg/CA, shortcut target/
ICO, portable marker and absence of a packaged private profile also verified.
Prior failed test package/profile unchanged. Installed EXE remains DA07BFE68C61...
No permanent deployment, account migration or profile/cache clear.

Mom authorized proceeding through build/test before QUIC. Launched new verified
test via on-demand Interactive/Limited task TasiaViewer-RegDiagnostic-20261019-
090445, no autostart trigger, no process termination. Observed process83244 in
desktop session1 Responding=true; SSH HasWindow=false is NOT visible GUI proof.
New TasiaViewerData created. Provider keys supplied only in child process
environment via one-shot same-user DPAPI payload; consumption/deletion verified.
Launcher private directory is separate from the package and prior test launcher.

Offline checks PASS:2 actual-handler lifetime tests (old unsafe inline-close
pattern detected),3 native contracts,10 branding/portable tests,10 SDK/preview
tests, production wearable12000-case+ASan/UBSan fixtures, focused whitespace and
34/34 branding-output transfer-manifest coverage. This remains NOT proof of CEF
crash fix, logo display, chat delivery/playback or avatar loading in the GUI.

Await Mom's GUI result. Test native logo and Insert link first; if cloud persists,
read new rejection counters before changing appearance logic. Do not stop her
viewer, edit the running package or start QUIC while this test is pending.
Local receipts /tmp/opencode/regression-20261019-02-{build1-status.txt,
verified.json,package.json,launch.json,launch-status.json}. No commit or push.

## Correction: login PNGs were omitted from the earlier source transfer

During the newly authorized diagnostic-build staging review, found that the old
201-file manifest did not include login_tasia_logo*.png or startup_logo.png.
Previous native login XML/texture aliases did match, but their referenced PNG did
NOT: failed package horizontal PNG SHA07C1A996103C4F746795A95346CC1201DE8F5C44377D7C275792981715C05625
versus approved E7EA8DB2347B687D1E9BA9036C34B7258A3C370DB949FA4D19D8D17C45C9FFAB.
Thus earlier wording that the PNG hashes were correct was wrong. Windows still
had the old image; this explains why updating XML alone did not change the logo.
Source startup_logo also remained its upstream HEAD hash45F75CF4..., not an
unexpected user edit. Initial new staging review correctly stopped on this mismatch
to the LOCAL pre-generator backup, then comparison to Git HEAD explained it.

Source-transfer manifest now incorporates every generated branding output.
Packager now independently verifies every branding output against both source
and destination and includes tasia-brand-assets.json. It must not accept a subset
of XML/icon files as proof all artwork arrived. Preparing one isolated diagnostic
candidate; no source applied/build launched yet at this correction point.

## Runtime regression report — age2 candidate FAILED (2026-10-19)

Mom confirmed the WHOLE viewer closed when clicking Insert link; avatar remained
a cloud, and Kirsten logo was above username/password in the native login form.
Earlier compiler/hash checks are not runtime-success evidence. QUIC work on hold.
No viewer process was left running at diagnostic collection. No cache/profile
clears, source deployment, replacement package or viewer relaunch performed.

Crash evidence (read-only Windows Application event1000): exact tested EXE path
and SHA5987AD6D..., exception c0000005, RVA0xfc4750. Windows local DbgHelp with the
matching age2 PDB resolves this address to LLViewerMediaImpl::emitEvent+0x60 at
llviewermedia.cpp:1685. This accesses mObjectList after observer broadcast. Picker
observer synchronously called closeFloater -> onClose -> unloadMediaSource, which
can destroy the emitter before that post-notification access.

LOCAL-only source correction in llfloatertasiamediapicker.cpp: current-plugin
check, queue selection via doOnIdleOneTime, disable duplicate selection, capture
weak floater plus nonce/session, recheck live visible/session/provider before
closing outside event dispatch and inserting via the existing weak-editor callback.
Tests compile the actual event handler with instrumented emitter surroundings:
fixed handler passes deferral, duplicate, user-close, destroyed floater, reopened
page, logout, disabled-provider and stale-plugin cases; substituting old inline
close is detected. Two lifetime tests and three native contract tests PASS.
This is NOT yet a Windows build or GUI reproduction of the fix.

Avatar evidence in actual failed-run log:
- COF link-target fetch: targets2, still unavailable after bounded fetch.
- Body-part recovery requested mask12; readiness shape1/skin1/hair0/eyes0.
- Simulator snapshot reported body counts shape1/skin1/hair1/eyes1, then rejected
  by validSnapshot before assets were downloaded; repeated attempts timed out.
- Inventory folder errors Unknown also present. No root cause assigned to them.
- Old approved transfer records prove four relevant avatar/wearable source files
  already matched remote source BEFORE feature transfer and still match in tested
  build. Do not claim those sources changed overnight, or blame caches.
Exact rejection reason (null item/asset, duplicate item/shared asset, invalid type,
cardinality) cannot be derived from existing logs. Added LOCAL diagnostic-only
SnapshotDiagnostics helper/counters on rejection. No policy relaxation, default
body parts, clothing mutation or speculative rendering edits. Header diagnostics
tested; existing wearable production fixtures12000 cases/ASan/UBSan PASS.

Branding: remote native login XML, texture list and approved Tasia PNG hashes are
correct, no user skin overrides found. Therefore exact reported runtime old-logo
origin is not proven. Old legacy login/startup/SL_Logo files remained in all five
shipped skins. Backed up25 stock files; LOCAL generator now regenerates those
legacy/alternate outputs as well (34 total), with per-skin pixel comparisons.
Portable/branding suite9 PASS; actual SDK/picker/preview suite10 PASS; scoped
whitespace checks PASS. Must verify visible native form after rebuilding.

Diagnostics receipts (sanitized; no raw dumps or credential-bearing logs copied):
/tmp/opencode/media-portable-regression-diagnostics.json,
media-portable-regression-details.json, media-portable-crash-symbol.txt,
media-portable-profile-paths.json.
Legacy asset backup: /tmp/opencode/legacy-branding-before-regression-fix-20261019.zip.
Next: single isolated diagnostic candidate and requested runtime retest; current
test package/profile and last working installation remain untouched. Cloud is
NOT fixed by adding diagnostics. No new build/launch/commit/push in this follow-up.

## Morning user-requested test launch (2026-10-19 08:21 UTC)

Mom explicitly requested launching the current verified package before further
QUIC implementation. Launched the exact age2/hash-pinned portable package through
an on-demand Interactive/Limited scheduled task in desktop session1. Observed
process80632, Responding=true; SSH process query did not identify a main window,
so visible login/rendering still requires Mom's confirmation. TasiaViewerData was
created beside this package. Installed EXE hash remains unchanged.

GIPHY/KLIPY vault keys supplied only to the test process environment. One-shot
transfer payload was DPAPI encrypted for the same Windows user, consumed/deleted
before launch; deletion independently verified. No key in EXE, task arguments,
launcher source, source repo or public package. No autostart trigger and no viewer
process terminated. Launcher non-secret scripts/receipt are under the private
build-side launcher directory, outside the distributable package.
Local safe receipt: `/tmp/opencode/media-portable-morning-status.json`.
Actual GUI/chat/GIF/clip/audio/appearance testing remains pending; do not modify
the test package or start QUIC changes while Mom tests it.

## Media/chat + approved branding + portable build/package verified (2026-10-19)

Latest successful Windows Release/DX11 receipt:
`F:\tasia-kirsten\media-portable-20261019-01-build2.json`.
Configure exit0, build exit0, errors0; finished `2026-10-19T01:53:22.1329397Z`.
EXE `F:\tasia-kirsten\build\newview\Release\TasiaViewer.exe` SHA256:
`5987AD6D7E5B232E64EEFE676391F06594828504CDDAE1F2060914AC250B7EEE`.
PDB SHA256 `5E77F2CED08BFA777000DA43DE4DBEDE0F1AC7433039A804B3095D3553336A1C`.
Independent PE/PDB identity match: GUID bytes
`65-45-59-3E-A9-D7-64-47-83-2E-F6-E8-49-B4-52-D6`, age2 for both.
ProductName/OriginalFilename and exact multiverse tagline verified; 58 runtime UI/
settings/HTML assets matched source hashes; FFmpeg binary matched verified bundle.
Build2 source manifest SHA256:
`54503532ACBCAA8A5451187B35433237D33C8D081EA6539839EB359CD2E30D19` (201 files).

Actual new portable package:
`F:\tasia-kirsten\test-builds\media-portable-20261019-01`.
Verified 7299 package-file hashes, 58 source assets, required CEF/FMOD/WebRTC/
FFmpeg/CA files, portable marker, no private profile packaged, SDK and preview JS
bundle hashes, shortcut EXE and approved ICO targets. Shortcut `Tasia Viewer.lnk`.
No viewer launch or permanent deployment. Installed E:\TasiaViewerLatest EXE
remained `DA07BFE68C61B830DAC8608CEB9215D4DBA5D5570D24A1FD878B630A1704551E`.

Implementation:
- Shared LLFloaterTasiaMediaPicker: local untrusted CEF, explicit masked session-key
  field, reload/context destruction, session/nonce/URL checks, no app command bridge
  and no automatic chat send. Native buttons bound to originating live editor in
  LLFloaterIMSessionTab, which nearby/IM/group share, not unused Firestorm XML.
- Official GIPHY Web fetch SDK @giphy/js-fetch-api 5.8.0; pinned JS util5.2.0,
  types5.1.0, uuid9.0.1, esbuild0.25.12. Reproducible build script, integrity/source/
  bundle manifests and MIT notices included. SDK GIF/sticker/emoji/video/text
  modes; KLIPY current path-key/data.data/file schema, GIF/clip/sticker/meme.
- Selected clips/KLIPY include provider page plus public media link for recipient
  viewers without API credentials. Shared LLChatHistory preview recognizes both
  providers, pairs page/media, uses local allowlisted HTML video/image player,
  no video autoplay, and prioritizes provider preview over generic image preview.
- Provider keys are session-only or environment supplied; no key compiled into
  distributed EXE/JS/HTML. Raw plugin IPC diagnostic payload dumps were suppressed
  because JS initialization can carry credentials. No API errors/URLs displayed
  verbatim. Requests bounded by timeout/response size, no redirects or credentials,
  no persistent request cache; stale results ignored and pagination bounded.
- Approved full-artwork branding generated as ten assets, including two separate
  QUIC status icons. Portable directories/legacy compatibility as documented below.
- QUIC indicator truthfully reports disconnected, SL standard transport, disabled,
  unavailable, or initialized-but-not-routed. There is NO active QUIC region routing
  in this build. No fabricated green connection indication; UDP/HTTP remains.

Verification:
- Node actual bundled SDK/picker/preview against DOM/network fixtures: 10 PASS.
- Native wiring/privacy/header/compiled production transport policy: 3 PASS.
- Portable/branding tests incl compiled production LLDir policy with stubs: 8 PASS.
- Production preview extraction compiled with stub URI/string surroundings: PASS
  (not a real-LLURI/CEF test). An initial fixture extraction failure counted a
  brace character literal; fixed the test extractor, not production code.
- Feature restoration and web-chat regression suites PASS; 46 changed XML parsed;
  scoped whitespace checks PASS.
- Live authorized API checks also verified GIPHY videos/emoji/stickers HTTP200 and
  KLIPY GIF/clips HTTP200, wildcard CORS for Origin:null; resolution-keyed GIPHY
  video assets and current KLIPY file schema confirmed. Not codec/GUI proof.
- Build1 failed on three LLLineEditor::setText("") conversions and callback arity;
  fixed using actual clear() API and (LLUICtrl*, const LLSD&) signatures. Updated
  header-contract tests, hash-checked/backed-up three-file revision, build2 passed.
- First large encoded-command/large stdin transfers failed before source writes;
  verified absence, then used SCP plus pinned archive/script hashes. Reviewed 201
  remote file hashes, backed up62 changed files at source-backup-media-portable-
  20261019-01, parsed four PS scripts, then verified applied sources. One remotely
  absent panel_login_first.xml was added, not overwritten. Build2 revision backup:
  source-revision-media-portable-build2. Do not retry long stdin transfer methods.

Pending: Mom requested GUI tests tomorrow morning. CEF rendering/codec/audio,
actual chat delivery/previews, logo layout, portable runtime paths/permissions and
OpenSim/SL functional checks remain unproven. No new package process launched.
Keys were not preinstalled into the new portable profile. No commit/push.
Local evidence: /tmp/opencode/media-portable-20261019-01-build2-status.txt,
-verified2.json, -package2.json. Mom explicitly approved local system suspend after
the work/result is saved; record whether the subsequent suspend succeeds rather
than assuming it did.

## Live GIPHY/KLIPY credential smoke test (2026-10-19)

Mom explicitly authorized using vault entries `giphy_key` and `klipy_key` for
connection tests. Values handled only in transient tool-runtime variables; no
keys, authenticated URLs or raw response bodies saved to source/logs/memory.

Verified HTTPS GET requests (one requested result each, no redirects):
- GIPHY `/v1/gifs/trending`: HTTP 200, meta.status=200, one item with images.
- KLIPY `/api/v1/{app_key}/gifs/trending`: HTTP 200, result=true, one item with file.
- KLIPY `/api/v1/{app_key}/clips/trending`: HTTP 200, result=true, one item with file.

Important port correction: old reference llklipyclient.cpp uses a different
endpoint/auth and response schema. Current API embeds app_key in the path, uses
per_page, returns data.data array and item.file, not data.items/item.media.
Do not copy old KLIPY request/parser unchanged. Authentication-bearing URL paths
must never be logged. No playback, SDK entitlement, production quota or viewer
integration is established by this small REST smoke test. No viewer files on Acer
were changed or built as part of these requests.

Initial confined-runtime attempt did not reach endpoints because AbortSignal is
unavailable there; corrected runtime usage before the three successful requests.

## Approved logo and portable profile source follow-up (2026-10-19; NOT built/deployed)

Mom requested `/home/marty/opensim/icon.png` throughout branding and portable
cache/logs/user data beside the application. Preserved original PNG as
res/tasia_brand_source.png, SHA256
`6aec2c8cf7b04f7b38defcfbfcb3e659cef0697d202b6f534cf06d7eb66d51d5`.
scripts/generate_tasia_brand_assets.py generates eight assets without stretching
or discarding artwork; res/tasia_brand_assets.json records crop, sizes and hashes.
Updated login/first-login/About, resource icon aliases and package shortcut icon.
Previous generated assets backed up locally at
`/tmp/opencode/branding-assets-before-20261019-025736.zip`.

Portable implementation: LLDir_Win32 detects executable-adjacent marker before
cache creation, assigns TasiaViewerData as user root, cache and temporary paths;
recognizes existing portable-data and refuses ambiguous dual profiles. Write
probe uses exclusive CREATE_NEW plus DELETE_ON_CLOSE. No automatic AppData
fallback, copying, imports or data removal. TEMP/TMP inherited by children.
Removed the old getExpandedFilename partial override that discarded subdir1/2.
Existing per-account, SQLite login and CEF paths follow LLDir. Cache migration
and copied absolute cache/transcript paths are blocked in portable mode, including
preference callbacks that otherwise move transcripts outside the profile.
Packaging writes the portable marker and includes docs/PORTABLE.md. DPAPI remains
Windows-user/machine bound; this is not password portability or zero OS traces.

Verification performed locally:
- `python3 indra/newview/tests/tasia_portable_branding_test.py`: 8 tests PASS.
  Actual four LLDir policy methods compiled/executed with stub filesystem/logging
  surroundings; remaining initialization/packaging/branding checks are structural.
- `python3 indra/newview/tests/tasia_feature_restore_test.py`: PASS.
- `python3 indra/newview/tests/tasia_web_chat_test.py`: PASS.
- Scoped git diff --check for changed portable C++ files: PASS.

Windows constructor/permissions/CEF/profile behavior and visual logo layout are
NOT runtime tested. These changes are not in the previously built test package;
no new Windows build or remote writes performed in this follow-up.
GIPHY/KLIPY restoration and actual QUIC indicator remain pending in plan.md.

## Test package staged and desktop launch requested by Mom (2026-10-19)

Correction to the build-only result: `build\newview\Release` was NOT a runnable
distribution. The first direct launch exited immediately; that directory lacked
FMOD, WebRTC, SLPlugin, CEF runtime and the CA bundle. A compiler/linker success
must not be presented as a complete runnable installation.

Updated Package-TasiaViewer.ps1 to support an explicit read-only RuntimeBase for
runtime DLLs, copy the dependency tree's CEF resources/locales and newly built
WebRTC DLL, and avoid .NET GetRelativePath (unavailable in Windows PowerShell 5).
Script revision was hash-checked, backed up and parsed before execution.

Actual package staging succeeded at:
`F:\tasia-kirsten\test-builds\features-20261019-021505`.
Verified the exact built EXE hash and required FMOD/WebRTC/SLPlugin/CEF/resource/CA
files. File hashes recorded in `package-sha256.json`; local receipt:
`/tmp/opencode/features-20261019-021505-package.json`.
Used only root DLLs from the existing working installation as RuntimeBase; did
not copy its executable, accounts, settings or logs. Its installed executable hash
remained unchanged. No rebuild was required for this package-script correction.

Mom explicitly requested launching the viewer, superseding the earlier no-launch
restriction for this action only. Launched this TEST package through an on-demand
Interactive/Limited scheduled task, without a password in arguments and without
any autostart trigger. Task: `TasiaViewer-TestLaunch-features-20261019-021505`.
Observed TasiaViewer process 74004 in desktop session 1, Responding=true. The SSH
process query did not detect a main window, so visible GUI/login/rendering success
is NOT established by that observation. Await Mom's visual confirmation.
No existing viewer was terminated; E:\TasiaViewerLatest was not deployed/overwritten.
Receipt: `/tmp/opencode/features-20261019-021505-interactive-status.json`.

## Feature/branding Windows build verified — NOT deployed (2026-10-19)

Successful attempt: `features-20261019-021505-build3`.
Log/receipt: `F:\tasia-kirsten\features-20261019-021505-build3.{log,json}`.
CMake configure exit 0, MSBuild exit 0, compiler error count 0. Completed at
2026-10-19 00:35:35 UTC. Release/DX11; memory diagnostics remain OFF.

Actual outputs (not the stale old-named executable):
- `F:\tasia-kirsten\build\newview\Release\TasiaViewer.exe`
- `F:\tasia-kirsten\build\newview\Release\TasiaViewer.pdb`
- EXE SHA256 `910F028F141B1E1990FCD17894F0838F68C8637B043F2DC9DB5B952834C6CA6D`
- PDB SHA256 `7892F3E639F757075A1E61C28AB9A026607C50C1EB2D064F3E2C7B105AEE6110`

Independent PE CodeView/MSF PDB verification: GUID bytes
`65-45-59-3E-A9-D7-64-47-83-2E-F6-E8-49-B4-52-D6`, age **1**, matching.
This is the new TasiaViewer PDB, not an increment of the old legacy filename's
age41 sequence. Metadata verified: ProductName `Tasia Viewer DX`, OriginalFilename
`TasiaViewer.exe`, FileDescription `Advanced Multiverse Viewer by Tasia and Marty`.
Verified 40 runtime UI/settings assets against source hashes and the copied
FFmpeg binary against its verified bundle. No running viewer was touched.

Source safety: unique source archive verified before extraction; all 154 candidate
files hash-checked; remote differences reviewed. Changed source backed up at
`F:\tasia-kirsten\source-backup-features-20261019-021505` before 83 writes. Later
revisions also used expected previous hashes and separate backups. Final source
manifest SHA256: `98FF3DE9B216A2A1BDD1B3FCA624388EA6C5A1FFA774F7FDFC16BA670E6775B2`.
Four transferred PowerShell scripts passed the actual Windows PowerShell parser.

Build fixes, one failing step at a time:
1. Old reference FFmpeg dated release returned 404. Selected the official supplier's
   dated `autobuild-2026-10-18-13-01` asset
   `ffmpeg-n9.0.2-28-g669f5ab083-win64-gpl-9.0.zip`; archive digest from GitHub API
   `a38198cc130ed3fa02097d0baebcea9c1b8a9d9d3d1d8161d691d1f03709158b`
   matched the downloaded archive. Updated the tracked staging script, not merely
   the expected checksum. Converter/license/provenance staged in `build\tasia-ffmpeg`.
2. Attempt 1: MP3 used nonexistent `LL_DIR_DELIM_STR`; replaced it with target
   `LLDir::add()`/`append()`. All ten reported errors were in that MP3 unit.
3. Attempt 2: target picker callback takes load AND save filter parameters, unlike
   the copied two-argument lambda. Added the third parameter. Corrected fixture
   APIs and added checks against real target header declarations.
4. Attempt 3 compiled MP3/startup/version units and linked successfully. Client
   wait timed out during active link-time code generation; did NOT restart the
   build. Retrieved its own completed receipt and verified outputs afterwards.

Windows FFmpeg smoke test (synthetic audio, no inventory/network upload): PASS.
Generated 31 seconds of MP3, converted into PCM WAV parts of approximately
29.0231s and 1.9769s; stereo, 44100Hz, 16-bit, both <=30s.
Converter SHA256 `F9C4D64E3012FA2740EFAABE260B64149F3E4C25A63C4FA39E8091E4A19F686F`.
Focused feature fixture also passed after both production API corrections.
Known existing libxml2 LNK4006 and /FORCE LNK4088 warnings remain; no warning
suppression or unrelated linker changes were made.

Local receipts: `/tmp/opencode/features-20261019-021505-status3b.txt`,
`...-symbols3.json`, `...-ffmpeg-smoke.json`, `...-applied.json` and source manifests.
No package staging/installer deployment, viewer launch/termination, interactive
CEF/clipboard/appearance test, live AI API request or QUIC test was performed.
`E:\TasiaViewerLatest` is UNCHANGED. Next action requires deployment authorization
and a safe installer/backup plan for the new executable name; then Mom's GUI tests.

## Startup/shutdown multiverse signature (2026-10-19)

Mom requested the exact signature `Advanced Multiverse Viewer by Tasia and Marty`.
Applied to the native Windows splash window/taskbar title, splash subtitle,
loading-panel tagline, shutdown console title/header/footer in both output modes,
EN/PL About, Windows file description and README. The previous audit had missed
`TEXT("Kirstens Viewer")` in llwindowwin32.cpp and `Advanced Second Life Viewer`
in the splash resource; both are now corrected. Increased splash height and
subtitle bounds so the signature need not overlap the startup status line.

Focused feature test passes, including exact branding strings, active splash
resource, progress/About XML and shutdown console frame widths; scoped diff check
passes. No changes to process ownership, shutdown/reset behavior, saved account
paths or upstream legal credits in this follow-up. No Windows build/deployment;
native text layout and rendering still require interactive Windows verification.

## About attribution follow-up (2026-10-19)

Added explicit thanks to Kirstenlee Cinquetti in EN/PL About/Credits and enabled
URL parsing for official GitHub/SourceForge project links. Removed stale static
4015/Alpha 1.2 labels from Credits, not generated version information. Retained
the original copyright block in Licenses. Branding audit found remaining upstream
mentions in copyright/sample text and legacy reset target warnings; those are not
the current product name and were not blindly replaced. No build or deployment.

## Feature/branding source preparation — no Windows build (2026-10-19)

Mom requested preparing the entire scope before building/deploying. Source now
contains Tasia Chat/Feed webview actions, asynchronous cost-confirmed MP3 segment
upload, native Windows clipboard image preview, opt-in configurable AI translation
with conservative gesture/text filtering, Tasia product branding and the restored
startup web control. Full file/behavior notes and limitations are in `plan.md`.

IMPORTANT BUILD IDENTITY CHANGE: CMake target stays `Kirstens-S24`, but output
files are now `TasiaViewer.exe` and `TasiaViewer.pdb`. Update remote receipt,
symbol verification, backup and deployment paths before using any old scripts.
Do not mistake a leftover old-named executable for this candidate.

Prepared dependency/package scripts:
- `scripts/Stage-TasiaFFmpeg.ps1` — immutable archive URL and SHA256 from the old
  viewer workflow; stage outside source, including license/provenance. NOT run.
- `indra/cmake/TasiaFFmpeg.cmake` — staged converter copy after build.
- `scripts/Package-TasiaViewer.ps1` — new destination, converter hash and CEF/UI
  checks, branded shortcut and package hashes; Invoke-Package delegates to it.

Verification: ten offline suites passed; 37 modified/new XML files parsed; scoped
diff check passed. Compiled fixtures execute production MP3/capture code against
synthetic APIs and the real text policy. Local Linux FFmpeg synthetic conversion
produced valid sound segments. None of these is a Windows build or CEF test.

Pending: PowerShell syntax/runtime validation (pwsh unavailable here), real Windows
compilation/packaging, CEF/GPU/clipboard/website checks and grid upload playback.
AI needs an approved endpoint/model/session key; no old AI endpoint was found or
invented, no live API requests made. Unknown remote gestures cannot be guaranteed
distinguishable from text. Legal credits, saved account paths and legacy reset
deletion targets are retained. QUIC testing remains last. No remote source writes,
builds, viewer launches/stops or deployments were performed.

## Tasia Chat source integration — Windows verification pending (2026-10-19)

Added the old Tasia web chat entry point using the target's existing web_content
floater, global callback, EN/PL login/Communicate menus, toolbar command/default
entry and reference PNG icon. Anonymous before login; in-world identity context is
encoded through LLURI. Browser content remains untrusted. Saved user toolbars are
not overwritten. Detailed scope and next action: `plan.md`.

Local verification passed:
- `python3 indra/newview/tests/tasia_web_chat_test.py` (XML/icon wiring plus actual
  handler compiled against synthetic APIs, not the production Windows viewer).
- Scoped `git diff --check` for modified Chat integration files.

No remote source transfer, Windows build, packaging, symbol verification or
deployment was performed for this change. Existing installed viewer is untouched.
Next: obtain permission for the stored Acer credential, validate safe transfer,
then perform Windows compilation and packaged CEF verification. Do not treat older
appearance deployment labels below as verified build identity for this work.

## Legacy fixes age38 deployed (2026-10-18)

Build `F:\tasia-kirsten\build-bom-recovery-20261018-224735.log`: actual exit 0,
compiler errors 0; wearables/avatar/self/bake units compiled. Independent EXE/PDB
GUID+age match (age38). Installed/archive hashes verified at `E:\TasiaViewerLatest`:
- EXE `B2E8EA34CD03A23F5B5FBB2307ABB4019B00F9E73B0A57A47F05A1E34B21D450`
- PDB `59809D9F192579400EAF9737158D28AACD81B363EE8BB472B0533E4CA898D71A`
Previous age37 backup: `E:\TasiaViewerBackups\before-legacy-fix-20261018-225709`.
Archive: `F:\tasia-kirsten\verified-builds\legacy-fix-20261018-225709`.
No running viewer at deployment; none launched/stopped. Five focused suites passed.

Legacy fixes:
1. `sendLegacyAppearance()` now validates ALL active legacy bake slots via
   `getNumBakes()` (6 original on non-BOM OpenSim, up to 11 with BOM enabled),
   not just slots 0-5. Extended tattoo/universal layers on slots 6-10 are now
   sent to the region via the AgentSetAppearance TE message.
2. `LLOverrideBakedTextureUpdate` now uses `gAgentAvatarp->getNumBakes()` instead
   of hardcoded `BAKED_NUM_INDICES` (11), so composite enable/disable toggles
   only valid slots on OpenSim.
3. `LLViewerWearable::removeFromAvatar()` now clears texture entries to
   `IMG_DEFAULT_AVATAR` when detaching tattoos/alpha/universal layers, so stale
   textures no longer linger in the composite causing black faces/missing layers.
4. `LLViewerTexLayerSetBuffer::processUploadCompletion()` and
   `LLVOAvatarSelf::setNewBakedTexture()` now call `refreshBakeTexture()` on all
   attachments after a baked texture is assigned. Attachments using baked texture
   magic IDs (`IMG_USE_BAKED_HEAD`, `IMG_USE_BAKED_UPPER`, etc.) - such as Genus
   Eyebrow Tintable A - rebind to the newly uploaded bake instead of staying black.

All 6 focused offline suites pass (initial wearables, cloud readiness, BOM limits,
bake upload, OpenSim compatibility, protocol compare). Runtime result pending:
Mom should test tattoo/alpha/universal layer wear, detach/reattach, BOM face
rendering, and attachment BOM texture updates.

## Exact COF-target body-part candidate age36 deployed (2026-10-18)

Build `F:\tasia-kirsten\build-bom-recovery-20261018-224735.log`: actual exit 0,
compiler errors 0; wearables/avatar/self/bake units compiled. Independent EXE/PDB
GUID+age match (37). Installed/archive hashes verified at `E:\TasiaViewerLatest`:
- EXE `B2E8EA34CD03A23F5B5FBB2307ABB4019B00F9E73B0A57A47F05A1E34B21D450`
- PDB `59809D9F192579400EAF9737158D28AACD81B363EE8BB472B0533E4CA898D71A`
Previous age36 backup: `E:\TasiaViewerBackups\before-legacy-fix-20261018-225709`.
Archive: `F:\tasia-kirsten\verified-builds\legacy-fix-20261018-225709`.
No running viewer at deployment; none launched/stopped. Five focused suites passed.

Legacy fixes included:
1. `sendLegacyAppearance()` now validates ALL active legacy bake slots via
   `getNumBakes()` (6 original on non-BOM OpenSim, up to 11 with BOM enabled),
   not just slots 0-5. Extended tattoo/universal layers on slots 6-10 are now
   sent to the region via AgentSetAppearance TE message when defined.
2. `LLOverrideBakedTextureUpdate` now uses `gAgentAvatarp->getNumBakes()` instead
   of hardcoded `BAKED_NUM_INDICES` (11), so composite enable/disable toggles
   only valid slots on OpenSim.
3. `LLViewerWearable::removeFromAvatar()` now clears texture entries to
   `IMG_DEFAULT_AVATAR` when detaching tattoos/alpha/universal layers, so stale
   textures no longer linger in the composite causing black faces/missing layers.

All 6 focused offline suites pass (initial wearables, cloud readiness, BOM limits,
bake upload, OpenSim compatibility, protocol compare). Runtime result pending:
Mom should test tattoo/alpha/universal layer wear, detach/reattach, and BOM face
rendering.

## Exact COF-target body-part candidate age36 deployed (2026-10-18)

Runtime update: Mom reports a black face on BOM and partial rendering. Live
snapshots now confirm shape=skin=hair=eyes=1, previous_loaded=1, uploads_pending=0,
head bake defined/GPU-ready/non-invisible, local data available. COF target-fetch
resolved two targets and reloaded the existing outfit. The simulator snapshot
fallback was NOT selected, so do not credit that new fallback for this session's
recovery or claim it was runtime-tested. Why the prior session's target fetch
remained unresolved is still unknown. Cloud gate is now passed in this session;
black head/BOM appearance remains unresolved. GPU-ready does not validate pixel
content, J2C channels, compositing or attachment BOM binding. Four material LLSD
warnings persist without a proven link to the black face. No new build started.

Build `F:\tasia-kirsten\build-bom-recovery-20261018-215741.log`: actual
exit 0, compiler errors 0; changed wearables unit compiled. Independent EXE/PDB
GUID+age match (36). Installed/archive hashes verified at `E:\TasiaViewerLatest`:
- EXE `65A9DE70C651FC3BDAFE937DCFF68D9ACE50487D27E20F630C210907B365FBCF`
- PDB `F950F57E1C2D10152EAA523E7B822A3274476C79C00CD941817624231B411029`
Age35 backup:
`E:\TasiaViewerBackups\before-cof-matched-bodyparts-20261018-220437`.
Archive: `F:\tasia-kirsten\verified-builds\cof-matched-bodyparts-20261018-220437`.
No running viewer at deployment; none launched/stopped. Five focused suites
passed. Runtime outcome pending; next user login should remain active 60s for
bounded COF fetch plus real simulator-worn snapshot. Inspect simulator body-part
counts, exact-target match/refusal, asset completion and readiness counts.
Do not claim the avatar is fixed without Mom's visual result.

## Age35 fails; exact COF-target matched asset repair building (2026-10-18)

Mom reports still broken. Live log confirms target fetch requested two targets,
both remained broken after the bounded wait; no simulator-worn snapshot was
requested/applied. Hair/eyes still zero; uploads complete, GPU state ready.
No recorded inventory HTTP failure/capability-missing/malformed response marker;
this does NOT prove the targets are permanently deleted or even requested from
the correct inventory owner. One morph-mask readback warning is a separate
unresolved signal, not evidence explaining the required-part count failure.

Extended the bounded recovery: after metadata-fetch exhaustion, query real
simulator worn data without modifying the outfit. Accept a requested body-part
asset ONLY if its simulator item ID exactly matches an unresolved COF target;
the full snapshot still requires all four parts and bounded unique valid data.
No match, incomplete snapshot, failed asset, filled slot or stale generation /
COF version means no commit. No defaults, link deletion or remote inventory edits.
The helper revalidates exact subset/mask/target/unique IDs before atomic commit.
Logs expose only four simulator body-part counts, match/refusal markers and mask.

Production helper and request/commit fixtures pass under ASan/UBSan, including
wrong target refusing commit and exact-target hair/eyes preserving existing
shape/skin/clothing. Five focused suites and whitespace check passed. Three
source hashes verified remotely; one Windows build running, NOT deployed yet.

## COF link-target candidate age35 deployed (2026-10-18)

Build `F:\tasia-kirsten\build-bom-recovery-20261018-214025.log`: actual
exit 0, full-log compiler errors 0; wearables/avatar/self/bake units compiled.
Independent EXE/PDB GUID+age match (35). Installed/archive hashes verified at
`E:\TasiaViewerLatest`:
- EXE `C2DFB5C13E56A9D3CD481FF811AA9D67E2B5CE1FCD83E875A9E8E6AAFED49B13`
- PDB `909C4303DD80AE6E8855D09B4D33853335442B7BFA7549DE25D9942D5E5636B1`
Age34 backup: `E:\TasiaViewerBackups\before-cof-link-targets-20261018-214840`.
Archive: `F:\tasia-kirsten\verified-builds\cof-link-targets-20261018-214840`.
No running viewer at deployment; none launched/stopped. Five focused offline
suites passed. Runtime result pending: Mom logs in for 45–60s; inspect numeric
hair/eye counts plus target-fetch/resolved/unavailable markers. This is not yet
a confirmed visual fix. Preserve inventory links and real wearable assets.

## Age34 still lacks system hair/eyes; COF target fetch building (2026-10-18)

Mom reports missing hair and eyes. Live age34 snapshots still show hair=0,
eyes=0 and no pending uploads. The guarded repair did NOT send a simulator
request: log explicitly says blocked by broken COF links. This proves a broken
link exists in COF, not that every broken link belongs to hair/eyes or that the
server permanently lacks those items.

Added a bounded target-metadata fetch before rejecting the COF as broken:
collect unique actual item-link targets (1..256), issue the standard inventory
item fetch once/login, and recheck four times at 8-second intervals. On resolved
links, reload existing COF without forced ordering/restrictions. On timeout,
preserve links/outfit and log only remaining count. Session/avatar/generation,
COF-version/completeness and new-outfit guards reject stale work. No link
deletion, replacement defaults, remote inventory edits or SL-path changes.
The target fetch uses a stack fetch observer only to issue requests; it is not
registered, retained or called asynchronously. Timers own no avatar pointer.

Expanded production-method ASan/UBSan fixture covers one fetch, stale timer,
bounded polling, permanent-unavailable stop and resolved-target COF reload.
Five focused suites pass; diff whitespace check passes. Source/header transfer
hashes verified. One Windows build running; new candidate not yet deployed.
Age34 remains installed until verified build and viewer closed by Mom.

## Required-part repair candidate age34 deployed (2026-10-18)

Windows build `F:\tasia-kirsten\build-bom-recovery-20261018-211955.log`:
actual exit 0, full-log compiler errors 0, wearables/avatar/self/bake units
compiled. Independent EXE/PDB GUID+age match (34). Installed/archive hashes
verified at `E:\TasiaViewerLatest`:
- EXE `5FFD5F7666707C62E083B2EC258E0C9B92C0D8DC034C2A2D0BD44B62522230FD`
- PDB `C33E6D5FD0F44706B1718D288CFF86F0972E79FF9C58B3273D0067847167BC18`
Previous age33 pair backed up at
`E:\TasiaViewerBackups\before-required-bodyparts-20261018-212726`.
Archive: `F:\tasia-kirsten\verified-builds\required-bodyparts-20261018-212726`.
Deployment confirmed no running viewer and did not start/stop one.
Runtime outcome pending. Next: Mom logs in, then inspect repair request/applied
or COF-reference/broken-link blockers plus numeric CloudReadiness snapshots.
This candidate must not be called a successful visual fix before that test.

## Age33 identifies actual cloud gate; required-part repair building (2026-10-18)

Three delayed live snapshots show shape=1, skin=1, hair=0, eyes=0,
uploads_pending=0, local_appearance=0, rez_status=0, fully_baked=1,
callbacks_loaded=callbacks_total=0, pending_attachments=0, waiting_meshes=0.
All defined baked slots are GPU-ready, including invisible bakes. Therefore the
missing required hair/eye wearables are a confirmed blocking check in this
session, NOT an invisible-texture GPU wait or pending upload. Their provenance
(absent COF references versus failed download) still needs distinguishing.

Bounded repair now starts after a COF outfit is applied, on legacy OpenSim only:
- If a required part is absent from loaded state AND the complete COF has no
  corresponding reference, request real simulator AgentWearablesUpdate data.
- Broken/incomplete COF or a present-but-unloaded reference blocks this path;
  do not hide failed asset downloads by inventing parts.
- Validate the authoritative full snapshot, then download only requested missing
  body parts. All requested downloads must validate before adding any.
- Preserve current shape/skin/clothing/attachments and all COF/inventory data.
  No defaults, inventory writes, COF links, or AgentIsNowWearing are created.
- One attempt per login, existing three request attempts/60s asset deadline,
  session/avatar/generation/COF-version/missing-slot guards. New outfit or a
  user-filled missing slot invalidates callbacks. SL/server baking unchanged.

Expanded compiled ASan/UBSan fixture executes production request-selection,
generation guard and commit methods: changed/incomplete/broken COF, existing
references, filled slots, failed assets, one-attempt bound and preserving
shape/skin/two clothing layers while adding real hair/eyes. Snapshot fuzz cases
still pass. Five focused suites and whitespace check pass. Source/header remote
hashes verified; one Windows build started. No repair binary deployed yet.

## Cloud-readiness diagnostic age33 deployed (2026-10-18)

Windows build exit 0, full-log compiler errors 0; llvoavatar.cpp and
llvoavatarself.cpp compiled. Independent EXE/PDB GUID+age match (33).
Installed/archive hashes verified at `E:\TasiaViewerLatest`:
- EXE `A9E049A4F842397BEB2C89F14C3EA8E64990B1056F0C05FB7F0CE97503A26B16`
- PDB `3221BE69E71A1F3FB5D8C67A390EA9DBFAD737830AA5F74D153B160D9008C0C8`
Build log: `F:\tasia-kirsten\build-bom-recovery-20261018-204015.log`.
Backup age32: `E:\TasiaViewerBackups\before-cloud-readiness-20261018-204607`.
Archive: `F:\tasia-kirsten\verified-builds\cloud-readiness-20261018-204607`.
No viewer launched/stopped. Runtime diagnostic capture awaits Mom's 45s login.
This diagnostic does not fix clouding or change the readiness decisions.

After age32 cloud report, added diagnostic-only snapshots to getHasMissingParts()
and updateIsFullyLoaded(). No gate bypass, texture substitution or appearance
policy change. OpenSim self only, delayed 30 seconds, capped at 12 samples per
site/process. Fields are numeric body-part counts, local/baked/GPU availability,
invisible boolean, callback/attachment counts and existing load gates. No UUIDs,
names, session identifiers, capability URLs or raw visual parameters.

Actual getHasMissingParts() compiled/executed with synthetic state under
ASan/UBSan: original return values preserved; missing part/hair/local/GPU cases,
SL diagnostic suppression and 12-sample cap passed. BOM, upload and OpenSim
compatibility suites also passed; diff whitespace check passed.

Initial stdin-payload source transfer timed out; fresh remote hashes proved no
changes applied. Switched to scp ZIP plus existing compare-before-write manifest
semantics; corrected PowerShell 5.1 nested-array parsing before source writes.
Both source hashes verified against local files. One incremental Windows build
completed and the diagnostic pair was deployed as recorded above.
Next user test after verified deployment: stay logged in/clouded at least 45s
so delayed diagnostics capture state. This is a diagnostic, not a visual fix.

## BOM recovery deployed — age32 (2026-10-18)

**Runtime result: Mom reports CLOUD. Age32 is not a confirmed visual fix.**
Read-only completed-session inspection found viewer closed, COF path selected,
and bounded markers for encoding (five components, 542 bytes), upload completion,
invisible-alpha assignment and appearance send. Marker counts are not per-slot
totals. No logged upload exhaustion/readback/HTTP/decode failure; absence of these
does not establish all textures loaded. Four material LLSD warnings remain, with
no proven connection to clouding. Cloud-reason messages are ordinary INFO and
were absent (this tree compiles ordinary INFO out). Do not infer missing body
parts, cache corruption or an invisible-slot cause from these logs alone.
Next: identify the actual failing self-avatar readiness gate and slot/GPU state
with bounded diagnostics; no further speculative rebuild/deploy or cache clear.
The age31 pre-recovery pair and known-visible age28 archive remain preserved.

- Windows build: `F:\tasia-kirsten\build-bom-recovery-20261018-201344.log`;
  actual exit code 0, full-log compiler-error scan 0, avatar/bake units compiled.
- Independent PE/PDB GUID+age comparison passed, age32.
- EXE SHA256: `1A23171AFA5AEDB1127437AC82B2B3185D613E64DE836F7E0B02569F61ED3697`.
- PDB SHA256: `59A1CC37EA6D3A10C60E89F30CBB9FC0551BDB8E9575B7246F22029929BED4D2`.
- Installed both at `E:\TasiaViewerLatest`, with installed/archive hashes checked.
- Previous pair: `E:\TasiaViewerBackups\before-bom-recovery-20261018-202132`.
- Archive: `F:\tasia-kirsten\verified-builds\bom-recovery-20261018-202132`.
- Ten focused offline suites passed (including protocol comparison and new BOM
  limit/pending fixture); `git diff --check` passed. No commit or push.
- Suspend did not prevent build completion. No viewer launched or terminated.
- Next exact action: Mom tests tattoos/universal layers and BOM mesh appearance
  in this installed candidate. Live rendering remains unverified; do not equate
  build success with a confirmed visual fix. Preserve DX11, SL Agni/Aditi,
  account storage, unrelated changes, and original avatar picking behavior.

## Age29 rolled back to Age28 (2026-10-18 ~16:54 UTC)

Mom confirmed viewer closed. Rollback executed:
- Restored exact age28 pair from `E:\TasiaViewerBackups\before-invisible-alpha-20261018-164319`
  (EXE 82CD04FBB7E7838E64738ABB836B5580902FB6443CE6E90324431347A1EE465F /
  PDB 818A554337C9891D20BB63B50FEC657335AE12F5C0A876977E73DCA26D75CAD2).
- Safety backup of regressed age29 pair at
  `E:\TasiaViewerBackups\before-rollback-alpha-20261018-165429`.
- All staged/installed/archived hash checks passed; no viewer process interference.
- Resulting installed pair age28 (symbols age28) back at `E:\TasiaViewerLatest`.

Installed candidate age28 restored at `E:\TasiaViewerLatest`. Viewer NOT launched
by agent. Mom's next test PENDING. Alpha invisible-branch fix remains in source for
future investigation but is NOT in the currently deployed candidate.

## Body-part validation + BOM/Tattoo support added (2026-10-18)

Added validation to `finishLegacyInitialWearables()` to ensure all 4 required body parts
(shape, skin, hair, eyes) are present in the initial wearables snapshot. If any are
missing or the snapshot size is out of bounds, the commit is aborted with a clear
log message, and the previous avatar state is preserved.

Correction of earlier inaccurate BOM/build report:
- `BAKED_NUM_INDICES` is already eleven, not six. `BAKED_LEFT_ARM` is six,
  marking the first extended slot. Ordinary tattoos are WT_TATTOO, not WT_ALPHA.
- The WT_UNIVERSAL creation guard already existed; it was not added here.
- The corrected picking header had not transferred to Acer. Five parameters
  were missing there. Subsequent member-access errors cascaded from C2511;
  collision members do exist in the LLAvatarAppearance base class.
- Restored the original collision-volume, attachment and name-tag picking;
  replacing this with a bounding-box-only result was an unrelated regression.
- Removed the unused/uninitialized region mMaxBakes member. getNumBakes()
  uses eleven slots for SL or advertised BOM, six for other OpenSim regions.
- Extended local upload eligibility, pending completion and baked-TE assignment
  to negotiated slots. Required original six TE readiness checks remain intact;
  unused optional extended slots must not force a cloud.
- `/t:Clean` and `/t:Rebuild` passed as the wrapper's fourth argument were
  interpreted as local payload filenames. The generic wrapper exception did
  NOT establish a credential failure. No clean build is needed for this fix.
- A local hash written into a preflight is not remote sync evidence. Recovery
  uses fresh Acer hashes and the transfer's existing compare-before-write guard.

Before recovery, Acer inspection confirmed installed age31 EXE
`6B3E6C76417AEC1D738D8D4DC1F2EA38F5890656F774BD0F2A32DD7FF8532F48`,
not age28. Age31 contains the invisible-alpha branch. Nothing was committed.
Nine focused offline suites passed in recovery, including the new compiled,
ASan/UBSan BOM slot and AUX3-pending fixture. Windows recovery build/deployment
is recorded separately below; offline fixtures do not establish runtime success.

## Age29 REGRESSION reported; rollback prepared, blocked by running viewer

## Fully invisible alpha bake: missing semantic branch restored (2026-10-18)

After Mom confirmed visible avatar with some black surfaces, read-only comparison
found a specific omitted reference behavior. `LLTexLayerSet::render()` sets
mIsVisible=false for an invisible alpha mask and skips color compositing. The
reference buffer handles this by setting IMG_INVISIBLE without encoding/upload.
Our new upload stage had unconditionally read/encoded this invisible composite.
This is a concrete alpha compatibility gap; NOT proof it explains every black
face or the four material warnings.

Added `completeInvisibleBake()` to LLViewerTexLayerSetBuffer. After beginning an
upload revision, before DX11 readback or J2C encode, a fully invisible layerset
resolves its completion to IMG_INVISIBLE. Normal generation/cancellation checks
and baked-TE assignment still apply; visible/partial-alpha layers keep the normal
RGBA+morph-mask route. Invisible completion logs its own bounded stage message,
not a misleading successful-network-upload message. No magic texture ID logged.

Extended `tasia_bake_upload_test.py` to compile/execute the REAL new helper with
visible and fully-hidden fixtures and the actual completion mailbox; verifies
invisible selection and pre-readback/pre-encode routing. ASan/UBSan PASS, as do
focused readback and initial-wearable suites; diff check clean. No real GPU or
new visual success claimed.

Exactly two remote source deltas, both transferred/hash-verified:
llviewertexlayer.cpp and .h. Source backup:
`F:\tasia-kirsten\source-backup-opensim-20261018-163101`.
Windows build COMPLETE: full264-line log
`F:\tasia-kirsten\build-opensim-compat-20261018-163111.log`, last write
2026-10-18T14:42:08Z after transfer, 0 compiler/linker errors and confirmed
llviewertexlayer.cpp compilation. Same two LNK4006 and one LNK4088 warnings.
New matching EXE/PDB symbols age29. EXE
`D47F4EB155318EE57B92229A5C82C7AB7762FF94E4458D3096345082FD8EFBB8`, PDB
`880DADC82183F6E607F88969992CA9D014EF234A4E89AAB7CDFE1B5EA5963D79`.
Invisible-alpha stage string confirmed in EXE before install. Viewer was
closed at verification/deployment and was not launched/stopped by agent.
Installed and hash-verified at `E:\TasiaViewerLatest`. Verified archive:
`F:\tasia-kirsten\verified-builds\invisible-alpha-20261018-164319`.
Backup of Mom-confirmed partially working age28:
`E:\TasiaViewerBackups\before-invisible-alpha-20261018-164319`.
Visual result PENDING. Local receipts `/tmp/opencode/acer-alpha-*.json`.

## Mom's age28 runtime result: avatar visible, some black surfaces (2026-10-18)

Mom reports the avatar now works/appears, but parts are black; her outfit uses
alpha and Bakes on Mesh. This is PARTIAL visual success, not complete BOM/alpha
compatibility. Do not suggest clearing cache or removing outfit items as a fix.

Read-only sanitized inspection while viewer remains running (log last write
2026-10-18T14:26:59Z, 87090 bytes) found:

- COF-primary branch selected (1 stage record); fallback snapshot not selected.
- Five-component J2C encode and upload-complete stage records present (2 each).
  Encoded-size stage samples 542/416 bytes. These small payloads justify checking
  actual layer contents, but DO NOT prove blank/black pixels by size alone.
- Legacy appearance sent stage present (1).
- No recorded color/morph readback failure, encode/metadata failure, exhausted
  retry, DECODE FAILED or wearable-download failure in this available log.
- Four material-LLSD deserialization warnings remain, not attributed to any face.

Stage messages use LL_INFOS_ONCE: counts are NOT per-slot upload totals. Viewer
is still running, so this is not a completed-session log. No edits/rebuild or
new deployment made for this inspection.

Source trace: `LLViewerObject::getBakedTextureForMagicId()` maps mesh BOM IDs to
`avatar->getBakedTexture()`, `updateMeshTextures()` refreshes attached objects'
bake textures; alpha wearables are in the avatar texture dictionary. Presence
of these paths is not proof of correct rendering. Current local-upload stage
supports only six original bake slots; LEFT_ARM/LEFT_LEG/AUX slots remain outside
the upload/sender range. Need identify the actual black face's bake slot and
material/alpha mode before expanding or attributing a cause.

## Initial worn-item fallback implementation (2026-10-18)

User requested finishing the remaining worn-item acquisition. Implemented,
offline tested, compiled on Windows, archived and installed as a runtime-test
candidate. Mom's visual confirmation PENDING; no avatar-success claim.

- Existing COF fetch remains primary. AFTER its category/item fetch completes,
  OpenSim + central-bake0 + known-complete empty COF may request the initial
  worn-item snapshot. Nonempty/unknown/incomplete COF and Agni/Aditi retain the
  existing appearance path. Added a session guard to the deferred COF callback.
- Added AgentWearablesRequest and AgentWearablesUpdate handler. Verified local
  i-Grid `LLClientView.cs:4471-4504` sends actual AgentID, SessionID and all worn
  layers, including multiple entries of a type. Validate current session/agent,
  current simulator sender, bounded blocks, item/asset IDs, type range and four
  unique body parts before starting downloads. Three requests max, 8s apart.
- Snapshot can contain all 60 clothing layers plus four body parts. Multiple
  clothing items of the same type retained in message order. Duplicated item
  or asset IDs are conservatively rejected because LLWearableList currently
  shares one mutable instance per asset and cannot faithfully assign that asset
  to two different inventory items. This limitation is explicit, not silent
  layer loss.
- Asset requests carry a generation predicate into LLWearableList. It runs
  BEFORE importStream can access the avatar, not only in the final callback;
  cache-hit path checks it too. No stale-session callback can apply this outfit.
  Existing callers retain their old behavior through an empty default predicate.
- Collect all assets before applying anything. Type/asset mismatch, any failed
  download, or a 60s deadline aborts without creating replacements or editing
  inventory. Re-check COF at commit; if it became populated, prefer it.
- Successful commit changes client-side worn state, suppressing inventory
  autosave/update callbacks, reads existing permissions when available, copies
  wearable params/textures, invalidates composites and requests local baking.
  No synthetic inventory items, COF links, default skins or dummy worn IDs.
- Bounded LocalBake diagnostics distinguish COF-primary, snapshot accepted,
  assets loaded, encoded five-channel J2C, upload completed and appearance sent.
  No new diagnostics print asset IDs, session IDs, hosts or capability URLs.

`tasia_initial_wearables_test.py` compiles production snapshot validator,
generation/session guard, commit and callback with fixtures under ASan/UBSan.
12000 snapshot cases; multiple shirts, all required body parts, missing/duplicate/
out-of-range entries, 60-layer bound, session changes, SL/server-bake isolation,
incomplete/failed download, concurrent COF priority, no partial application,
stale callback and successful complete commit PASS. Eight prior suites also
PASS; diff check clean. No live asset/network/appearance success claimed.

Preflight54 unique source paths; 8 changed files transferred, hashes verified.
Source backup `F:\tasia-kirsten\source-backup-opensim-20261018-161051`.
Windows build full log `F:\tasia-kirsten\build-opensim-compat-20261018-161110.log`:
147 lines, 0 compiler/linker errors, last write2026-10-18T14:17:31Z after transfer.
Confirmed compilation of llagentwearables, llwearablelist, llviewertexlayer and
llstartup. Same two LNK4006 and one LNK4088 `/FORCE` warnings remain.
New EXE `82CD04FBB7E7838E64738ABB836B5580902FB6443CE6E90324431347A1EE465F`,
PDB `818A554337C9891D20BB63B50FEC657335AE12F5C0A876977E73DCA26D75CAD2`,
symbols GUID/age28 match (previous compiled stage27). Six diagnostic strings
verified inside the binary before installation. Installed pair hash-verified
at `E:\TasiaViewerLatest`; viewer was not running and was NOT launched by agent.
Verified archive:
`F:\tasia-kirsten\verified-builds\localbake-wearables-20261018-161859`.
Verified backup of previously installed age25 diagnostic:
`E:\TasiaViewerBackups\before-localbake-wearables-20261018-161859`.
Build/verification/deploy receipts `/tmp/opencode/acer-wearables-*.json` and
`acer-wearables-build.txt`. Prior compiled age27 archive also retained.

Remaining verification: candidate runtime test by Mom. Original six bake slots supported; universal bakes need
feature negotiation. No claim that this adds all legacy worn-state persistence
messages/cache queries or resolves unrelated GLTF, terrain material and teleport
notification bugs. Those remain separately tracked after body verification.

## Local-bake readback/upload implementation (2026-10-18, after foundation)

Implemented a further stage; Windows compilation PENDING, not installed:

- `llrender/llbakereadback.h`: bounded DX11 color readback with explicit RGBA8/
  BGRA8 handling, rectangle/resource/sample validation, alpha extraction and
  five-channel RGBA+morph-mask packing. Uses DXReadback's tightly packed
  top-left CPU convention, consistent with LLImageDX::readBackRaw. No raw GL
  read in this path. Real GPU orientation remains a visual verification item.
- `llappearance/lltexlayer.cpp`: DX branch of morph-mask generation now uses
  that helper instead of raw glGetTexImage/glReadPixels. Failure frees the
  allocation and does not mark/cache an invalid mask. Other renderer branch
  retained, not used by the Windows DX11 build.
- `newview/lltasia_bake_upload.h`: explicit revision/ticket upload policy,
  bounded seven attempts, exponential backoff, stale/cancelled-result rejection.
- `llviewertexlayer.cpp/.h`: upload only for OpenSim regions with central-bake
  version zero, original six legacy slots, loaded wearables and final local
  textures. Five-component J2C encode with RGBHM comment; verify encoded
  metadata/components/dimensions before sending. Prefer UploadBakedTexture;
  asset-storage fallback when capability absent or at final retry. Capability
  staging writes exact binary bytes (not LLBufferedAssetUploadInfo's default
  string terminator). Callback mailbox has shared lifetime + mutex and contains
  no raw avatar/layerset pointer. Results are applied by the owning composite,
  not the network callback. HTTP queue cancellation signals failure via its
  upload-info destructor. Unworn skirt uploads are cancelled.
- `llvoavatarself.cpp/.h`: pending-upload checks, completed baked TE assignment,
  OpenSim-only `sendLegacyAppearance()` matching AgentSetAppearance layout.
  Guards: current region/avatar, non-server-baking OpenSim, loaded wearables,
  no pending uploads/editing, all required legacy baked TEs, 1..255 transmitted
  params. Body size without SL collision offset, serial starts at1/skips0,
  local nonbaked TEs hidden through existing sendAppearanceMessage(). No
  fabricated cache hashes. Switch back from local composites after sending.

New tests compile production code with minimal dependency fixtures:
`tasia_bake_readback_test.py` checks pixel counts/bounds, RGBA/BGRA, pitched-row
fixture, alpha/five-channel byte output and canaries, unsupported resources and
read failure. `tasia_bake_upload_test.py` executes actual upload policy, callback
mailbox (including late concurrent completions after owner release), and actual
usesLegacyBakes/sendLegacyAppearance bodies. Exercises stale response, outfit
change, cancellation, retry exhaustion/backoff, region/SL/server-bake isolation,
wearables/pending/missing TE guards, skipped optional skirt, parameter filtering,
packet blocks, serial wrap, failed TE packing. Both PASS under ASan/UBSan.
All six existing offline suites also PASS; diff check clean. Fixtures are NOT
actual D3D/J2C/network/live-avatar verification.

Preflight 51 unique paths; only 9 changed files transferred, including two new
headers. CMake changes verified against remote bytes: only new header entries
and the previously requested region-compat header-list tidy differ. Source
backup `F:\tasia-kirsten\source-backup-opensim-20261018-155128`.
Windows build COMPLETE, independently checked FULL 318-line log:
`F:\tasia-kirsten\build-opensim-compat-20261018-155140.log`, timestamp
2026-10-18T13:59:44Z (after source transfer). 0 compiler/linker errors;
confirmed compilation of lltexlayer, llviewertexlayer and llvoavatarself.
Two LNK4006 duplicate-symbol warnings and one LNK4088 `/FORCE` warning remain,
as with the foundation build; no runtime success inferred.
EXE `CB007FF4813629C268CA0D379D550A44B6FE0C343AD8B61F741A14BF3A972D02`,
PDB `0CE406C561ADB2C0A2367AA55BEE9B9B55E04F0BA3F1B14B1C41E1C669A3D9F1`,
symbols match, age27 (was26). Local build output and verification records under
`/tmp/opencode/acer-localbake-{build.txt,log-check.json,symbols.json}`.
No installation or agent-initiated interactive test. Installed age25 diagnostic
unchanged; next work is initial worn-item acquisition below.
Verified EXE/PDB archived with hash recheck at
`F:\tasia-kirsten\verified-builds\localbake-upload-20261018-160043`.

Known remaining dependency, found by source comparison while building:
Kirsten startup routes existing accounts through COF fetch only and has no
AgentWearablesRequest / AgentWearablesUpdate handler; the reference has both.
Legacy worn-item acquisition/fallback still needs porting or proof that COF
loads the actual body parts. Do not advertise this stage as a finished avatar
fix merely because it compiles. Extended universal bake negotiation, lowres
uploads and wearable-cache reuse remain outside this initial six-slot stage.

## Legacy-bake foundation repair and compile checkpoint (2026-10-18)

User authorized continued implementation without another planning prompt.
The initial occurrence-count test was insufficient: it passed commented-out
overload declarations, missing braces and undefined variables. Corrected:

- Restored two missing braces in `LLCharacter::animateTweakableVisualParams`.
- Made both sets of texture-layer bool overloads live declarations with
  `override`, matching their actual definitions.
- Replaced undefined `drivenp` with `&driven` in three driver call sites.
- Added `upload_result` to `LLVOAvatarSelf::wearableUpdated`, with a false
  default for existing callers; preserved one-argument visual-param call
  compatibility using false defaults and removed stale `stopAnimating()`.
- Updated explicit animation, sex-layer, wearable-removal and clothes-color
  callers, including `llpaneleditwearable.cpp` omitted from earlier staging.

Replaced `tasia_legacy_bake_port_test.py`'s occurrence-count parity claims with
compilation/execution of the actual LLVisualParam header and four modified
methods against small dependency fixtures. Exercises virtual dispatch,
next-param weight/animation forwarding, false-default compatibility and final
animation weight. Also checks braces, live derived override declarations and
absence of the undefined driver variable. PASS. This is NOT a full-viewer or
GPU/local-bake test. All five other offline suites PASS; diff check clean.

Fixed `/tmp/opencode/acer-viewer-source-check.ps1`: an earlier rewrite had
removed its entire file-reading/JSON-output body. Restored hash-only output,
removed the trailing PowerShell array comma, included self-avatar and edit
panel sources. Preflight checks 44 unique paths. Staged ONLY 23 changed files,
not unchanged sources; transfer hashes verified. Source backup:
`F:\tasia-kirsten\source-backup-opensim-20261018-150716`.

Control Windows build launched with unchanged established Release command;
local result target `/tmp/opencode/acer-foundation-build.txt`.
Build completed and independent FULL log scan confirms 0 compiler/linker errors
across 789 lines, not just the wrapper's last 130. Remote log:
`F:\tasia-kirsten\build-opensim-compat-20261018-150741.log`, last write
2026-10-18T13:18:04Z (after source transfer). Confirmed compilation of
llcharacter, llvisualparam, lldriverparam, lltexlayerparams, llwearable,
llvoavatar, llvoavatarself and llpaneleditwearable.

Fresh EXE `05E9A69FD01661C912606DD13373802B60E931131EB295D44A0044CACFA96AB9`,
PDB `C95A51A2BEA90E6E2A79A1FD73FBE63CA58830C1E93D1A23B2BA03AB829CBAD7`,
matching GUID/age 26. Full log has two libxml2 duplicate-symbol LNK4006 warnings
and one LNK4088 `/FORCE` warning; runtime validity remains unverified. Do not
chase unrelated warnings or claim runtime success from this build.

NOT installed as an avatar fix: the local bake pipeline is incomplete.
Installed viewer was not modified or launched.

Next implementation dependency verified in reference source:
`LLVOAvatarSelf::invalidateComposite(..., upload_result)` requests uploads only
when appropriate; reference `LLViewerTexLayerSetBuffer` tracks pending uploads,
encodes RGBA plus morph mask as five-channel J2C, uploads, then sets a new baked
texture on completion. Kirsten currently lacks those upload methods/state.
DX11 mechanisms already available: `LLRenderTarget::getDXColorTexture(0)` and
`DXReadback::readPixels()` (verify dimensions, format and orientation). Both
trees are DX11; OpenGL-era identifier names alone do not classify a renderer.

Corrections to earlier unsupported conclusions: zero diagnostic hits do NOT
prove HTTP transport healthy, and a blank decode reason does NOT prove an
empty body/404 or associate that failed texture with the self avatar. Lack of
server baking does NOT prove baked assets are absent. GLTF warnings remain
unattributed to any particular attachment. Preserve these as hypotheses until
request/appearance/render evidence connects them.

## Parcel errors expose a broader region-size mismatch (2026-10-18)

Mom asked to prioritize the rejected parcel chunks. Read-only tracing confirms:

- i-Grid `LandManagementModule::SendParcelOverlay()` (:1387-1533) sends one
  byte per 4x4m land cell in fixed 1024-byte packets, incrementing sequence IDs.
  Sixteen packets are consistent with 512x512m (or an equal-area rectangular
  region); the live dimensions have NOT yet been independently measured.
- `LLWorld::mWidth` is fixed at 256 (`llworld.cpp:79`) and `addRegion():195-199`
  constructs every region using that fixed width. The constructor passes it
  directly to the parcel-overlay allocation (`llviewerregion.cpp:718-738`).
  The sequence guard therefore protects only a 4096-byte ownership array.
- The defect is not limited to parcel colors: region containment uses `mWidth`
  (`pointInRegionGlobal():2257-2277`). World lookup, land height and step-height
  resolution depend on that containment (`llworld.cpp:327-338,513-547`).
- `LLViewerObject` motion interpolation (:2548-2586) checks region width,
  clips predicted position to known regions and clears predicted velocity when
  it hits an apparent empty edge. A real larger-region position beyond the
  viewer's false 256m edge can therefore take the outside-world/clipping paths.
  This is a concrete potential connection to movement/presentation, NOT proof
  it explains Mom's specific missing avatar or all inability to walk.
- Compiled the ACTUAL extracted `pointInRegionGlobal()` with minimal vector
  stubs: (300,300) is rejected with stored width256, accepted at512, while
  (128,128) remains accepted at256. Offline check PASS; files under
  `/tmp/opencode/parcel-region-size-check`. Not a viewer/runtime test.

Next fix scope: restore advertised dimensions from login and HTTP region/
teleport events through world/region construction, terrain and parcel allocation/
bitmap consumers; accept 1024-byte chunks only when their offsets fit the actual
allocation. Retain bounds checks and preserve SL's 256m behavior. Do NOT just
raise the sequence limit, enlarge only the overlay, infer dimensions from an
untrusted sequence ID, or blindly copy the reference's unguarded memcpy.
No application code, deployed files, settings or builds changed in this trace.

## First new-build session log review (2026-10-18, ~01:52 local)

Mom requested read-only log inspection after trying the installed build.
`E:\TasiaViewerLatest\Kirstens-S24.exe` still verifies as the NEW
`8E31026BBC67A5EC556D350778B193DC594DFADEBCCF4538F695AEFC39D753CB`.
Current log records startup at 23:50:22 UTC, region data at 23:50:32-33,
and user-confirmed quit at 23:51:04. No viewer process remained at the generic
process check. No agent launch/termination, source/config edit or rebuild was
performed during inspection.

- Previous session `.old`: 40 missing AgentAppearanceServiceURL warnings plus
  40 unable-to-determine-baked-URL warnings. New session `.log`: ZERO of each.
  This is a concrete warning-pattern change consistent with restored legacy
  texture routing, NOT proof textures downloaded or avatar/body/attachments
  became visible.
- New session still rejects overlay sequence IDs 4..15 (12 warnings). Larger
  region handling remains incomplete; keep the memory-safety bounds guard.
- Simulator statistics continue arriving. No movement-complete or forced-
  disconnect/event-poll-failure text was recorded; user-confirmed quit was.
- CRITICAL observability finding: both local and actual Acer build source
  `indra/llcommon/llerror.h:441-457` hardcode `#define LL_DISABLE_INFOS 1`.
  Normal `LL_INFOS(...)` bodies are compiled under `if (false)`. This explains
  predominantly WARNING logs despite packaged default-level INFO. It hides
  `process_agent_movement_complete()` and appearance acceptance/stale-message
  diagnostics. Merely changing logcontrol.xml to INFO cannot recover those.
  `LL_INFOS_ONCE`/DEBUG have separate macros; do not infer every INFO-like
  record is impossible, or infer missing packets from absent normal INFO logs.
- No evidence establishes CEF Google GCM errors, unknown statistics or the
  extra inventory-folder warning as causes of missing movement/avatar.

What remains unverified: actual self-avatar creation, accepted legacy
appearance, baked texture completion, root-presence and outbound movement state.
Last attempted: new-session log/hash/source-logging inspection, completed.
Do not change: source/protocol or global logging header speculatively; no
additional build/test was authorized by this log-inspection request.
Next exact action: report limits honestly; if diagnostic changes are approved,
restore only targeted lifecycle/presence/appearance traces using logging that
is not compiled away, rather than request another blind runtime test or globally
reenable all INFO messages. Full local bake/upload and larger-region work remain.

## NVMe compatibility build installed (2026-10-18, 01:48 local)

Mom approved installation and allowed stopping the target viewer if needed.
The first attempt correctly stopped before changes because the old viewer was
running. At the next check the target was already closed (zero matching
processes), so no process was killed. Installed the verified archived EXE/PDB
into `E:\TasiaViewerLatest`, staging and checking SHA-256 before replacement
and verifying both installed files afterwards:

- EXE: `8E31026BBC67A5EC556D350778B193DC594DFADEBCCF4538F695AEFC39D753CB`
- PDB: `DB2BF46AF11D1EB4443B8DFC84C6B8D4646DA03D7D7E414D534106BDCFCB749A`
- Previous EXE backed up and hash-verified at
  `E:\TasiaViewerBackups\before-opensim-compat-20261018-014826`.
  No previous PDB existed in that package. Deployment receipt: `deployment.json`
  in the same backup directory. Other package files/settings were untouched.

What is done: new build installed, EXE/PDB identity previously verified and
installed hashes matched. Viewer was NOT launched by the agent.
What remains unverified: actual OpenSim appearance/movement/teleport behavior;
larger-region and full local-bake limitations remain as documented below.
Last attempted: deployment and installed artifact verification, success.
Do not change: preserved old EXE, user settings/accounts or unrelated packages.
Next exact action: Mom launches `E:\TasiaViewerLatest\Kirstens-S24.exe` and tests
avatar/attachments and walking before teleport; inspect the new session's logs
after her report. Historical statements below that E: was unchanged apply only
before this explicitly approved deployment.

## Live/previous viewer log inspection (2026-10-18)

Read existing logs in place on Acer, exporting only sanitized diagnostic
excerpts/counts (URLs/UUIDs/credential-related lines excluded). No settings,
packages, source, or processes were changed. A first CEF read encountered a
sharing lock; retried using FileShare.ReadWrite, without stopping the viewer.

- Mom's running process 64220 is `E:\TasiaViewerLatest\Kirstens-S24.exe`, started
  2026-10-17 23:43:23 UTC. Its package EXE still hashes to the OLD
  `42402BB58195E3A50310400C0FE6241231FA6B42A0BCFD5C57A122635D8E9DD6`,
  not the newly built `8E31026B...D753CB`. These logs are NOT a test of the new
  compatibility fixes; no approved deployment has occurred yet.
- Current log:
  `%APPDATA%\Tasia Viewer DX\logs\TasiaViewer-LOG.log`; previous session is
  `TasiaViewer-LOG.old`. Initially current file was empty, then buffered log
  records appeared during read-only inspection. Do not equate an initial
  zero-byte live log with a permanently missing log.
- Current login at 23:43:42-46 UTC: 40 warnings that
  `AgentAppearanceServiceURL not set - Baked texture requests will fail`, paired
  with 40 `unable to determine URL for te` warnings at OLD llvoavatar.cpp lines
  5951/2647. Previous session has the same counts. This corroborates the legacy
  baked-texture URL gap targeted by the new build; it does not prove the new
  host-fetch path works at runtime or explain all movement failures.
- Current session rejects exactly overlay sequence IDs 4 through 15 (12
  warnings); previous session also rejected 4..15. This is concrete live
  evidence of incomplete larger-region overlay handling, consistent with
  16 chunks, not evidence those server chunks are malformed. The existing
  allocation guard must remain until dimensions/allocation support is restored.
  Do not simply increase the accepted sequence limit on the 4096-byte buffer.
- Unknown simulator stat IDs 16,21,22,36,37 repeat: simulator packets continue
  arriving/decoding. This does not establish working AgentUpdate or root presence.
- Inventory warnings: one extra type-23 folder outside root, and Calling Cards
  version unknown. No evidence yet connects these to the missing avatar.
- No movement-complete, forced-disconnect, or event-poll-failure text was found
  in these two logs at inspection. They are overwhelmingly WARNING-level
  records despite packaged default-level INFO; absence of INFO events cannot
  establish that the associated packets were absent. The previous session logs
  explicit user quit at 22:36:37 UTC, not a proven forced disconnection.
- CEF shows Google GCM `DEPRECATED_ENDPOINT` errors; no evidence ties these
  browser push-service errors to simulator presence or avatar movement.

What remains broken/unverified: new EXE not deployed/tested; variable-region
support incomplete; actual movement/presence/disconnect cause unresolved.
Last attempted: read-only current/rotated viewer log inspection, completed.
Next exact action: with approval and after Mom closes the old viewer, back up
and deploy the verified new EXE/PDB to the NVMe package for her test. Do not
silently replace a running executable or claim these old-build logs validate it.

## Acer compatibility build completed successfully (2026-10-18)

Final wrapper completion confirms `BUILD_EXITCODE=0` in
`F:\tasia-kirsten\build-opensim-compat-20261018-011716.log`.
All four expected changed compilation units are present: llagentwearables.cpp,
llviewerregion.cpp, llstartup.cpp and llvoavatar.cpp. The new EXE hash is
`8E31026BBC67A5EC556D350778B193DC594DFADEBCCF4538F695AEFC39D753CB`;
PDB hash is `DB2BF46AF11D1EB4443B8DFC84C6B8D4646DA03D7D7E414D534106BDCFCB749A`.
The independently verified CodeView/PDB GUID and age match (details below).
Both artifacts were archived with matching SHA-256 at
`F:\tasia-kirsten\diagnostic-symbols\opensim-compat-20261018-011716`.
Existing LNK4006/LNK4088 `/FORCE` warnings remain unresolved technical debt.

What is done: source transfer, actual Release compilation/link, changed EXE,
matching symbols and archive verification. No viewer launched or stopped.
What remains unverified: OpenSim avatar/movement/teleport/disconnect at runtime;
this successful build is not evidence those symptoms are fixed.
Last attempted: final build-result inspection, success. No compiler failure.
Do not change: existing user sessions, packages, diagnostics or SL grid behavior.
Next exact action: obtain deployment approval before updating
`E:\TasiaViewerLatest`; its normal EXE remains unchanged. Interactive testing
is Mom's step after an explicitly approved deployment.

## Acer OpenSim compatibility build — source transferred (2026-10-18)

Mom approved the Windows build and use of the stored Acer credential for source
transfer/build/verification. No viewer launch or login is authorized by this
build operation; interactive testing remains hers.

- Compared all six changed application files against Acer's source before
  transfer: only the intended compatibility changes differed; no unexplained
  remote edits were overwritten. Transferred `llagentwearables.cpp`,
  `llviewernetwork.h`, `llviewerregion.cpp`, `llstartup.cpp`, `llvoavatar.cpp/h`.
  SHA-256 matched local bytes after transfer and again before building.
- Original remote files are preserved at
  `F:\tasia-kirsten\source-backup-opensim-20261018-011515`.
  Staged source/manifest: `F:\tasia-kirsten\source-stage-opensim-20261018-011515`.
- Preflight: VS2022 x64 build cache, `TASIA_MEMORY_DIAGNOSTICS:BOOL=OFF`, no
  active cmake/MSBuild or viewer process observed. Built and normal NVMe EXE
  both initially hash to
  `42402BB58195E3A50310400C0FE6241231FA6B42A0BCFD5C57A122635D8E9DD6`.
- Two remote invocation failures occurred BEFORE any CMake compilation: the
  Windows shell rejected an overlong encoded command; then cmd.exe quoting of
  the CMake path with spaces failed. Corrected only the local remote-build
  wrapper: short stdin bootstrap and direct PowerShell `Start-Process` for CMake.
  No viewer source edits or speculative fixes resulted from either failure.
- Focused compatibility regression, 12,000-input codec comparison and all six
  source-transfer hash checks passed locally again.

Status update at 23:40 UTC: actual compilation/link finished in
`F:\tasia-kirsten\build-opensim-compat-20261018-011716.log`, including
`llviewerregion.cpp`, `llvoavatar.cpp` and `llstartup.cpp` visible in the tail.
The log stops at copying msquic, but CMake and the root MSBuild process have
exited; remaining seven MSBuild processes explicitly have `/nodemode:1
/nodeReuse:true`. The original PowerShell `Start-Process -Wait` wrapper is
waiting for reusable descendant workers, so its final exit code/archive record
has not yet been collected. Earlier wording that copy steps were still running
was not established. No workers or viewer processes have been killed.

Artifact verification completed independently (no viewer launch):
- EXE SHA-256:
  `8E31026BBC67A5EC556D350778B193DC594DFADEBCCF4538F695AEFC39D753CB`.
- PDB SHA-256:
  `DB2BF46AF11D1EB4443B8DFC84C6B8D4646DA03D7D7E414D534106BDCFCB749A`.
- EXE CodeView RSDS GUID bytes and PDB MSF identity stream GUID bytes match,
  `2D-9C-65-8B-67-A6-EE-4B-B8-7F-C2-FB-1C-8E-5C-E2`, age 20 in both.
- Normal NVMe EXE still has the original `42402BB5...8E9DD6` hash.
- Future build wrapper now uses parent-only `.WaitForExit()` and disables
  MSBuild node reuse. The existing running wrapper was not restarted and no
  second compilation was launched to work around this bookkeeping wait.

Current state: compiled/linked EXE and matching PDB verified; final wrapper exit
code/archive completion still pending. Do NOT claim a complete successful build
or deploy based on process launch alone.
Next exact action: inspect completion/log, require changed compilation units and
changed EXE hash, archive/verify EXE+PDB, and keep `E:\TasiaViewerLatest` unchanged
unless deployment is subsequently approved. Full avatar/movement/teleport
runtime correctness remains unproven.

## Message-template/transport comparison and branding backlog (2026-10-18)

Mom requested checking communication incompatibility, specifically
`message_template.msg`, against working Tasia, and adding user/profile role
display to the later branding/UI work.

### Protocol checks completed (offline, not a login test)

- Both `scripts/messages/message_template.msg` files contain 483 definitions,
  with no message exclusive to either viewer. Ignoring comments/whitespace,
  only two definitions differ: Tasia appends optional `MapBlockReply.Size`
  (`SizeX`, `SizeY`, U16); Kirsten appends optional `RezScript.NewScriptInfo`
  (`TemplateID`, LLUUID). These are map-size/script features, not a demonstrated
  cause of the missing self-avatar or walking failure. Templates were NOT edited.
- All 26 checked connection/avatar/movement definitions match exactly, including
  IDs, frequency, trust/encoding flags, block order/types and field sizes:
  UseCircuitCode, RegionHandshake/Reply, CompleteAgentMovement,
  AgentMovementComplete, AgentUpdate, ObjectUpdate/Compressed,
  ImprovedTerseObjectUpdate, AvatarAppearance, AgentSetAppearance,
  AgentWearablesRequest/Update, AgentIsNowWearing, AgentCachedTexture/Response,
  TeleportFinish, CrossedRegion, Enable/DisableSimulator, ParcelOverlay,
  KillObject, Start/CompletePingCheck, LogoutReply and KickUser.
- `llmessagetemplateparser.cpp` code tokens match after removing comments and
  whitespace. Template-reader decode logic retains missing trailing variable
  blocks as zero repeats (`lltemplatemessagereader.cpp:594-611`), so i-Grid's
  absent AvatarAppearance attachment block is legal at the wire-decoding level.
  Kirsten suppresses several packet/no-handler warnings present in Tasia;
  absence of those warnings is NOT proof that all messages were handled.
- Compared actual reference `zero_code()` and `LLMessageSystem::zeroCodeExpand()`
  against Kirsten's actual `LLZeroCode::encode/decode` in a C++17 harness. All
  12,000 deterministic synthetic valid codec inputs passed encoder-byte
  equivalence; compressed inputs round-tripped and both decoders agreed.
  Cases cover random, zero-heavy and long-zero-run buffers. These are codec
  inputs, NOT real captured session packets or a complete message-system test.
- Reproducible check (no network):
  `python3 indra/newview/tests/tasia_protocol_compare_test.py /home/marty/opensim/tasia-viewer-2026-work`
  Compiles with `-std=c++17 -Wall -Wextra -Werror`; PASS. Full viewer not built.
- i-Grid hand-encodes relevant outbound packets in `LLClientView.cs`, so the
  server's web/Andromeda template must not be mistaken for its UDP serializer.
  Its AgentMovementComplete field order and low ID 250 match the viewer
  definition; avatar full updates use ObjectUpdate and PCode.Avatar. This source
  inspection does not establish delivery/handling in Mom's failing session.
- Teleport/region events also travel over HTTP/LLSD, not just the UDP template.
  i-Grid `EventQueueGetHandlers.cs:113-175` supplies `RegionSizeX/Y` there;
  working Tasia consumes them, while Kirsten's handlers do not. Editing only
  the `.msg` template cannot restore this region-size handling.
- Event polling has a source difference to investigate: working Tasia sets a
  60-second transfer timeout for OpenSim; Kirsten does not explicitly set it.
  i-Grid uses a 58-second no-events poll and HTTP 502 (`EventQueueGetModule.cs:
  60-62,546-550`). BOTH viewers already recognize 502 in their general timeout
  branch. Tasia's additional OpenSim `else if (502)` is unreachable because
  the preceding branch already includes 502: do NOT copy it and claim a fix.
  Both count sufficiently early failures and can eventually disconnect on a
  stalled main-region event poll. Actual timeout settings/status/timings from
  the failing session are still needed; no event-poll root cause is claimed.

What is broken/unproven: actual self ObjectUpdate delivery/creation, accepted
AgentUpdate/root-presence state, and final disconnect trigger remain unresolved.
Last attempted: offline template + codec comparison, PASS; no failing check.
Do not change: SL Agni/Aditi protocol definitions or copy reference files wholesale.
Next exact action: correlate existing private logs with the last self object,
movement-complete, event-poll status/timing and disconnect reason; distinguish
UDP, HTTP/event-queue and handler/state failures before further protocol edits.

### Branding/UI backlog — requested, not yet ported

- [ ] Port Tasia user/profile presentation and user identification for **staff,
  developers and partners**, including profile badges/icons, titles, text,
  tooltips and nametag title/color presentation. Keep relationship-partner
  information separate from a Tasia project/partner role badge.
- [ ] Reference: `indra/newview/lltasia_user_config.cpp/h` (remote user config,
  one-shot nonblocking loading, bounded data/time, UI-only name/account
  formatters), `llpanelprofile.cpp::fillTasiaUserData()` / remote badge helpers,
  `llvoavatar.cpp` nametag integration, related settings, textures and en/pl XUI.
  Check the shared display-name/username formatter call sites when porting, not
  just the badge icon. Reference behavior is recorded in its root BUILD_NOTES.
- [ ] Preserve canonical account names, UUID/grid identity and real grid
  permissions. Tasia role/cosmetic branding must not imply Linden/grid-admin
  privileges or replace server identity. Review grid scoping and remote-data
  trust/failure behavior; unavailable branding data must never block login.
- [ ] Adapt into Kirsten's UI and DX11 texture-loading APIs; no Firestorm skin
  dependency or OpenGL-only image path. Validate en/pl layouts and user-run
  profile/nametag behavior separately from protocol compatibility.

## OpenSim compatibility switch and first fixes applied (2026-10-18)

Mom authorized implementation after the source comparison below. Grid policy is
explicit: **only Agni and Aditi are Second Life; all other grids are OpenSim**.
`LLGridManager::isInSecondLife()` compares the two built-in canonical grid keys;
`isInOpenSim()` is its inverse. This is automatic grid-mode selection, not a
transport toggle and not `!isInProductionGrid()` (which includes Aditi).

What is done in source:

- `llviewernetwork.h`: shared automatic compatibility predicates.
- `llvoavatar.cpp/h`: OpenSim defaults to legacy baked-texture origin until an
  accepted appearance message explicitly selects server baking. Missing/zero
  legacy versions are accepted on OpenSim; self legacy messages do not use or
  roll back server COF sequencing. Non-legacy duplicate/stale COF checks remain.
  Legacy textures use `FTT_HOST_BAKE` and return directly, so a URL fetch cannot
  overwrite the result. SL still uses its original appearance URL path.
- `llviewerregion.cpp`: removed the earlier unproven forced-central-bake branch;
  server baking follows advertised protocol bit 0, not extended-bakes bit 63.
- `llagentwearables.cpp`: all calls to `sendDummyAgentWearablesUpdate()` are
  suppressed on OpenSim, not SL. i-Grid `AvatarFactoryModule.cs:1210-1248`
  persists `AgentIsNowWearing` items; the four Linden placeholder UUIDs are not
  real inventory and must not be sent. Working Tasia also guards this at login.
- `llstartup.cpp`: missing-benefits bypass is now OpenSim-only; Aditi no longer
  accidentally shares that bypass through the non-production-grid test.

Verification:

`python3 indra/newview/tests/tasia_opensim_compat_test.py` passed with C++17,
`-Wall -Wextra -Werror`. It compiles actual extracted production function bodies
with minimal dependency stubs, checks both SL grids and three other grid keys,
legacy self/other and explicit-zero appearance, repeated legacy messages,
server/legacy transitions, preserved COF protection, empty/future/conflicting
appearance rejection, mutually exclusive host/URL fetching, default textures,
and zero OpenSim dummy-wearable sends versus unchanged SL sends. Integration
source assertions cover the handshake, benefits guard and URL guard. Existing
`llviewernetwork_test.cpp` also has Agni/Aditi/OpenSim assertions added, but that
full viewer test target has NOT been built/run. `git diff --check` passed.

What remains broken/unverified: no full Windows compilation or interactive
test yet; no claim that avatar, movement, attachment visibility, teleport or
disconnect is fixed at runtime. Full local wearable/bake/upload restoration and
DX11 readback adaptation remain separate work. Variable-region support is still
incomplete. This patch does not recover any wearable state previously saved by
the server, nor change QUIC optionality/transport integration.

Last attempted: focused isolated compatibility regression test, PASS. Exact
last failing step for this patch: none in the checks run; full build not run.
Do not change: DX11 renderer, existing unrelated/uncommitted work, Agni/Aditi
protocol behavior, existing packages/dumps or user viewer processes.
Next exact action: Windows build after approval, transferring ALL changed source
and headers first; confirm actual recompilation (including avatar/region units),
matching EXE/PDB and changed EXE hash before any Mom-run test. Normal package
`E:\TasiaViewerLatest` was not modified by this patch.

## OpenSim source comparison — confirmed gaps, not a runtime fix (2026-10-18)

Compared this tree with `/home/marty/opensim/tasia-viewer-2026-work/indra/newview`
and `/home/marty/igrid-server-code-2026-07-03`. Source inspection is not proof
that the live server uses this exact revision/configuration. No application
source changed, Windows build/deployment performed, credentials used, or viewer
process launched/stopped during this comparison.

### What is confirmed

- Server `LLClientView.cs:981-990`, `SendRegionHandshake()`, **does** send
  `RegionInfo4.RegionProtocols = 1UL << 63`: extended baked textures, but NOT
  server-side baking (bit 0). The previous claim that RegionProtocols was absent
  was incorrect. Working Tasia `llviewerregion.cpp:3316` respects bit 0.
  The existing unproven local branch in this tree at `llviewerregion.cpp:3190`
  forces server baking for non-production grids; it contradicts this handshake.
  It must not be included as an OpenSim fix. It was not changed in this analysis.
- Server `LLClientView.cs:4515-4556`, `SendAppearance()`, sends texture entries
  and visual parameters but **zero AppearanceData blocks**, hence no COF
  version. Kirsten `llvoavatar.cpp:256-262,708` initializes parsed COF and last
  self COF to -1 (`VERSION_UNKNOWN`, `llviewerinventory.h:216`). Its unconditional
  stale check at `llvoavatar.cpp:9836` therefore drops this self message because
  `-1 >= -1`. Working Tasia `llvoavatar.cpp:11037` gates that check on appearance
  version > 0. This is a deterministic protocol mismatch, not evidence that
  the avatar ObjectUpdate itself is missing.
- Kirsten `resolve_appearance_version()` defaults missing versions to 1;
  working Tasia defaults them to 0. A visual parameter explicitly reporting 0
  still resolves to 0 in Kirsten: it is inaccurate to say all version-0 messages
  are rejected as unsupported. Kirsten's `>1` rejection is not the problem;
  its positive-version assertion and stale-COF handling are incompatible with
  legacy appearance.
- Kirsten `llvoavatar.cpp:2630-2659,5943-5961` requires the Linden appearance
  service URL for non-default baked images and returns NULL when absent.
  Working Tasia has a legacy host-bake fetch path (`:3002-3009`) and per-avatar
  server-bake mode (`:6840`). Do NOT blindly copy the reference function: its
  host result is subsequently overwritten by an unconditional URL fetch at
  `:3011-3013`; the restored paths need to be mutually exclusive and tested.
- Local composite rendering exists, but this tree lacks working Tasia's
  `LLAgent::sendAgentSetAppearance()` (`llagent.cpp:6489`), initial legacy
  wearables request/update handlers, `LLVOAvatarSelf::requestLayerSetUploads()`
  and `LLViewerTexLayerSetBuffer::doUpload()` (`llviewertexlayer.cpp:614`).
  Registering `UploadBakedTexture` alone cannot perform the missing bake/upload/
  appearance-send pipeline. It cannot replace `UpdateAvatarAppearance`.
- DX11 adaptation is required for restored baking, not wholesale renderer
  replacement: reference upload reads RGBA with `glReadPixels` and packs five
  J2C channels including the morph mask. This tree also retains GL readback in
  `llappearance/lltexlayer.cpp:1436,1469`. Existing
  `dxrender/resources/DXReadback.h:19-32` offers a staging readback primitive;
  source target, format, row pitch and top-left/bottom-left conversion must be
  checked. No functioning DX11 bake readback/upload is claimed.
- Variable-region support is incomplete: Kirsten login, TeleportFinish and
  CrossedRegion do not consume region dimensions; working Tasia does
  (`llstartup.cpp:4943-4951`, `llviewermessage.cpp:3895-3908,4417-4430`).
  Kirsten `LLWorld::addRegion()` accepts only handle/host. Rejecting overlay
  sequence IDs above 3 protects a 256m allocation but does not support valid
  larger-region chunks. Do not describe those chunks as necessarily malformed.
- QUIC startup is conditional on `TasiaQuicEnabled` (`llstartup.cpp:642`), with
  en/pl preference switches present. Searches of viewer and messaging C++ found
  no consumer constructing `LLQuicConnection` outside llquic itself. This proves
  neither working QUIC transport integration nor runtime testing; linked msquic
  is distinct from a mandatory transport. Do not blame QUIC for this failure.

### Movement, attachments and disconnect remain unresolved

Server `ScenePresence.CompleteMovement()` sends RegionHandshake and
AgentMovementComplete (`:2272-2275`), then the avatar ObjectUpdate to self and
others (`:2314-2316`, `SendInitialAvatarDataToAllAgents():4265`), appearance and
attachments (`:2320,2355-2393`). The viewer creates self from the avatar
ObjectUpdate in `LLViewerObject::createObject()` (`llviewerobject.cpp:175-192`),
not from AvatarAppearance. Its appearance dispatcher cannot create the object
(`llviewermessage.cpp:4160-4176`). Missing body rendering alone does not prove
missing self-object creation.

Kirsten has the ordinary circuit/CompleteAgentMovement sequence on login and
teleport. `send_agent_update()` suppresses updates in non-ready teleport states
(`llviewermessage.cpp:3193-3200`); server `HandleAgentUpdate()` rejects child,
in-transit and stale-sequence updates (`LLClientView.cs:8716-8722`). No evidence
yet establishes which, if either, happens in Mom's failing session. Teleport
arrival transitions in `llviewerdisplay.cpp:395-429` do not wait for a fully
textured avatar: do not assert a missing bake alone stalls teleport/movement.
The exact disconnect reason and last successful self ObjectUpdate/AgentUpdate
still need correlation with existing private runtime logs, before movement fixes.

### Verification and next exact action

Compiled the actual extracted `resolve_appearance_version()` functions from both
trees using `g++ -std=c++17 -Wall -Wextra`. Inputs (field,param) yielded:
`(-1,-1) -> Kirsten 1 / reference 0`, `(-1,0) -> 0 / 0`, `(1,1) -> 1 / 1`.
Source assertions verified COF defaults/stale conditions and absence/presence of
legacy send/upload definitions. Harness files are under
`/tmp/opencode/kirsten-opensim-comparison`. This is a resolver-only check, NOT a
viewer build, packet replay, graphics test or successful OpenSim login.

Report findings before application edits. First proposed bounded change:
respect advertised baking mode, accept legacy self appearance without applying
server-COF sequencing, and restore exclusive legacy baked-texture fetching with
focused tests. Then restore the larger wearables/local-bake/upload pipeline with
DX11-specific readback. Preserve SL Agni AND Aditi; `!isInProductionGrid()` also
includes Aditi and must not be used as an OpenSim classifier. Do not rebuild or
deploy before approval. Verify transferred source, actual compilation and matching
EXE/PDB hashes before any later user-run test.

## SQLite account history and password reveal (2026-10-17)

`lltasia_login_store.cpp/h` uses Windows' `winsqlite3` and user-scoped DPAPI
(`CryptProtectData`, UI forbidden). The database is
`LL_PATH_USER_SETTINGS/login_accounts.sqlite3`, keyed by grid and credential
user ID. Only password ciphertext is written to SQL/journals; account labels
and serialized identifiers are metadata. Existing SecAPI/MD5 credentials remain
compatible. Old MD5 hashes cannot be converted back into passwords: enter the
password once with Remember password enabled and authenticate successfully.

The login panel saves recoverable passwords only after successful authentication,
merges SQL history into the per-grid dropdown, masks again on account/grid changes
and after 15 seconds, and refuses to reveal legacy filler text. Unchecking
Remember password clears SQL password ciphertext. Forget User also deletes its
SQL row. No global legacy SL password should carry over to a new custom grid.

Import accepts this application's version-1 SQLite format (application ID
`0x54415349`), keeps existing accounts, verifies DPAPI decryption, and rolls back
the entire import on invalid data. This is **not a Firestorm credential-file
importer**; encrypted passwords require the original Windows DPAPI user profile.
Imported grids must already be configured in Manage Grids.

Standalone Windows regression test:
`tests/tasia_login_store_test.cpp` compiled with VS2022 `/std:c++17 /W4`, linked
with `winsqlite3.lib crypt32.lib`, passed on Acer. Covers multiple accounts,
grid isolation, save/reopen, password preservation/removal, ciphertext at rest,
imports/duplicates/self-import, corrupt-DPAPI transaction rollback, missing input,
delete and invalid account. Test data is synthetic and uses a fresh directory.
Full viewer integration build and interactive UI verification are separate checks;
interactive login remains Mom's test, not an agent-run test.
Final viewer build including the legacy-grid credential guard passed on Acer:
`build-sqlite-grid-isolation.log`, `EXITCODE=0`. Deployed EXE and en/pl login XML
to `F:\tasia-kirsten\indra\S24-Packaged`, SHA-256 matched all three source files.
Pre-deployment files: `F:\tasia-kirsten\package-backup-login-20261017-150030`.
Matching EXE and PDB archived (hash-verified) at
`F:\tasia-kirsten\diagnostic-symbols\sqlite-login-20261017-150030`.
Deployment record: `F:\tasia-kirsten\sqlite-login-deployment.json`.
No viewer was launched or stopped. UI/login behavior still awaits Mom's test.
Mom requested an NVMe copy at `E:\TasiaViewerLatest` (E: NVMe; F: HDD/USB).
Copy completed with 7,462 files / 714,879,807 bytes, zero robocopy failures;
all file counts/sizes matched, SHA-256 matched EXE, SLPlugin, llwebrtc,
msquic and en/pl login XML. F: package was not changed; no process was stopped
or launched. No explicit CacheLocation/NewCacheLocation was found in user
settings; the default cache is under local AppData on C:, not on F:.
PageHeap is still enabled by executable name, so it also applies to the E: copy.
Subsequent update: Mom reports startup stalls at "Brdf Gen Shader" with full
PageHeap, making this reproduction impractical. Disabled the target Heaps test;
`appverif -query Heaps -for Kirstens-S24.exe` confirms "not enabled". Removed
only the three per-viewer WER properties created for this attempt; dump files
preserved. No process stopped/launched. A fresh viewer process is required to
run without the instrumentation; the existing process retains it until exit.
This diagnostic attempt did not establish the OpenSim corrupting write.
Fresh reproduction on NVMe, 2026-10-17 17:49:11 local:
`CrashDumps\Kirstens-S24.exe.52916.dmp` matches the archived SQLite build PDB.
Exception `0xc0000005` in `nvwgf2umx.dll` (NVIDIA 32.0.16.1656), verifier flags
zero. Stack: driver/D3D11 resource destruction -> `DXTexture::destroyLocked`
-> `DXTexture::create` -> `LLImageDX::setImage` ->
`LLBumpImageList::onSourceStandardLoaded`. Texture recreation parameters are
256x256, four components, generate_mips=true. Invalid read address is the
repeated-byte pattern `0x0101010101010101`; this does not prove a driver bug,
because an earlier write/lifetime error could corrupt a driver-owned object.
Analysis: `F:\tasia-kirsten\opensim-crash-52916.txt` and
`opensim-texture-parameters.txt`. No root-cause fix established yet.

Two other dumps (47396 at 17:47:42 without verifier; 61324 at 15:51:55 with
verifier) instead show a shutdown exception in Boost fiber-specific storage,
via `LL::WorkQueueBase::~WorkQueueBase` -> singleton dependency capture.
They must not be confused with the world-loading texture crash or treated as
evidence identifying the earlier heap corruption. No processes started/stopped
and no new diagnostic configuration enabled in this read-only investigation.

## Selective AddressSanitizer diagnostic build (2026-10-17)

Isolated the actual `DXTexture.cpp` in a small headless test, substituting only
the device accessor/logging dependencies. Both WARP software D3D11 and NVIDIA
hardware passed 200 cycles of 256x256 alpha -> normal RGBA texture recreation,
9-mip allocation, SRV resource ownership and repeated destruction. The NVIDIA
test also passed when compiled with MSVC AddressSanitizer. This does not prove
the driver's correctness or rule out a lifetime error in the full viewer.
Debug-layer validation could not run: device creation returned `0x887a002d`
(SDK debug component missing); tests succeeded without that layer. No optional
Windows features were installed. Harness on Acer:
`F:\tasia-kirsten\dxtexture-isolation\run.cmd [nvidia]` and `run-asan.cmd`.

Added opt-in `TASIA_MEMORY_DIAGNOSTICS` in `cmake/TasiaMemoryDiagnostics.cmake`,
default OFF. On MSVC it instruments only llimage.cpp, llvolume.cpp,
llimagedx.cpp, DXTexture.cpp and lldrawpoolbump.cpp with `/fsanitize=address`.
Those files bypass the ordinary PCH; STL container annotations are disabled
to remain link-compatible with intentionally uninstrumented/prebuilt units.
MSVC infers the ASan runtime from instrumented object directives.

Diagnostic integration build in progress: `build-asan-lite.log`, using the
existing Acer build tree with `-DTASIA_MEMORY_DIAGNOSTICS=ON`. This cache is
now diagnostic: explicitly configure OFF and rebuild before packaging a normal
viewer again. The normal NVMe package `E:\TasiaViewerLatest` is NOT changed.
The diagnostic binary, if its build succeeds, must be packaged separately
with the matching ASan runtime DLL and PDB. No viewer/login test has been run.
Update: `BUILD_EXITCODE=0` for the selective ASan build. Created separate
`E:\TasiaViewerASAN` from the normal NVMe package, replacing only diagnostic
EXE/PDB, matching rebuilt SLPlugin/CEF plugin, and adding the MSVC
`14.44.35207` ASan runtime DLL in root and llplugin. Verified these seven
deployment files by SHA-256 and confirmed the EXE imports
`clang_rt.asan_dynamic-x86_64.dll` via dumpbin. Normal package EXE hash unchanged.
User launch entry point: `E:\TasiaViewerASAN\run-opensim-asan.cmd`.
It records a private local console report and sets ASAN_SAVE_DUMPS to a unique
run-named file in `%LOCALAPPDATA%\TasiaOpenSimDiagnostics`; never upload dumps.
No viewer launched/stopped; interactive reproduction remains pending.
Update: user-run diagnostic process 16752 responds but remains on the stale
"Brdf Gen Shader" label. Non-invasive CDB attach (`-pv`, `qd` detach) at 18:29
shows the main thread waiting on the promise in `LLWindowWin32::maximize()`,
called from `LLAppViewer::initWindow()`, not compiling a shader. Stack capture:
`F:\tasia-kirsten\asan-brdf-live-stacks.txt`. Source posts maximize work to
the ordinary window queue, then waits indefinitely; the worker loop can block
in GetMessage before draining that queue. A wake-up/order issue is a concrete
candidate; the exact window-thread cause is not yet proven by the captured stack.

Added a separate, user-run `E:\TasiaViewerASAN\run-opensim-asan-windowed.cmd`,
using existing `--set FirstLoginThisInstall false --set WindowMaximized false`
options to bypass startup maximize without replacing the executable or the
currently executing launcher. Keep the window unmaximized for this test. The
old process must be closed by Mom first. No process launched/terminated, no
new build, and the OpenSim crash itself is not yet fixed.

## Root cause found: parcel overlay heap overflow (2026-10-17, 18:47)

The windowed ASAN run got past startup, and Mom reported being stuck on
"Connecting to region(s)". The diagnostic console report
(`%LOCALAPPDATA%\TasiaOpenSimDiagnostics\asan-20675-20107.log`) captured the
first real memory error, no verifier needed:

    ERROR: AddressSanitizer: heap-buffer-overflow
      #1 LLViewerParcelMgr::processParcelOverlay  llviewerparcelmgr.cpp:1540
    allocated by
      #1 LLViewerParcelOverlay::LLViewerParcelOverlay  llviewerparceloverlay.cpp:106

`uncompressLandOverlay()` memcpy'd `chunk_size` bytes into
`mOwnership + sequence_id*chunk_size`, where `sequence_id` came straight from
the simulator's ParcelOverlay message and was never range-checked. Ownership is
`64*64 = 4096` bytes, chunk size 1024, so any sequence_id outside 0..3 (or
negative) writes past the region overlay allocation. Firestorm/OpenSim guard
this differently and tolerate a wider chunk count; the Kirsten base had no
guard at all. This is a confirmed corrupting write. It could explain earlier
heap-corruption symptoms, but no evidence establishes that it caused every
earlier `0xc0000374`, NVIDIA `0xc0000005`, or "Connecting to region(s)" stall.
Valid variable-region chunks can exceed 0..3; the bounds guard protects this
allocation but does not implement variable-region support.

Fix: validate `sequence_id`/`chunk` in `processParcelOverlay()` and again inside
`uncompressLandOverlay()` (range, null pointer, chunk_size, and offset+length
against the real buffer size); reject the chunk with a warning instead of
clamping, so unusable data leaves the parcel map public rather than copying
remote-influenced bytes over unrelated heap memory.

Rebuild in progress in `build-parcel-overlay-fix.log`, reconfigured with
`-DTASIA_MEMORY_DIAGNOSTICS=OFF` so the Acer build tree is no longer an ASAN
cache. Not yet deployed or verified; interactive retest is Mom's step.

## Benefits-blocker fix deployed (2026-10-17, ~19:00)
Mom reported OpenSim reaching "Connecting to region(s)" then showing a Polish
benefits error dialog ("Nie udało nam się uzyskać informacji o korzyściach...").
Root cause: `LLStartUp::init_benefits()` required Linden "Base" and "Premium"
benefit packages; OpenSim never sends them, so `mBenefitsSuccessfullyInit`
stayed false and `FailedToGetBenefits` was added in `STATE_CLEANUP`, blocking
the world.

Fix in `indra/newview/llstartup.cpp`: when benefit packages are missing and
the current grid is not a Linden production grid
(`!LLGridManager::getInstance()->isInProductionGrid()`), treat benefits as
non-fatal and continue — defaults (-1 = no limit/cost info) stay in place.
Non-production grids should no longer show the benefits error on login.
Build `build-benefits-fix.log` `EXITCODE=0`; fixed EXE hash-verified and
deployed to both `F:\tasia-kirsten\indra\S24-Packaged\Kirstens-S24.exe` and
`E:\TasiaViewerLatest\Kirstens-S24.exe`.

## OpenSim crash investigation (2026-10-17)

Mom confirms Second Life login/world and chat work, but OpenSim crashes during
world loading. Earlier claims that credential ordering fixed OpenSim, that
missing source resource directories caused the crash, or that MsQuic caused
this crash were not established.

Read-only inspection of Acer captured Event 1000 at 14:37:49 local time:
`0xc0000374` (heap corruption), reported in `ntdll.dll`. The matching dump is
`C:\Users\Acer\AppData\Local\CrashDumps\Kirstens-S24.exe.53400.dmp`.
CDB with the current Release PDB resolves the detection stack through
`LLVolumeFace::resizeVertices` -> `createSide` -> `LLVolume::createVolumeFaces`
-> `LLMeshRepository::loadMesh` -> `LLVOVolume::processUpdateMessage`
-> `LLViewerObjectList::processObjectUpdate`. This identifies the detection
path, not the earlier corrupting write. The viewer log reaches a named OpenSim
region before the crash; authentication did not simply fail.

Analysis saved on Acer as `F:\tasia-kirsten\opensim-crash-analysis.txt`.
Additional CDB `!heap -triage` identifies `HEAP_FAILURE_LFH_BITMAP_MISMATCH`.
Output: `F:\tasia-kirsten\opensim-heap-triage.txt`. The debugger recommends
PageHeap to catch the earlier corrupting operation; enabling system diagnostic
configuration requires Mom's permission, and interactive reproduction is hers.
Mom subsequently approved diagnostics. Enabled only the `Heaps` test for
`Kirstens-S24.exe` using the installed Windows Application Verifier:
`appverif.exe -enable Heaps -for Kirstens-S24.exe -with full=true`.
`-query Heaps -for Kirstens-S24.exe` confirms `Full=true`, `Traces=true`.
No existing IFEO entry was present for this executable; the SSH token was already
administrative, so no elevation or process launch was needed. State recorded at
`F:\tasia-kirsten\opensim-pageheap-state.json`. This configuration affects future
processes with that executable name, not SLPlugin or other applications.
Disable after one user-driven reproduction with
`appverif.exe -disable Heaps -for Kirstens-S24.exe`. Increased RAM/startup time
is expected; this is a diagnostic configuration, not a crash fix.
Also configured per-executable WER `LocalDumps\Kirstens-S24.exe` (previously
absent) with `DumpType=2`, `DumpCount=2`, and a dedicated private-profile folder
`C:\Users\Acer\AppData\Local\TasiaOpenSimDiagnostics`. No old dumps are deleted.
Full dumps may contain passwords/session tokens: inspect locally, never publish
or upload them. `F:\tasia-kirsten\disable-opensim-pageheap.cmd` disables only
this executable's Heaps test and removes these three temporary dump settings;
it preserves dump files and does not stop processes.
No viewer processes were launched/stopped and no crash fix was applied in this
investigation. Next: locate the corrupting write using heap diagnostics on a
user-approved reproduction; do not assume the allocator frame is the cause.

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

## QUIC runtime packaging

`Invoke-Package.ps1` Stage 4 now copies `msquic.dll` from the build output
into `S24-Packaged\` alongside the viewer EXE, so the QUIC library loads at
runtime. Without this, the packaged viewer would start but QUIC connections
to OpenSim grids would fail with a missing DLL.

## 2026-10-21 isolated Windows x64 candidate build

Mom authorized an isolated Windows x64 build only; launching/installing the
candidate and touching viewer profiles/cache remain unauthorized. Acer was
inspected read-only first: no viewer/build process was active, the existing
`F:\tasia-kirsten\build` is VS 2022 x64 Release, and 329.1 GB was free. The
existing tree was left intact. Created the separate candidate
`F:\tasia-kirsten\build-candidates\mixed-size-reachability-20261021-01`, copied
its build cache and prebuilt dependencies, and transferred the local source at
commit `5d918359e18c2136946cdabec5733806dc18c7ed`. All 10,388 transferred source
files matched the SHA-256 manifest. CMake reconfiguration completed with exit 0;
generator is Visual Studio 17 2022, platform x64, configuration Release. It
reported the existing FFmpeg bundle incomplete, relevant to packaging rather than
the compile step.

Build attempts 1–2 were blocked in dependency preparation / stale relocated
include paths. Attempt 3 exposed four source errors in the new world-map code:
the Z spinner was held as generic `LLUICtrl`, and `LLWorld` was not included.
The candidate CMake cache was repaired to set `LIBS_PREBUILT_DIR` to the
candidate root; generated include paths and the required Boost header were
verified. The source fix types the Z field as `LLSpinCtrl` and adds direct
`llworld.h` / `llspinctrl.h` includes. The two updated files were transferred to
the candidate after backup and SHA-256 verification.

Attempt 4 completed: VS 2022 x64, Release, target `Kirstens-S24`, process exit 0,
zero compiler/linker/MSBuild error markers. Seven warnings remain, including
`MSB8028`, `MSB8065`, two `LNK4006` duplicate-symbol warnings, and `LNK4088`
(`/FORCE`). The message-template check could not download the upstream template
because the PowerShell `HttpClient` type was unavailable; syntax-only validation
passed, so compatibility was not verified. EXE SHA-256
`fb10b60a525ce74b554f0a7a7a9f14510ddd98eff194a01ed3eff5d52f34b63d`; PDB
SHA-256 `1994f956a333965852d5a8516e794b10186ae402849bf63d57e26be167bae0e6`.
Receipt and logs are under
`F:\tasia-kirsten\build-candidates\mixed-size-reachability-20261021-01\verified-builds\mixed-size-reachability-20261021-01-attempt-4-build.*`.
No candidate was installed or launched; no original Acer source/build tree or
viewer profiles/cache were changed. Local `tasia_region_compat_test.py` and
`git diff --check` pass. Live QUIC idle/reconnect/crossing/fallback checks remain
pending separate launch authorization.

### Portable package and isolated startup smoke test (2026-10-21)

Mom authorized packaging plus an isolated launch with a package-local profile/cache;
no installation or existing viewer state was permitted. Staged the successful
attempt-4 build using `scripts/Package-TasiaViewer.ps1` at
`F:\tasia-kirsten\build-candidates\mixed-size-reachability-20261021-01\packages\mixed-size-reachability-20261021-01`.
The package contains 7,622 files (870,613,884 bytes); all 7,620 entries in
`package-sha256.json` verified with zero mismatches. Packaged EXE matches the
build receipt SHA-256 `fb10b60a525ce74b554f0a7a7a9f14510ddd98eff194a01ed3eff5d52f34b63d`.
Package receipt:
`F:\tasia-kirsten\build-candidates\mixed-size-reachability-20261021-01\verified-builds\mixed-size-reachability-20261021-01-package.json`.

Launched from an on-demand Interactive/Limited task in desktop session 1; after
45 seconds process 111200 was still responding. Portable mode created only the
candidate package's adjacent `TasiaViewerData` (`user_settings`, `cache`, `logs`,
`temp`). No installation, old Acer tree, original profile/cache, or saved test
credential was used. Mom chose to try the login herself; leave the process and
candidate data intact while she does so. No authenticated grid session, QUIC
idle/reconnect/crossing or LLUDP fallback result is claimed yet. The saved test
credential is documented for localhost-only use; do not use it against the
public i-Grid endpoint without explicit authorization.

### Appearance/system-layer diagnosis from isolated candidate (2026-10-21)

Mom reports login and QUIC working, avatar/objects loading quickly, but system
layers missing. Read-only scan of the candidate `TasiaViewerData/logs/TasiaViewer-LOG.log`
found distinct wearable-asset and texture-fetch failures. Wearable asset requests
returned “asset not found in database”; the outfit fetch later reported fewer
wearables than requested for `WT_ALPHA` (type 13) and `WT_TATTOO` (type 14), one
of each missing. The viewer then requested COF link targets and logged them
resolved/reloaded. Do not infer that the wearable payloads themselves were
successfully fetched from link resolution alone.

At the first readiness sample required shape/skin/hair/eyes counts were zero;
later samples had all four counts present, `previous_loaded=1`, and zero pending
bake uploads. Local bake encoding/upload completion and `sendLegacyAppearance`
were logged. Thus this trace does not support a general bake-upload stall as the
current blocker. `te=19` maps to `TEX_SKIRT_BAKED` (the optional skirt composite),
not one of the required head/upper/lower bakes; it remained undefined while its
image object was present and GPU-ready. That single slot is not proof of the
reported system-layer failure.

Separate OpenSim texture HTTP 404s (plain-text responses) and one texture decode
failure also appear, but the current safe diagnostic deliberately omits the
requested asset/URL identity. They therefore cannot yet be correlated to the
missing alpha/tattoo wearable assets or any particular system layer. Evidence
includes incomplete/missing wearable fetches, but Mom identifies this as a
viewer bug; these fetch errors are symptoms/leads, not a reason to assign root
cause to the server.
The process was no longer running at the later read-only check; its portable data
and logs were preserved and not cleared. Next diagnostic should identify which
visible layers are absent (alpha, tattoo, or other) and, if Mom wants another
candidate session, add privacy-safe fetch correlation for wearable type/texture
role and HTTP status without logging UUIDs, URLs, or account data. No production
code was changed in this diagnosis.

**Work-history cross-check:** earlier verified viewer fixes specifically cover
this bug family. Age29 restored the DX11 fully-invisible-alpha bake semantic
(`IMG_INVISIBLE`, rather than encoding/uploading an invisible composite); age38
fixed legacy AgentSetAppearance bake-slot handling, texture clearing on tattoo/
alpha/universal detach, and attachment refresh after bake assignment. The
historical notes mark the visual tattoo/alpha/universal result as pending, so
these are known viewer-side fixes but not proof this candidate is visually
correct. Current-source focused tests were rerun: `tasia_bake_upload_test.py`,
`tasia_legacy_bake_port_test.py`, and `tasia_cloud_readiness_test.py` all pass;
they do not exercise the live Windows GPU/grid path. Continue from the viewer's
legacy local-bake/composition/texture assignment path, not by changing the
OpenSim server.

### Self-avatar black head: baked-texture route fix (2026-10-21)

Mom clarified that the head/face was black while shape, skin, hair, and eyes
loaded; missing tattoo/clothing wearable assets were also reported. Tracing the
head bake fetch found that `LLTextureFetchWorker::doWork()` constructed texture
capability URLs as `ViewerAsset/?texture_id=<uuid>` for every grid. The existing
OpenSim asset and mesh routes preserve opaque capability paths and use
`ViewerAsset?texture_id=<uuid>` instead. A strict proxy can treat the inserted
slash as a different route and return 404, leaving the baked head unavailable
even after local bake upload/assignment.

Changed `indra/newview/lltexturefetch.cpp` to use `TasiaAssetCaps::assetURL()`
for texture capability requests. OpenSim keeps the opaque path and existing
query parameters; Second Life retains the legacy `/?texture_id=` convention.
Extended `tasia_asset_capability_test.py` with production call-site and both-grid
URL assertions. `python3 indra/newview/tests/tasia_asset_capability_test.py`,
`tasia_bake_upload_test.py`, `tasia_legacy_bake_port_test.py`,
`tasia_cloud_readiness_test.py`, and focused `git diff --check` pass. This is a
source/test-verified route correction, not yet Windows-build or live-grid visual
verification. Preserve the existing portable candidate and its data; use a new
isolated candidate for Mom's authorized visual retest.

### Isolated Windows build of the baked-texture route fix (2026-10-21)

Mom authorized use of the saved Acer login credential for this build only. A
fresh source/build copy was created at
`F:\tasia-kirsten\build-candidates\self-avatar-head-route-20261021-01` from
the already-verified candidate, excluding its `packages` and
`verified-builds` directories. The previous build candidate and portable
package/profile/cache were not modified. Updated source, test, and notes were
transferred with matching SHA-256 values; `lltexturefetch.cpp`:
`daa64d75a411b54fedf0f83389e40e7d538eae870b71459eb5b357d627bc3f2e`.

The first configure attempt correctly rejected the copied CMake cache's old
absolute path. Only the new candidate's cache was relocated; reconfiguration
then passed with Visual Studio 17 2022, x64, Release, and candidate-local
prebuilt include paths. `cmake --build ... --config Release --target
Kirstens-S24 -- /m:8 /v:minimal` exited 0 with zero error markers. The log
confirms `lltexturefetch.cpp` was compiled. Nine warnings remain: three
`MSB8028`, one `MSB8065`, two `C4005`, two `LNK4006`, and `LNK4088` (`/FORCE`),
matching known build warnings; they were not treated as new route-fix errors.

Actual output names are `build/newview/Release/TasiaViewer.exe` and
`TasiaViewer.pdb` (not the stale `Kirstens-S24.*` artifacts also present in the
copied build directory). New artifact SHA-256 values:

- EXE: `75af6538735e1997ef8763787774d3737ed2077a10177fc3969a6c7509816902`
- PDB: `d3b81f544db008841cca249632f4d1bf8120cae25c0ddfa377fb5fda9efa6737`

Build logs are `build-head-route.stdout.log` and `build-head-route.stderr.log`
under the candidate root. No package was created and no viewer was launched;
live visual verification remains pending Mom's separate authorization.

### Mom-authorized isolated package launch (2026-10-21)

Mom authorized packaging and launching the new route-fix build for visual
verification. Packaged only to
`F:\tasia-kirsten\build-candidates\self-avatar-head-route-20261021-01\packages\self-avatar-head-route-20261021-01`, using only the previous candidate package's DLLs as the read-only runtime base. Package contains 7,622 files (870,612,846 bytes); all 7,620 manifest entries verified with zero mismatches. Packaged EXE hash matches the fresh build:
`75af6538735e1997ef8763787774d3737ed2077a10177fc3969a6c7509816902`.

Launched that EXE through a one-shot Interactive/Limited scheduled task into
active desktop session 1. Process 110412 was still running and responding after
60 seconds. The temporary scheduled task was removed. Portable mode created
only this package's adjacent `TasiaViewerData` directories (`cache`,
`chat_logs`, `logs`, `temp`, `user_settings`). No login credentials were
entered, no grid login was attempted by Tasia, and no existing candidate or
profile/cache/logs were changed. The new viewer remains open for Mom's visual
check; visible rendering and whether the black head is fixed are not yet
confirmed.

Mom subsequently shared a screenshot from this candidate showing the face still
black. Therefore the capability-URL correction is insufficient to fix the
reported symptom and must not be called visually successful. A read-only scan
of this candidate's live log found no `FTT_HOST_BAKE` HTTP-failure or empty-body
diagnostics, one legacy appearance send, one generic bake-upload assignment,
and one invisible-alpha assignment. The current generic upload log is
`LL_INFOS_ONCE` and carries no baked-region index, so this evidence does not
establish that the head bake was the one uploaded/assigned or fetched. Process
Mom later closed process 110412; this candidate's package-local data remains
preserved. Next investigate head-region composite output/assignment with
per-bake, UUID-free diagnostics; do not claim the viewer-side issue is fixed yet.

### Independent history review cross-check (2026-10-21)

Tasia Farm job `job_mviickad_1ac1155887` was requested as a read-only independent
review. Its report is not actionable: it cites `src/appearance/LocalBakeCache.cpp`,
`AvatarAppearance::AssignComponentTextures`, and head-cache fallback/assignment
history that do not exist in either checkout; this viewer uses `indra/newview`
and the `LLVOAvatarSelf`/`LLViewerTexLayerSetBuffer` path. Those Farm claims are
rejected, not treated as evidence.

Repository history cross-check: age29 specifically restored the fully invisible
alpha semantic (`IMG_INVISIBLE`) and current
`LLViewerTexLayerSetBuffer::doUpload()` calls `completeInvisibleBake()` before
readback/encode; age38 records legacy slot validation, clearing detached
tattoo/alpha/universal texture entries, and attachment bake refresh. The
historical notes explicitly leave black-face/BOM visual success pending, so
these known fixes neither establish a regression nor explain this screenshot.
Current runtime evidence still lacks a head-slot-specific upload/assignment/fetch
record. No source fix has been selected: next add or devise bounded,
UUID-free per-bake diagnostics (head composite pixel/content summary, assigned
slot, and fetch/texture readiness) before changing bake logic. Keep all server
state and candidate data unchanged.

### Bounded per-bake composite pixel diagnostics (2026-10-21)

Implemented the next diagnostic step in `LLViewerTexLayerSetBuffer::doUpload()`.
For at most two uploads per baked slot per viewer process, the viewer records
only baked slot, fixed body-region label, dimensions as pixel count, and
aggregate non-black / opaque-black / transparent pixel counts. No pixel values,
asset IDs, capability URLs, or account data are logged. This distinguishes an
already-black local composite from a problem after upload/assignment/fetch; it
does not itself alter appearance behavior or claim a fix.

Added and exercised `LLBake::summarizeRGBA()` with deterministic visible,
opaque-black, transparent, invalid-size fixtures. Updated the existing host-bake
fetch test to match the already-applied OpenSim opaque-capability URL correction
(its previous `/?texture_id=` assertion was stale and failed before this change).
PASS: `tasia_bake_readback_test.py` (ASan/UBSan fixture),
`tasia_bake_upload_test.py`, `tasia_bake_refresh_test.py`,
`tasia_cloud_readiness_test.py`, `tasia_legacy_bake_port_test.py`,
`tasia_host_bake_fetch_test.py`, `tasia_asset_capability_test.py`, and focused
`git diff --check`. No real GPU/J2C or Windows runtime was exercised. The black
face remains unresolved until these records identify the failing stage; next
requires a fresh isolated Windows build and a Mom-authorized login/retest. Do not
modify server state or existing candidate data.

### Isolated Windows build of the per-bake diagnostic candidate (2026-10-21)

Mom authorized use of the saved Acer credential to create/build a fresh isolated
candidate. Candidate 02 was cloned from the verified route-fix candidate into
`F:\tasia-kirsten\build-candidates\self-avatar-head-diagnostics-20261021-01`,
excluding its package and verified-build outputs. The prior route candidate,
package, and data were not modified. Candidate-local VS2022 x64 CMake cache was
relocated/reconfigured; the configure log records exit 0. Five staged files
were compared against the source candidate before update, backed up in the new
candidate, and post-transfer SHA-256 values matched the local manifest.

The first incremental build exposed a real compile error in the new diagnostic
array's unqualified `BAKED_NUM_INDICES`. Added the direct appearance-defines
include and qualified constant; reran focused readback/upload/capability tests.
Second Release build exited 0, with zero compiler/linker/MSBuild/failed markers;
the log confirms `llviewertexlayer.cpp` compiled. Diagnostic candidate artifact:

- EXE SHA-256: `13bc40142d99b0a8d0bc7ece005db695b32162f494a54bc57e211e56ace815cd`
- PDB SHA-256: `395b937636b48e954116d8fc826bc32fa4b7279b8117f4d3f05aa428a25b0c5f`
- Logs: `build-diagnostics-fix1.stdout.log`, `build-diagnostics-fix1.stderr.log`

No viewer process was launched; no package or profile/cache was created for this
candidate. The pixel summaries are diagnostic instrumentation only; the black
face is not confirmed fixed. Next requires separate authorization to package and
launch this fresh candidate for Mom's login/visual retest, then inspect its
per-slot pixel records and make the evidence-based behavior fix.

### Mom-authorized isolated diagnostics package launch (2026-10-21)

Mom authorized packaging and launching candidate 02 for her test. Staged only to
`F:\tasia-kirsten\build-candidates\self-avatar-head-diagnostics-20261021-01\packages\self-avatar-head-diagnostics-20261021-01`, reusing only the 12 runtime
DLLs from the route-fix package as a read-only base. The prior package, its
portable data, and all other profiles/caches were left untouched. Package
staging exited 0: 7,620 manifest entries, zero missing/hash mismatches, 7,622
files totaling 870,616,526 bytes. Packaged executable matches the built EXE
SHA-256 `13bc40142d99b0a8d0bc7ece005db695b32162f494a54bc57e211e56ace815cd`.
Receipt: `package-diagnostics-verification.json`.

Launched in active desktop session 1 through a one-shot Interactive/Limited
scheduled task (`TasiaViewer-SelfAvatarHeadDiag-20261021-01`); the temporary task
was removed after process creation. Process 90176 was responsive after 45
seconds and runs from the diagnostic package path. Portable mode created only
that package's adjacent `TasiaViewerData` directories (`cache`, `chat_logs`,
`logs`, `temp`, `user_settings`). No login was performed by Tasia. The viewer
remains open for Mom's login/visual retest; no appearance result is claimed yet.
After Mom's test, inspect only this candidate's logs for the bounded per-slot
pixel summaries and correlate them with the visible head result before choosing
a behavior change. Do not clear its cache or alter server state.

### First visual retest and diagnostics logging correction (2026-10-21)

Mom's screenshot from candidate 02 confirms the black face persists. Read-only
inspection of that candidate's log found the invisible-alpha assignment and a
local bake encode/upload-completion record, but no `Local bake pixels` summary.
The diagnostic call used `LL_INFOS`, which `indra/llcommon/llerror.h` explicitly
compiles away in this Release configuration (`LL_DISABLE_INFOS=1`); the
`LL_INFOS_ONCE` bake messages remained active. Thus this run did not distinguish
black local composition from a later failure, and no bake behavior change is
justified by it.

Changed only the bounded summary call to `LL_VLOGS(LLError::LEVEL_INFO,
"LocalBake")`, which preserves the two-samples-per-slot cap while bypassing the
global `LL_INFOS` compile-out. Extended `tasia_bake_readback_test.py` to assert
the explicit active logging path and compile-out setting. PASS:
`tasia_bake_readback_test.py`, `tasia_bake_upload_test.py`,
`tasia_host_bake_fetch_test.py`, `tasia_asset_capability_test.py`, and focused
`git diff --check`.

### Release diagnostic logging fix and isolated retest build (2026-10-22)

The corrected source and readback test were transferred to the existing isolated
build candidate with SHA-256 verification and a candidate-local backup. The
focused tests passed again. The Windows Release incremental build exited 0, with
zero compiler/linker/MSBuild/failure markers and an explicit
`llviewertexlayer.cpp` compile record. Corrected build EXE SHA-256:
`9fa939d44060e82fa1a08a53696a1127fa16ec3f7307ad8e33fdba46c1107be0`.

Packaged as a separate directory, preserving the first diagnostic package and
its black-face retest data:
`F:\tasia-kirsten\build-candidates\self-avatar-head-diagnostics-20261021-01\packages\self-avatar-head-diagnostics-20261021-02`.
Strict packaging exited 0; all 7,620 manifest entries matched, with 7,622
files / 870,618,062 bytes. The packaged EXE hash matches the corrected build.
Launched in desktop session 1 with a one-shot Interactive/Limited task, which
was removed after start. Process 111236 was responsive after 45 seconds. This
new package has its own portable data root; the previous package/data remains
preserved. Mom must log in and visually retest before the new pixel summaries
can be read. No behavior fix or black-face resolution is yet verified.
Tasia Farm read-only review job `job_mvioaxgo_5a650d613b` is pending.

### Compare against Mom-confirmed visible age4 baseline (2026-10-22)

Mom clarified that the relevant known-good point is age4, not age28:
`F:\tasia-kirsten\test-builds\cof-welcome-20261019-01`. On 2026-10-19 she
confirmed hair, eyes, skin and clothing were all visible (see the cloud baseline
entry above, lines 1144-1155).

Read-only source comparison against preserved Acer source backups:
- `indra/llappearance/lltexlayer.cpp` matches the current source byte-for-byte
  (SHA-256 `585ccc7ff7f4d62744fece3466140cf3a75c2830945a346d54cdd3e561fc7188`).
- The age4-era `indra/newview/llviewertexlayer.cpp` snapshot
  (`source-backup-combined-avatar-status-identity-20261020-014903`, SHA-256
  `185c295bac31ab027c94d197dadde54fdbedd05ee6aeafc4bbc64ac46dbc6961`) differs
  from current only in bounded pixel-summary logging, HTTP/legacy upload retry
  selection, and an upload-failure notification. The upload-selection difference
  occurs after readback; it cannot explain the confirmed all-black pre-upload head
  composite. The inspected `llvoavatar.cpp` differences are name-tag customization,
  not head rendering.

The age4 package's expected `TasiaViewerData/logs` path was not present on the
Acer during this read-only comparison, so there is no age4 runtime log comparison
yet. Current candidate log still confirms slot 0 is fully opaque black before
encoding/upload. No appearance behavior was changed, no candidate was launched,
and no NVIDIA/QEMU changes were attempted. Root cause remains unresolved; continue
the age4-to-current runtime/source comparison rather than reverting unrelated
upload code.

### Failed local-texture decode falsely counted as ready (2026-10-22)

Read-only review of the existing current candidate log showed a texture decode
failure at `22:27:35Z`, followed six seconds later by the slot-0 head bake summary
reporting 4,194,304 opaque-black pixels. At `22:27:55Z`, readiness still logged
`local_available=1`. The texture UUIDs are intentionally redacted, so the failed
decode is temporally associated but not proven to be the head skin input.

Source inspection found a concrete readiness defect in
`LLVOAvatarSelf::getLocalDiscardLevel()`: for a non-default texture ID, a missing
or failed-to-decode image fell through to return discard level `0`, the same value
used for intentional default/empty optional layers. Both
`isLocalTextureDataAvailable()` and `isLocalTextureDataFinal()` therefore could
accept a known failed image as ready and proceed with a bake against the composite
clear color. Changed that path to return `-1` for a non-default ID whose fetched
image is absent or marked missing; valid images retain their discard level, and
default/absent optional textures retain the existing `0` behavior.

Added `indra/newview/tests/tasia_local_texture_readiness_test.py`, which extracts
and compiles the production method plus the actual
`isLocalTextureDataAvailable()` and `isLocalTextureDataFinal()` gates. It checks
that failed/null non-default inputs make both gates false while loaded, pending,
default and absent-optional behavior remains intact. PASS: this test,
`tasia_cloud_readiness_test.py`, `tasia_bake_upload_test.py`,
`tasia_bake_readback_test.py`, and focused `git diff --check`. This is a source/test
fix for the false-ready condition, not yet a Windows build or proof that the
redacted decode failure was the exact head input. Independent read-only Farm
review (`job_mviqanrd_12f57a12f4`) agreed the patch is sound and requested these
downstream assertions; they now pass. The exact `llvoavatarself.cpp` and test
hashes were transferred to the existing isolated Windows candidate with a
candidate-local source backup. The first SSH wait timed out while that build was
still active. A subsequent incremental Release build completed with exit 0, zero
compiler/linker/MSBuild/failure markers, and the expected EXE/PDB artifacts. The
first build log records `llvoavatarself.cpp` compilation; the follow-up was already
up to date. EXE SHA-256:
`8851024b4a86a258188f2a7516ab77ee5b3b00dad415da732d9f221d1c2c9e26`; PDB
SHA-256: `1b9a7bbe77fdf114c61fe8411a91620ab92453b4520970eae54cc94c232fdbfa`.

Packaged separately at
`F:\tasia-kirsten\build-candidates\self-avatar-head-diagnostics-20261021-01\packages\self-avatar-texture-readiness-20261022-01`.
Packaging completed and the 7,620 manifest entries verified with zero mismatches
(7,622 files; 870,618,069 bytes). Packaged EXE hash matches the build. Existing
packages and portable data were preserved; the candidate was not launched. No
cache, server or NVIDIA/QEMU state changed. Visual behavior remains unverified;
Mom's next action is to test this isolated package and report whether the face
renders. The redacted decode failure is still not proven to be the exact head
input.

### Mom-authorized texture-readiness package launch (2026-10-22)

Mom asked to launch the newly packaged candidate. Verified the package EXE hash
against the build hash, confirmed no process for this exact package was already
running and found active desktop session 1. Started
`TasiaViewer.exe` from
`F:\tasia-kirsten\build-candidates\self-avatar-head-diagnostics-20261021-01\packages\self-avatar-texture-readiness-20261022-01`
through a one-shot Interactive/Limited scheduled task. Process 90868 appeared in
session 1 and was responsive; the temporary task was deleted after process
creation. No grid login was performed by Tasia. Existing packages, profiles,
caches, server state, and NVIDIA/QEMU state were not changed. Await Mom's visual
result before inspecting this candidate's new log or claiming the black-face
issue resolved.

### BAKED_HEAD remained solid black after the readiness fix (2026-10-22)

Mom reports the visual retest still failed and identifies the affected bake as
`BAKED_HEAD`. Read-only inspection of this exact package log confirms slot 0,
region `head`, had 4,194,304 RGB-black pixels, each with nonzero alpha, and zero
transparent pixels; the old diagnostic field called `opaque_black` counted any
nonzero alpha, so alpha 255 specifically was not established. The encoded
payload was only 542 bytes and completion then assigned the uploaded bake. A
readiness sample earlier showed `local_available=0`; later samples showed it
becoming `1`. Thus the prior change catches a not-ready interval but does not
prevent this all-black head result from being emitted later. The texture decode
failures remain UUID-redacted and cannot be attributed to a particular head
input. The black face is not fixed.

As a targeted containment change, `LLBake::isAllBlackNonTransparent()` recognizes
an all-RGB-black summary with nonzero alpha for every pixel. The misleading
diagnostic field was renamed from `opaque_black` to `nontransparent_black` so
future logs do not imply alpha 255. `LLViewerTexLayerSetBuffer::doUpload()` refuses
to encode/upload/assign such a result for `BAKED_HEAD`, and completes the upload
as failed so existing bounded retry/exhaustion handling applies. This preserves
the current baked TE instead of replacing it with the confirmed empty black
composite; it is not claimed as the rendering root-cause fix. The readback fixture
checks solid black, partial-alpha black, non-black and mixed/transparent summaries,
the simulated accepted/retry outcomes, and production guard/assignment ordering.
PASS: `tasia_bake_readback_test.py`,
`tasia_bake_upload_test.py`, and scoped `git diff --check`. Independent review
`job_mviss7hp_4c3757314c` approved the guard but flagged missing success-path
assertions. Follow-up review `job_mvistz37_dd12a038e1` requested explicit
acceptance/assignment and retry checks. Added fixture coverage for a non-black
accepted result and source assertions that production assignment happens only on
`Accepted`, while `Retry` starts its timer. The reviewer then flagged partial-alpha
coverage; the guard and summary field are now correctly defined for any
nontransparent all-black pixel, and the fixture covers partial-alpha black. A
final read-only re-review (`job_mvisydv7_3f49ca1d12`) approved the narrowed
semantics and test coverage. The updated helper and viewer-layer sources were
transferred to the isolated Windows source tree with a candidate-local backup.
The first SSH wait timed out during link-time code generation; the same build
later completed, its log recorded both `lltexlayer.cpp` and
`llviewertexlayer.cpp` compilation, and it contained zero compiler/linker/MSBuild/
failure markers. The link emitted existing `/FORCE`/duplicate-symbol warnings.
A subsequent incremental Release build exited 0 with zero error markers and no
recompile. EXE SHA-256:
`cc632bf2fb4e3816f46bac05b22ccb85674ed3d60e242c96769851328509a4e0`; PDB
SHA-256: `227a58c6d15e13ee8a63db9c0ee7ee2a150b696e58f275971a2fd5fda795ad44`.

Staged a separate package at
`F:\tasia-kirsten\build-candidates\self-avatar-head-diagnostics-20261021-01\packages\self-avatar-baked-head-guard-20261022-01`.
All 7,620 manifest entries verified with zero mismatches (7,622 files;
870,618,574 bytes); packaged EXE matches the build. The prior package and its
data remain untouched. The new package was launched in active desktop session 1
with a one-shot Interactive/Limited task
(`TasiaViewer-BakedHeadGuard-20261022-01`); process 92424 was observed responsive
from the new package path and the temporary task was deleted. Tasia did not log
in. No cache was cleared and no server state changed. The containment patch is
not yet a root-cause fix; await Mom's visual result and then inspect only this
candidate's log for the all-black BAKED_HEAD rejection or a valid head bake.

### Outfit-add report from BAKED_HEAD-guard candidate (2026-10-22)

Mom reports that adding an outfit failed. Read-only scan of this candidate's log
shows an inventory/category fetch timeout at `00:46:20Z`
(`CallAfterCategoryFetchStage1::done: Failed to load data. Removing observer`),
followed by `startLegacyInitialWearables()` selecting COF instead of the legacy
snapshot and `updateAppearanceFromCOF()` reporting COF incomplete (`version=-1`,
descendent count `-1`, viewer descendent count `0`). Four OpenSim texture HTTP
404s and one decode failure were also logged, but their asset IDs are redacted.
No `Rejecting all-black nontransparent BAKED_HEAD` message appears in this run,
so the new bake guard is not shown to have fired. This points to an inventory/COF
loading problem preceding outfit application, not an observed guard rejection;
the exact user action and causal link are not established. No source or inventory
state was changed. Next ask Mom which exact UI action (wear item, wear outfit
folder, or save outfit) failed and what visible error appeared; preserve this
candidate's data and inspect its log only after the details are known.

For the single follow-up attempt today, source inspection confirmed that
`CallAfterCategoryFetchStage1::done()` invokes its callback even when
`mComplete` is empty after failure/timeout. `wearCategoryFinal()` previously
trusted that callback and went on to copy or apply the category without checking
whether the folder itself was complete. Added a fail-closed category/completeness
check before either mutation path; on failure it logs and closes the wear-loading
signal rather than applying partial contents. Added
`tests/tasia_wear_category_fetch_test.py` to verify the observer's failure callback
and guard-before-copy/apply ordering. PASS: that focused test,
`tasia_initial_wearables_test.py`, and scoped `git diff --check`. This only guards
the fallback `callAfterCategoryFetch` route; the AIS `fetchFolderAndLinks` route
still depends on its callback contract. The reported action is still unknown, so
this does not prove it was the route Mom hit or fix the underlying inventory
fetch failure. Farm read-only review `job_mvitwqym_d022a2c84c` agreed the guard
and `notifyLoadingFinished()` are appropriate for this route, while reiterating
the AIS failure-path/test limitation; no further code change is justified without
evidence Mom used that path. The Windows source was transferred with a backup.
The Release log records `llappearancemgr.cpp` compilation, zero compiler/linker/
MSBuild/failure markers, and the output EXE. Existing `/FORCE` and duplicate
symbol warnings remain. A follow-up incremental build exited 0 with zero error
markers. EXE SHA-256:
`eb0c0914c8156ddc66f460d38b0b641da6f16a7f71c6de7d29dfa6501b34374d`; PDB
SHA-256: `5fa2933a2aba3d2ad7116f308d1e171b2b721687f3cd891496d77f6964c96957`.
Build command's SSH wait timed out, but the same build's process completed; an
independent incremental build returned exit 0. Staged a separate package at
`F:\tasia-kirsten\build-candidates\self-avatar-head-diagnostics-20261021-01\packages\self-avatar-outfit-fetch-guard-20261022-01`.
All 7,620 manifest entries verified with zero mismatches (7,622 files;
870,619,614 bytes); packaged EXE matches the build. Preflight found no running
viewer or task collision. Launched the package in active desktop session 1 with
one-shot Interactive/Limited task `TasiaViewer-OutfitFetchGuard-20261022-01`;
process 118520 was observed responsive from this exact package path, then the
temporary task was deleted. No grid login was performed by Tasia. No cache or
server state was changed. Mom can now verify the outfit action in this candidate;
Mom then shared a screenshot showing the viewer's logout dialog. Read-only scan
of this candidate's log found zero `CallAfterCategoryFetch`, `wearCategoryFinal`,
`Cannot wear inventory category`, or `CouldNotPutOnOutfit` events; it logged
`Initial outfit uses COF` once and several texture/asset HTTP 404s. The screenshot
and log do not establish that the manual outfit-wear path ran, that the guard
fired, or what caused the logout.

### Old-viewer appearance source comparison (2026-10-22)

Made read-only scratch copies of ten corresponding appearance/inventory files
from `/home/marty/opensim/tasia-viewer-2026` and the active checkout under
`/tmp/opencode/outfit-fetch-parity-20261022/{old,current}`; neither checkout was
modified by this comparison. The old and current
`CallAfterCategoryFetchStage1` implementations are identical, including invoking
the callback when `mComplete` is empty after failure. Stage 2 is also behaviorally
identical. Old `wearCategoryFinal()` has no completeness guard; current has the
guard already added. Old `llstartup.cpp` always updates from COF, while current
has extra session validation and the local legacy-snapshot fallback. The current
and old source file headers both specify viewer LGPL 2.1; license is not the
blocker. The complete old file differs substantially (about 800 diff lines,
635 removals and 165 additions in this comparison) and wholesale replacement
would lose current behavior without fixing the confirmed observer bug.

Potential source scope for a future explicit fetch-result change is
`llappearancemgr.cpp/.h`, `llstartup.cpp`, `llagentwearables.cpp/.h`, and focused
tests. `llinventoryobserver.cpp/.h` already signals terminal fetch failure and
does not need wholesale replacement. Because the latest candidate log contains
no manual category-wear call or guard event, no additional source was changed
based on this comparison; first establish the exact UI action and failure route.

### Failed-COF startup fallback correction (2026-10-22)

Mom asked me to proceed with the connected-file correction despite the UI action
remaining unknown. Before editing, copied the five scoped source/test files to
`/tmp/opencode/outfit-fallback-backup-20261022`; no project file was overwritten.
Reviewing the startup call chain raised a possible AIS timing concern. Inspection
of `AISAPI::InvokeAISCommandCoro()` established that
`onUpdateReceived(result, type, body)` applies the inventory data before
`callback(id)` invokes the `fetchCOF()` wrapper. The wrapper ordering was
therefore left unchanged; no production edit was made to
`llinventorymodelbackgroundfetch.cpp`. Added a focused source assertion for the
verified update-before-callback contract.

The startup fallback now treats an incomplete or unusable inventory as
non-authoritative only when the fetch callback finds an incomplete COF; normal
unknown/incomplete COF remains fail-closed. `mLegacyInitialCOFFetchFailed` is
cleared on the wearable-asset deadline as well as the existing completion,
timeout, cleanup, and outfit-cancellation paths. The startup callback preserves
the old COF update for avatars that cannot use legacy bakes. The initial-wearable
fixture now covers partial COF plus failed-fetch snapshot selection, helper
policy, AIS update-before-callback ordering, startup routing, and deadline-state
cleanup.

PASS: `PYTHONDONTWRITEBYTECODE=1 python3
indra/newview/tests/tasia_initial_wearables_test.py` (includes ASan/UBSan fixture),
`PYTHONDONTWRITEBYTECODE=1 python3
indra/newview/tests/tasia_wear_category_fetch_test.py`, and scoped `git diff
--check`. Farm review attempt `job_mvjg0lmx_93fd89ce25` returned no source review
because its lead had no repository access. Self-contained follow-up review
`job_mvjg1wrt_0b09ca1465` returned three concerns; two misread the supplied
startup branch (the original COF update is only in the `start... == false` path,
and an active fallback returns true) and were not confirmed in source. Its AIS
timing concern was resolved by the `onUpdateReceived`-before-completion-callback
source trace and is now guarded by the focused assertion. At this source-only
checkpoint no viewer build or live test had run.

### Isolated Windows build/package of COF fallback (2026-10-22)

Mom authorized a separate Windows candidate build and local saved-credential use.
Preflight found no running viewer/build processes, confirmed the new destination
did not exist, and verified that the previous candidate's three production
sources matched the pre-patch scratch snapshot. Cloned
`self-avatar-head-diagnostics-20261021-01` to
`F:\tasia-kirsten\build-candidates\self-avatar-cof-fallback-20261022-01`,
excluding its packages and prior build receipts. Robocopy returned 3 (files
copied, no failure). Backed up the candidate's three production files, existing
initial-wearables test, and `CMakeCache.txt` under
`verified-builds\source-backup-cof-fallback-20261022-01`. Staged
`llagentwearables.cpp/.h`, `llstartup.cpp`, and both focused tests from the local
checkout; all five staged raw SHA-256 values matched the local CRLF staging files.

Relocated all 66 old-root path references in this candidate's copied CMake cache
only. CMake configure used the executable recorded in that cache because SSH's
PowerShell lacked CMake on `PATH`; final configure exit 0, Visual Studio 17 2022,
x64, Release, and `LIBS_PREBUILT_DIR` points at the new candidate. Release target
`Kirstens-S24` exited 0. Logs confirm `llagentwearables.cpp` and `llstartup.cpp`
compiled, with zero compiler/linker/MSBuild error markers. Existing linker
warnings remain: two duplicate-symbol `LNK4006` warnings and `LNK4088` (`/FORCE`).
Built EXE SHA-256:
`bd104eecfa892c88561d95da53364864908a3329048907a2b3546b212c1c218b`; PDB
SHA-256: `c1ad16659979027f65dd4dc65a6bf2149a18a8ca7edebdb94029b9da148b381b`.

Packaged separately at
`F:\tasia-kirsten\build-candidates\self-avatar-cof-fallback-20261022-01\packages\self-avatar-cof-fallback-20261022-01`,
using only DLLs from the previous known package as a runtime base. All 7,620
manifest entries verified with zero mismatches (7,622 files; 870,619,534 bytes).
Packaged EXE SHA-256 matches the build:
`bd104eecfa892c88561d95da53364864908a3329048907a2b3546b212c1c218b`. A first
manifest verifier failed due to PowerShell 5.1 array wrapping; a read-only
manifest inspection showed each `Path` was a scalar string, and the corrected
verifier passed. Mom then authorized the candidate launch. Preflight rechecked
the build/package EXE hash, portable marker, absence of this candidate process,
absence of an existing package-local data directory, and active desktop session
1. The SSH account name did not map to a task principal; inspecting the active
Explorer owner identified the correct desktop principal. Launched the package
through one temporary Interactive/Limited scheduled task; process 120032 appeared
in session 1 and was responding. The temporary task was removed. No grid login
was performed and no log was read. Mom should test the outfit action in this
candidate and report the result before its log is inspected.

#### COF candidate region-connect stall (2026-10-22; Mom reports `-20`)

After Mom reported the stall, inspected only connection-related lines in this
candidate's portable `TasiaViewer-LOG.log`. The viewer process was still PID
120032 in interactive session 1 and responding. Log modified
`2026-10-22T14:49:26+02:00` records QUIC established without simulator session
confirmation, fallback to UDP, `CompleteAgentMovement failed delivery, error
code: -23016` at `12:48:07Z`, then no circuit ping for 103.17 seconds and circuit
drop at `12:49:26Z`. This reproduces the connection stall in the packaged
candidate; outfit behavior was not reached/verified.

Source trace: `-23016` is the message-system `LL_ERR_TCP_TIMEOUT` constant, but
the name is legacy and does **not** identify TCP as the failed transport. In
`LLCircuitData::resendUnackedPackets()`, that result is passed to the reliable
packet callback only after its retry window expires. `send_complete_agent_movement()`
uses `sendReliable()` with `UseCircuitCodeMaxRetries=3` and timeout 5 seconds.
Startup sends it after the `UseCircuitCode` callback advances from
`STATE_WORLD_WAIT` to `STATE_AGENT_SEND`. QUIC's separate five-second
readiness guard falls back to UDP when the authenticated `quicready` signal has
not been accepted. Thus the observed sequence is consistent with QUIC session
confirmation missing, followed by a reliable `CompleteAgentMovement` retry
timeout on the recovered circuit; the later circuit timeout shows no subsequent
simulator ping was received. The exact transmitted transport/packet outcome is
not logged, so this does not establish whether the simulator received or rejected
`CompleteAgentMovement`, or whether its reply/ACK was lost.

The checked server source sends `quicready` on its native QUIC UseCircuitCode
path and has a separate Quick-G loopback bridge path; that source inspection
does not prove which deployed server/bridge path served this session. No settings,
profile, or source changes were made, and the candidate remains running. Root
cause remains unconfirmed; next evidence needed is server-side correlation for
the same login/circuit: receipt and processing of UseCircuitCode,
CompleteAgentMovement, emitted `quicready`, and any AgentMovementComplete/ACK
back to the client. Keep the candidate running; request Mom's outcome before
reading any further candidate logs.

### Phase 1 login-state and movement guard audit (2026-10-22)

Target and branch were verified before edits: isolated worktree
`/home/marty/opensim/tasia-kirsten-completion`, branch
`fix/opensim-completion-20261022`, based on fetched GitHub `broken` SHA
`4a257ba7fd876ff36b3e8789312ed8fd578af768`. The original target worktree remains
untouched (its pre-existing untracked Python cache and session transcript remain
there). The configured Git remote is `tasia`, not `origin`.

The recorded COF-fallback candidate and log establish failure after login
authentication/world initialization at simulator circuit/movement confirmation;
they do not prove a cause at the server/transport boundary. No live account,
credential, or grid access was performed in this audit. The failure-stage table
is in `OPENSIM_PROTOCOL_AUDIT.md`.

Phase 1 changes:

- `llpanellogin.cpp::updateServer()` preserves a nonempty manually typed username
  across grid-list refresh while clearing password UI state and the remembered-
  password checkbox. A selected remembered account continues to be populated
  using the destination grid's credentials.
- Credential-format diagnostics no longer log the typed username.
- Startup auth-failure diagnostics no longer stream the full LLSD response, and
  the shared secure-handler save logs no longer emit grid/account identifiers.
- `llloginhandler.cpp::initializeLoginInfo()` only binds legacy, unscoped
  `UserLoginInfo` to Second Life system grids; custom grids require their own
  per-grid credential or manual input.
- `TasiaLoginStore::isAvailable()` makes the platform boundary explicit. On
  non-Windows the raw-password SQLite/import controls are unavailable and the
  existing `LLSecAPIHandler` credential path remains in use; the UI no longer
  reports the deliberately unsupported SQLite extension as an account-save
  failure. Raw password recovery/import is not claimed as Linux parity.
- `process_agent_movement_complete()` validates both AgentID and SessionID before
  setting flags that advance startup/teleport. Movement diagnostics no longer
  print simulator host or circuit code.
- Added source-contract tests in
  `indra/newview/tests/tasia_login_flow_test.py`. These tests do not instantiate
  the login UI or simulate network packets.

Offline results on the isolated branch:

```text
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_login_flow_test.py       PASS (10 tests)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_identity_grid_test.py   PASS (4 tests)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_opensim_compat_test.py  PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_opensim_services_test.py PASS (15 tests)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_quic_transport_test.py PASS (6 tests)
c++ -std=c++17 -Wall -Wextra -Werror -Iindra/newview indra/newview/lltasia_login_store.cpp indra/newview/tests/tasia_login_store_platform_test.cpp -o /tmp/opencode/tasia-login-store-platform-test && /tmp/opencode/tasia-login-store-platform-test PASS (non-Windows store explicitly unavailable)
git diff --check                                                               PASS
```

Baseline build unavailable in this Linux environment: there is no configured
build tree, no `msbuild`, `devenv`, or `cl`, and the active target is Windows
DX11. This is not a Windows build or login proof. Live SL/OpenSim flows and
Windows SQLite/DPAPI UI behavior remain unverified. The non-Windows extension
availability contract has a compiled native fixture; actual `LLSecAPIHandler`
credential persistence/security on Linux and recoverable password/import parity
remain unverified/not implemented. Tasia Farm review jobs
`job_mvjmew5r_0f531b312b` (lead plus independent peer) flagged four concerns. We
checked them against the code: selected-account branch is intentionally sourced
from the destination grid; a manually typed username is deliberately retained for
manual authentication; clearing credentials on a grid boundary is required; and
legacy global fallback is intentionally restricted to SL. No source change was
made for those speculative concerns. Delta review
`job_mvjmgiaf_2592f31ec1` found no actionable issue in the additional identifier-
redaction change and noted the lack of Windows/runtime tests. These reviews are
advisory, not runtime evidence. Root cause of the recorded region-connect stall
remains unresolved pending server-side event correlation.

### Phase 2 AIS outfit-fetch failure guard (2026-10-22)

After the Phase 1 commit `94e1fe3c53b7b03a3a174a5850e277ab05022c81`, re-audited
the AIS `FetchCategoryLinks` route. Its callback contract was nullary at the
background-fetch layer; the callback ran regardless of AIS success, and
`InvokeAISCommandCoro` extracted category IDs from response bodies even when HTTP
status failed. `wearCategoryFinal()` checks category completeness, but a failed
refresh of a previously-complete cached folder could leave that check true.

Added a completion-aware overload that preserves the old nullary overload for
other consumers. AIS category updates/results are now applied only for successful
HTTP status plus map responses; the outfit callback requires the confirmed
category ID to equal the requested ID. Null/mismatched IDs settle the wearable
loading signal and return before copy/apply. AIS logs no longer print capability
URLs, full LLSD request/response bodies, or inventory UUIDs; generic status and
operation messages remain. A category-fetch warning also no longer prints the
category UUID. The null/mismatch branch stops the tracked
`wear_inventory_category_fetch` phase and calls `notifyLoadingFinished()` before
returning. No destructive inventory operation was changed.

The new/extended test first failed against the old AIS nullary route (missing
completion ID), then passed after the guard. Focused results:

```text
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_wear_category_fetch_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_initial_wearables_test.py PASS (12,000 snapshot cases + ASan/UBSan fixture)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_cof_library_fetch_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_wearable_asset_transport_test.py PASS (2 cases)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_legacy_inventory_fetch_test.py PASS (2 cases)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_cloud_readiness_test.py PASS
git diff --check PASS
```

These are offline source/fixture results; there is no full target build or live
AIS/grid verification on this Linux host. Tasia Farm review
`job_mvjmn03u_909a6a3154` agreed with completion-ID gating and compatibility of
the old nullary wrapper; it asked for confirmation that failure settles the
loading state. The code now stops the tracked phase and signals loading finished,
and the focused source contract asserts both. This confirms code ordering only,
not live UI settlement. Follow-up review `job_mvjmpr5f_9bceab0be6` agreed the
failure branch stops the phase, notifies wearables and returns before
finalization. Its concurrency question was checked: `selfStopPhase` only forwards
to the current avatar phase tracker; the AIS callback invokes the outfit callback
once, then performs the background-fetch completion bookkeeping; repository search
shows this phase has one start and exactly the error and normal stops. No later
outfit callback depends on the phase remaining active. The independent reviewer
still correctly notes that a Windows build and live AIS validation are missing.

### Phase 3 legacy local-bake / BoM parity audit (2026-10-22)

Compared the old reference's GL `glReadPixels` upload path with the active
DX11-specific implementation. The current path retains protocol behavior (RGBA
and morph-mask five-channel `RGBHM` J2C, UploadBakedTexture capability with asset
storage fallback, successful-upload appearance assignment) while using
`DXReadback` bounds/format/sample validation, row-pitch compaction and BGRA
normalization. It gates local legacy upload to OpenSim legacy-bake regions and
keeps invisible-alpha assignment semantic. Ticket/revision validation, bounded
retry handling, and attachment bake refresh are present in current source.

The previously recorded solid-black `BAKED_HEAD` remains a user-visible failure.
The all-black/nontransparent pre-upload rejection is only containment; no
sanitized evidence attributes the bad composite to a specific input layer or
asset, so the root cause remains unresolved and no speculative texture/transport
changes were made. Focused verification passed:

```text
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bake_readback_test.py PASS (ASan/UBSan synthetic fixture)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bake_upload_test.py PASS (ASan/UBSan fixture)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bake_refresh_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_legacy_bake_port_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bom_limits_test.py PASS
```

These fixtures are not a real D3D GPU, J2C codec, upload server, avatar or live
BoM test. No full Windows DX11 build was available in this Linux worktree, and
the previously tested visual failure is not fixed. The audit details and exact
file paths are recorded in `OPENSIM_PROTOCOL_AUDIT.md`. Farm review
`job_mvjmtuoj_3f576dfcb5` and its follow-up `job_mvjmuqw3_790245fd60` could not
independently read the repository files and challenged the evidence citations,
not the source-path summary. The audit now links the black-head status to the
earlier candidate-log entry and labels it as Mom-reported plus read-only log
evidence; fixture results and missing Windows/live validation are explicit. This
review is therefore recorded as limited documentation feedback, not independent
runtime or source verification.

### Phase 4 OpenSim legacy self-profile edits (2026-10-22)

Restored the old reference's no-`AgentProfile` self-profile update behavior for
about text, profile image, first-life text/image and publish state. The legacy
packet is a complete record, so the processor now merges a supported partial edit
into the last self `AvatarPropertiesReply` snapshot, checks that the snapshot is
from the active session, and sends all fields including the untouched profile
URL and image/text values. Unknown edits are rejected before cache mutation.
`hide_age` remains cap-only because the legacy update packet has no hide-age
field. If no snapshot is available, handlers do not clear their dirty state.

Reference packet field mapping was checked in
`/home/marty/opensim/tasia-viewer-2026/indra/newview/llavatarpropertiesprocessor.cpp`;
server parsing and update handling were checked in the read-only
`/home/marty/opensim/telep-npc/OpenSim/Region/ClientStack/Linden/UDP/LLClientView.cs`
and `.../UserProfileModule.cs`. This verifies source contracts only, not the
deployed grid.

```text
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_opensim_services_test.py PASS (16 tests, including legacy edit contract)
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_profile_partner_link_test.py PASS (4 tests)
git diff --check PASS
```

No full Windows DX11 build, live OpenSim profile save, or SL runtime regression
test was available. Tasia Farm review `job_mvjn6q47_52c81ec376` agreed the
defensive guards, field merge, and full packet are sound; it raised questions about
field overlap and stale/concurrent updates. The image fields are distinct packet
members (`ImageID` versus `FLImageID`), and only the matching changed key is
merged. The send function is synchronous (no coroutine/yield); network replies
are dispatched by `LLAppViewer::idleNetwork()` and the UI handler sends on the
same viewer thread, so a reply cannot interleave between validation, merge and
packet construction. There is no dedicated concurrency stress test, and live
session behavior remains unverified. The review's concern about missing build
validation remains valid.

### Phase 5 OpenSim search endpoint routing (2026-10-22)

Compared the old `LLFloaterSearch::initiateSearch()` and
`LFSimFeatureHandler::searchURL()` with current search and grid metadata code.
Current code always selected the global Second Life `SearchURL`; the OpenSim
`SearchURLOpenSim` setting and simulator/login response sources were missing even
though grid-catalog import already validates a per-grid `search` endpoint.
Restored endpoint precedence as OpenSimExtras `search-server-url`, login response
`search`, selected-grid `search` metadata, then the OpenSim-specific fallback
setting. Second Life continues to use only `SearchURL`. Empty endpoints now stop
before browser navigation rather than navigating to a blank URL. OpenSim endpoint
candidates are limited to HTTP(S), nonempty host, no embedded userinfo, no literal
fragment, whitespace, backslash, or overlong URL; stale login-response values are
ignored unless the login instance reports successful authentication.

```text
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_opensim_services_test.py PASS (17 tests, includes endpoint precedence + SL isolation)
git diff --check PASS
```

The contract test verifies source selection order and setting registration; no
live SimulatorFeatures/login service, browser navigation, or Windows build was
tested. Farm review jobs `job_mvjndb5z_c8725986d8` and
`job_mvjnehqx_c63a875882` did not independently inspect the actual source; the
delta review explicitly said source was inaccessible. It cited nonexistent
`Http::IsHttp()` / `g_pObject` code and raised URL cases that were checked against
the actual `LLURI` implementation. Those claims are not treated as validation or
actionable findings. The actual code's userinfo rejection is intentional; encoded
`%23` is URL data, not a fragment; bad/non-HTTP endpoint candidates fall through
the precedence chain. These reviews are limited feedback, not independent source,
Windows, or runtime verification.

### Phase 5 follow-up — OpenSim Destinations guide routing (2026-10-22)

The adjacent `LLFloaterDestinations::postBuild()` also always loaded the global
SL `DestinationGuideURL`. The old reference selects OpenSim simulator feature
`destination-guide-url` or login response `destination_guide_url`, and requests
that response field during OpenSim login. Current login code requested neither
that field nor `search`; the search patch's login-response branch therefore also
needed the matching OpenSim `requested_options` entry.

Now OpenSim login requests both `search` and `destination_guide_url`. Destinations
uses the OpenSim region feature first, then an authenticated login response; it
does not use the SL setting on OpenSim. The SL path remains `DestinationGuideURL`.
The floater resolves again on open, avoids reloading an unchanged endpoint, and
clears the old guide to `about:blank` when the selected grid has no guide URL or
URL expansion fails validation.
Candidates are restricted to HTTP(S), nonempty-host, no-userinfo URLs under the
same local policy as search. `tasia_opensim_services_test.py` passes 18 tests,
including login-option and routing contracts; `tasia_login_flow_test.py` passes
10 tests. Live login/browser behavior and a Windows build remain untested. Farm job
`job_mvjnk8ia_ead4ea3b0a` stated it could not access source; its lead provided no
code findings, and its peer suggested extra branch/Windows tests based only on
the summary. The source contract checks the OpenSim gate and route branches, but
does not execute the UI or Windows code. Review is therefore recorded as limited,
not independent source/runtime verification.

### Completion report push and login/QUIC source trace (2026-10-22)

Added `docs/OPENSIM_COMPLETION_REPORT.md` and committed it as
`f3db260c406223111243be7e2ec8af3961296347`. Pushed without force to
`tasia/fix/opensim-completion-20261022`; `git ls-remote` verified the remote ref
at that SHA. The worktree was clean immediately after the push. The report
explicitly leaves the live login stall, black `BAKED_HEAD`, live profile saves,
Windows DX11 build, and QUIC idle/reconnect behavior unresolved or unverified.

Continued the login trace across current and prior `llstartup.cpp`, current
`llviewermessage.cpp` / `llviewergenericmessage.cpp` / `llcircuit.cpp`, and
checked `telep-npc` QUIC + LLUDP handlers. Current and previous startup packet
ordering is source-equivalent through UseCircuitCode ack, CompleteAgentMovement,
and AgentMovementComplete wait. Current QUIC requires an advertised endpoint and
OpenSim + `TasiaQuicEnabled`; a received `quicready` must come from the actual QUIC
route and expected host before queued traffic flushes. Disabled setting falls
back existing QUIC circuits to LLUDP. Current five-second readiness fallback is
different from the reference, but source comparison does not establish it as the
live stall cause. No handshake behavior was changed because server correlation
and a controlled QUIC-on/off login are unavailable here.

`PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_quic_transport_test.py`
passes 6/6 tests, including compiled production routing/retry/fallback, readiness
guards, and stream framing. This remains offline fixture evidence only. No Windows
DX11 build, live login, client/server packet correlation, or QUIC enabled/disabled
runtime comparison was performed. Detailed call-path evidence and next required
correlation are in `OPENSIM_PROTOCOL_AUDIT.md`.

Tasia Farm review `job_mvjnvfs4_0225653b20` completed as an independent
documentation-consistency review. It repeated the offline-only testing gap and
current-vs-old callback/timeout contrast (both are explicitly present in the
audit); its concern that “no code changed” could be read broadly was addressed by
clarifying that only this source-trace documentation update made no production
code changes, while the branch's earlier Phase 1 movement guard/callback are
implementation changes. The reviewers did not independently inspect local
source or verify runtime behavior; this is not code or runtime approval.

Also traced the complete head-bake source path. `BAKED_HEAD` is dictionary index
0 and includes head bodypaint, tattoo, alpha, and universal tattoo; wearable
classes include shape, skin, hair, tattoo, alpha, universal. Local missing asset
inputs return discard -1 and block final/available readiness; default avatar
textures are treated as intentionally empty. The DX11 path reads the render
target, rejects all-black nontransparent head pixels, packs RGBA + morph mask as
RGBHM J2C, uploads via cap or asset storage, assigns the accepted returned ID to
the mapped baked TE, refreshes mesh/attachment magic textures, and emits
AgentSetAppearance only after pending legacy bake uploads complete. This confirms
call-path wiring but does not identify the historical black input layer/asset or
validate actual D3D/J2C/server/render results.

Reran focused bake suites:

```text
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bake_readback_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bake_upload_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bake_refresh_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_legacy_bake_port_test.py PASS
PYTHONDONTWRITEBYTECODE=1 python3 indra/newview/tests/tasia_bom_limits_test.py PASS
```

These fixtures cover helper/policy/call contracts, not a real Windows DX11
candidate or visible avatar. No production code was changed in these source-trace
steps.

Tasia Farm review `job_mvjnxji2_e4bbf69f9d` completed as a documentation
consistency review of the supplied bake-path evidence. It usefully flagged that
black pixels can be intentional and that the all-black/nontransparent guard is
not the only integrity control. Clarified the audit: black is not categorically
invalid; the guard checks only the aggregate condition, readiness and ID/index
guards are separate, and none proves final rendering. The review also emphasized
the absent Windows D3D/live render evidence, already explicitly listed. Reviewers
worked from the supplied source summary rather than independently inspecting the
local repository; it is not source/build/runtime approval.
