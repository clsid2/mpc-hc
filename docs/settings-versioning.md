# Versioned settings

How MPC-HC's settings are stored, versioned, migrated, and protected against
older builds. Introduced for issue #2347.

Relevant code:
- `src/mpc-hc/Profile.{h,cpp}` — the `CProfile` store engine (INI + registry
  backends, legacy import, fork-to-local, binary encoding).
- `src/mpc-hc/mplayerc.cpp` — `CMPlayerCApp::SetupSettingsStore()` and helpers
  (the version/migration logic), plus the `GetProfile*`/`WriteProfile*` overrides.

Related: [`settings-history-store.md`](settings-history-store.md) (the separate
MediaHistory store) and the HKLM machine-defaults import (`ApplyHKLMDefaults`).

---

## The store

Settings live in a single store whose **name has no version in it**:

| | Legacy (pre-#2347) | Current |
|---|---|---|
| Registry | `HKCU\Software\MPC-HC\MPC-HC` | `HKCU\Software\MPC-HC\Settings` |
| Portable INI | `<exe-basename>.ini` | `<exe-basename>.settings.ini` |

The legacy store is **never modified** — it's only read once, to import
pre-#2347 settings (below), so an old MPC-HC build keeps working unchanged.

`CProfile` picks the location at construction: if a settings INI (new or legacy)
sits next to the executable it runs **portable/INI**, otherwise **registry**. In
registry mode the key is opened/created **lazily** on first access (never at
static-init time — the store object is a member of the global `theApp`).

## The format version is a field, not the filename

The on-disk *format* version is recorded **inside** the store, in a `[Version]`
section:

```
[Version]
Format       = v1      ; on-disk format version (SETTINGS_FORMAT_VERSION)
LastWrittenBy = 2.7.3.44 ; MPC-HC build that last wrote the store (informational)
```

- `Format` is the constant `SETTINGS_FORMAT_VERSION` in `Profile.h` (currently
  `"v1"`). It is bumped **only** on an incompatible layout change — not per
  release. Within a format version, all app versions interoperate; changes are
  additive (a new key an old build ignores) or, if a value's format must change,
  it goes under a **new key**.
- `CompareVersionStrings` compares these tokens numerically (`v2 < v11`), so any
  monotonic `v1 → v2 → …` sequence orders correctly. Always keep the `v` prefix.

This replaces the earlier scheme that put the version in the store name
(`Settings-v1` / `<exe>.settings-v1.ini`) plus a separate index file — that was
removed as redundant and confusing.

---

## Startup — `SetupSettingsStore()`

Runs early in `InitInstance`, after the store location is detected and before
`LoadSettings()`. It reads the store's `[Version] Format` and acts on it:

```cpp
CStringW fmt;
bool haveStore = m_Profile.ReadString(L"Version", L"Format", fmt) && !fmt.IsEmpty();

if (!haveStore) {
    // (1) first run: import pre-versioned legacy settings, then migrate up
    if (m_Profile.MigrateFromLegacy())
        ApplySettingsMigrations(m_Profile, LEGACY_EQUIVALENT_VERSION, myS);
} else if (CompareVersionStrings(fmt, myS) < 0) {
    // (2) older format: upgrade this store in place
    ApplySettingsMigrations(m_Profile, fmt, myS);
} else if (CompareVersionStrings(fmt, myS) > 0) {
    // (3) newer format: fork to a private local file (see below)
    const CStringW local = CProfile::LocalIniPath();
    bool existed = PathUtils::Exists(local);
    m_Profile.ForkToLocalIni(local, !existed);   // seed from current if new
    m_bWarnNewerFormat = !existed;               // warn once, later
}
// history store (portable mode) + stamp Format/LastWrittenBy
```

Cases:

1. **First run (no store):** `MigrateFromLegacy()` copies the pre-#2347 settings
   into the new store (registry: recursive key copy with native value types; INI:
   verbatim parse+merge). Legacy is the `v1` layout, so the migration chain runs
   from `LEGACY_EQUIVALENT_VERSION`. Non-destructive.
2. **Older format:** the migration chain transforms the store in place up to our
   version, then the `Format` stamp is updated.
3. **Newer format (downgrade protection):** see below.

## Downgrade protection — fork to a local file

Because the store name is now version-independent, an **older build opens the
same file a newer build wrote**. That is safe *only because every #2347-era build
checks `[Version] Format`* (the pre-#2347 legacy builds use a different store
entirely, so they never touch this one). When a build finds `Format` **newer**
than it understands, it must not clobber it:

- `CProfile::ForkToLocalIni()` detaches from the shared store **without deleting
  it** and switches this instance to a private `<exe-basename>.settings.local.ini`
  (history: `.history.local.ini`).
- If that local file already exists (this build forked before) it is loaded and
  reused silently. If it's new, it is **seeded from the current store** (registry
  values converted to INI form) so the older build starts from the newer data it
  can read, and the user is told **once** (`IDS_SETTINGS_NEWER_VERSION`, shown by
  `ApplySettingsPolicies` on the next committed normal launch).

