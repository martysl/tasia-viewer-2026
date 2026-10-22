# Tasia Viewer feature restoration and branding plan

Date: 2026-10-20

## Current status: broad OpenSim audit in progress; live profile/QUIC tests pending

## Mom's prioritized roadmap

The live SL grey-avatar-texture and overlapping-top-bar report remains an active,
separate diagnostic; keep the running session and cache untouched until Mom is
offline. For planned implementation work, follow this order:

1. **Finish the OpenSim compatibility audit and fix real gaps.** Compare the
   relevant viewer paths with the old viewer and the OpenSim server, identify
   missing or incorrect behavior, then implement one verified gap at a time.
   Preserve Second Life behavior with explicit SL/OpenSim isolation tests.
2. **Fix bake, BOM, and texture issues across SL and OpenSim.** Include the live
   grey face/hair/clothing report, server/local bake selection, BOM slot limits,
   and intermittent texture-fetch/render failures. Compare both grid paths and use
   focused tests plus safe live verification; do not clear cache or disrupt an
   active session without Mom's go-ahead.
3. **Debug and fix QUIC.** Gather real OpenSim and SL-compatible transport logs,
   trace every error/fallback through client and server paths, and repair proven
   causes rather than guessing. Test success, fallback, and recovery.
4. **Restore Account Type customization.** Compare with the old viewer and bring
   back the expected customization behavior, with regression coverage.
5. **Make Grid Manager endpoints functional end to end.** Audit downloaded endpoint
   configuration (including search/apps and similar services) against the code
   paths that consume it. A downloaded endpoint is not complete until its feature
   actually works; test each supported endpoint and its failure behavior.
6. **Fix the minimap.** Identify and repair its display/data problems with focused
   tests and a live visual check.
7. **Restore teleport from the minimap.** Verify selecting a destination sends the
   right location and completes an end-to-end teleport without breaking ordinary
   map navigation.
8. **Only after items 1–7 are complete, plan Firestorm ports and viewer styling.**
   First make a feature-by-feature candidate list, check compatibility and scope,
   then port and verify each feature separately; treat Firestorm-inspired viewer
   appearance as a separate, deliberate design pass.

## Requested future feature: DJ tip response

- Investigate the removed DJ/tip-response function and document its prior behavior.
- Restore detection of an incoming tip and send the configured thank-you/message in
  chat; verify where the message is configured and which chat channel it should use.
- Keep this separate from the current live SL texture/top-bar repair.

## Requested future feature: DJ music app and voice-chat TTS

- Add a DJ app/function that plays music from a music API/source through the
  SHOUTcast server; a small HTML/PHP control interface is acceptable.
- For voice-chat speech output, disable microphone listening while TTS is active
  to avoid simultaneous mic capture/listening.
- Use Edge TTS by default with a selectable Edge voice. Offer OpenAI TTS as an
  optional provider when configured, with a user-selectable provider/voice.
- Keep this separate from the tip-response feature and the current SL
  texture/top-bar repair; define API, SHOUTcast control/auth, and voice-chat
  integration details before implementation.

Mom authorized a new isolated Windows candidate. Nine reviewed C++ files were
transferred only after exact remote-baseline checks and backed up at
`F:\tasia-kirsten\source-backup-profile-legacy-udp-20261020-01`. Release build
completed with exit 0 and zero compiler/linker error markers. Four warnings remain:
`MSB8065`, two `LNK4006` duplicate-symbol warnings, and `LNK4088` (`/FORCE`).
EXE SHA256 `771eb85cca46af03db44873316de7cf0f8636e9038996ba4f41856a18f3776df`;
PDB SHA256 `b3adae354fbfadca17e4b434bf080e2ba610a23d5fbbb807c2b33883652dd64e`.

Fresh portable candidate:
`F:\tasia-kirsten\test-builds\profile-legacy-udp-20261020-01`.
The first staging pass omitted 10 runtime DLLs (FMOD, vccorlib, and the current
MSVC redistributable set); static file-hash verification alone missed this. They
were added from the repository's checked-in runtime locations after per-file source
hash validation. The package script now stages and requires these dependencies.
All 7,307 package-manifest hashes are verified; 7,309 files total including the
manifest and shortcut; 58 CEF `.pak` files; package EXE matches the build; portable
marker and package-local shortcut verified; no `TasiaViewerData` was packaged.
Build/package receipts are
`F:\tasia-kirsten\verified-builds\profile-legacy-udp-20261020-01-build.json` and
`...-package.json`; build log is `...-build.log`.

Focused checks pass: services/profile 15/15, identity/grid 4/4, name preservation
4/4, PowerShell package-script parser (0 errors), and `git diff --check`. Mom has
launched this candidate and reached the world; the screenshot shows `I-Grid Admin`
below the account username. The fresh log contains no profile-floater start or
profile-route/reply markers yet, so the legacy profile request/reply path remains
untested. It records 1,409 mesh-related 404 hits and a QUIC session fallback; these
are separate observations, not evidence that QUIC caused the mesh failures. On
Mom's disconnect report, the log showed no simulator ping for 100.633 seconds,
then marked the circuit dead and dropped it at `13:24:39Z`; no fatal/crash marker
was found. Earlier QUIC fallback at `13:23:00Z` says the simulator did not confirm
the QUIC session and the client recovered over UDP. The screenshot also shows
widespread blue triangulation; no matching wireframe log marker was found. Its
cause remains unresolved. The older isolated candidate and both desktop
installs/profiles remain untouched. Next: reconnect, test own/other profile and
Groups/Picks/Notes tabs, then inspect the new session log; investigate rendering,
HTTP 404s and simulator-circuit timeout independently.

## Requested OpenSim / QUIC / Grid Manager / login audit (2026-10-20)

Confirmed the active tree is `/home/marty/opensim/tasia-kirsten`; the harness
checkout `/home/marty/Kirstensviewer` is separate and remains untouched. The
working tree already contains extensive uncommitted changes; preserve them.
Compared the protocol templates to `/home/marty/opensim/tasia-viewer-2026` using
the repository comparator: 12,000 synthetic packets pass, both templates contain
483 messages, and all 26 critical avatar/movement/connection definitions match.
The exact field-level deltas are listed in `OPENSIM_PROTOCOL_AUDIT.md`: old-only
`MapBlockReply.Size` and current-only `RezScript.NewScriptInfo`; no message IDs
are unique to either template.

Verification this audit pass: all 57 discoverable `tasia_*test.py` tests except
the argument-requiring protocol comparator passed; running that comparator with
the old viewer path passed separately. `git diff --check` passed. A bare unittest
discovery initially reports the comparator's intentional usage error because it
requires an explicit reference-tree argument; that is a test invocation issue,
not a product test failure. No full Windows build, DPAPI account-store test, GUI
login interaction, live profile tab, or live QUIC handshake was performed.

Corrected the compatibility matrix where its October 18 notes were stale: the
current tree already has tested host-bake HTTP routing, supported-type legacy
wearable transfer, source-level bake/upload handling, and QUIC integration. These
remain harness evidence only; visual texture failures and the unconfirmed live
QUIC handshake remain open. SQLite/DPAPI stays the required account store. The
protocol ledger now sets the immediate next audit step: design and test legacy
inventory delete/purge behavior, especially async purge confirmation before parent
folder deletion. Grid Manager discovery/metadata and the full SQLite-backed login
UI comparison remain separate follow-ups; do not import the old credential
encoding. Obtain a fresh QUIC session log or packet trace before altering handshake
timeouts or acceptance conditions.

### Legacy OpenSim inventory read path compared and restored (2026-10-20)

Compared the active source directly with
`/home/marty/opensim/tasia-viewer-2026/indra/newview/llviewerinventory.cpp` and
`llinventorymodel.cpp`, and verified the corresponding message handlers in the
checked `/home/marty/opensim/telep-npc` server. The active tree had kept HTTP/AIS
fetches but had lost the old no-cap UDP fallback for `FetchInventory` and
`FetchInventoryDescendents`, as well as the `FetchInventoryReply` and
`InventoryDescendents` callbacks. This meant adding only send requests would
still not restore legacy inventory reads.

Restored the item and folder request paths, registered both response handlers,
and applied replies only on OpenSim grids with a matching AgentID. Folder replies
update categories/items, folder version/count, and notify inventory observers.
HTTP/AIS paths retain priority; this does not change Second Life behavior. Did
not port inventory item/folder deletion or purge in this slice; those operations
need their own callback/model consistency tests.

Verification: `tasia_legacy_inventory_fetch_test.py` compiles and executes the
actual production item/folder request methods against service doubles, including
OpenSim vs SL gating, HTTP/AIS priority and legacy UDP route selection; reply
registration/grid/agent guards are checked. It passes. OpenSim services/profile
15/15, identity/grid 4/4, and `git diff --check` pass. This is source-fixture
evidence only: no full viewer build or live inventory request was performed.

### Inventory mutation and asset-path comparison (2026-10-20)

Follow-up comparison of old/current `remove_inventory_item`,
`remove_inventory_category`, and `purge_descendents_of` confirms a separate
compatibility gap. The old viewer sends UDP `RemoveInventoryItem`,
`RemoveInventoryFolder`, and `PurgeInventoryDescendents` without AIS; active
helpers currently only warn when AIS is unavailable. The checked OpenSim server
wires remove packets to `InventoryService.DeleteItems`/`DeleteFolders` and handles
folder purge asynchronously via `InventoryService.PurgeFolder`. No mutation
fallback was ported: the old client optimistically edits local inventory and
fires callbacks without server acknowledgement, while category deletion chains
purge to delete. Add dedicated callback/model and failure tests before restoring.

The asset comparison found that old source permits generic non-Second-Life UDP
asset transfer when ViewerAsset is missing. Current source deliberately narrows
that recovery to OpenSim body-part/clothing assets and only missing-capability or
HTTP 404/410 cases; it re-finds live pending requests and checks session identity
after coroutine suspension. Retained this narrower fallback; no source evidence
justifies broadening it to mesh, arbitrary assets, or HTTP permission failures.

### Grid Manager and remembered-password comparison (2026-10-20)

The old reference has no `llfloatergridmanager.cpp` or equivalent Tasia catalog/
metadata-discovery UI; that feature is current-tree-specific. Its `LLGridManager`
does retain manual grid/login URI selection, so the active code continues to keep
exact login paths and uses the authority only as a grid key. Active source guards
custom login URIs, imports bounded catalog XML atomically, persists user metadata,
and restricts remote URL fields; the identity/grid fixtures cover the catalog
merge and write bounds. Hypergrid login/teleport URI variants are not yet audited.

Remember-password comparison: old login UI treats the credential authenticator's
MD5 secret as a non-recoverable placeholder. The active UI first checks the
per-grid SQLite account for a recoverable password, hashes that only for the
existing login authenticator, and stores the original only after successful
authentication via the encrypted SQLite/DPAPI store. Unchecking remember-password
clears that row; forgetting the username removes its account. Legacy secure-store
fallback remains limited to system grids, so global SL credentials cannot be
borrowed by OpenSim. No credential encoding was changed. Existing standalone
store tests do not exercise the full login panel, password checkbox, or grid-switch
GUI flow; those still need Windows/UI verification, and the Linux session did not
run the Windows DPAPI executable.

### Stepwise OpenSim protocol delta ledger (2026-10-20)

Created `OPENSIM_PROTOCOL_AUDIT.md` as the ordered source of truth for protocol
differences/misses, evidence, intentional differences, and remaining verification.
Re-ran `tasia_protocol_compare_test.py` against the actual old viewer: 12,000
synthetic packets pass; 483 messages in each template; all 26 critical definitions
match; only `MapBlockReply.Size` and `RezScript.NewScriptInfo` differ at the field
level. Compared those differences directly with the handlers and checked server.
The current map path misses optional region size data the server sends for
regions >256m; the old map path consumes it. This is the next focused change,
because it crosses message decode and map/minimap extents and needs bounded-size
tests. `NewScriptInfo` is current-only but is not classified as a compatibility
miss until server packet decoding/template-script behavior is verified.

