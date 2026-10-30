# QUIC teleport / region crossing regression tests

Problem observed on 2026-10-30: Tasia Viewer logs in over QUIC,
but after teleport switches to LLUDP. In the viewer source, the
TeleportFinish handler reused ANY existing circuit without considering
SimQuicHost/SimQuicPort. A neighbour discovered by EnableSimulator
could therefore pin a destination to LLUDP even when TeleportFinish
advertised QUIC. CrossedRegion had the same limitation.

This branch changes viewer-side circuit selection:
- TeleportFinish and CrossedRegion prefer the advertised QUIC endpoint,
  even when an LLUDP circuit already exists for the destination.
- LLMessageSystem::enableQuicCircuit reuses an existing QUIC connection
  only if both host AND port match; if a circuit is closed or points at
  a different endpoint it starts a fresh connection.
- If destination doesn't advertise QUIC, retain the original
  LLUDP/existing-circuit behavior.
- On synchronous QUIC connection creation failure, honour the
  TasiaQuicFallbackLLUDP setting only when an LLUDP or new circuit can
  actually be used. Do not claim an existing QUIC-only circuit was
  converted to LLUDP by enableCircuit().
- Log SimQuicHost, SimQuicPort and the pre-existing circuit type on
  TeleportFinish, to distinguish missing advertisement from upgrade.

Manual testing checklist (not yet executed):
1. QUIC login -> teleport to an already-visible neighbouring QUIC region:
   destination must upgrade any pre-seeded LLUDP circuit to QUIC.
2. QUIC login -> teleport to an unvisited QUIC region: creates QUIC.
3. Return teleport: existing connected QUIC circuit to same host:port
   should be reused; closed circuit must reconnect.
4. Same advertised hostname, changed destination QUIC port: must not
   reuse a QUIC connection bound to the old port.
5. Destination doesn't advertise SimQuicHost/SimQuicPort: LLUDP allowed.
6. QUIC connection creation fails with TasiaQuicFallbackLLUDP enabled:
   fallback only if LLUDP is or can be used.
7. Repeat with fallback disabled: show an explicit failure, not a silent
   LLUDP downgrade.
8. Region crossing through CrossedRegion, with and without a pre-existing
   LLUDP neighbour circuit.
9. Regression check Second Life and non-QUIC OpenSim grids.

Useful viewer log filter:

    grep -Ei 'TeleportFinish:|CrossedRegion:|EnableSimulator:|QUIC (connected|connecting|peer-initiated|transport-initiated)|falling back to LLUDP' TasiaViewer.log

If logs show SimQuicHost='' or SimQuicPort=0, investigate the
simulator's advertisement in its EnableSimulator / TeleportFinish
EventQueue payload. If QUIC is advertised and attempts still fail,
check transport/routing on the simulator/ROBUST rather than assuming
the viewer-only change fixes it.

Build: rebuild Tasia Viewer (Linux/Windows as needed); no ROBUST or
simulator restart is needed for viewer-only changes. The server-side
route pre-registration is a separate change in igrid_future PR #7.
