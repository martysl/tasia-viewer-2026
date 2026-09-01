# Secrets Policy

## Viewer-side
- No tokens hardcoded in source
- No passwords in source
- No WebDAV credentials in viewer
- No database passwords in viewer
- No admin tokens in viewer

## CI/Workflow
- `FMOD_DEPS_TOKEN`: GitHub PAT for private deps release downloads
  - Stored in GitHub Secrets
  - Currently full PAT (replace with fine-grained read-only token)
- `TASIA_GIPHY_API_KEY`: GIPHY API key used only at configure time to generate obfuscated local fallback source files
  - Stored in GitHub Secrets
  - Never printed in logs
  - Generated files must remain ignored and uncommitted
- No tokens printed in logs
- No token files committed to repo

## Token file usage
- Local token file at `~/.config/opencode/token.txt` may be used only when explicitly allowed
- Never commit token files

## FMOD
- Raw FMOD files never in public repo
- FMOD .tar.bz2 packages never in public repo
- FMOD private release in `martysl/tasia-private-deps` only
- GitHub Secrets store only token text, never FMOD files

## Backend
- WebDAV credentials server-side only
- Database credentials server-side only
- Session tokens (not secrets) may be stored client-side for auth