Ledger records already-restored legacy inventory reads, the separately unported
inventory mutations, profile paths, wearable/appearance, region/terrain, QUIC,
HTTP lifecycle, mesh/limits, and login/grid work. No further protocol code was
changed during ledger creation. See the ledger's “Next exact action”; continue
one step at a time and update the ledger and matrix after each verified slice.

### Old viewer QUIC source comparison (2026-10-20)

Used the actual reference implementation at
`/home/marty/opensim/tasia-viewer-2026` (the QUIC files also exist in its
`tasia-viewer-2026-work` tree), not just server code. The old viewer's
`message.cpp::sendMessage()` queues non-`UseCircuitCode` traffic until ready;
`llviewergenericmessage.cpp::process_generic_message()` handles `quicready` by
marking the circuit ready and flushing that queue. It does not verify that the
control message arrived over QUIC. `LLCircuit::sweepDeadQuicCircuits()` removes a
QUIC circuit on transport failure and explicitly says no LLUDP fallback. There
is no age-based handshake-ready timeout in that source. Connection retries are
configurable in `settings.xml`, defaulting to one.

Current source strengthens the `quicready` check to require the same received
QUIC route and adds UDP recovery, but falls back after a fixed five seconds even
when the QUIC transport itself connected. The checked native server associates
QUIC before sending `quicready`; Quick-G bridges simulator replies back over the
viewer QUIC stream. Thus there is no source evidence to justify weakening the
transport-origin check. The five-second timeout is a concrete behavioral
difference and a candidate for investigation, not yet a proven root cause: the
old server path, bridge deployment and actual ready-packet arrival in Mom's
session are not captured. Preserve this distinction; do not claim that making
the timeout longer fixes a missing server confirmation.

## Prior isolated candidate runtime findings (read-only inspection, 2026-10-20)

- The test log records QUIC fallback at `11:16:37Z`: TLS/QUIC transport connected
  to the advertised host, but the simulator did not confirm the QUIC session.
  The client then recovered the circuit over UDP. It records no certificate/TLS
  error and no `quicready` success. This is a missing application-level session
  confirmation, not evidence that certificate verification caused the fallback.
  The isolated profile's `TasiaQuicVerifyCert` was saved as `0` at shutdown
  (`11:18:23Z`), but the log does not establish when Mom changed it relative to
  QUIC initialization. The QUIC UI is green only when the circuit is marked ready;
  the report of a green interval is not corroborated by this log, which has only
  the fallback warning. Do not treat verify-off as the QUIC enable/disable switch.
- Same session: 848 mesh-header HTTP requests returned `Http_404` (not retried),
  plus six asset HTTP 404s. These are HTTP capability fetches, separate from QUIC
  simulator packet transport; their timing after UDP fallback does not prove
  causation. Self legacy appearance was accepted; shape/skin/hair/eyes were
  present, but `local_appearance=0`. No evidence here connects the mesh 404s to
  QUIC or establishes that the self avatar was fully rendered.
- The profile log contains no `AgentProfile`/profile request outcome to identify
  which path was taken. Source audit found current behavior uses the `AgentProfile`
  HTTP capability when advertised, using legacy UDP only when the capability is
  absent or the HTTP result is unsuccessful. The i-Grid source tree has legacy
  `AvatarPropertiesRequest` replies (profile, interests, groups) plus generic
  `avatarpicksrequest`, `avatarnotesrequest` and `avatarclassifiedsrequest` routes.
  The viewer has handlers for these paths, but live request/reply completion is
  still unverified. A successful-but-incomplete AgentProfile response could avoid
  the current HTTP-failure fallback; determine the actual selected route before
  changing it. The server source inspected is not proof of the deployed server's
  exact version/configuration.
- Mom reports the custom rank `I-Grid Admin` should appear below the username and
  says tag coloring is now working. The previous package's rank/title appeared
  above the username; the current isolated candidate includes the source change to
  place it below the username while retaining its configured tag color.

## Old-viewer UDP profile audit and current-source port (2026-10-20)

Compared `/home/marty/opensim/tasia-viewer-2026`'s profile UDP layer with current
viewer code and the checked `/home/marty/opensim/telep-npc` server implementation.
Ported the server-supported paths rather than copying the old viewer wholesale:

- `AvatarPropertiesRequest` → `AvatarPropertiesReply` remains the legacy base
  profile route. I-Grid additionally emits `AvatarInterestsReply` and a separate
  `AvatarGroupsReply` (EventQueue-backed in the checked source). The viewer now
  consumes the groups payload into the profile group list; Interests stays a
  registered no-op because the current profile UI has no interests panel.
- Picks use generic `avatarpicksrequest` → `AvatarPicksReply`; Notes use
  `avatarnotesrequest` → `AvatarNotesReply`. Added payload structs, decoding,
  observer notifications and pending-request cleanup. Picks and Notes tabs now
  request their own legacy data when opened on OpenSim without `AgentProfile`;
  own-picks quota tracking follows the same endpoint. The parent no longer marks
  those lazy tabs “started” before their requests can run.
- Notes writes use the server-supported `AvatarNotesUpdate` only when the OpenSim
  `AgentProfile` capability is absent. Generic requests are duplicate-suppressed
  at the common send boundary. The classifieds-list panel no longer treats a
  `ClassifiedInfo` payload as a classifieds list.
- Deliberately did NOT copy the old viewer's generic `avatargroupsrequest`: the
  inspected i-Grid module has no such registration; it sends groups with the base
  profile reply. Also did NOT port the old full-record `AvatarPropertiesUpdate`
  save yet: the packet replaces several profile fields together, with legacy
  text limits (AboutText 512 bytes, First Life 255 bytes). Current source lacks a
  safe shared snapshot to preserve untouched fields, so a speculative fallback
  risks erasing/truncating profile text. Image uploads and hide-age also lack a
  verified legacy write path here. These are explicitly still unsupported by the
  source-only port, not claimed fixed.

Added bounded route/status/result and tab-completion logs without capability URLs,
profile text, or avatar UUIDs. The focused source tests pass as listed above. The
current source has now been compiled and staged as the isolated candidate described
at the top of this plan; live protocol/tab completion remains unverified.

## Historical: profile loading / circuit drop / missing music Stop report

Mom reports the profile floater remains loading, then the in-world session drops;
the attached view still shows the old customization and no visible music Stop.
Read-only inspection of the isolated package log found:
- No profile exception/fatal/crash entry or explicit profile-response diagnostic;
  this log alone cannot identify why the profile stays loading.
- Avatar readiness samples report shape/skin/hair/eyes present, but
  `local_appearance=0`; this does not prove that profile identity/cosmetics loaded.
- A simulator circuit timed out after about 100 seconds and was dropped; the
  following `LogoutRequest` was sent on an unknown circuit. The log then reaches
  normal subsystem cleanup. This supports a circuit/session loss, not a proven
  profile-triggered viewer crash. No Windows crash-dump/event evidence was checked.
- Source confirms the status-bar media button is a pause toggle calling
  `setAllMediaPaused`; parcel-music Stop is wired in Nearby Media instead. Thus the
  expected visible Stop affordance is absent from the status bar in this source.

No viewer was relaunched and no package/profile/source was changed during this
inspection. Keep the desktop installs/profiles untouched. Next: repair profile
loading and define an obvious music Stop control in a separate focused change;
first verify which profile request/reply the target OpenSim grid supports and keep
the circuit-timeout investigation separate from profile UI. Rebuild only after the
candidate is closed and with Mom's isolated-test consent. Do not infer a crash from
the orderly log shutdown or claim profile/music behavior fixed from source review.

## Historical: earlier combined Windows build candidate

## Historical: 2026-10-20 local profile/audio changes (pre-build status superseded)

Mom clarified the stuck floater is her OWN profile and the expected name styling
is her own entry in the remote `config.json`. Local changes now prepared:
- OpenSim legacy replies complete the unsupported Picks and Notes panels instead of
  leaving their loading state active; absent legacy fields are not fabricated.
- When the identity catalog applies, refresh the already-open self profile so
  explicit `display_name`/`username` overrides appear without closing the floater.
- Add an EN/PL `Stop Music` action in the Avatar/Movement menu. It leaves status-bar
  geometry alone and explicitly stops parcel music and the standalone player.

Focused tests: OpenSim services/profile/menu 13/13, identity/grid 4/4, name
preservation 4/4; `git diff --check` clean. These do not establish Windows build or
live profile/audio success. The observed simulator circuit timeout remains a
separate unresolved issue. Do not alter either desktop install or the isolated
profile; any new Windows build is pending Mom's explicit test-build go-ahead.

The canceled wearable-only attempt compiled/linked but exited during MSBuild
post-link staging; no success receipt, deployment, or runtime claim. Its standalone
CMake asset-copy commands now pass. After the agreed repairs were complete, staged
the current wearable, status-bar, name mapping, grid-cache filename, OpenSim mesh
capability, and bake-upload error changes together in one backed-up Windows build.
Build receipt `/tmp/opencode/acer-combined-avatar-build.json` reports success:
exit 0, zero errors, all 22 source hashes match, and fresh EXE/PDB hashes recorded.
The EXE SHA256 is `C6FCBBE0BEBB3F44FDC03561ADF7E76F9CA1B9654E7D381E00A156D7FCFE8014`;
PDB SHA256 is `C1C317F42C4F6DE035ACADD29545C506D077C965E799FE9EB13E3AAF32A960E1`.
A first isolated package (`combined-avatar-status-identity-20261020-01`) was
created with 7,297 files but its interactive launch returned Windows
`0xC0000135` (missing DLL); no viewer process/profile was created. It is preserved
and not considered runnable. A second isolated package is now running at
`F:\tasia-kirsten\test-builds\combined-avatar-status-identity-20261020-02`. Its
EXE hash matches the build; root runtime DLLs were copied read-only from the
previously verified `opensim-services-ui-20261019-01` package (no EXE/settings/
profile copied), and its source EXE was unchanged. User-requested interactive
launch succeeded: PID91900, session1, Responding=true; fresh isolated
`TasiaViewerData` was created. MainWindowTitle was blank at the last check, so Mom's
visual confirmation of login/UI is still required. No provider keys were injected.
Do not replace either desktop package or touch its profile.

The first inline-payload wrapper stalled before making backups or changing files;
it was stopped after confirming no compiler process. A file-based archive with the
22 intended files transferred successfully. The guarded apply-and-build completed
successfully (receipt/hash details above). The isolated test copy was run on I-Grid
and Mom has reported remaining mesh/outfit, QUIC indicator, media-key and nametag
problems; see the runtime investigation below. Pre-build focused recheck was 16/16;
the added nametag-path regression passes 4/4. Build/tests do not substitute for
live feature verification.

The first archive-apply attempt stopped before backup/write on a PowerShell JSON
array-count check. The remote archive was independently confirmed to contain all
22 manifest entries; the parser was corrected and the guarded apply/build completed.
Mom authorized launching the result for her test. The separate isolated candidate
is running; do not overwrite either desktop viewer or touch its profile. FTP is
preferred for subsequent file transfer; FTP itself cannot launch the Windows
executable.

Immediately before this build, all 29 `tasia_*test.py` suites passed (report:
`/tmp/opencode/combined-regression-20261019/results.json`). This does not prove live
grid appearance, status-bar rendering, or upload service behavior. Once the build
finished with verified receipt and EXE/PDB hashes; desktop deployment remains
separate pending Mom's instruction and runtime regression check.

## 2026-10-20 follow-up: live test report (diagnosis in progress)

- Current test package's latest `TasiaViewer-LOG.log` (00:35:25Z–00:40:27Z):
  repeated HTTP 404s from texture/asset fetches and mesh headers; `Initial outfit
  uses COF`; server reports it does not support baking; one local bake upload did
  complete and was assigned, and legacy appearance was sent/accepted. The log
  contains no QUIC/TLS/certificate-tag lines, so it does not identify a QUIC
  certificate failure.