The newer build is never modified; each older build keeps its own local file.

## Migration framework

Format upgrades are an ordered table in `mplayerc.cpp`:

```cpp
struct SettingsMigrationStep { const wchar_t* from; const wchar_t* to; SettingsMigrationFn apply; };
static const std::vector<SettingsMigrationStep> s_settingsMigrations = {
    // { L"v1", L"v2", &Migrate_Settings_v1_to_v2 },   // added when v2 ships
};
```

Each step transforms a store **in place** from `from` to `to`.
`ApplySettingsMigrations(profile, from, to)` walks the chain;
`CountSettingsMigrations` returns the step count or `-1` if no path exists. The
table is **empty today** (only `v1`), so no step runs. To introduce `v2`: bump
`SETTINGS_FORMAT_VERSION` and add `{ L"v1", L"v2", fn }`; existing `v1` stores
then upgrade in place, and `v1` builds fork away from `v2` stores.

## Interaction with reset

`/reset` and an HKLM `SettingsReset` clear the store and immediately re-stamp
`[Version] Format`, `LastWrittenBy`, and `HistorySplit`, so the next launch does
not treat the cleared store as a fresh install and re-import legacy settings.

## Command-line / policy timing

`SetupSettingsStore()` runs for every launch (it must, before `LoadSettings`),
but the **user-visible policies** — the newer-format notice and the HKLM
machine-defaults import — are deferred to `ApplySettingsPolicies()`, called only
once a **normal interactive launch** is committed (after `/help`, `/close`,
`/regvid`, `/admin`, and single-instance forwarding have returned). So utility
invocations neither pop a modal nor apply machine policy.

---

## Key identifiers (quick reference)

- `SETTINGS_FORMAT_VERSION` / `HISTORY_FORMAT_VERSION` / `LEGACY_EQUIVALENT_VERSION` (`Profile.h`) — bump the first to introduce a new format
- Stores: `HKCU\Software\MPC-HC\Settings` / `<exe>.settings.ini`; legacy `…\MPC-HC` / `<exe>.ini`
- Fork files: `<exe>.settings.local.ini`, `<exe>.history.local.ini`
- Fields: `[Version] Format`, `[Version] LastWrittenBy`, `[Version] HistorySplit`
- `CMPlayerCApp::SetupSettingsStore()` / `SetupHistoryStore()` / `StampSettingsStoreInitialized()` / `ApplySettingsPolicies()`
- `CProfile::MigrateFromLegacy()` — one-time pre-versioned import (non-destructive)
- `CProfile::ForkToLocalIni(iniPath, seedFromCurrent)` — downgrade fork
- `s_settingsMigrations` + `CountSettingsMigrations` / `ApplySettingsMigrations`
- `CompareVersionStrings()` — numeric `vN` compare
- `IDS_SETTINGS_NEWER_VERSION` — the "newer settings, using a separate file" notice
