# OpenSim completion status report

Updated: 2026-10-22  
Target: `fix/opensim-completion-20261022`  
Base: `4a257ba7fd876ff36b3e8789312ed8fd578af768` (`tasia/broken` at start of work)

## Status

This is a review-branch progress report, **not a declaration that OpenSim
completion is done**. It records source changes and offline verification in this
branch. A visible, correctly textured avatar and a reliable live login have not
been demonstrated for the current candidate.

The initial implementation commits present when this report was opened, newest
first, were:

| Commit | Change |
|---|---|
| `a9307ab8444c07d1c6629cb6386961febdab1726` | Route Destinations using OpenSim grid endpoints |
| `f041143` | Route Search using OpenSim grid endpoints |
| `56a3a68` | Restore OpenSim legacy self-profile edits |
| `d671181` | Document local bake/BoM parity and limits |
| `4d1f0f4` | Reject failed/mismatched AIS outfit fetches |
| `94e1fe3` | Isolate grid credentials and validate movement identity |

The implementation branch was six commits ahead of `tasia/broken` before this
documentation-only report commit; its implementation head was
`a9307ab8444c07d1c6629cb6386961febdab1726`.

The report was committed as `f3db260c406223111243be7e2ec8af3961296347` and
pushed without force to `tasia/fix/opensim-completion-20261022`. A subsequent
`git ls-remote` verified that the remote ref matched this SHA. Later documentation
checkpoints are tracked in `BUILD_NOTES.md` and the commit history.

Follow-up commits after that first report:

| Commit | Change |
|---|---|
| `a51b859` | Trace login/QUIC and `BAKED_HEAD` source paths; update audit/report |
| `9a6d05f` | Document profile wire/session limits and add source-contract test |
| `d3e1d6acb3cb05ed8ae826f1c2271a4a6e5f91db` | Preserve caller data in Variable-1 clamp; compile focused production-method fixture |
| `aebdbf34305d9712be6b3047106b38473b074e60` | Expand Variable-1 boundary fixture; record follow-up review |

The last commit above was pushed without force, and `git ls-remote` verified the
remote branch at that exact SHA before this documentation-only evidence update.

## Verified source/test work

- Login/grid state corrections, including movement completion identity/session
  validation and per-grid credential isolation.
- AIS outfit-fetch failure guard that refuses failed or mismatched category
  results before applying an outfit.
- Source audit of local bake/BoM behavior and the DX11-specific readback/upload
  route; the existing black-head guard remains containment only.
- OpenSim legacy full-profile edit merge/send path, preserving untouched values
  from the active-session reply snapshot.
- OpenSim Search and Destinations endpoint selection with HTTP(S) validation and
  Second Life endpoint isolation.
- Grid Status page/RSS routing now selects OpenSim region feature or saved
  per-grid metadata, preserves SL-only defaults, avoids untrusted OpenSim browser
  content, retains TLS peer verification, and separates RSS cache/file state by
  grid. Grid Manager discovery, persistence and en/pl controls were extended.
- Land transaction endpoint isolation: removed the function-static helper URI
  and resolve the selected grid's validated `/landtool.php` endpoint per request.
- Relevant compatibility and protocol status is detailed in
  `OPENSIM_COMPATIBILITY.md`, `OPENSIM_PROTOCOL_AUDIT.md`, and `BUILD_NOTES.md`.

Offline focused tests recorded for these changes:

```text
tasia_opensim_services_test.py: 18 passed
tasia_login_flow_test.py: 10 passed
tasia_identity_grid_test.py: 4 passed
tasia_profile_partner_link_test.py: 4 passed
tasia_bake_readback_test.py: PASS (sanitized ASan/UBSan fixture)
tasia_bake_upload_test.py: PASS (ASan/UBSan fixture)
tasia_bake_refresh_test.py: PASS
tasia_legacy_bake_port_test.py: PASS
tasia_bom_limits_test.py: PASS
git diff --check: PASS
```

These are offline tests/source fixtures, not a Windows viewer build or live
grid/runtime verification. The per-phase test records and independent-review
limitations are in `BUILD_NOTES.md`.

## Unresolved blockers

1. **Login circuit/movement stall:** one prior candidate authenticated and
   reached simulator connection, but its QUIC session was not confirmed; it
   fell back to UDP, `CompleteAgentMovement` reliable delivery timed out, and
   the circuit later dropped after missing pings. This locates the observed
   failure at simulator circuit/movement confirmation, but does not establish
   whether the server missed a request or a reply/ACK was lost. No server-side
   event correlation or current QUIC-on/off controlled test is recorded.
2. **Black `BAKED_HEAD`:** the prior visual failure remains unresolved. The
   all-black/nontransparent upload rejection is containment; the bad composite
   input layer/asset has not been identified. No current-candidate D3D readback,
   J2C upload, returned bake UUID, or rendered avatar validation is recorded.
3. **Profile runtime correctness:** packet mapping and source fixtures support
   preservation of unedited fields. The sender checks the cache's session tag,
   but `AvatarPropertiesReply` contains no SessionID and the handler tags replies
   with the current session when received; delayed-reply isolation is therefore
   not proven. The legacy Variable 1 `FLAboutText` field clamps to 255 bytes
   including NUL, despite the 65,000-character UI limit. Live saves against
   HTTP-capable and legacy-only grids, byte-limit/UTF-8 behavior, and stale-reply
   handling have not been exercised.
4. **Build/runtime:** no Windows DX11 build or current-candidate live OpenSim/SL
   verification is included in this branch evidence. QUIC idle >300s,
   interruption/reconnect, crossing, teleport, and packet-loss behavior remain
   unverified.
