# The MediaHistory ("history mode") settings store

How MPC-HC stores **MediaHistory** (recently-played files, resume positions,
per-file audio/subtitle selections, A-B repeat, etc.) separately from the rest of
the settings. Part of the issue #2347 settings rework.

Relevant code:
- `src/mpc-hc/Profile.{h,cpp}` — the `CProfile` store engine.
- `src/mpc-hc/mplayerc.{h,cpp}` — `CMPlayerCApp` owns the stores and routes to them.
- `src/mpc-hc/AppSettings.cpp` — the MediaHistory reader/writer
  (`CRecentFileListWithMoreInfo`), unchanged by this feature.

See also [`settings-versioning.md`](settings-versioning.md) for the overall
storage model (the store keeps its historical location; only these two things —
MediaHistory and the downgrade-protected `v2` subsections — move).

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
| `m_Profile` | Main settings store: `<exe-basename>.ini` or `HKCU\Software\MPC-HC\MPC-HC`. | Always present. |
| `m_HistoryProfile` (`unique_ptr`) | MediaHistory store: a second INI-mode `CProfile` at `<exe-basename>.history.ini`. | Created **only in portable/INI mode**; **null in registry mode**. |

The history store is a full `CProfile`, so `history.ini` uses the same on-disk
format as the settings INI (UTF-8 with BOM-sniffing on read, `[section]` /
`key=value`, A-P-encoded binary values).

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
  `m_Profile` (the registry `MPC-HC` key), exactly as everything else.

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
  (registry MPC-HC key / <exe>.ini)    (<exe>.history.ini)
```

## Location option and fallback (`HistoryInAppData`)

By default the history INI sits next to the executable. Two things relocate it
to `%APPDATA%\MPC-HC\<exe-basename>.history.ini` instead:

- the **`HistoryInAppData`** advanced option (Options → Advanced) — e.g. a
  shared/portable install where each user should keep private history;
- automatically, when the history file **cannot be created** in the player
  folder (read-only install running off a settings INI).

The option lives in the main settings store, which is read before the history
store is set up, so `ResolveHistoryIniPath()` (mplayerc.cpp) can honor it during
startup; it also carries an existing history file over when the location
changes, so toggling the option doesn't lose history. The **saved playlist**
(`default.mpcpl`) follows the history file's folder (`GetPlaylistSavePath()`),
so both relocate together.

## One-time split (migration)

MediaHistory has always lived in the main settings store, so on the first
portable run it is moved out once, in `SetupHistoryStore()`:

```cpp
m_HistoryProfile = std::make_unique<CProfile>(CProfile::HistoryIniPath());
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

An older build that later runs in the same folder simply doesn't know about
`history.ini`; it regenerates its own `MediaHistory` inside `<exe>.ini` as it
always did. That's non-critical (history, not configuration) and can't corrupt
anything — the two histories just diverge.

## Lifecycle

- **Flush:** `FlushProfile()` flushes both stores (each writes only if dirty), on
  the same cadence as before (`OnIdle`, `SaveSettings`).
- **Reset** (`/reset` or HKLM `SettingsReset`): clears both stores, then re-stamps
  `[Version] HistorySplit = 1` in the settings store.
- **Change settings location** (Options → Player → "Store settings in .ini file"):
  `ChangeSettingsLocation()` re-points/creates or drops `m_HistoryProfile` for the
  new mode, then `SaveSettings(true)` rewrites the full in-memory history into the
  new location (no cross-mode value copy, so no corruption).

## Export

- **INI mode:** a full export (`ExportSettings`) bundles `<exe>.ini` **and**
  `history.ini` into a single `.zip` (under their real names), so a restore
  ("extract into the program folder") can't miss either file.
- **Registry mode:** exports one `.reg` of the whole `MPC-HC` key, which already
  contains MediaHistory — no zip needed.

## Registry vs INI — summary

| Aspect | INI (portable) | Registry (installed) |
|---|---|---|
| History location | `<exe>.history.ini` (separate file) | `HKCU\Software\MPC-HC\MPC-HC` (same key) |
| `m_HistoryProfile` | non-null | null |
| Routing for `MediaHistory[...]` | `*m_HistoryProfile` | `m_Profile` |
| One-time split | performed | not applicable |
| Export | companion inside the `.zip` | already in the `.reg` |

## Key identifiers (quick reference)

- `CProfile::HistoryIniPath()` → `<exe-basename>.history.ini`
- `CMPlayerCApp::m_HistoryProfile` — the history store (null in registry mode)
- `CMPlayerCApp::SetupHistoryStore()` — creates the store and runs the one-time split
- `ProfileForSection(section)` / `IsMediaHistorySection(section)` — routing
- `CProfile::MoveSectionTree(root, dst)` — the one-time move
- `[Version] HistorySplit = 1` (settings store) — split-done marker
