# Grid catalog and cosmetic identification

These integrations are source-prepared, not Windows/live-service verified.
Failures leave saved grids and simulator/account identities unchanged.

## Grid manager

`Get grid info` requests the entered login origin's `/get_grid_info` endpoint.
Standard OpenSim XML fields are mapped like the existing Firestorm implementation:
`welcome`, `economy`/`helperuri`, `profileuri`/`web_profile_url`, `register`,
`password`, `help`, `about`, `search`, `gatekeeper`, and `gridname`.
Results populate an editable preview; **Add** or **Save URLs** is required to save.
Discovery never replaces the entered login endpoint or moves saved credentials.
Metadata includes registration/password-reset links used by the login screen.
Changing a login endpoint requires adding a new grid, not silently rekeying an
existing account. Built-in Agni/Aditi endpoints remain protected.

The optional public catalog is:
`https://api.tasiaviewer.work/viewer/config/grid.xml`.
Enable **Allow server grid-list additions** to check it on the login screen,
when opening Grid Manager, or with **Check grid list**. This is additions-only:
existing grids are not overwritten; saved removal tombstones prevent resurrection.
No current-grid selection, password, account data or other preference is imported.
An invalid entry or failed disk write rolls the entire addition batch back.

Format: LLSD XML map/array, maximum 256 entries and 256 KiB. Accepts the existing
viewer names (`login_uri`, `label`, `login_page`, `helper_uri`) and Firestorm names
(`loginuri`, `gridname`, `loginpage`, `helperuri`). Up to eight explicit login URIs
are retained. Metadata URLs must be HTTP(S), without embedded credentials.
DTD/entity declarations and partial HTTP responses are rejected.

`indra/newview/app_settings/tasia_default_grids.xml` holds the bundled default
list: the original Tasia/Firestorm OpenSim entries plus the currently published
catalog (I-Grid, I-Grid Beta, Misfitz, Darkhearts, Fresh Metaverse and local
OpenSim). Bundled defaults are merged **additions-only** for every profile:
existing user entries and removal tombstones always win, and nothing is
silently replaced. Reference URLs come from the published gridinfo documents;
each was re-read live during preparation, but continued reachability is not
guaranteed.

## Grid + UUID identification

**One shared page for every grid:** `https://i.let-us.cyou/hg/config.json`

The viewer downloads this single file regardless of which grid you are on, and
each entry names the grid it belongs to. Nothing else is requested, and **no
avatar UUID, login secret, session ID or grid query is ever sent** — every
resident is a passive row in this public file.

```json
{
  "users": [
    {
      "grid": "i.let-us.cyou:8002",
      "uuid": "11111111-1111-1111-1111-111111111111",
      "tag_color": "#48E094"
    },
    {
      "grid": "secondlife.com",
      "uuid": "22222222-2222-2222-2222-222222222222",
      "tag_color": "#48E094"
    },
    {
      "grid": "os.tasia.work.gd:22000",
      "uuid": "33333333-3333-3333-3333-333333333333",
      "tag_color": "#48E094"
    }
  ]
}
```

### The `grid` field

Accepted forms (case-insensitive):

| Written as | Applies to |
|---|---|
| `i.let-us.cyou:8002` | I-Grid (the exact grid key) |
| `i.let-us.cyou` | I-Grid (host without port) |
| `https://i.let-us.cyou:8002/` | I-Grid (full URL) |
| `secondlife.com` | Second Life (Agni) |
| `aditi` | Second Life Beta (Aditi) |
| `os.tasia.work.gd:22000` | Fresh Metaverse |

Names are resolved against the viewer's own grid list, so the spelling above is
all that is needed — the viewer maps it to its internal key.

**If `grid` is omitted**, the entry belongs to the grid that hosts the file,
i.e. `i.let-us.cyou:8002` (I-Grid). Omit it only for I-Grid residents; for
every other grid the field is required, otherwise the entry never matches.

### Other fields

Only `uuid` is required (plus `grid` outside the publishing grid). Empty or absent
`display_name` and `username` preserve the corresponding real name independently.
Only explicitly nonempty values override that UI field. `page_name` is website-only
and is NEVER a fallback for a name. Native group titles/account type are preserved.

| Purpose | Accepted keys |
|---|---|
| Nametag and profile name color | `tag_color` or `color` — `#RRGGBB` or `#RRGGBBAA` |
| Explicit display-name override | `display_name` (empty/missing preserves the real display name) |
| Explicit username override | `username` (empty/missing preserves the real username) |

Legacy title/badge/note/image fields remain parse-compatible but do not inject
extra nametag lines. `page_name`, `account_type` and alternate override aliases
are not used as replacement identity text.

Limits: 1024 rows and 256 KiB. A duplicate grid+UUID pair rejects the whole
file (the previous good snapshot is kept) rather than picking one at random.

Enabled by default, switchable in Grid Manager through
`TasiaRemoteUserConfigEnabled`; URL is `TasiaRemoteUserConfigURL`. The viewer
refreshes at most once per five minutes while connected. In-flight results are
guarded by session, grid, generation and the enabled setting. Grid changes clear
the previous presentation; network or format failures keep the last good
same-grid snapshot. A valid empty catalog removes prior overrides.

Colors are local presentation. Profile name colors are applied when its name
callback runs (reopen an existing profile if needed). A nonmatching/disabled
entry restores that panel's original theme colors and real names. Explicit
nonempty display_name/username change UI text only; empty values preserve the
real fields, without combining them with page_name. Account classifications and
native group titles are never replaced. This does
not change name caches, chat routing, UUIDs, ownership, membership, permissions
or authentication. Badge images are not downloaded/rendered by this port.

## Fetch safety and tests

Catalog/config requests require verified HTTPS and refuse redirects. Explicit
OpenSim grid discovery additionally permits HTTP (needed by local/legacy grids).
All requests have a ten-second timeout, no automatic retries, and an LLCore body
limit enforced before buffering each chunk—not merely an advisory Range header.

`python3 indra/newview/tests/tasia_identity_grid_test.py` compiles actual production
identity parsing, grid+UUID lookup, catalog merge/removal/metadata policy, and HTTP
write-limiting methods against fixture framework types with ASan/UBSan. It also
validates settings/XML/UI wiring. It is not full viewer or live HTTP/GUI coverage.