- The test profile still stores `TasiaQuicVerifyCert=0`; `TasiaQuicEnabled` is
  absent from user settings and falls back to the packaged default `1`. This does
  not match Mom's report that certificate verification was enabled at the time of
  the symptom. This is not a verify-on test; Mom should confirm/toggle the checkbox
  and restart when testing certificate validation.
- I-Grid server source/docs (`/home/marty/opensim/telep-npc`) describe simulator
  QUIC transport and per-region endpoint advertisement; region-crossing can fall
  back to UDP if the wrong simulator port is advertised. Server config falls back
  to a self-signed `CN=localhost` certificate when its configured certificate is
  missing; the actual live I-Grid cert/config has NOT been checked. Mesh, texture,
  capability and bake uploads are HTTP paths, distinct from the QUIC LLUDP circuit.
- Compared `/home/marty/opensim/tasia-viewer-2026`: its nametag supports explicit
  username alias rendering when SL display-name mode is off. The current build's
  legacy-name branch rendered only the full/display alias and omitted an explicit
  username alias. Added the independent username line, retaining the real full
  name and page_name exclusion; focused identity/name-preservation test is 4/4.
- Mom authorized key provisioning. The isolated package now has a same-user DPAPI
  `TasiaViewerData\secrets\media-keys.dpapi` payload (342 bytes), hash pin, and
  restricted ACL; `Launch-TasiaViewer.ps1 -ValidateOnly` confirmed the EXE hash and
  both encrypted keys without launching. `Tasia Viewer.lnk` now starts that
  encrypted launcher. Updated package manifest has 7308 entries, excluding the
  private profile and mutable `.log` files. The key-enabled candidate was launched
  via the launcher: PID91180, session1, Responding=true, receipt
  `/tmp/opencode/acer-launch-media-combined-test-02.json`. This verifies setup, not
  successful GIPHY/KLIPY API/UI behavior.
- Next: (1) Mom tests the key-enabled copy and confirms the expected vs shown
  nametag and whether the alias is `display_name` or `username`; (2) enable
  certificate verification in the test profile and restart for a verify-on test,
  then capture the exact QUIC/certificate error and verify the live I-Grid
  certificate/advertised endpoint; (3) trace repeated HTTP 404s independently
  before changing QUIC or claiming outfit/mesh success.

## Latest identity contract: page_name is website-only; empty names preserve real names

Mom clarified the exact mapping after her screenshots: `page_name` is for the
website ONLY. Empty/missing/whitespace display_name or username preserves that
corresponding real name; explicit nonempty values may override only that UI field.
Colors are independent. My interim blanket color-only interpretation was too
broad and has been corrected. Native group titles/name preferences stay intact.

LOCAL SOURCE FIXED, NOT WINDOWS-BUILT/DEPLOYED YET:
- Parser reads only explicit display_name/username, never page_name or alternate
  name override aliases. Account type stays canonical.
- Name formatters preserve empty fields individually; when both empty, preserve
  original complete-name formatting exactly. Native group titles stay unchanged.
- Profile names default to real values; explicitly nonempty overrides apply to
  the matching field only. Color independent; restore each panel's original
  theme colors and names on nonmatching/disabled entry. No badge/note substitution.
- Network labels/docs now describe the clarified mapping.
- Tests: name-preservation3 PASS (actual formatters/profile callback, independent
  one-field overrides), identity/grid4 PASS (missing/empty/whitespace names with a
  populated website page_name ignored), services11 PASS before the label refinement.

LOCAL status-bar layout fix also prepared and actual-method fixture now PASS:
calculate extents entirely in child-local space, translate all controls including
hidden ones, keep parent's right edge fixed; idempotence/window resize/search
visibility tested. This is NOT in the running desktop EXE yet.

Existing Windows wearable build hit LOCAL SSH tool timeout900s, not a compiler
failure. Single post-timeout diagnosis found cmake92040/MSBuild/link85604 still
running, log at code generation, no compiler errors yet, no result receipt yet.
Do not restart it or change Windows compiled sources. Attached event-based
WaitForExit observer (no polling/rebuild) via background shell
sh_1559667e0001X0q01VMv2T2otM; expected receipt
/tmp/opencode/acer-wait-existing-avatar-build.json. Original attempt tag
wearable-transport-20261019-205859. All newer code edits are Linux-only and do not
alter that Windows snapshot. Next combined build must include names+bar fixes.

Upload screenshot explicitly says file upload failed: Service unavailable.
No longer ask Mom to retype this already supplied message. Its precise service
route/root cause remains under investigation; latest error body was HTML.

## Latest runtime observation: user-requested launch of existing desktop OS copy

On Mom's explicit request, launched EXISTING desktop tasia-os B507A613... through
its verified encrypted key script via an interactive limited scheduled task, no
startup trigger. Receipt /tmp/opencode/acer-launch-desktop-os-now.json: task0,
PID87588/session1. Explicitly told Mom this copy does NOT yet include the new
wearable-transport fix; do not replace/stop the running viewer automatically.

Mom now reports an unavailable-service message and missing RIGHT-HAND status bar.
Latest19:06-19:07Z log confirms repeated mesh404s AND UploadBakedTexture failures:
upload error bodies contain HTML titled "i-grid Property", not an asset protocol
reply. This is evidence to investigate advertised capability routing/HTTP handling,
NOT proof of a viewer-only or server-only cause. Request exact on-screen message.
The SAME old binary now reports hair1/eyes1 (previous_loaded1, local_appearance1),
but uploads_pending1 and head bake undefined. Therefore missing hair/eyes is NOT
a stable binary-only failure and must not be declared solved by the untested fix.

Status-bar production geometry regression test exists and FAILS on current code:
tasia_status_bar_layout_test.py. It initially had a fixture indentation warning
(fixed); current failure is the actual excessive computed bar width. Repair after
the wearable build completes; no production layout patch applied yet.

## Current execution: repair all agreed features, appearance first

Mom explicitly requests all discussed functions to work. Do not equate broad
scope with proof of runtime success or permission to discard profiles/secrets.

First focused source repair (Windows build in progress): restore missing OpenSim
wearable asset transfer support from tasia-legacy, restricted to bodypart/clothing.
Refresh ViewerAsset even when a previous region's URL was nonempty. Fall back once
via the existing TransferRequest completion path for absent cap or HTTP404/410,
never for SL, other asset types, or HTTP403. Re-find pending requests after yields,
retain completion ownership, adjust metrics, and guard coroutine storage/session
identity. New test tasia_wearable_asset_transport_test.py:2 PASS (compiled actual
fallback body with ASan/UBSan plus coroutine integration checks). No deployed fix
or live appearance success yet. Source delta preserves prior Windows source+EXE/PDB.

Remaining verification/repair matrix:
- Appearance/mesh/eyes/hair: BLOCKING live regression.
- Windows grid-scoped inventory/favorites filename sanitizer is implemented;
  focused production-helper test passes. Windows build and real cache-path check
  remain.
- Mesh capability refresh/GetMesh2/GetMesh handling and opaque OpenSim asset URLs
  are implemented; focused tests pass. Build and live 404 diagnosis remain; do not
  assume all 404s share one cause or alter SL behavior.
- QUIC/TLS/UDP recovery: fixture-tested; live ready handshake still fails in latest
  I-Grid log, requires protocol comparison with server. Teleport/crossing unproven.
- Grid lists/identity/profiles/marketplace/currency: fixtures only; actual grid
  requests and grid+UUID identity updates still require real session verification.
- Ordinary IM vs conference: regression guard present, live proof absent.
- Native globe and persistent audio Stop: fixtures only; need native GUI checks.
- Chat/Feed, clipboard/FFmpeg MP3, GIPHY/KLIPY/previews: preserve and regression-test;
  encrypted launch-chain test passed, provider UI live proof still required.
- AI translation architecture: use the Tasia API server; do not ship provider keys
  or call AI providers directly from the viewer. Define the server route/auth,
  provider/model policy, limits and error contract before implementation.
- Launcher/news: launch-chain and metadata tests passed; real WPF UI unproven.
  Safe download/apply/rollback updater is NOT implemented and is not a complete
  installer. Published nine-file payload must never be treated as full installation.

## ACTIVE BLOCKER: live I-Grid appearance failure after audit repair

Mom reports broken avatar, specifically hair and eyes, in latest desktop OS copy.
Read-only Acer inspection confirms EXE B507A613...; latest OS session ended normally
at 2026-10-19T18:52:14Z, no viewer running when inspected. No new build, cache purge,
profile/outfit replacement or XML change has been made for this report.

Real log evidence (grouped, UUIDs and capability URLs redacted):
- Wearable downloads failed8 times; required types2 and3 requested1/found0.
- Self readiness explicitly shape1/skin1/hair0/eyes0, hair_defined1.
- COF broken-target fetch2, library lookup2, then links resolved and outfit reload.
- Mesh header HTTP4041007, mesh LOD HTTP40470; texture HTTP404 also present.
- QUIC connected but no simulator session confirmation; recovered over UDP.
- Inventory cache rename fails on `<UUID>.i.let-us.cyou:8002.inv.llsd.gz` (Windows
  invalid colon). This is a separate confirmed path defect, not yet proven cause
  of the missing bodyparts. Favorites path contains the same unsafe grid suffix.

Current diagnosis: missing wearable assets are directly confirmed, NOT explained
by preferences XML. Cause of HTTP failures still unproven; distinguish stale/wrong
capability routing, request URL construction, absent assets and simulator errors.
Inspection found viewer asset requests are HTTP-only; mViewerAssetUrl is cached,
and mesh requests append `/?mesh_id=` to ViewerAsset. These are investigation
leads, not verified root causes. Do not patch them speculatively or blame a model.
Next: compare same avatar/grid with a known-working viewer; inspect a live failing
asset request safely (no capability tokens in logs/chat), then one focused fix.
Receipts: /tmp/opencode/acer-live-avatar-audit.json and acer-live-avatar-grouped.json.

## Audit correction: two desktop folders; empty launcher script repaired (20:35 Acer time)

Mom clarified that she wants TWO desktop folders, not 66. The earlier interpretation
of "full" was wrong; it did not authorize choosing 66 as the intended count.

Done:
- Preserved the used tasia-os-01 profile by renaming that folder to `tasia-os`;
  renamed tasia-sl-01 to `tasia-sl`. Moved the other 64 copies, without deletion,
  to `C:\Users\Acer\TasiaViewerArchives\desktop-copies-20261019-203519`.
  Existing OS grid choice Alternate Metaverse retained; SL remains Agni.
- Found a real launch failure: `Launch-TasiaViewer.ps1` was ZERO BYTES in the
  candidate and desktop copies. Earlier hash/parse audits incorrectly accepted
  that empty script. The provisioning wrapper substituted a removed placeholder
  instead of setting TASIA_LAUNCHER_B64. Do not reuse that old provisioning path.
- Added authoritative `scripts/Launch-TasiaViewer.ps1`; copied actual bytes to
  both desktop folders AND the canonical candidate, without changing key values.
  It validates EXE hash and DPAPI keys, sets child-only environment variables,
  rejects a second instance using the same profile, and supports `-ValidateOnly`.
- Rebuilt WPF launcher: missing/empty scripts are rejected before Process.Start;
  wait for PowerShell exit and report launch failures instead of silently closing.
  New launcher SHA256 E326F955AD52A210BFF6A98BCE01DB3CD1504DDEFC1A4114EFFFA5EC3BBC6AE0.
- Real logs showed undefined DJ/animation/retries/fallback/timeout controls in the
  mixed panel, in addition to content extending below the panel. Replaced its
  registration with dedicated compact `panel_preferences_tasia_network.xml`
  and `panel_preferences_tasia_media.xml` EN/PL tabs. Only defined settings are
  exposed. Unsupported legacy UI was not made functional by inventing controls.
- Existing TEST shortcut now targets the desktop OS launcher. Both folder-local
  shortcuts target their own launcher. Rebuilt manifests from the prior verified
  baseline plus explicit repaired files; runtime .log files excluded, not blessed.

