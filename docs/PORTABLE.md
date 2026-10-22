# Portable Windows package

The supplied `TasiaViewer.portable` marker enables portable mode. Launch
`TasiaViewer.exe` normally, including by shortcut. Its working directory does not
affect the data location.

```
TasiaViewer.exe
TasiaViewer.portable
TasiaViewerData/
  user_settings/       # settings and login_accounts.sqlite3
  logs/                # viewer logs and crash reports
  cache/               # texture, object, inventory and browser caches
  chat_logs/           # per-account chat transcripts
  temp/                # temporary files, including converter output
  <account>/           # account-specific settings
```

Keep the whole package in a writable directory. Portable mode refuses to fall
back to AppData if the data root is unavailable or unwritable. Previously saved
absolute cache and transcript paths cannot redirect portable data to another
installation. Each copied package uses its own adjacent data folder. Existing
AppData data is neither imported nor removed automatically.

Saved passwords remain Windows DPAPI protected: copying this directory to another
Windows account or computer does not make passwords decryptable there. Sign in
again on that computer. Treat the complete data directory as private; settings,
account identifiers, transcripts and logs are not all encrypted.

Do not copy a live profile while the viewer is running. Back it up with the viewer
closed. Package staging never copies an existing profile. Removing the marker
restores the traditional AppData locations, without migrating or deleting data,
unless an older `portable-data` folder also exists. That legacy folder still
enables portable mode and is reused in place. If both data folders exist, startup
stops and asks you to select the intended profile; it never merges them.

Portable here describes viewer-managed storage, not a promise of zero OS traces:
Windows, GPU drivers and third-party components can maintain their own records.
