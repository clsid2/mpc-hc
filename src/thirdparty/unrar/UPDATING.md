# Updating the vendored UnRAR source

This directory contains the UnRAR source from rarlab, vendored wholesale (rarlab
publishes no git repository, only source tarballs from
https://www.rarlab.com/rar_add.htm). MPC-HC carries five small local patches on
top, documented below. `unrar.vcxproj` / `unrar.vcxproj.filters` are
MPC-HC-maintained project files and are never overwritten by an update.

## Local patches (re-apply after every update)

Every change is marked with an `// MPC-HC` comment in the source.

| File | Change | Reason |
|---|---|---|
| `arcread.cpp` | `ReadHeader15()` and `ReadHeader50()`: `return 0;` at the top of the `if (Decrypt)` block | Archives with encrypted headers silently yield zero entries instead of firing the password callback — a media player must never prompt for archive passwords. Covers RAR4 and RAR5 formats. |
| `extract.hpp` | `class CmdExtract`: `CheckUnpVer()` and the `DataIO` / `Unp` / `TotalFileCount` / `FileCount` members made `public` | RARFileSource streams media directly out of archives without extracting to disk, driving `ComprDataIO` manually. The official DLL API cannot do this. |
| `file.cpp` | `File::File()`: `AllowExceptions=false;` | I/O failures (missing volume, read error) return `false` instead of throwing `RAR_EXIT` up through a DirectShow filter graph thread. |
| `volume.cpp` | `MergeArchive()`: `try/catch (RAR_EXIT)` around `Arc.CheckArc(true)` | A corrupt/truncated next volume in a spanned set otherwise throws via `ErrorHandler::Exit()` (not covered by `AllowExceptions`) and aborts playback; with the catch, `MergeArchive` fails cleanly. |

## Known consumer couplings

- `src/thirdparty/RARFileSource/` (vendored in-tree, not a submodule) uses UnRAR
  **internals**, not just the DLL API. When upstream refactors internals, expect
  breakage here. During the 6.24 → 7.2.7 update:
  - upstream deleted `array.hpp`; `Array<byte>` uses became `std::vector<byte>`,
  - `Archive::FileName` / `FileHeader::FileName` became `std::wstring`,
  - `GetVolNumPart()` was removed from `pathfn.cpp`; RFS.cpp now carries a local
    copy (`RfsGetVolNumPart`).
- `src/Subtitles/VobSubFile.cpp` and `src/mpc-hc/SubtitlesProvidersUtils.cpp`
  use only the stable C DLL API (`RAROpenArchiveEx` etc.) — normally unaffected.

## Update procedure

Run from this directory in PowerShell. Review each step's output rather than
assuming success — upstream restructures things occasionally.

```powershell
# 1. Find and download the latest source (check https://www.rarlab.com/rar_add.htm)
$ver = '7.2.7'   # <-- set to latest
$tmp = Join-Path $env:TEMP "unrarsrc"
Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
New-Item -ItemType Directory $tmp | Out-Null
Invoke-WebRequest "https://www.rarlab.com/rar/unrarsrc-$ver.tar.gz" -OutFile "$tmp\u.tar.gz"
tar -xzf "$tmp\u.tar.gz" -C $tmp

# 2. Note the file-list delta (drives vcxproj edits in step 5)
$old = Get-ChildItem . -Include *.cpp,*.hpp -Name
$new = Get-ChildItem "$tmp\unrar" -Include *.cpp,*.hpp -Name
Compare-Object $old $new   # <= means removed upstream, => means added

# 3. Replace sources (keeps vcxproj/filters/UPDATING.md, which live only here)
Remove-Item *.cpp, *.hpp, acknow.txt, license.txt, readme.txt, dll.def, dll.rc
Copy-Item "$tmp\unrar\*.cpp", "$tmp\unrar\*.hpp", "$tmp\unrar\acknow.txt",
          "$tmp\unrar\license.txt", "$tmp\unrar\readme.txt",
          "$tmp\unrar\dll.def", "$tmp\unrar\dll.rc" .

# 4. Re-apply the five patches from the table above (search for the anchor
#    lines; `git diff` against the previous tree shows them exactly). Verify:
Select-String -Path *.cpp, *.hpp -Pattern 'MPC-HC' | Select-Object Path, LineNumber, Line
# Expect: 2x arcread.cpp, 2x extract.hpp (public/private pair counts as marked
# lines), 1x file.cpp, 1x volume.cpp.

# 5. Update unrar.vcxproj + unrar.vcxproj.filters for the step-2 delta
#    (add/remove ClCompile and ClInclude entries; dll.cpp must stay compiled —
#    the RARDLL;SILENT define set in the vcxproj is what MPC-HC's consumers use).

# 6. Build all four configurations and fix any RARFileSource fallout (see
#    "Known consumer couplings"): build.bat Build x64 MPCHC Release, etc.
```

Last update: 6.24 → 7.2.7 (August 2026, clsid2/mpc-hc issue #3990).
