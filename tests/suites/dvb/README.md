# MPC-HC scan harness

Part of the MPC-HC test framework (`tests/` in this repository): the
`tests/emulator` submodule provides the virtual BDA driver, generated
streams, encoding matrix and the transport plumbing these scripts
dot-source; the player under test is this repository's own build. Configure
once via `testbed.config.psd1` in `tests/`.

Drives MPC-HC's tuner scan against the virtual tuner and reports what the
scan actually produced, as data rather than as a screenshot.

`Invoke-Suite.ps1` is the orchestrator's entry point: a headless scan
(`/dvbscan`, which writes the channel-record JSON to a file and exits --
no scan dialog, no web server) per standard the rig provides, asserted
against the encoding matrix. The scripts below are the deeper standalone
layer: dialog-driven scans exercise the interactive path the headless
switch bypasses, and the frame probes assert on rendered pixels.

## Layout

| | |
|---|---|
| `harness.ps1` | runs **inside the guest**: launches MPC-HC, drives the scan dialog, harvests the results |
| `Install-MpcHarness.ps1` | copies `harness.ps1` in and registers the scheduled task that runs it |
| `Invoke-MpcScan.ps1` | one run, from the host |
| `Invoke-MpcBatch.ps1` | a plan of runs and rig changes |
| `Add-AtscVhfMap.ps1` / `Remove-AtscVhfMap.ps1` | map ATSC VHF channels onto existing captures, and undo |
| `plans/` | the plans used for the ATSC work |

## Use

```powershell
.\Install-MpcHarness.ps1                       # once, and after editing harness.ps1
.\Invoke-MpcBatch.ps1 -Plan .\plans\atsc-channel-plan.json -OutDir .\results
```

A result is one JSON document per run: the channel list's `N`, name, frequency
and encryption columns, the complete `CBDAChannel` record behind each row, a
timestamped progress trace, the wall-clock scan time, and the channel list
persisted to the registry on exit.

The `tuner` monikers in the plans are specific to this machine's device
instances. Read the current ones out of the guest before reusing a plan:

```powershell
Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses\{71985f48-1ca1-11d3-9cc8-00c04f7971e0}' -Recurse |
    Where-Object { $_.PSChildName -like '#*' } | Select-Object -ExpandProperty Name
```

The trailing GUID differs between the two devices - it is not the same value
with the instance number changed.

## Why it is built this way

**It runs in the guest, through a scheduled task.** Window stations are
per-session. A PowerShell Direct session lands in session 0 and cannot see,
enumerate or message the windows of an application on the interactive desktop,
so a driver script started that way finds nothing. The task runs with an
interactive token as the console user, which puts driver and application in the
same session. Someone must be logged on at the guest console.

**It reads the channel list, not the screen.** MPC-HC's scan dialog keeps a
zero-width final column holding each channel's complete `CBDAChannel::ToString()`
record - format version, name, frequency, origin number, encryption flag, PIDs,
and the ATSC virtual channel number. Reading that column with `LVM_GETITEMTEXT`
gives exact values with no pixel interpretation.

**It waits on the Save button.** `CTunerScanDlg::SetProgress` disables Save for
the duration of a scan and re-enables it at the end. The progress bar is not
usable for this: it is reset to zero on completion, so it never reads 100.

**Comparisons are made in the guest.** Several captures carry Cyrillic and
accented service names. Any text hop between the player and the comparison can
replace those characters and manufacture a difference that looks like a parsing
bug. `harness.ps1` compares seeded and read-back records where they were
produced, case-sensitively, and reports the count.

## The one thing that will mislead you

**Cycle the tuner device before every run.** The driver serves correctly for the
first MPC-HC session after the device starts, and then stops locking. Later
scans run to completion, report no error, and find zero channels - while the
device still reports `OK` and the service still reports `Running`. Nothing in
the stream map or the device state shows it.

`Invoke-MpcScan.ps1` does the disable/enable itself, so this only bites if you
drive MPC-HC by some other route. The tell is in the progress trace: a missed
frequency costs a fraction of a second and a real lock costs seconds, so a scan
with no long pauses in it never locked at all.

## Frame assertions

`Test-MpcFrame.ps1` adds the layer the channel list cannot provide: proof that
something actually rendered. It captures two frames via MPC-HC's own Save Image
(auto) on F5 — the decoded picture, not a screen grab — and probes known
colours in them.

Three checks: the frame matches one of the channel's two colours; the subtitle
probe grid from `subtitle-probes.json` matches; and two frames a second or so
apart differ, so a stream frozen on its first frame is caught.

Four things bit during development, all now handled, and all of them the sort
that produce a confident wrong answer rather than an error:

- **MPC-HC runs as the console user, the harness does not.** Snapshots land in
  *that* user's Pictures folder. `$env:USERPROFILE` in a PowerShell Direct
  session is a different profile whose Pictures folder is empty, so the search
  covers every profile.
- **`SnapshotPath` and `SnapshotExt` are read at startup.** Setting them while
  MPC-HC is running has no effect, so the search accepts both `.png` and
  `.jpg`. JPEG is tolerable here: the sample colours are far enough apart that
  chroma subsampling cannot move one into another's range.
- **Snapshot filenames contain square brackets**, which are wildcard syntax to
  PowerShell. Neither `-Path` nor `-LiteralPath` survives `Copy-Item
  -FromSession`, so the bytes come back base64 encoded instead.
- **The advancement check must not sample the flat colour.** It alternates on a
  two-second cycle, so two captures an even number of seconds apart are the
  same colour and a flat-area comparison reports no change on a perfectly
  healthy stream. It samples the overlay band, whose per-second counter differs
  between any two distinct seconds.

## Input model: window messages, not keystrokes

The harness sends no keystrokes and needs no focus, so nothing on the desktop
can intercept it. `harness.ps1` drives the scan with
`PostMessage(WM_COMMAND, ...)` and `SendMessage(BM_CLICK)` to specific window
handles, fills the edit boxes with `WM_SETTEXT`, and reads results out of the
hidden `TSCC_CHANNEL` column cross-process with `LVM_GETITEMTEXT` via
`VirtualAllocEx`/`ReadProcessMemory`. `Test-MpcFrame.ps1` posts
`ID_FILE_SAVE_IMAGE_AUTO` (807) the same way.

Two run prerequisites:

- **Set `UpdaterAutoCheck` to 0 in the profile before a run.** Not because a
  prompt can steal input from the harness, but because a modal window is
  visible state that a captured frame will contain.
- **Treat an empty result as suspicious rather than as evidence.** The cause is
  more likely to be the rig than the player. In particular the virtual tuner
  stops locking after an MPC-HC graph teardown, and every later scan then finds
  zero channels while the device still reports `OK` — which is why
  `Invoke-MpcScan.ps1` cycles the device before every run.

## A note on probe colours

A subtitle probe whose expected colour matches the channel's video colour can
pass with no subtitle rendered at all: the probe simply reads the flat
background. This has been observed — a Red cell passing at distance 1 while the
five other cells failed, because the video was red at that instant. The suite
still fails correctly, since every cell must match, but an individual cell's
pass is not on its own evidence that anything was composited. Choosing grid
colours disjoint from the channel palette would remove the ambiguity.
