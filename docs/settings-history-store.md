# The MediaHistory ("history mode") settings store

How MPC-HC stores **MediaHistory** (recently-played files, resume positions,
per-file audio/subtitle selections, A-B repeat, etc.) separately from the rest of
the settings. Part of the issue #2347 settings rework.

Relevant code:
- `src/mpc-hc/Profile.{h,cpp}` — the `CProfile` store engine.
- `src/mpc-hc/mplayerc.{h,cpp}` — `CMPlayerCApp` owns the stores and routes to them.
- `src/mpc-hc/AppSettings.cpp` — the MediaHistory reader/writer
  (`CRecentFileListWithMoreInfo`), unchanged by this feature.

See also [`settings-versioning.md`](settings-versioning.md) for the shared
versioning/migration model these stores use.

---

## Why a separate store

MediaHistory grows with every file ever opened (one subsection per entry, up to
the configured limit) and is rewritten constantly during playback. Keeping it in
the main settings file made that file large and churn-heavy. Issue #2347 asked to
move MediaHistory into its own file.

The split applies to **portable (INI) mode only**. In **registry** mode history
stays inside the settings key (the registry handles a large, frequently-updated
key fine, and it keeps the design minimal).

## The two stores

`CMPlayerCApp` owns two `CProfile` instances:

| Member | Backing | Lifetime |
|---|---|---|
| `m_Profile` | Main settings store: `<exe-basename>.settings.ini` or `HKCU\Software\MPC-HC\Settings`. | Always present. |
| `m_HistoryProfile` (`unique_ptr`) | MediaHistory store: a second INI-mode `CProfile` at `<exe-basename>.history.ini`. | Created **only in portable/INI mode**; **null in registry mode**. |

The history store is a full `CProfile`, so `history.ini` uses the same on-disk
format as the settings INI (UTF-8 with BOM-sniffing on read, `[section]` /
`key=value`, `b64:`-prefixed Base64 for binary).

## Section routing

Every profile accessor on `CMPlayerCApp` funnels through one helper:

```cpp
CProfile& CMPlayerCApp::ProfileForSection(LPCWSTR lpszSection)
{
    if (m_HistoryProfile && IsMediaHistorySection(lpszSection))
        return *m_HistoryProfile;
    return m_Profile;
}
```

`IsMediaHistorySection()` is true for the section named exactly `"MediaHistory"`
and for any subsection starting with `"MediaHistory\"`. So:

- **INI mode:** any read/write of a `MediaHistory[...]` section transparently hits
  `m_HistoryProfile` (`history.ini`).
- **Registry mode:** `m_HistoryProfile` is null, so those calls fall through to
  `m_Profile` (the registry `Settings` key), exactly as everything else.

Because the routing lives inside the generic accessors, the MediaHistory code in
`AppSettings.cpp` needs **no changes** — it already calls `theApp.GetProfile*/
WriteProfile*` with the `"MediaHistory"` section.

```
AppSettings.cpp (MediaHistory code)
        │  theApp.GetProfileString("MediaHistory\\<hash>", ...)
        ▼
CMPlayerCApp::GetProfileString ── ProfileForSection("MediaHistory\\<hash>")
        │                                   │
        │ (registry mode / non-history)     │ (INI mode + history section)
        ▼                                   ▼
     m_Profile                        *m_HistoryProfile
  (registry Settings / settings.ini)   (<exe>.history.ini)
```

## Version stamp

Like the settings store, the history file carries its own `[Version]` section,
versioned **independently** of the settings and of the app:

```
[Version]
Format        = v1        ; HISTORY_FORMAT_VERSION - MediaHistory on-disk format
LastWrittenBy = 2.7.3.44  ; MPC-HC build that last wrote it (informational)
```

Bump `HISTORY_FORMAT_VERSION` (in `Profile.h`) on an incompatible MediaHistory
layout change. These are written straight to `m_HistoryProfile` (not through the
routed `WriteProfile*`), because `[Version]` is not a `MediaHistory` section.

## One-time split (migration)

MediaHistory has always lived in the main settings store, so on the first
portable run it is moved out once, in `SetupHistoryStore()`:

```cpp
m_HistoryProfile = std::make_unique<CProfile>(CProfile::HistoryIniPath());
// (fork if the history file is a newer format - see settings-versioning.md)
if (!m_Profile.HasEntry(L"Version", L"HistorySplit")) {
    m_Profile.MoveSectionTree(L"MediaHistory", *m_HistoryProfile);  // move + subsections
    m_Profile.WriteString(L"Version", L"HistorySplit", L"1");        // done marker
    m_Profile.Flush(true);
}
```

`CProfile::MoveSectionTree(root, dst)` moves the `MediaHistory` section and every
`MediaHistory\<hash>` subsection into the history store (verbatim raw-value copy,
safe since both are the same format) and erases them from the settings store. The
`[Version] HistorySplit = 1` marker in the settings store makes it run once.

## Lifecycle

- **Flush:** `FlushProfile()` flushes both stores (each writes only if dirty), on
  the same cadence as before (`OnIdle`, `SaveSettings`).
- **Reset** (`/reset` or HKLM `SettingsReset`): clears both stores, then re-stamps
  `[Version] Format` / `LastWrittenBy` / `HistorySplit` in the settings store.
- **Change settings location** (Options → Player → "Store settings in .ini file"):
  `ChangeSettingsLocation()` re-points/creates or drops `m_HistoryProfile` for the
  new mode, then `SaveSettings(true)` rewrites the full in-memory history into the
  new location in the correct format (no cross-mode value copy, so no corruption).
- **Newer history format:** `SetupHistoryStore()` forks to
  `<exe>.history.local.ini` if `history.ini` declares a `Format` newer than this
  build understands (same protection as the settings store).

## Export

- **INI mode:** a full export (`ExportSettings`) bundles `settings.ini` **and**
  `history.ini` into a single `.zip` (under their real names), so a restore
  ("extract into the program folder") can't miss either file.
- **Registry mode:** exports one `.reg` of the whole `Settings` key, which already
  contains MediaHistory — no zip needed.

## Registry vs INI — summary

| Aspect | INI (portable) | Registry (installed) |
|---|---|---|
| History location | `<exe>.history.ini` (separate file) | `HKCU\Software\MPC-HC\Settings` (same key) |
| `m_HistoryProfile` | non-null | null |
| Routing for `MediaHistory[...]` | `*m_HistoryProfile` | `m_Profile` |
| One-time split | performed | not applicable |
| Export | companion inside the `.zip` | already in the `.reg` |

## Key identifiers (quick reference)

- `CProfile::HistoryIniPath()` → `<exe-basename>.history.ini`; `HistoryLocalIniPath()` → `.history.local.ini`
- `CMPlayerCApp::m_HistoryProfile` — the history store (null in registry mode)
- `CMPlayerCApp::SetupHistoryStore()` — creates the store, forks, one-time split
- `ProfileForSection(section)` / `IsMediaHistorySection(section)` — routing
- `CProfile::MoveSectionTree(root, dst)` — the one-time move
- `[Version] HistorySplit = 1` (settings store) — split-done marker
- `[Version] Format` (history file) — history format version (`HISTORY_FORMAT_VERSION`)
