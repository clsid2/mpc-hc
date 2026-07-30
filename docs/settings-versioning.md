# Settings storage & downgrade protection

How MPC-HC's settings are stored and how a few format-fragile values are
protected against being clobbered by an older build. Reworked for issue #2347.

Relevant code:
- `src/mpc-hc/Profile.{h,cpp}` — the `CProfile` store engine (INI + registry
  backends, binary encoding).
- `src/mpc-hc/mplayerc.cpp` — `CMPlayerCApp::SetupSettingsStore()` / helpers and
  the `GetProfile*`/`WriteProfile*` overrides that delegate to `CProfile`.

Related: [`settings-history-store.md`](settings-history-store.md) (the separate
MediaHistory store) and the HKLM machine-defaults import (`ApplyHKLMDefaults`).

---

## The store stays where it always was

External tools read MPC-HC's settings directly (for example madVR reads some
player values), so this rework **keeps the on-disk location and byte format
unchanged**:

| | Location |
|---|---|
| Registry (installed) | `HKCU\Software\MPC-HC\MPC-HC` |
| Portable INI | `<exe-basename>.ini` next to the executable |

`CProfile` replaces the old hand-rolled profile code in `CMPlayerCApp`, but it is
a drop-in: it reads/writes the same sections and keys, and binary values use the
legacy **A-P encoding** (two chars per byte, low nibble first, `'A'`+nibble) so
the bytes are identical to what MFC's `WriteProfileBinary` produced. An older
build, and any external reader, keeps reading the store unchanged.

`CProfile` picks the location at construction: if `<exe-basename>.ini` sits next
to the executable it runs **portable/INI**, otherwise **registry**. In registry
mode the key is opened/created **lazily** on first access — never at static-init
time, because the store object is a member of the global `theApp`.

The one relocation is **MediaHistory**, which moves to a separate file in
portable mode — see [`settings-history-store.md`](settings-history-store.md).

## Scalar migration is unchanged

There is **no whole-store format version and no store-level migration
framework** in this design. Ordinary settings evolve the way they always have:

- New settings are additive — an old build simply ignores a key it doesn't know.
- Scalar migrations that must transform an existing value are handled by the
  pre-existing `CAppSettings::MigrateSettings()`, keyed on `[Settings]
  SettingsVersion` (`IDS_R_VERSION`) versus `APPSETTINGS_VERSION`. That mechanism
  predates #2347 and is untouched here.

An empirical sweep of ~45 archived official releases confirmed that scalar
settings only ever change **additively** — no in-place type or encoding change of
an existing key has ever shipped — so no store-wide versioning is needed to keep
old and new builds interoperating on scalars.

---

## Downgrade protection — `DVBConfiguration2`

The sweep found exactly **one** value that needs physical separation: the saved
DVB channels. Their serialization is pipe-delimited with a leading
format-version token, and it has changed shape repeatedly (tokens 0…6 so far);
an older build **throws** on a newer token, drops the channel, and — because
channels are rewritten on **every** `SaveSettings` — overwrites the newer list
just by running. Only a key an old build never touches can prevent that.

The new build stores the whole DVB configuration (BDA scalars + channels) in a
**replacement section** old builds don't know about, following the same pattern
`Commands2` and `FileFormats2` used when their formats changed:

```
IDS_R_DVB2 = "DVBConfiguration2"   (SettingsDefines.h)
```

A *sibling* section (not a subkey of the legacy one) keeps `DVBConfiguration` a
plain leaf key: old builds clear it on every save with MFC's non-recursive
`RegDeleteKey`, which would fail if a subkey were nested inside it (leaving
stale channel entries a downgrade would read back as phantom channels).

Rules:

- **Write:** the new build writes **only** to `DVBConfiguration2`, clearing it
  first (so a shrunken channel list leaves no stale trailing entries). The
  legacy `DVBConfiguration` section is **left frozen** — deliberately not
  written or cleared.
- **Read + one-time migrate:** the new build probes `BDASymbolRate` in
  `DVBConfiguration2` with a default of `-1` (never a legitimate stored value).
  `-1` means the section hasn't been written yet — first run after an upgrade —
  so everything is read once from the legacy section and the next save migrates
  it. Once the section exists, the legacy entries are never read again, so
  clearing all channels in a new build can't resurrect the frozen legacy list.
- **Old builds** only ever read/write the legacy section, never
  `DVBConfiguration2`, so they can't see or corrupt the new-format data. A
  downgrade keeps operating on its own frozen legacy copy.

The accepted tradeoff: after upgrading and re-saving, a subsequent downgrade sees
its **pre-upgrade** DVB settings (the frozen legacy snapshot), not any changes
made by the newer build. Nothing is lost or corrupted in either direction.

### Why the toolbar layout does *not* get this treatment

`Toolbars\PlayerToolBar → ButtonSequence` looked like a candidate (its shape
changed once, in 2.5.5.30), but it already carries **in-band versioning** — the
companion `ButtonLayoutRevision` key — and current builds read every shipped
revision correctly. Two further properties make physical separation unnecessary,
and in fact harmful:

- Old builds rewrite the layout **only when the user customizes the toolbar**
  (`SaveToolbarState` fires from the customization handlers only) — there is no
  passive overwrite-on-every-save like the DVB channels.
- Today's builds all write the identical revision-1 format, so a split key would
  protect nothing while making a downgrade show a stale frozen layout it could
  have read perfectly.

If the layout format changes again, it stays self-contained: bump
`ButtonLayoutRevision` and handle the older revisions in the reader, exactly as
the revision-0 → revision-1 transition already does. The revision key *is* the
upgrade process — no new keys or namespace needed.

## Unrelated hardening — validated binary reads

`CAppSettings` reads a few audio-renderer `double`s straight from a binary blob.
Those reads now guard on `dSize == sizeof(double)` before dereferencing, so a
truncated or foreign value can't be misread. This is defensive only; it is not
tied to any format version.

---

## Interaction with reset and HKLM defaults

- `/reset` and an HKLM `SettingsReset` clear the store and re-stamp
  `[Version] HistorySplit = 1`, so the next launch doesn't treat the cleared
  store as a fresh install and re-run the MediaHistory split.
- The **HKLM machine-defaults import** (`ApplyHKLMDefaults`, issue #2347) is
  deferred to `ApplySettingsPolicies()`, called only once a **normal interactive
  launch** is committed (after `/help`, `/close`, `/regvid`, `/admin`, and
  single-instance forwarding have returned) so utility invocations don't apply
  machine policy. See the source for the `SettingsReset` / `SettingsTimestamp`
  gating.

---

## Key identifiers (quick reference)

- Store: `HKCU\Software\MPC-HC\MPC-HC` / `<exe>.ini` (unchanged location, A-P binary)
- `CProfile` — the store engine; `AfxGetProfile()` reaches the app's instance
- `IDS_R_DVB2` = `DVBConfiguration2` (`SettingsDefines.h`) — downgrade-protected replacement section for the DVB settings (`BDASymbolRate` probe with default `-1` detects first run after upgrade)
- `[Version] HistorySplit = 1` — MediaHistory split-done marker
- `CAppSettings::MigrateSettings()` + `[Settings] SettingsVersion` (`IDS_R_VERSION`) — pre-existing scalar migration (unchanged)
- `CMPlayerCApp::SetupSettingsStore()` / `SetupHistoryStore()` / `ApplySettingsPolicies()`
- `ApplyHKLMDefaults()` / `ImportHKLMTree()` — HKLM machine-defaults import (#2347)
