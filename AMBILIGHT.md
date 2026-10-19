# MPC-HC Ambilight

This player fork pairs with https://github.com/teguhsatria892/VideoRenderer.
It adds an Ambilight button immediately left of volume on the bottom toolbar.
Open a video using the paired renderer, then click the button.

The modeless panel exposes Enabled, eleven sliders with numeric fields, four
directions and Reset. Changes preview immediately, even while paused, and
save automatically without OK or Apply. Closing the panel preserves changes.
Controls reconnect when the playback graph changes and are disabled when a
compatible active renderer is unavailable.

First-run preset: Enabled; Intensity 53, Blur 55, Edge 27, Spread 55,
fade start/curve 15/35, Brightness/Contrast/Colors/Saturation 100,
Debanding 0, all directions enabled. Reset uses these numeric defaults.

Player integration lives in src/mpc-hc/AmbilightDialog.*, its .rc2 resource,
PlayerToolBar.*, and MainFrm.*. include/AmbilightSettings.h mirrors the
renderer protocol; public renderer ABI and COM IDs remain unchanged.

For isolated use, keep a portable mpc-hc64.ini next to this executable and put
the paired MpcVideoRenderer64.ax in MPCVR. Select MPC Video Renderer under
Playback > Output. The player loads this local DLL before a registered one.

Build using the parent workspace's build-player.cmd with Visual Studio 2022,
ATL/MFC, Windows SDK and NASM. Initialize submodules recursively first.
The renderer's AMBILIGHT.md documents its GPU pipeline, tests and SDR limits.

## Workspace and installer scripts

The `ambilight-workspace` folder preserves the build scripts and Inno Setup
installer definition used for this build. Clone the `ambilight` branch of both
forks into sibling folders named `mpc-hc` and `VideoRenderer`, with recursive
submodules. Copy the contents of `mpc-hc/ambilight-workspace` into their parent
workspace before running its scripts. Adjust the VS/NASM paths as needed.

The installer script expects the official portable runtime assets in
`outputs/MPC-HC-Ambilight` (LAVFilters64, language/resource folders and DLLs),
the built player and renderer, and the validation logs named in the script.
It packages the runtime, source archives, first-run preset, desktop/Start Menu
shortcuts and an independent per-user uninstaller using Inno Setup 6.
The destination-folder page always appears, including on reinstall/update.
Generated installers, user configuration and playback history are not stored
in Git. The current installer is `MPC-HC-Ambilight-Setup-x64-v5.exe`.
