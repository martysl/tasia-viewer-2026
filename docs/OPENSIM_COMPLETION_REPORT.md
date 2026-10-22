# OpenSim completion status report

Updated: 2026-10-22  
Target: `fix/opensim-completion-20261022`  
Base: `4a257ba7fd876ff36b3e8789312ed8fd578af768` (`tasia/broken` at start of work)

## Status

This is a review-branch progress report, **not a declaration that OpenSim
completion is done**. It records source changes and offline verification in this
branch. A visible, correctly textured avatar and a reliable live login have not
been demonstrated for the current candidate.

The implementation commits currently on this branch, newest first, are:

| Commit | Change |
|---|---|
| `a9307ab8444c07d1c6629cb6386961febdab1726` | Route Destinations using OpenSim grid endpoints |
| `f041143` | Route Search using OpenSim grid endpoints |
| `56a3a68` | Restore OpenSim legacy self-profile edits |
| `d671181` | Document local bake/BoM parity and limits |
| `4d1f0f4` | Reject failed/mismatched AIS outfit fetches |
| `94e1fe3` | Isolate grid credentials and validate movement identity |

The branch was six commits ahead of `tasia/broken` when this report was written.
The exact branch head is `a9307ab8444c07d1c6629cb6386961febdab1726`.

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
   preservation of unedited fields and session-scoped snapshots, but live
   HTTP-capable and legacy-only grid saves, long descriptions, and session
   isolation have not been exercised.
4. **Build/runtime:** no Windows DX11 build or current-candidate live OpenSim/SL
   verification is included in this branch evidence. QUIC idle >300s,
   interruption/reconnect, crossing, teleport, and packet-loss behavior remain
   unverified.
5. **Compatibility:** see the parity matrix and protocol audit for partial or
   unsupported items. In particular, variable-region seams/topology, broad
   OpenSim service parity, and some map/Hypergrid/economy/mesh/material behavior
   are not claimed complete.

## Review and release boundary

Tasia Farm reviews for profile, Search, and Destinations were limited because
reviewers reported that they could not inspect the source; they are not treated
as independent source or runtime validation. No merge, deployment, or force-push
is authorized by this report. This branch is intended for review only.

Next required evidence: correlate the login handshake on both client and server
with QUIC enabled and disabled; trace the baked-head input-to-render pipeline on
the Windows DX11 candidate; exercise profile updates against HTTP-capable and
legacy-only grids; run a genuine Windows build; then update this report with the
exact results and remaining gaps.