Evidence:
- Local/Windows comparison of 221 changed/new technical source files: only the
  four newly edited XML paths differed at that point. All C++ matched. The 294-file
  prior build snapshot differed only in the two previously reordered preferences
  XML files. Build4 really passed; build5 failed its output-mtime check, not compile.
- One fresh incremental viewer target build exit0; launcher and selftest publish
  exit0. Viewer unchanged B507A613... as expected for XML/launcher-only repairs.
- Launcher selftest 13 PASS, including rejecting empty/missing launch scripts and
  live news/build-data fetch. Both desktop copies and canonical candidate pass
  EXE+DPAPI ValidateOnly, 2389-byte script, 7311 manifest entries each.
- A compiled mock child (NOT the real viewer) received both provider keys; parent
  environment unchanged; wrong EXE hash and empty encrypted file rejected.
- Local tests: OpenSim services11, QUIC4, sound/globe4, identity/grid4 PASS.
- Independent final audit: exactly two desktop viewer folders, 64 archived copies,
  no manifest mismatches or duplicate user-setting keys, correct shortcut targets,
  no viewer running, permanent installed executable DA07BFE6... unchanged.

Receipts on Linux: `/tmp/opencode/acer-repair-build.json`,
`acer-finalize-two.json`, `acer-launch-chain-test.json`, `acer-two-final-audit.json`.
Windows: `F:\tasia-kirsten\audit-repair-*.log`, `audit-repair-final.json`.

Still not proven: real WPF interaction, native preferences layout and live grid/
QUIC/globe/audio behaviour. Next exact action: Mom opens `tasia-os\Tasia Viewer.lnk`
and checks the first Network tab (QUIC and certificate toggle) and Media tab.
Do not call the viewer "fully working" based only on builds/fixtures again.
No public-server changes, installation replacement, commit or push this audit.

## Release/update + news infrastructure is LIVE; launcher integrated

Server side (api.tasiaviewer.work docroot, published with backups):
- /viewer/updates/build_data.json - release metadata, version 26.9.4015, channel
  Tasia, platform win, file base URL and both metadata URLs.
- /viewer/updates/files/mainifest.list - "<sha256> <size> <path>" per file for
  the nine files of this release; every listed file is actually published.
- /viewer/updates/news.txt - plain UTF-8 release notes (never HTML).
- /viewer/updates/files/... - the real payload: TasiaViewer.exe plus the changed
  app_settings and XUI files.
All three endpoints verified over HTTP; the payload EXE downloads (47 MB).
/viewer/config/grid.xml was already live and is used by the viewer.

Launcher (Tasia Launcher/, .NET 8 WPF, built on the Windows machine):
- Reads the real version from the viewer executable, normalises the Windows
  comma form ("26.9,4015,0") to the dotted release form, shows update status,
  parses the manifest, and shows news with an offline cache.
- Starts the viewer through Launch-TasiaViewer.ps1, so the media provider keys
  are still supplied; a direct EXE start would silently disable GIPHY/KLIPY.
- Rejects manifest paths that are absolute or contain "..".
- Self test (separate console project, same shared sources): 10/10 PASS, including
  reading the published news and build_data over HTTP.
- Published self-contained, single file, 151 MB; installed next to the viewer in
  the package and pointed at by the desktop shortcut.

Final package audit (opensim-services-ui-20261019-01): 7307 manifest entries, no
duplicates, no mismatches, launcher present, key payload present, QUIC enabled,
certificate verification enabled, grid catalog enabled, identity catalog enabled,
network preferences tab first in both languages, Polish QUIC indicator present,
32 bundled grids, installation unchanged, no viewer running.

Known: the launcher only reads the release metadata; it does not install updates
(the prototype states this explicitly). Downloading and applying a release is the
next separate step, and the manifest is already the verified input for it.

## ROOT CAUSE FOUND: the media key file was EMPTY (why the keys never worked)

Mom's screenshot showed the age5 package, but auditing the provisioning exposed
the real defect: the provisioning script expected the encrypted payload in
$env:TASIA_PRIVATE_PAYLOAD_B64, and the launcher wrapper never set that
variable. The script therefore protected EMPTY data, and its own round-trip
check compared empty with empty and passed. The launcher has been decrypting an
empty key file on every launch, so GIPHY/KLIPY never received a key.

Fixed: the payload is now passed as a real PowerShell environment variable, the
script refuses to write a payload shorter than 32 bytes, an existing empty or
unreadable key file is replaced instead of kept, and the decrypted payload is
verified to contain key material. Verified on the remote: blob 342 bytes,
decrypted 119 bytes, contains the giphy key, secrets directory restricted to the
user and SYSTEM.

Also fixed: the package manifest is now rebuilt from the files on disk (one
entry per file, no duplicate path spellings), and the "Tasia - Network"
preferences tab was moved directly after "Tasia - Core" so it is visible
without scrolling the tab list.

## FINAL CANDIDATE: opensim-services-ui-20261019-01 (use this one)

Path: `F:\tasia-kirsten\test-builds\opensim-services-ui-20261019-01\Tasia Viewer.lnk`
Shortcut starts the encrypted key launcher, so GIPHY/KLIPY are supplied automatically.

Final audit of that package: launcher expected-hash matches the EXE, shortcut
uses the launcher, DPAPI key payload present, 7304 manifest entries with no
duplicates and no mismatches, QUIC default ON, grid catalog default ON, Tasia
preferences tab registered, Polish status bar has the QUIC indicator, 32 bundled
grids, no private profile packaged, installation unchanged, no viewer running.

Two older packages remain on disk and are superseded:
- opensim-services-20261019-01: correct launcher, but QUIC default OFF and no
  preferences tab (predates the UI fix).
- globe-audio-20261019-01: the age5 package Mom launched; QUIC off, 3 bundled
  grids, old embedded launcher hash.

Launcher hardening: the launcher no longer embeds a hash that can go stale. It
reads the expected EXE hash from TasiaViewerData\secrets\viewer.sha256, which is
written at provisioning time, so a wrong or replaced executable is always
refused. A run with a stale package path had written a launcher with the age5
hash into a newer package; both affected packages were repaired and re-verified.

Also fixed during this round: my own audit scripts used String.Split(string),
which PowerShell binds to a char array, so the QUIC-default check reported a
false negative. The regex-based audit confirms QUIC is ON in the final package.

## Mom reported "no changes, no QUIC settings or status" — three real causes

Diagnostic (read-only) findings, not guesses:
1. Mom launched the AGE5 package (globe-audio-20261019-01, log written 16:12Z);
   the new opensim-services package had a profile folder but no log, i.e. it was
   never started. Both age5 and the new package remain on disk, unchanged.
2. The QUIC/TLS settings were UNREACHABLE: panel_preferences_tasia.xml existed and
   held the checkboxes, but it was never registered in floater_preferences.xml, so
   the panel could not be opened. Now registered as "Tasia - Network" (EN) and
   "Tasia - Sieć" (PL), with the certificate-verification, grid-catalog and
   identity toggles added to it as well.
3. The Polish status bar was a reduced stub: no volume button, no preset icons,
   no menu search and NO QUIC indicator, so the status could never appear in the
   Polish UI. It is now generated as a full mirror of the English layout
   (scripts/generate_tasia_pl_status_bar.py, 25 named controls) and a test
   asserts the Polish set contains the English set.

Also: TasiaQuicEnabled now defaults to 1 for fresh profiles, so the status is
meaningful and the safe UDP fallback covers failures. Existing stored user
choices are still respected (a stored 0 stays 0).

Build opensim-services-20261019-01-build5 running with these fixes; the previous
build4 package stays untouched. New tests: preferences tab registered in both
languages, QUIC default enabled, Polish status bar completeness. Live GUI proof
still pending.

## READY FOR MOM'S TEST: OpenSim services + full grid list (age7)

Package: `F:\tasia-kirsten\test-builds\opensim-services-20261019-01`
Launch the package-local **Tasia Viewer.lnk** (it now starts the encrypted key
launcher, so GIPHY/KLIPY are supplied automatically).

Build opensim-services-20261019-01-build4: configure0/build0/errors0, finished
2026-10-19T15:57:38Z. EXE B507A613CDDA9D4F95926835AEFBF1FF4E3BBD26372AEF417BB2C9410FB4574E;
PDB D7BF0962986F96722A40033081C99342E4DF6A6C96FCF35358F8E026B193AAC3. Independent
EXE/PDB match GUID 65-45-59-3E-A9-D7-64-47-83-2E-F6-E8-49-B4-52-D6, age 7.
292-source manifest 3A9B939A3955B6014ADF51B15CB3E40670602560E663647F18BC41F29CB3441F.
Package verified: 7303 file hashes, 95 runtime assets, branding, FFmpeg/CEF/SDK,
portable marker and local shortcut. No private profile packaged; viewer NOT
launched; installation and age5 package unchanged. Media keys provisioned with
DPAPI round-trip verified and a restricted ACL.

Build history for this candidate: build1 failed on a zero-argument commit
callback, build2 passed, build3 failed on two API mistakes in the new grid-name
resolver (const getGrid, wrong iterator type), build4 passed. A separate
post-build restage recorded in $tag-restaged.json re-ran the runtime data copy
because CMake's copy dependency did not include app_settings/*.xml; that
dependency is now fixed for future builds (recorded post-build source delta,
CMakeLists BD150C1C... -> E369840C...).

What to test: GIPHY/KLIPY pickers (now automatic), the full grid list on the
login screen (32 bundled + published catalog), I-Grid profile contents, Buy L$
on I-Grid, the Marketplace button, the crowded status bar, Fresh Metaverse
login at os.tasia.work.gd:22000, and the 3D globe plus Stop behaviour. Live
profile/currency/marketplace/grid behavior and real QUIC negotiation are still
unproven.

## Fresh Metaverse discovered (os.tasia.work.gd:22000) — real QUIC endpoint found

Mom asked about os.tasia.work.gd:22000. Verified live:
- Login URI http://os.tasia.work.gd:22000/ (grid "Fresh Metaverse", 31 regions).
- get_grid_info (HTTP, not HTTPS) returns welcome/economy/about/register/help/
  password/gatekeeper/search/web_profile_url, all consistent with the fields the
  Grid Manager already understands. Economy helper is
  https://os.tasia.work.gd/ostools/ and its own page states the viewer calls
  {economy}currency.php — exactly what getCurrencyHelperURI() builds.
- The landing page advertises "Robust 22000 • QUIC 22002", so this simulator
  family DOES advertise QUIC. The server-side implementation
  (telep-npc/Docs/QUIC_REGION_CROSSING_FIX.md) confirms the wire contract the
  viewer must honour: the login response and EnableSimulator/TeleportFinish/
  CrossRegion events carry sim_quic_host + sim_quic_port, and the destination
  region's own QUIC port must be used. That matches the negotiation already
  implemented in lltasia_quic.h and the llmessage QUIC routing.
- Fresh Metaverse has no /hg/config.json (404) and no own marketplace, so it
  uses the shared Tasia-NGC marketplace at https://marketplace.i.let-us.cyou.

Changes: Fresh Metaverse added to the bundled grid list (32 entries) with its
real endpoints, and its marketplace resolves to the shared Tasia-NGC
marketplace. Identity catalog for it stays absent rather than guessed.

Also fixed in this round: llstatusbar.cpp used LLView::getNextSibling(), which
does not exist (build1 error C2039). Now iterates LLView::getChildList(), and
the fixture asserts getNextSibling() is absent so it cannot regress.

## OpenSim service/grid-list fixes after Mom's live test (age6 candidate next)

Mom's live report from the age5 package: GIPHY/KLIPY not supplied automatically,
user profile/config not loading, only 4 grids, "Buy L$" and Marketplace wrong,
status bar too crowded (something pushed off-screen).

Fixed in source (Windows rebuild pending):
- GIPHY/KLIPY: package now has a persistent DPAPI-encrypted launcher
  (TasiaViewerData\secrets\media-keys.dpapi, restricted ACL) and the package
  shortcut starts that launcher, which injects TASIA_GIPHY_KEY/TASIA_KLIPY_KEY
  into the child process only. Keys never in EXE/JS/HTML, never printed.
  Provisioned and round-trip verified; viewer not restarted by me.