5. **Compatibility:** see the parity matrix and protocol audit for partial or
   unsupported items. In particular, variable-region seams/topology, broad
   OpenSim service parity, and some map/Hypergrid/economy/mesh/material behavior
   are not claimed complete.

## Additional economy fix in progress

The checked `SampleMoneyModule` registers `preflightBuyLandPrep` and
`buyLandPrep` at `/landtool.php`. Removed the land floater's process-lifetime
static URI and added a selected-grid resolver using the existing HTTP(S),
userinfo/query validation policy; invalid/missing helper metadata now produces a
visible error instead of an empty XML-RPC request. Added two-grid native TUT
coverage and a compiled production-resolver fixture. The service suite passes
20 tests. Tasia Farm reviewed the actual diff (`job_mvjouys9_dfa2a2af7f`) and
recommended native integration testing; no purchase/preflight was executed.
Native viewer test/build and live routing remain unverified. This does not
complete currency purchase, economy discovery/presentation, or land transaction
runtime support; details are in the protocol audit.

## Grid Status fix in review

The source diff now consumes OpenSim `OpenSimExtras.GridStatus` and
`GridStatusRSS`, then per-grid persisted metadata; SL page and RSS settings are
not used as OpenSim fallbacks. The RSS request no longer disables TLS peer
verification, ignores results after a grid switch, and uses separate per-grid
cache keys/files. Discovery/UI persistence was added in the Grid Manager. The
focused service suite passes 21 tests and `git diff --check` passes. Tasia Farm
reviews (`job_mvjp1pgw_2607d5b2e5`, follow-up `job_mvjp3opc_fa09151a6d`) found
no actionable source defect and noted runtime/UI persistence, edge-case and
native build gaps. Native viewer build, live RSS/TLS, and browser tests have not
run. See `OPENSIM_PROTOCOL_AUDIT.md` for details.

## OpenSim currency routing fix in review

The OpenSim currency UI no longer requires Linden payment-info-on-file, while
Second Life retains the existing check. `currency-base-uri` is used only when
its authority (host and port) is anchored to the selected grid's configured
helper/login URI. HTTP stays supported for HTTP-only anchors; an HTTPS anchor
requires HTTPS. The helper routes to the checked
`/currency.php`. Explicit per-grid `currency_uri` is also supported. The checked
OpenSim sample module's quote/buy handlers are demonstration stubs, so this
proves routing compatibility only—not real purchase fulfillment. The focused
21-test service suite passes. **REVIEW PENDING — TASIA FARM UNAVAILABLE:** peer
workers were unavailable and the lead's response was description-based, not an
inspection of the local source diff. No financial request or native/live test
was run.

## Source-trace checkpoint

The current-vs-reference login source trace found equivalent high-level startup
ordering through `UseCircuitCode` acknowledgement, `CompleteAgentMovement`, and
the `AgentMovementComplete` wait. Current QUIC additionally requires
transport-authenticated `quicready` and has a five-second LLUDP fallback that the
reference does not. The timeout is a verified source difference, **not a proven
cause** of the reported circuit stall. The production QUIC harness passed 6/6;
no live enabled/disabled comparison or server-side packet correlation was
available.

The `BAKED_HEAD` dictionary/source trace confirms slot 0 and explicit bodypaint,
tattoo, alpha, and universal-tattoo inputs, plus missing-asset readiness guards
and the DX11 readback → RGBHM/J2C → upload → baked-TE/appearance-send path. The
readback, upload, attachment-refresh, legacy-bake, and BoM fixtures all pass. They
do not identify the black-pixel input or prove a visible head; the guard remains
containment only. Detailed trace and exact remaining evidence are in
`OPENSIM_PROTOCOL_AUDIT.md`.

Profile follow-up found and fixed a caller-buffer mutation in the shared Variable
1 serializer: it now clamps through a temporary copy, retaining the 255-byte
wire format without modifying the cached description. A compiled fixture of the
production method passes short, exact-limit, oversized, and null/empty cases.
Follow-up Farm review `job_mvjob864_6f502b43e4` confirmed the guarded copy,
synchronous lifetime, and unchanged wire-bound behavior from the supplied source
facts; it identified no remaining concrete issue in that narrow patch. The
review was summary-based, not independent source/build/runtime validation. The
254-byte payload limit, UTF-8 boundary behavior, live persistence, and
delayed-reply session isolation remain open.

## Review and release boundary

Tasia Farm reviews for profile, Search, and Destinations were limited because
reviewers reported that they could not inspect the source; they are not treated
as independent source or runtime validation. The later login/QUIC and bake-path
documentation checks (`job_mvjnvfs4_0225653b20` and
`job_mvjnxji2_e4bbf69f9d`) were also summary-based consistency reviews, not source,
build, or runtime validation. The bake review prompted the clarification that
black pixels can be legitimate and that readiness/assignment guards do not prove
the final composite or render. Profile audit review `job_mvjo4mx9_5efe0c6038`
found no factual errors in the supplied summary, while reiterating the lack of
stale-packet, wire-byte, and persistence tests; it also did not independently
inspect the repository. No merge, deployment, or force-push is authorized by this
report. This branch is intended for review only.

Next required evidence: correlate the login handshake on both client and server
with QUIC enabled and disabled; trace the baked-head input-to-render pipeline on
the Windows DX11 candidate; exercise profile updates against HTTP-capable and
legacy-only grids; run a genuine Windows build; then update this report with the
exact results and remaining gaps.