- Profile: OpenSim grids without an AgentProfile capability got NO request at
  all. Now falls back to the legacy UDP AvatarPropertiesRequest and maps the
  smaller legacy reply onto defined profile fields (no groups/picks/notes
  invented). HTTP profile replies are also session/grid guarded.
- Identity config: the requested viewer/config/config.json returns HTTP500. The
  working catalog is https://i.let-us.cyou/hg/config.json and has NO grid
  field, so entries are now scoped by the publishing grid's host
  (sourceGridForUrl) with an explicit grid field still honoured. page_name is
  accepted as display-name alias.
- Grids: the bundled default list was only 3 entries and only applied to fresh
  profiles. Now 31 entries (original Tasia list + published catalog) loaded for
  EVERY profile through the same additions-only, tombstone-aware importer, and
  the published catalog URL is grid.xml (grids.xml 404s) with the setting ON.
  Second Life built-ins stay protected and are not duplicated.
- Marketplace: grid-specific URL (https://marketplace.i.let-us.cyou for I-Grid),
  no SL OpenID cookie forwarding, untrusted content, host-precise matching.
- Currency: helper URI resolved per request (no cross-grid cached endpoint),
  no SL HTML currency flow on OpenSim, empty helper reports an error instead of
  guessing. ostools currency.php answers 405 to GET, so no success is claimed.
- Status bar: extra Stop button removed; the bar now sizes itself to its
  children (reshapeToFitContent) so nothing is pushed off-screen, and the stop
  action lives on the existing media control / Nearby Media panel.

Local: tasia_opensim_services_test.py 8 PASS; all 11 suites PASS; diff clean.
Still unproven: live profile/currency/marketplace/grid-list behavior, the
identity catalog applying to real residents, and the QUIC path.

## READY FOR MOM'S TEST: globe/audio combined candidate (age5)

Windows Release/DX11 build2 PASSED: configure0/build0/errors0, completed
2026-10-19T14:29:50Z. Independent EXE/PDB match: GUID
65-45-59-3E-A9-D7-64-47-83-2E-F6-E8-49-B4-52-D6, age5.
EXE467C90230D073CE6E829512161CC5CB1DAEDFBE3580A385D66192911BDA9A3F4;
PDB76D961EA5A323430CCBCEDD79CC659BAE4AF0A51DFED70459E0F2771B4D6EF33.

Isolated package F:\tasia-kirsten\test-builds\globe-audio-20261019-01 verified:
7303 file hashes,95 source/runtime assets, branding, FFmpeg/CEF/SDK, portable marker
and local shortcut. No private profile packaged; viewer NOT launched. Installed
E:\TasiaViewerLatest and age4 candidate unchanged. No commit/push.

Post-build verifier caught missing default-grid XML in direct build output.
Cause: Copy-ViewerManifest.ps1 explicit criticalFiles list omitted the new file.
Added it, added regression assertion, syntax/hash checked and backed up the script,
then recopied runtime data WITHOUT compiling or changing EXE/PDB. Exact staging
script change E52E4631... -> 5C8D25230D1295D894A6CD9E6C302B37B987654F9A948670D6B2A7B84A3C361D;
separate restaged receipt preserves this post-build change from frozen sources2.
Reverification then passed all95 assets and full package hashes.

Next exact action: Mom opens the package-local Tasia Viewer.lnk, logs in and tests
actual globe rendering/rotation/zoom/region selection + Teleport, flat-map toggle,
and Stop/Play surviving parcel updates. Local fixture passes and binary/package
verification do NOT prove those GUI behaviors. Checklist /tmp/opencode/globe-audio-test-checklist.md.
Updates/release and startup news remain a separate task, not in this candidate.
Receipts /tmp/opencode/globe-audio-20261019-01-{build2-status.txt,restaged.json,
verified.json,package.json}. No build remains active.

## Sound + native 3D globe; combined test build now requested

ACTIVE RETRY: globe-audio-20261019-01-build2. Applied a reviewed five-file delta
(two callback source files, regression test, two status documents), backed up at
F:\tasia-kirsten\source-backup-globe-audio-20261019-01-fix2. All285 source hashes
rechecked. New frozen manifest1C757E3D149BB6FCDBA6B37577E463CE05447D31CDE304CD6E766C0C6069C177;
delta archiveE21662A7996AB854C07F7E382811577639DC745B36793A91FF0B1BECBCB0DCD5.
No viewer launch/deployment; result and package verification pending. Wait for the
active build2 completion notification; do not duplicate the attempt.

Attempt1 FAILED at compile (configure0/build1, two C2064 diagnostics): UI commit
slots require `(LLUICtrl*, const LLSD&)`, but the new Stop and three grid-manager
button lambdas had zero-argument signatures. No globe/render/transport compile
diagnostic reported in this attempt. Corrected only these four signatures and
added a compiled regression binding their extracted production signatures to the
real two-argument contract. Sound/globe suite now4 PASS under ASan/UBSan.
Focused delta transfer and incremental attempt2 next; do not repeat the full
source transfer or discard build1 evidence. No package/deployment/GUI proof yet.

INITIAL BUILD SNAPSHOT: globe-audio-20261019-01-build1 (Windows Release/DX11; failed as above).
285-file source manifest26B84CD470EAD78CCDA1F1442E5759E7EF77CB6FE131C67D32D08DF5B1E028C7,
archiveA814A1145DE8810A060B4B426662B93B35F29769752FCFD19FF433A5B467578A.
67 changed files backed up under F:\tasia-kirsten\source-backup-globe-audio-20261019-01;
age4 EXE/PDB preserved under verified-builds\age4-before-globe-audio. Source preflight
stopped before writes on one remote mismatch; review showed only the old upstream
splash URL in llviewernetwork.cpp, which this source replaces with Tasia's URL.
All285 applied hashes and34 branding outputs verified before compile. Installed EXE
DA07BFE68C61... and age4 candidate29ED66029FF4... unchanged. No viewer process was
listed at preflight; none stopped or launched. Build result/package/GUI still pending.
Local receipts: /tmp/opencode/globe-audio-20261019-01-{preparation,review,applied}.json;
build status will be globe-audio-20261019-01-build1-status.txt. Do not duplicate the
active attempt. New status note is later than the frozen remote source manifest.

Mom explicitly changed the order: fix sound and a real Earth-looking 3D map,
then build for testing. Update/release integration and startup news are SEPARATE,
not prerequisites for this candidate. Preserve current running viewer/install.

Sound: dedicated Stop status control, actual orphan stream detection, session-only
explicit-stop latch shared by global and nearby-media controls. Parcel crossings
and delayed ask/play replies cannot restart a manually stopped stream. Explicit
Play clears the latch. Standalone playlist player also stops from this control;
UI/browser audio remains covered by existing master mute, voice not altered.

Map: native DX globe panel, original procedural Earth-like planet art, shaded
spherical mesh with front-hemisphere clipping, atmosphere/star backdrop, drag
rotation/wheel zoom, known-region markers. Marker click selects original global
position through existing trackLocation path; never auto-teleports. Existing map
sidebar/Teleport remains, with a 3D/flat toggle and grid-name travel banner EN/PL.
The globe is explicitly a virtual-grid atlas, not terrestrial geography or Google/
NASA/Street View integration. Bounded nearby map request; named searches preserved.

tasia_sound_globe_test.py:3 PASS including actual production audio start/stop
methods and real globe math under ASan/UBSan. GUI/native rendering still needs the
Windows candidate; no live sound/map success claimed. Next: reviewed/hash-pinned
source transfer and isolated Windows Release/DX11 build; do not modify packages
already running. Updates/news remain deferred to a separate change.

## Active milestone: identification + grid manager source prepared

Mom confirms cloud/avatar is **fixed for now**, with extended testing pending.
QUIC source work is parked for the later combined Windows/live-network test.
Current focus moved to identification + grid manager at Mom's request.

- Identification now wired to local nametag title/color/aliases and profile
  cosmetics. Required grid+UUID matching replaces unsafe UUID-only matching.
  Public config refresh every5min, bounded raw fetch, strict HTTPS/no redirects,
  stale session/grid/generation guards, no UUID or credentials sent to server.
  Enabled by default; switch in Grid Manager. Original users-array cosmetic fields
  preserved; required `grid` field is an extension pending server-schema validation.
- Grid Manager now previews standard OpenSim `/get_grid_info` XML and edits nine
  service URLs. User must click Add/Save URLs. Login registration/reset links use
  selected OpenSim metadata, not SL URLs; Agni/Aditi remain unchanged/protected.
- Optional catalog is off by default, additions-only, atomic-on-save, preserving
  existing entries and persisted removed-grid tombstones. Includes Firestorm field
  aliases and explicit multi-login-URI support. Bundled I-Grid/Beta/local defaults
  apply only to fresh grid files, not existing profiles. No account migration.
- Added receive-time LLCore body limit (default unlimited for unrelated callers),
  shared safe configuration fetcher and documentation `docs/REMOTE_CONFIG.md`.
- New identity/grid fixture suite:4 PASS under ASan/UBSan (actual production policy
  bodies; framework/parser/network doubled). Existing portable/branding10 and
  feature-restoration fixtures PASS. Full Windows compilation and live UI/service
  behavior still untested. No build, deployment, credential use, commit or push.

Remaining for this milestone: live server schema/response verification, combined
Windows compilation and GUI tests after all agreed source work. Latest read-only
checks still return HTTP500 for identification config and HTTP404 for grid catalog.
Profile values
refresh on normal profile callbacks/reopen; external badge images are not fetched.
Next implementation item after source review: stoppable autostarting audio, then
update/release artifacts, map/teleport review and startup news. Keep build deferred.

## Current continuation: QUIC source prepared; build deliberately deferred

Mom requested one build AFTER the agreed source work is complete. No Windows
transfer/build/launch or stored credential use in this continuation; preserve
the running age4 candidate and every existing profile/installation.

- Added actual LL packet send/receive routing through per-circuit QUIC,
  OpenSim-only login/EnableSimulator/TeleportFinish/CrossedRegion negotiation,
  authenticated-connection `quicready` gating, bounded startup queue, five-second
  bootstrap timeout and UDP recovery. Existing established UDP circuits are not
  silently upgraded. Disabling QUIC recovers existing routes to UDP.
- UI reports active only for the current ready simulator, with EN/PL tooltips
  and generated Q badge; initialized library alone still means inactive.
- Certificate verification is on by default for fresh profiles and can explicitly
  be disabled with `TasiaQuicVerifyCert` (restart required). Existing stored user
  choices are not rewritten. Fixed the previously log-only bypass: now sets the
  actual MsQuic flag. Certificate bypass does NOT itself force UDP fallback.
- Fixed datagram send-context double-free risk, callback teardown order, bounded
  stream frame allocation/receive queues and completed peer-stream cleanup.
- Four QUIC tests pass, including compilation/execution of actual circuit and
  stream-parser bodies against transport doubles under ASan/UBSan. Existing
  media-native(3), portable/branding(10), COF-library and welcome tests also pass.
  No Windows compilation, live TLS/QUIC traffic, teleport or GUI proof yet.
- IM correction: previous participant-count rule could convert real one-peer
  conferences. It now additionally requires canonical agent-pair session ID and
  no voice-invite metadata; fixture regression(3) passes. Original cause and
  automatic voice-backed session startup still need verification; NOT live-fixed.

Remote contract checks (read-only): grids.xml, updates/build_data.json and
updates/news.txt at api.tasiaviewer.work returned HTTP404; identification JSON at
i.let-us.cyou returned HTTP500. No schema or successful service behavior inferred.
Items3–8 still require implementation, with reference-source contract review.

Evidence correction to older entries below: Mom verified visible avatar and
correct welcome URL. That alone does NOT prove which library items resolved or
that diagnostic masks never fired; that needs an actual log review. Likewise,
there is no verified assertion that the target never advertises QUIC endpoints.

Next exact work: review QUIC full integration/build compatibility, finish the IM
text-versus-explicit-call path safely, then grid metadata/defaults and opt-in list
updates. Finish remaining identification/audio/update/map/news source tasks before
the single isolated Windows build requested by Mom. Do not build prematurely.

MOM CONFIRMED: age4 cloud candidate (cof-welcome-20261019-01) fully visible
avatar — hair, eyes, skin and clothing — and welcome-pack now shows the correct
URL (https://startup.tasiaviewer.work/welcomepack/, currently "Coming Soon").
This is the first successful live cloud baseline in the new Tasia viewer. The
bounded advertised-library-owner lookup fixed the two unresolved COF targets;
diagnostic null-body masks were not needed this run. Cloud work is now complete;
do NOT claim any remaining avatar optimization.

Next authorized direction from Mom: implement actual QUIC transport, keeping the
working avatar baseline, IM-conference fix, and welcome-pack redirect. OpenSim
grids do not advertise SimQuicHost/SimQuicPort in the current target, so QUIC
remains correctly reported as NOT IN USE unless a simulator advertises it. Need
to adapt the old reference transport integration (not copy whole files) and add
safe UDP fallback. Grid manager, grid+UUID identification, sound stop, update
system, map styling and news stay queued after QUIC.

## Mom's backlog added (2026-10-19, ordered)

1. [x] Cloud/avatar fixed for now per Mom; extended live testing remains pending.
2. [x] QUIC source integration with UDP fallback prepared; combined Windows/live tests pending.
3. [ ] Grid manager: complete default/known grid URLs Firestorm-style; optional
   opt-in autoupdate from `https://api.tasiaviewer.work/viewer/config/grids.xml`.
   Must NEVER overwrite user-removed grids or act without explicit consent.
4. [ ] Grid+UUID based user identification from
   `https://i.let-us.cyou/viewer/config/config.json` with auto-changing values,
   matching the original Tasia behavior. Verify the endpoint contract first.
5. [ ] Fix un-stoppable autostarting media/sound.
6. [ ] Update system: read `https://api.tasiaviewer.work/viewer/updates/build_data.json`
   and `.../updates/files/mainifest.list` (our build generates it, we upload it);
   release/update flow with separate downloadable files.
7. [ ] Map/teleport review; possible Google-Earth/NASA-Street-View-style map
   styling plus grid-name header text. Large feature; scope before coding.
8. [ ] Startup news notification from
   `https://api.tasiaviewer.work/viewer/updates/news.txt`.

Guardrails for items 3, 4, 6 and 8: treat every remote response as untrusted
input (no HTML/JS injection into UI, bounded sizes, HTTPS only, no credential
forwarding, no silent user-data overwrite), and never let an auto-update disable
safety checks or downgrade a pinned dependency. Mom's consent is required before
any network request is added that phones home or changes local profiles.

Current source milestone: #3/#4 identification and grid manager prepared; live service/schema and Windows/GUI checks pending. Next source item: #5 audio stop controls. Cloud fixed for now; QUIC source prepared.

LATEST BUILD/TEST: cof-welcome-20261019-01-build1 Windows Release/DX11 passed
config0/build0/errors0. Independent EXE/PDB age4 match,88 runtime assets,all34 brand
outputs and7300 isolated-package file hashes verified. Package:
F:\tasia-kirsten\test-builds\cof-welcome-20261019-01.
Requested test launch09:35UTC; observed process88780 responding desktop session1,
new portable profile created; API environment payload consumed/deleted. GUI login,
cloud result and welcome-pack navigation await Mom. NOT a proven cloud fix yet.
No other viewer stopped; old profiles/packages and E:\TasiaViewerLatest unchanged.
Next exact action: Mom logs in, leaves outfit unchanged and reports cloud result;
read new library-lookup outcome/null-body masks if needed. QUIC still pending
confirmed avatar baseline. Do not modify/replace the running candidate.

ACTIVE FOLLOW-UP (Mom's remaining scope): cloud, welcome pack, then actual QUIC.
Age3 runtime rejection now known: entries6, repair1/mask12, null_items0,
null_assets2, invalid_types0, duplicate_items0, shared_assets0, bodycounts1/1/1/1.
This does not yet identify which two types lack asset IDs or prove library origin.

LOCAL cloud candidate: one bounded FetchLib2 lookup for unresolved COF links,
using the login-advertised library owner instead of treating every unknown item
as personal inventory. Request only unresolved real targets (deduplicated,max256),
OpenSim legacy/current session/COF only; completion rejects stale session,
generation, COF identity/version/completeness. Existing snapshot and body-part
acceptance unchanged: no synthetic assets/default parts or clothing mutation.
Adds body-part-null masks for any still-rejected reply. Compiled request/response
fixtures and existing12000-case/ASan/UBSan wearable tests PASS; live fix unproven.

LOCAL welcome redirect: exact https://startup.tasiaviewer.work/welcomepack/ in
native handler and default setting; copied old URL cannot resurrect upstream page.
Untrusted/no-download browser; actual postBuild fixture PASS. Public URL responds
"Coming Soon" today; no website content changes requested or performed.

Next: build/package these two changes together without touching the running age3
package/profile, then runtime-test cloud lookup and redirect. QUIC integration
waits until the cloud baseline is confirmed; old reference has message/circuit,
login/region endpoint and QUIC-ready handshake integration to adapt (not a whole
viewer copy, no SL-only grid policy, preserve UDP fallback and TLS verification).

NEW NOTE (Mom): redirect the avatar welcome-pack window(s) to
`https://startup.tasiaviewer.work/welcomepack/`.
- [x] Locate the current avatar welcome-pack UI/handler and change its destination locally.
- [ ] Verify the exact URL and embedded-window behavior in a future candidate.
Mom subsequently requested implementation; included in age4 candidate now launched
for testing. Old age3 package was not changed. Destination currently Coming Soon.

LATEST: regression-20261019-02-build1 Windows Release/DX11 PASSED config0/build0/
errors0. Independent EXE/PDB identity age3,87 runtime assets and all34 branding
outputs verified. New isolated package contains7300 hash-verified files at
F:\tasia-kirsten\test-builds\regression-20261019-02.
Authorized test launch requested09:04UTC; process83244 observed responding in
desktop session1, portable data created, API keys supplied in child environment
and temporary encrypted payload deleted. SSH query does not confirm window
visibility. WAIT FOR MOM'S GUI TEST: native logo, Insert link, then cloud logging.
No runtime fix is claimed yet. Avatar acceptance policy unchanged; diagnostics only.
Old failed package/profile and E:\TasiaViewerLatest preserved. QUIC still on hold.

2026-10-19 continuation authorized: build/test diagnostic candidate, then fix the
remaining baseline issues BEFORE any QUIC transport work. No appearance-policy
relaxation and no overwrite of either failed test profile or working installation.

LOGIN LOGO ROOT CAUSE NOW FOUND: earlier 201-file transfer manifest omitted the
three generated login_tasia PNGs and startup_logo.png. Matching XML/alias hashes
did NOT mean their referenced PNGs matched approved artwork. Packaged horizontal
PNG SHA07C1A996... differs from approved E7EA8DB2.... Prior notes claiming the PNG
hash was correct were mistaken. The expanded source manifest now includes every
branding-manifest output; packager validates ALL outputs against both source and
destination, including ICO relocation, and includes tasia-brand-assets.json.
An initial staging review caught this omission before any new source apply.

CURRENT BLOCKER — Mom's runtime test FAILED: whole viewer crashed on Insert link,
avatar remained a cloud, and native login form still showed Kirsten branding.
Do not describe age2/media-portable-20261019-01 as functionally working. QUIC work
is on hold. No rebuild, replacement package or relaunch performed after this report.

Crash: Windows c0000005/RVA0xfc4750 resolves with exact age2 symbols to
LLViewerMediaImpl::emitEvent, llviewermedia.cpp:1685, after observer notification.
The picker synchronously closed/unloaded that emitter inside its observer.
LOCAL FIX prepared: defer close/selection with doOnIdleOneTime; weak floater,
nonce/session/enabled/current-plugin guards; reject duplicate/stale selections.
Two compiled production-handler lifetime tests PASS, including rejection of the
old unsafe inline-close pattern; three native contract tests PASS. Not Windows built.

Cloud: actual logs show unresolved COF targets=2; missing hair/eyes (mask12),
simulator body counts1/1/1/1, then full snapshot validation rejected the reply.
Null/duplicate/type cause is not logged yet. Pre-feature approved source hashes
confirm llagentwearables/llvoavatar/llvoavatarself/legacy-policy were unchanged in
the tested build; this does NOT excuse the new portable runtime failure or prove
profile/cache causation. LOCAL diagnostics now report only structural rejection
counters, never IDs; acceptance/loading policy unchanged. Existing 12000-case/
ASan/UBSan wearable suite PASS. No speculative appearance logic changes made.

Logo: Mom confirmed the native form above username/password. Packaged login XML,
texture aliases and Tasia PNG hashes match expected source; decoded source PNG is
the approved artwork. Exact runtime origin of old logo still unproven. Stock
legacy logo files/alternate-skin overrides remained. LOCAL generator now covers
legacy login/startup/system logos in all five shipped skins (34 assets total),
with byte/pixel/hash tests (9 portable/branding tests PASS). 25 pre-change legacy
files backed up at /tmp/opencode/legacy-branding-before-regression-fix-20261019.zip.
New logo coverage is not visual runtime proof.

Next exact action: Mom tests the newly launched isolated diagnostic candidate.
Check native login logo and Insert link, then collect new structural snapshot
rejection counters if avatar remains a cloud. Do not overwrite failed test data
or E:\TasiaViewerLatest, clear data, bypass validation, or start QUIC work.

Morning update: Mom requested launching this current test package. Process80632
observed running/responding in desktop session1, portable data directory created;
GIPHY/KLIPY keys supplied in process environment via consumed/deleted temporary
DPAPI payload. Await Mom's visible GUI/function results. Installed viewer untouched.
Defer actual QUIC implementation until Mom finishes testing this package.

Latest result: media-portable-20261019-01-build2 Windows Release/DX11 succeeded,
EXE/PDB age2 identity verified. New separate portable package staged at
F:\tasia-kirsten\test-builds\media-portable-20261019-01; 7299 package file hashes
and 58 source/runtime assets verified. Includes approved branding, portable,
GIPHY Web fetch SDK/KLIPY chat picker and a truthful QUIC-not-used indicator.
NOT launched or deployed over E:\TasiaViewerLatest. Mom deferred GUI tests until
morning and approved local computer suspend after finishing/logging the work.

Status: Windows Release/DX11 build3 and independent EXE/PDB/branding checks passed.
Complete test package staged; Mom requested its launch and a process is running
in desktop session 1. Await visible GUI/login confirmation and functional testing.
Installed E:\TasiaViewerLatest remains unchanged. AI/clipboard/BOM/CEF interaction
and QUIC still need runtime verification; no installer deployment performed.

Execution order: Mom explicitly requested completing all source preparation first,
not stopping for a Windows build or deployment after each individual feature.

## Scope and working directories

- Target: `/home/marty/opensim/tasia-kirsten` (current Windows DX11 viewer).
- Reference: `/home/marty/opensim/tasia-viewer-2026-work` (old Tasia viewer).
- `/home/marty/Kirstensviewer` is a different checkout; do not implement this work there.
- Restore Tasia's web chat, add a Tasia Feed webview button, restore MP3 upload
  and bundled Tasia FFmpeg, restore image upload from clipboard, then complete
  Tasia branding and repair the startup/login page.
- Retain the earlier requirement for AI translation of ordinary text, not gestures.
- QUIC testing comes last, after functional and branding verification.
- Use Mom's `/home/marty/opensim/icon.png` throughout application branding, including login.
- Restore GIPHY and KLIPY clips API/pickers; preserve secret handling.
- Show actual transport status, never equate QUIC enabled/initialized with active traffic.
- New Windows packages enable portable storage beside the EXE by default.

## Latest follow-up: approved branding, portable profile, remaining API/transport work

- [x] Preserve approved source PNG unchanged, generate aspect-preserving PNGs and
  multiresolution ICO with a reproducible script and output hash manifest.
- [x] Wire login/first-login/About, splash/application/system textures, Chat/Feed
  and packaged shortcut icon. Preserve upstream copyright/credits.
- [x] Replace old partial portable path override with early executable-relative
  LLDir initialization. New packages contain TasiaViewer.portable; data uses
  TasiaViewerData. Existing portable-data remains recognized, no auto-migration.
- [x] Portable settings/accounts/logs/cache/chat/temp/browser paths follow the
  portable root. Ignore copied absolute cache/transcript paths; prevent preference
  path moves and automatic old cache migration/purge. Preserve DPAPI encryption.
- [x] Fail explicitly for unwritable root or two competing portable profiles.
- [x] Eight focused tests passed, including compiled production LLDir policy
  methods with stub surroundings; feature and web-chat regression suites passed.
- [x] Windows compile and isolated package verification completed; GUI tests remain.
- [x] Add shared native/CEF picker and callbacks to actual LLFloaterIMSessionTab
  editor shared by nearby, IM and group chat. Selected links go only into the
  originating draft; no auto-send, stale-session callback or closed-editor access.
  Pinned official @giphy/js-fetch-api 5.8.0 Web fetch SDK enables GIF/sticker,
  emoji, video clip and animated text modes. KLIPY includes GIF/clip/sticker/meme.
- [x] Mom authorized vault giphy_key/klipy_key connection tests. Live GIPHY GIF,
  KLIPY GIF and KLIPY Clip trending requests returned HTTP 200 with one media
  result each. This does not verify SDK entitlements or viewer integration.
- [x] Use CURRENT KLIPY schema, not old reference unchanged: path app_key,
  per_page, data.data and item.file. Prevent authenticated URL logging.
- [x] Implement honest transport indicator (EN/PL tooltip + icon). Audit found LLQuicGlobal startup
  initialization but no callers constructing LLQuicConnection outside its own
  library. This is NOT evidence of active viewer transport: do not light a green
  QUIC icon from the setting or initialized library. Transport integration is
  separate, unfinished work; retain ordinary OpenSim/SL behavior.

### Morning runtime checklist (not yet verified)

Use `F:\tasia-kirsten\test-builds\media-portable-20261019-01\Tasia Viewer.lnk`,
NOT the older bare-build or installed shortcut. Only launch when Mom requests it.
This package has no saved profile or embedded API keys. Keys may be supplied from
the approved vault at test launch via TASIA_GIPHY_KEY/TASIA_KLIPY_KEY environment,
or entered into the masked session-only picker fields; no public bundle secrets.

1. Confirm approved logo/login/native splash and creation of adjacent
   TasiaViewerData; no automatic AppData profile import. DPAPI remains machine/user
   bound. Test moving a CLOSED portable package and check cache/chat paths.
2. In nearby, IM and group conversations, click GIF/K; search/trending/types/more;
   pick media and check the correct draft gets links, nothing auto-sends. Test
   cancel/close/reopen/switch conversations/session/logout and too-long drafts.
3. Send to another viewer: GIF links preserve compatibility. Clips and KLIPY
   include public asset links alongside provider page, requiring no recipient API
   key. Inline shared LLChatHistory previews use allowlisted local HTML player,
   with video controls/no autoplay and proper GIPHY/KLIPY attribution. Verify audio,
   codec support and preview settings in CEF; offline tests do not establish them.
4. Retest clipboard, MP3, Chat/Feed and existing working OpenSim appearance/outfits;
   SL compatibility remains required. No appearance code changed in this follow-up.
5. QUIC icon currently correctly reports NOT IN USE. Actual region routing is NOT
   implemented; initialized MsQuic must never turn a green connection icon on.

Evidence: BUILD_NOTES top section; local /tmp/opencode/media-portable-20261019-01-
build2-status.txt, -verified2.json, -package2.json. No commit/push made.

## Guardrails

- Preserve existing uncommitted work, DX11, SQLite/DPAPI account storage,
  OpenSim appearance/outfits/BOM fixes, region handling, and Second Life support.
- Old viewer policy is not target policy: do NOT port its Second Life grid lock.
  Only Agni/Aditi are Second Life; other grids are OpenSim.
- Port focused behavior and dependencies, not entire Firestorm source files.
- Do not alter appearance code as part of these features. Mom reported the latest
  viewer working; that report does not prove every prior root-cause claim.
- No credentials in source, logs, plan, URLs, or reports. Ask before using stored
  secrets. Web identity parameters are not proof of authentication.
- No blind global search-and-replace. Preserve legal notices, upstream credits,
  protocol identifiers, and migration access to existing settings/accounts.
- Work one feature at a time; update this checklist with files, evidence, and
  remaining failures. Source checks alone do not establish runtime success.
- Do not launch or terminate Mom's viewer, push changes, or use sudo without permission.

## 0. Baseline and source map

- [x] Locate the active target and reference checkouts; inspect target git status.
- [x] Read existing target BUILD_NOTES and reference project instructions/status.
- [x] Locate reference web chat, MP3 converter, packaging, and clipboard upload.
- [ ] Record a fresh source/build baseline before implementation; do not rely on
  stale build labels or the inaccurate older entries in BUILD_NOTES.md.
- [ ] Compare target menu callbacks, command registrations, CMake sources,
  settings, notifications, translations, and packaging for each feature.
- [ ] Identify the existing AI translator implementation and gesture-origin data.

## 1. Restore Tasia Chat as an embedded website

Reference: `indra/newview/llviewermenu.cpp`, `handle_tasia_chat()`;
`app_settings/commands.xml`, `skins/default/toolbars.xml`, menu XML and icon assets.

The reference opens `https://chat.tasiaviewer.work/` through
`LLFloaterWebContent`, with encoded avatar/grid/display parameters and hidden
address controls. This is NOT the ordinary viewer IM/chat system.

- [x] Adapt the handler to target APIs and add menu/toolbar entry, icon and EN/PL labels.
- [x] Handle pre-login state explicitly; do not pass invalid/stale avatar identity.
- [ ] Review browser trust, navigation restrictions, session/cookie behavior and
  external links; do not blindly copy `trusted_content = true`.
- [ ] Test opening, closing, reopening, Unicode names, grid changes and failure UI.

## 2. Add Tasia Feed webview button

Requested destination: `https://api.tasiaviewer.work/feed/`.
Existing snapshot upload code is separate (`tasiafeedconnect.cpp`); do not replace
the requested website button with the old snapshot-upload floater.

- [x] Add a named Tasia Feed command and accessible button/menu entry using the
  embedded browser, with EN/PL labels and an appropriate icon.
- [ ] Keep existing snapshot sharing functional; inspect authentication without
  inventing or changing backend contracts.
- [ ] Test correct destination, page rendering, navigation, login/session behavior,
  reopening and network-error handling in the packaged CEF runtime.

## 3. Restore MP3 upload and bundled Tasia FFmpeg

Reference: `indra/newview/llmp3batchupload.{cpp,h}`, `llviewermenufile.cpp`,
`CMakeLists.txt`, notification XML, file-picker filters and `viewer_manifest.py`.
The old implementation converts MP3 into WAV segments and uploads sound assets;
it does not simply upload the raw MP3 as an AT_SOUND asset.

- [x] Trace conversion, negotiated duration limits, segmentation, WAV validation,
  upload queue, inventory naming, progress, cancellation and cleanup.
- [x] Port the implementation and UI registrations to target APIs.
- [ ] Package `tasia-ffmpeg.exe` and its required license/dependencies. Verify its
  actual origin and package path; never assume a system FFmpeg is installed.
- [x] Use structured process arguments, validate output and platform-specific exit
  status, avoid UI blocking and handle missing/broken converter explicitly.
- [x] Confirm total upload cost before uploading, including multiple segments.
- [ ] Test short/long/corrupt MP3, Unicode/spaced paths, cancel, converter failure,
  failed upload, temporary-file cleanup and sound playback from inventory.
- [ ] Preserve existing WAV uploads and grid-specific sound limits.

## 4. Restore clipboard image upload

Reference: `indra/newview/llviewermenufile.cpp`, clipboard helper,
`LLFloaterUploadClipboard`, and `skins/default/xui/en/floater_upload_clipboard.xml`.
The reference Windows helper uses an STA PowerShell process to save clipboard
image data to a temporary PNG, then opens the regular `upload_image` preview.
Target already has the XML file; that does not establish working registration.

- [x] Port the helper, floater/menu registration and EN/PL messages; review whether
  native clipboard access is safer than copying the shell-based implementation.
- [x] Support Paste/Ctrl+V inside the upload floater without stealing normal chat paste.
- [x] Require preview and normal upload confirmation; never upload automatically.
- [ ] Handle empty/non-image clipboard, screenshot/alpha images, large dimensions,
  corrupt input, cancellation, filename safety and temporary-file cleanup.
- [ ] Test clipboard capture -> preview -> upload -> inventory texture display.

## 5. AI translator: ordinary text via the Tasia API server

Retained from Mom's preceding request; do not confuse web chat with this feature.

Architecture decision (2026-10-20): translation will be hosted on the Tasia API
server and exposed to the viewer through a first-party API route. Provider
credentials stay server-side. The existing direct OpenAI-compatible client is only
prepared prototype code and must not be treated as the final architecture or
activated against a third-party endpoint. Define the route, viewer authentication,
provider/model, rate limits, privacy/retention and error response with Mom before
implementing the server integration.

- [x] Locate and inspect the old provider, settings, requests and chat integration.
- [x] Define filtering using actual gesture/message-origin information; do not
  assume all gesture-generated text has a distinguishable network flag.
- [x] Preserve gesture triggers/commands and exclude supported gesture-origin
  messages; document any ambiguous incoming-chat limitation before implementation.
- [ ] Test ordinary text, gesture output, commands, URLs, disabled translation,
  provider failure and duplicate-translation prevention. No hardcoded AI keys.

## 6. Tasia branding and startup/login page

Finding: target `indra/newview/llviewernetwork.cpp` already defines
`DEFAULT_LOGIN_PAGE = "https://startup.tasiaviewer.work"`.
The defect must be traced through settings/grid overrides, URL selection and CEF
layout/loading; adding the same constant again would not diagnose it.

- [x] Inventory Kirsten/Kirstens/S24 product branding in UI, all locales, About,
  window titles, splash/login screens, icons, executable metadata, installer,
  shortcuts, manifests, help links and package names.
- [ ] Replace product-facing branding with Tasia consistently. Map internal build
  targets/paths separately and migrate only where necessary and verified.
- [x] Preserve required copyrights, license notices and historical attribution.
- [ ] Preserve account/settings/cache compatibility or implement explicit migration;
  do not strand saved DPAPI/SQLite logins by renaming directories blindly.
- [x] Trace `LLPanelLogin::loadLoginPage()`, grid login-page selection, `LoginPage`
  settings and login XUI; fix the cause of the missing startup website.
- [ ] Verify HTTPS availability, redirects, packaged CEF rendering, first run,
  returning-user settings, grid switching, login form layout and offline fallback.
- [ ] Revisit staff/developer/partner identification after core branding; use real
  existing authorization data, never infer roles from display names.

## 7. Verification, build and deployment

- [ ] Validate touched XML, callbacks, CMake lists and actual package contents per feature.
  XML/wiring/source checks pass; no package has been generated yet.
- [x] Add focused tests that exercise changed logic; record coverage limitations.
- [x] Compile changed production units with Windows MSVC/DX11 before claiming success.
- [x] Before remote writes, obtain fresh remote hashes; validate payload paths,
  previous hashes and staged hashes. Never fabricate PreviousSHA256 or reuse a
  hardcoded stale archive. Do not reuse weakened transfer scripts unchanged.
- [ ] Run one controlled build with a unique receipt, source hashes, actual changed
  compilation units and relevant error output; inspect an active build rather
  than launching another after a client timeout.
- [ ] Verify EXE/PDB GUID+age, matching hashes and complete packaged CEF/FFmpeg resources.
  EXE/PDB and direct-build UI/FFmpeg verification passed; complete package pending.
- [ ] Deploy with backup and installed/archive hash checks under existing deployment
  authorization; leave the viewer stopped/running exactly as Mom left it.
- [ ] Give Mom a concise interactive checklist; distinguish build success from her
  runtime result. Smoke-test OpenSim appearance/outfits and SL compatibility.
- [ ] Update existing BUILD_NOTES.md and this plan with verified results, without
  repeating unsupported age numbers or root-cause claims.

## 8. QUIC — last

- [ ] Inspect existing viewer/server QUIC support and negotiation before enabling it.
- [ ] Test only with a compatible endpoint, explicit opt-in and HTTP fallback.
- [ ] Compare behavior with QUIC off/on; do not make QUIC a dependency of uploads,
  website buttons, login, appearance or branding.

## Next exact action

Test package and explicitly requested desktop launch completed; see BUILD_NOTES.
Next: obtain Mom's visual result from the running TEST package, without stopping
her viewer. Interactive login/rendering and each restored function remain to be
tested. Initial launch of the bare build output failed because its runtime was
incomplete; use the new test package and its Tasia Viewer shortcut instead.
No permission to overwrite E:\TasiaViewerLatest was inferred from the launch
request. A permanent deployment still requires a separate safe backup/approval.
The CMake target stays `Kirstens-S24`, actual executable is `TasiaViewer.exe`.

AI integration is not active: Mom prefers hosting translation on the Tasia API
server. Next define its route/auth/provider policy; keep any provider secret on the
server and never embed it in the viewer. No AI network request or secret was used.
QUIC runtime testing still follows the functional build and interactive tests.

## Progress: Tasia Chat, 2026-10-19

- Baseline source copies, HEAD and SHA256 manifest saved outside the repository:
  `/tmp/opencode/tasia-chat-baseline-20261019-013615`.
- Added `handle_tasia_chat()` and a persistent global `Tasia.Chat` callback in
  `indra/newview/llviewermenu.cpp`. Uses the existing `web_content` floater with
  a stable `tasia_chat` target, hidden chrome, fixed localized title and
  `trusted_content=false`. No external-browser preference is consulted.
- Before login (or without an avatar ID), opens the bare website without identity.
  In-world, preserves the reference website's five encoded context fields via
  `LLURI::mapToQueryString`; cache misses fall back to the account username.
- Added EN/PL login and Communicate menu entries, toybox command, default toolbar
  entry and the reference PNG icon. Existing user toolbar layouts are not reset;
  users can add the command through toolbar customization or use the menu.
- Browser trust review: untrusted content cannot inherit trusted viewer dispatch;
  hidden controls do NOT impose an origin allowlist. Standard CEF navigation,
  cookies, popups and external links still require runtime verification. Existing
  web_content URL history stores navigated URLs, including public identity context;
  no credentials or session secrets are added to these URLs.
- PASS: `python3 indra/newview/tests/tasia_web_chat_test.py`.
  Checks XML/icon wiring and compiles/runs the actual handler with synthetic APIs:
  stale pre-login identity, missing ID, cached/uncached Unicode names, grid changes,
  query delegation, browser flags and null floater return.
- PASS: scoped `git diff --check` for the ten modified integration files.
- Limitations: fixture mocks LLURI/viewer APIs; no Windows production compilation,
  CEF rendering, cookie/authentication test, remote transfer or deployment performed.
- Pending manual checks: anonymous login-menu page, in-world identity selection,
  close/reopen, existing and new toolbar layouts, network failure and browser links.

## Progress: remaining source preparation, 2026-10-19

### Feed and startup
- Added `Tasia.Feed` / `tasia_feed`, independent `tasia_feed` web target, EN/PL
  menu/toolbar labels and the existing Feed icon. Both ordinary and icon menu
  variants expose Chat, Feed, MP3 and clipboard actions. Snapshot sharing code is
  unchanged; no new backend/authentication contract was invented.
- Found `login_html` commented out in `panel_login.xml`. Restored the real CEF
  control as untrusted content; the old NVIDIA warning is historical, not proof
  that current CEF is safe. Runtime GPU/CEF verification remains required.
- Startup uses `https://startup.tasiaviewer.work/` independently of a grid splash
  page. Explicit ForceLoginURL remains supported and its query is parsed after
  applying the override. Grid authentication configuration is untouched.

### MP3 and FFmpeg
- New `llmp3batchupload.{cpp,h}` adapts the old conversion/segmentation approach to
  target APIs. Uses the target's real 30-second validator for all grids, with
  29-second PCM WAV segments (44100 Hz, stereo, 16-bit). Does not invent a larger
  OpenSim limit or enable raw MP3 asset upload.
- Conversion runs asynchronously through LLProcess with structured arguments,
  Windows exit-code validation, cancellation, 10-minute timeout, 512-part cap,
  per-batch owned temporary directory and cleanup. No shell interpolation.
- Confirms aggregate price, checks balance/quote again, uploads parts sequentially
  via NewFileAgentInventory, stops on error/cancel/session/grid change/disconnect,
  and keeps already completed inventory items. Existing WAV upload is untouched.
  Legacy UDP-only regions without the inventory capability get an explicit error.
- CMake and both menu variants register the feature and progress/cancel controls.
- `scripts/Stage-TasiaFFmpeg.ps1` prepares the old viewer's immutable BtbN release
  with the same pinned archive SHA256, license and provenance receipt. It has NOT
  been executed: no download, converter launch, or bundled binary is claimed.
- `indra/cmake/TasiaFFmpeg.cmake` copies a complete staged converter bundle after
  building. Package staging requires the converter and checks its receipt hash.
  Public distribution still needs corresponding-source/license compliance review.

### Clipboard
- New `LLFloaterUploadClipboard` uses native Win32 PNG clipboard data, with an
  opaque GDI bitmap fallback for screenshots. PNG dimensions/size and bitmap
  dimensions/area are bounded; no subprocess, clipboard logging or automatic upload.
- Opens the existing image-upload preview with a temporary-image ownership flag.
  `LLFloaterImagePreview` removes the temporary PNG immediately after decoding;
  the normal upload path uses decoded pixels, so cancellation does not leak files.
- Ctrl+V is confined to this floater. EN/PL UI added. This is Windows-native;
  non-Windows builds show an explicit unsupported-platform message.
- New upload floaters are on both existing SLapp web-opening blacklists.

### Translation
- Inspected the old target/reference translators: Azure, Google and DeepL exist;
  a Tasia AI provider was not found. Added opt-in OpenAI-compatible HTTPS support
  with configurable endpoint/model, a masked session-only key (`Persist=0`),
  privacy disclosure, verified TLS peer/host, no redirects/retries, four-request
  concurrency bound, timeout and bounded response parsing. No network API was used.
- Sends only the current message and a translation instruction, not avatar identity
  or chat history. No tool definitions; asks provider not to store completions.
- Incoming translation gate excludes self, muted/system/object chat, emotes,
  commands, URL-only/prefixed content, multiline art and locally known gesture
  triggers/replacements/chat steps. Conservative Unicode text filter skips
  emoji/symbol-only messages. Unknown remote gestures have no reliable provenance
  flag; an ordinary-looking unknown gesture may still be translated.
- The direct-provider prototype remains unconfigured/off and is not the approved
  deployment path. No live AI translation service is available yet.
- Architecture update (2026-10-20): route translation through the Tasia API server;
  replace the direct provider/key client before activation. Provider credentials
  belong only in server-side secret storage. Endpoint/auth/model, rate limits and
  retention/error contract remain to be agreed and implemented.

### Branding and packaging
- Mom's exact product signature: **Advanced Multiverse Viewer by Tasia and Marty**.
  Added to native Windows splash/taskbar title, loading panel, shutdown console
  title/header/footer (ANSI and fallback), EN/PL About, Windows file description
  and README. The native splash title/old SL-only subtitle had been missed by
  the earlier newview/XUI audit; corrected now. Splash height/text bounds were
  expanded for the longer signature. This is source-verified, not Windows-tested.
- Follow-up: About/Credits now explicitly thanks Kirstenlee Cinquetti for the
  upstream viewer and native DX11 renderer, with clickable official GitHub and
  SourceForge links, in EN/PL. Removed stale hardcoded upstream build/version
  labels from Credits; actual build identity remains in the generated Info tab.
- Product-facing menus, engine preferences/help, console text, About header,
  Windows resource metadata, app icon and shutdown text now identify Tasia.
  Uses the old Tasia app ICO; copyright/legal credits remain intact.
- Actual executable/PDB names are TasiaViewer; legacy CMake target, control names,
  callback IDs, window-class compatibility identifiers and historical source/legal
  references remain deliberately unchanged. No saved-account directory migration.
- Shutdown sweep recognizes the new executable, still checks current PID/child
  ownership, and orders the current process last using a strict PID comparator.
  No processes were started, stopped or inspected on Acer.
- IMPORTANT safety exception: legacy factory-reset folder targets and their exact
  warning paths were NOT redirected to live Tasia account/settings directories.
  Branding must not silently activate deletion of SQLite/DPAPI accounts. Those
  legacy paths are technical references, not the product's display name.
- `scripts/Package-TasiaViewer.ps1` stages into a NEW destination only, checks the
  new executable/CEF/FFmpeg files, copies fresh source UI, adds a Tasia shortcut
  and records file hashes. Existing Invoke-Package entry delegates to it; its
  historical body below the return is retained, not used for the new product.
- Runtime manifest copy includes commands/toolbars/skins; CMake tracks changed
  UI assets so incremental builds cannot omit the new menus/icons.

### Verification evidence and limits
- Windows follow-up: successful `features-20261019-021505-build3`, configure/build
  exit 0 and error count 0. Independently matched new EXE/PDB GUID+age (age 1),
  exact product metadata and 40 runtime UI/settings asset hashes. New output EXE
  SHA256 `910F028F141B1E1990FCD17894F0838F68C8637B043F2DC9DB5B952834C6CA6D`.
- Real Windows converter smoke now passes on synthetic MP3: two <=30-second PCM
  WAV parts, stereo/44100Hz/16-bit. FFmpeg dated release was re-pinned after the
  original disappeared, using the published GitHub archive digest and validating
  the download. All four transferred PowerShell scripts parsed on Windows.
- MSVC exposed two MP3 API mismatches hidden by the original fake APIs: missing
  delimiter macro and picker callback arity. Both production code and fixtures
  now use/verify the actual target interfaces. Focused regression test passes.
- No deployment or viewer launch. Full packaging/CEF, actual clipboard, live AI,
  appearance/outfit runtime and QUIC verification are still pending. Earlier
  "not compiled" entries below describe historical source-preparation checks.
- PASS: ten offline suites: feature restore, web chat, bake upload, initial
  wearables, cloud readiness, BOM limits, OpenSim compatibility, bake readback,
  host-bake fetch and region compatibility.
- PASS: all 37 changed/new XML files parsed and scoped `git diff --check`.
- Feature tests compile the real MP3 translation unit and native clipboard capture
  function against synthetic APIs, plus the actual UTF-8 text policy. Cover price
  confirmation/repricing, serial uploads, cancellation, bad WAV, session change,
  part limit, failed-directory ownership, PNG bounds, bitmap RGBA ordering/opacity
  and clipboard resource release. These do not substitute for production headers.
- Real LOCAL LINUX FFmpeg synthetic test converted 31 seconds of generated MP3
  audio into two valid <=30-second stereo/44100Hz/16-bit WAVs. This does NOT test
  the Windows bundled executable or upload/playback in the viewer.
- PowerShell is unavailable locally. Scripts, Windows/MSVC, real clipboard,
  packaged CEF/GPU rendering, websites, paid AI API, inventory playback/persistence,
  deploy hashes/symbols and QUIC remain unverified. No live secrets were used.
