param()
$ErrorActionPreference = 'Stop'
$job = Get-Content C:\vtuner\job.json -Raw | ConvertFrom-Json
$out = @{ label = $job.label; started = (Get-Date).ToString('o') }

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool EnumThreadWindows(uint tid, EnumWindowsProc cb, IntPtr p);
  public delegate bool EnumWindowsProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageStr(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint a, bool inh, uint pid);
  [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, IntPtr sz, uint t, uint pr);
  [DllImport("kernel32.dll")] public static extern bool VirtualFreeEx(IntPtr p, IntPtr a, IntPtr sz, uint t);
  [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr w);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, IntPtr n, out IntPtr r);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
}
"@

$WM_SETTEXT = 0x000C; $WM_COMMAND = 0x0111; $WM_CLOSE = 0x0010
$BM_CLICK = 0x00F5; $PBM_GETPOS = 0x0408
$LVM_GETITEMCOUNT = 0x1004; $LVM_GETITEMTEXTW = 0x1073

function Get-ProcWindows([int]$procId) {
  $res = @()
  foreach ($t in (Get-Process -Id $procId).Threads) {
    $cb = [W+EnumWindowsProc]{ param($h, $p)
      $sb = New-Object Text.StringBuilder 256; [W]::GetClassName($h, $sb, 256) | Out-Null
      $tb = New-Object Text.StringBuilder 512; [W]::GetWindowText($h, $tb, 512) | Out-Null
      $script:acc += [pscustomobject]@{ h = $h; cls = $sb.ToString(); text = $tb.ToString(); vis = [W]::IsWindowVisible($h) }
      return $true }
    $script:acc = @()
    [W]::EnumThreadWindows([uint32]$t.Id, $cb, [IntPtr]::Zero) | Out-Null
    $res += $script:acc
  }
  $res
}

function Read-ListView($hList, [int]$procId, [int]$sub) {
  $h = [W]::OpenProcess(0x0438, $false, [uint32]$procId)
  if ($h -eq [IntPtr]::Zero) { throw "OpenProcess failed" }
  $n = [int][W]::SendMessageW($hList, $LVM_GETITEMCOUNT, [IntPtr]::Zero, [IntPtr]::Zero)
  $mem = [W]::VirtualAllocEx($h, [IntPtr]::Zero, [IntPtr]2048, 0x3000, 4)
  $rows = @()
  try {
    for ($i = 0; $i -lt $n; $i++) {
      $lv = New-Object byte[] 88
      [BitConverter]::GetBytes([uint32]1).CopyTo($lv, 0)            # mask LVIF_TEXT
      [BitConverter]::GetBytes([int]$i).CopyTo($lv, 4)              # iItem
      [BitConverter]::GetBytes([int]$sub).CopyTo($lv, 8)            # iSubItem
      [BitConverter]::GetBytes([int64]($mem.ToInt64() + 128)).CopyTo($lv, 24)  # pszText
      [BitConverter]::GetBytes([int]900).CopyTo($lv, 32)            # cchTextMax
      $w = [IntPtr]::Zero
      [W]::WriteProcessMemory($h, $mem, $lv, [IntPtr]88, [ref]$w) | Out-Null
      [W]::SendMessageW($hList, $LVM_GETITEMTEXTW, [IntPtr]$i, $mem) | Out-Null
      $buf = New-Object byte[] 1800
      $r = [IntPtr]::Zero
      [W]::ReadProcessMemory($h, [IntPtr]($mem.ToInt64() + 128), $buf, [IntPtr]1800, [ref]$r) | Out-Null
      $s = [Text.Encoding]::Unicode.GetString($buf)
      $z = $s.IndexOf([char]0); if ($z -ge 0) { $s = $s.Substring(0, $z) }
      $rows += $s
    }
  } finally {
    [W]::VirtualFreeEx($h, $mem, [IntPtr]0, 0x8000) | Out-Null
    [W]::CloseHandle($h) | Out-Null
  }
  $rows
}

function Shot($path) {
  $b = [Windows.Forms.SystemInformation]::VirtualScreen
  $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
  $g = [Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
  $bmp.Save($path, [Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
}

# --- 1. clean slate -------------------------------------------------------
Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
New-Item -ItemType Directory -Force -Path C:\vtuner\out | Out-Null

# The pool hands out whichever guest is free, and every clone carries its own
# copy of whatever was deployed when the image was taken. A job that does not
# name a binary therefore measures an artefact of some earlier run on some
# other guest, and reports it as though it were this one. Say so instead.
# allowExistingBinary is the deliberate opt-out, for the rare run that really
# does mean "whatever is installed".
if (-not $job.exe -and -not $job.allowExistingBinary -and -not $job.readChannelsOnly) {
  throw ("job '$($job.label)' names no binary. On a pooled guest that measures whatever the " +
         'clone image happened to carry. Pass -HostBinary, or set allowExistingBinary to say you mean it.')
}
if ($job.exe) {
  # A lingering mpc-hc64 from another session holds the image and this copy
  # fails. Unguarded, the whole run dies at its first step with only the
  # transcript to say why. Retry briefly -- a process being torn down releases
  # the file within a second or two -- and if it persists, say plainly that the
  # rig is busy rather than leaving a bare Copy-Item error.
  $copied = $false
  for ($attempt = 1; $attempt -le 10 -and -not $copied; $attempt++) {
    try {
      Copy-Item $job.exe C:\mpc-hc\mpc-hc64.exe -Force -ErrorAction Stop
      $copied = $true
    } catch {
      if ($attempt -eq 10) {
        $holder = (Get-Process mpc-hc64 -ErrorAction SilentlyContinue |
                   ForEach-Object { "pid $($_.Id) started $($_.StartTime.ToString('HH:mm:ss'))" }) -join ', '
        throw ("RIG BUSY: cannot replace C:\mpc-hc\mpc-hc64.exe after 10 attempts. " +
               "Something still holds it$(if ($holder) { " ($holder)" }). Another session is probably " +
               "driving the guest; results from a shared rig cannot be trusted. Original error: $($_.Exception.Message)")
      }
      Start-Sleep -Milliseconds 500
    }
  }
}
$out.exeHash = (Get-FileHash C:\mpc-hc\mpc-hc64.exe).Hash

# --- 2. settings ----------------------------------------------------------

# MPC-HC reads this as GetProfileInt(IDS_R_SETTINGS, IDS_RS_DEFAULT_CAPTURE, 0)
# -- AppSettings.cpp:2226 -- and 0 is analog:
#   iDefaultCaptureDevice == 1 ? PM_DIGITAL_CAPTURE : PM_ANALOG_CAPTURE
# in MainFrm.cpp:16875. An absent value therefore does not fail. It selects
# analog capture and opens the capture options page instead of scanning, which
# presents exactly like a tuner fault. The value went missing from this shared
# profile once already and nobody could attribute who dropped it. Stamping it
# on every run means its absence cannot be inherited from anyone.
$S = 'HKCU:\Software\MPC-HC\MPC-HC\Settings'
# Create-if-missing, NEVER New-Item -Force: on the registry provider, -Force
# RECREATES an existing key EMPTY, deleting every value in it. This block
# shipped with -Force and therefore wiped the launching user's entire MPC-HC
# profile on every run, destroying among other things a manually stamped
# UpdaterAutoCheck=0 in the window between a caller's stamp and the launch --
# which put the first-run update prompt back on screen over a working player
# and 404'd the web interface, while the caller's before/after reads of their
# own stamp looked perfectly correct.
if (-not (Test-Path $S)) { New-Item -Path $S | Out-Null }
Set-ItemProperty $S -Name DefaultCapture -Value 1 -Type DWord
$out.defaultCapture = (Get-ItemProperty $S -Name DefaultCapture).DefaultCapture

# The web interface, which /dvb/channels.json lives behind. Stamped here for
# the same reason DefaultCapture is: it was previously private knowledge in one
# session's own run script, so that session could read the endpoint and anyone
# using this harness could not -- and the failure said only "Unable to connect
# to the remote server". LocalhostOnly stays 0 and the reader uses 127.0.0.1;
# see the note on the capture below.
Set-ItemProperty $S -Name EnableWebServer         -Value 1     -Type DWord
Set-ItemProperty $S -Name WebServerPort           -Value 13579 -Type DWord
Set-ItemProperty $S -Name WebServerLocalhostOnly  -Value 0     -Type DWord
Set-ItemProperty $S -Name WebServerUseCompression -Value 0     -Type DWord

# The first-run update prompt. When UpdaterAutoCheck is absent, MPC-HC raises
# an AfxMessageBox INSIDE InitInstance (UpdateChecker.cpp:234), before
# CWebServer::Init() at mplayerc.cpp:2324. The modal's nested pump keeps the
# player fully working -- scans, tuning, playback -- while the web server
# listens with all three route maps empty, so every request 404s. A day was
# spent attributing that to the guests, the builds and the branch; it was this
# box, invisible on an unattended session, blocking a profile that had never
# answered it. 0 = never check, and no prompt.
Set-ItemProperty $S -Name UpdaterAutoCheck -Value 0 -Type DWord
$out.webServer = [bool](Get-ItemProperty $S -Name EnableWebServer).EnableWebServer

$K = 'HKCU:\Software\MPC-HC\MPC-HC\DVBConfiguration2'
# Same -Force trap as $S above: the old form silently wiped this key every
# run, which made every run implicitly clearChannels regardless of the job.
# Explicit clearing below is the only clearing.
if (-not (Test-Path $K)) { New-Item -Path $K | Out-Null }
# Provenance for anything that later asserts on this result. ParsePMT resets
# only fps and chroma (Mpeg2SectionData.cpp:518-519); width, height and aspect
# ratio are never reset and survive from FromString on a channel loaded from
# the saved list. So a reading taken from a re-tuned saved channel is partly
# memory rather than measurement, and an assertion against it can pass on
# stale state. Recording whether the list was cleared means the assertion path
# can refuse such a result instead of trusting it.
$out.channelsCleared  = [bool]$job.clearChannels
$out.readChannelsOnly = [bool]$job.readChannelsOnly

if ($job.clearChannels) {
  (Get-ItemProperty $K).PSObject.Properties | Where-Object { $_.Name -match '^\d+$' } |
    ForEach-Object { Remove-ItemProperty $K -Name $_.Name -Force }
}
# BDATuner is shared mutable state. A run that does not set it inherits
# whichever tuner the previous run selected, so a session alternating between
# the DVB-T and ATSC devices leaves the wrong one in force for whoever goes
# next -- and the scan that follows looks entirely normal while describing the
# wrong device. A scanning job must name its tuner rather than inherit one.
# readChannelsOnly never opens a device, so it stays exempt.
if ($job.tuner) {
  Set-ItemProperty $K -Name BDATuner -Value $job.tuner
} elseif (-not $job.readChannelsOnly) {
  throw ("job '$($job.label)' names no tuner. Scanning with whatever the previous run left " +
         'selected yields a plausible result for the wrong device, which is worse than stopping.')
}

# Record what was actually in force. If a result is ever disputed, this says
# which tuner and which capture mode produced it instead of leaving it to be
# reconstructed from who was on the rig at the time.
$out.bdaTuner = (Get-ItemProperty $K -Name BDATuner -ErrorAction SilentlyContinue).BDATuner
if ($job.seedChannels) {
  $i = 0
  foreach ($ch in $job.seedChannels) { Set-ItemProperty $K -Name "$i" -Value $ch -Type String; $i++ }
}
Set-ItemProperty $K -Name BDAScanFreqStart -Value ([int]$job.freqStart) -Type DWord
Set-ItemProperty $K -Name BDAScanFreqEnd   -Value ([int]$job.freqEnd)   -Type DWord
Set-ItemProperty $K -Name BDABandWidth     -Value ([int]($job.bandwidth / 1000)) -Type DWord
Set-ItemProperty $K -Name BDASymbolRate    -Value ([int]$job.symbolRate) -Type DWord
Set-ItemProperty $K -Name BDAIgnoreEncryptedChannels -Value ([int]$job.ignoreEncrypted) -Type DWord
Set-ItemProperty $K -Name BDAUseOffset     -Value 0 -Type DWord

if ($job.settingsOnly) {
  $out.note = 'settings written, no run'
  $out | ConvertTo-Json -Depth 6 | Set-Content "C:\vtuner\out\$($job.label).json"
  return
}

# --- 3h. headless scan ------------------------------------------------------
# /dvbscan (in the tree since #4138) runs the tuner scan without the scan
# dialog and writes the channel-record JSON to /dvbscanout, then closes
# itself. No window messages and no web server: the file is the result. The
# tuner and capture settings still come from the registry stamped above; the
# switches carry only what the dialog fields would have, in kHz throughout
# (/dvbbandwidth is TunerScanData units, not the profile's MHz).
if ($job.headless) {
  $scanOut = "C:\vtuner\out\$($job.label).channels.json"
  Remove-Item $scanOut -Force -ErrorAction SilentlyContinue
  $argl = @('/dvbscan', "$($job.freqStart)-$($job.freqEnd)", '/dvbscanout', "`"$scanOut`"")
  if ($job.bandwidth)  { $argl += @('/dvbbandwidth',  "$($job.bandwidth)")  }
  if ($job.symbolRate) { $argl += @('/dvbsymbolrate', "$($job.symbolRate)") }
  $out.args = $argl -join ' '
  $t0 = Get-Date
  $p = Start-Process C:\mpc-hc\mpc-hc64.exe -ArgumentList $argl -PassThru
  $limit = 300; if ($job.timeoutSec) { $limit = [int]$job.timeoutSec }
  $deadline = (Get-Date).AddSeconds($limit)
  while ((Get-Date) -lt $deadline -and -not $p.HasExited) { Start-Sleep -Seconds 2 }
  if (-not $p.HasExited) {
    # A hung headless scan left running would poison the next job's binary
    # copy with a RIG BUSY that names the wrong culprit.
    $out.error = "player still running after ${limit}s; killed"
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  }
  $out.scanSeconds = [int]((Get-Date) - $t0).TotalSeconds
  if (Test-Path $scanOut) {
    # ReadAllText, not Get-Content -Raw: under Windows PowerShell the latter
    # returns a string carrying ETS note properties (PSPath and friends),
    # which ConvertTo-Json then explodes into an object -- a 59 MB result
    # whose channelsJson is {value:...} instead of a string.
    $out.channelsJson = [IO.File]::ReadAllText($scanOut)
  } elseif (-not $out.ContainsKey('error')) {
    # Distinguish "scanned and found nothing" (a written, empty list) from
    # "never got as far as writing", which is a rig or build fault.
    $out.error = 'player exited without writing the scan output'
  }
  $out.finished = (Get-Date).ToString('o')
  $out | ConvertTo-Json -Depth 6 | Set-Content "C:\vtuner\out\$($job.label).json"
  return
}

# --- 3. launch ------------------------------------------------------------
$p = Start-Process C:\mpc-hc\mpc-hc64.exe -PassThru
$deadline = (Get-Date).AddSeconds(60)
$main = [IntPtr]::Zero
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $main = $p.MainWindowHandle; break }
}
if ($main -eq [IntPtr]::Zero) { $out.error = 'no main window'; $out | ConvertTo-Json -Depth 6 | Set-Content "C:\vtuner\out\$($job.label).json"; return }
$out.mainWindow = $main.ToString()
Start-Sleep -Seconds 3

if ($job.tuneAndHold) {
  # Open the device and stay running. Test-MpcFrame captures and asserts but
  # deliberately does not tune, so something has to leave a live tuned player
  # behind -- and every earlier attempt at frame probing died on there being
  # no mode that does. 802 is the open-device command; with a saved channel
  # list of one, what it tunes is unambiguous.
  [W]::PostMessage($main, $WM_COMMAND, [IntPtr]802, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Seconds 14
  if ($job.subtitleSelect) {
    # ID 2300 = first subtitle stream. The whole subtitle chain measured green
    # except this: nothing in the old harness ever selected the track.
    [W]::PostMessage($main, $WM_COMMAND, [IntPtr]2300, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Seconds 3
  }
  $sb2 = New-Object Text.StringBuilder 512; [W]::GetWindowText($main, $sb2, 512) | Out-Null
  $out.title = $sb2.ToString()
  try {
    $resp = Invoke-WebRequest -Uri 'http://127.0.0.1:13579/dvb/channels.json' -UseBasicParsing -TimeoutSec 10
    $out.channelsJson = $resp.Content
  } catch { $out.channelsJsonError = $_.Exception.Message }
  $out.holdOpen = $true
  $out.finished = (Get-Date).ToString('o')
  $out | ConvertTo-Json -Depth 6 | Set-Content "C:\vtuner\out\$($job.label).json"
  # The player stays running on purpose; whoever captures frames owns closing it.
  return
}

if ($job.readChannelsOnly) {
  # Open the device so the loaded channels are actually exercised, not merely parsed.
  [W]::PostMessage($main, $WM_COMMAND, [IntPtr]802, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Seconds 12
  [W]::PostMessage($main, $WM_COMMAND, [IntPtr]33415, [IntPtr]::Zero) | Out-Null  # ID_VIEW_NAVIGATION
  Start-Sleep -Seconds 4
  Shot "C:\vtuner\out\$($job.label).png"
  $sb2 = New-Object Text.StringBuilder 512; [W]::GetWindowText($main, $sb2, 512) | Out-Null
  $out.title = $sb2.ToString()
  # Close gracefully so settings are written back. A channel whose record failed
  # to parse is dropped at load time and will be missing from the read-back.
  [W]::PostMessage($main, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
  $p.WaitForExit(30000) | Out-Null
  if (-not $p.HasExited) { $out.forcedKill = $true; Stop-Process -Id $p.Id -Force }
  Start-Sleep -Seconds 2
  $rt = @()
  (Get-ItemProperty $K -ErrorAction SilentlyContinue).PSObject.Properties |
    Where-Object { $_.Name -match '^\d+$' } | Sort-Object { [int]$_.Name } | ForEach-Object { $rt += $_.Value }
  $out.seededCount = @($job.seedChannels).Count
  $out.rewrittenCount = @($rt).Count
  # Compare in-guest so no host-side text transport can colour the result.
  #
  # Format version 7 appends two ATSC fields, so a v7 read-back of a v6 seed
  # carries two trailing fields the seed never had. Set aside exactly as many
  # as the version difference accounts for, and no more. Stripping two
  # unconditionally is correct only against a build that already has the ATSC
  # fix; run against one that does not -- a baseline binary, which is exactly
  # what a before-and-after comparison uses -- it removes two real fields from
  # every record and then reports differences of its own making.
  function Get-TrailingAtscFields([string] $record) {
    $v = 0
    if ([int]::TryParse((($record -split '\|')[0]), [ref] $v) -and $v -ge 7) { return 2 }
    return 0
  }

  $seed = @($job.seedChannels)
  $same = 0; $differ = @()
  for ($i = 0; $i -lt $seed.Count -and $i -lt $rt.Count; $i++) {
    $a = $seed[$i].Substring($seed[$i].IndexOf('|') + 1)
    $b = $rt[$i].Substring($rt[$i].IndexOf('|') + 1)
    $extra = (Get-TrailingAtscFields $rt[$i]) - (Get-TrailingAtscFields $seed[$i])
    for ($k = 0; $k -lt $extra; $k++) {
      $cut = $b.LastIndexOf('|')
      if ($cut -lt 0) { break }
      $b = $b.Substring(0, $cut)
    }
    if ($a -ceq $b) { $same++ } else { $differ += "[$i] in=$a out=$b" }
  }
  $out.identicalRecords = $same
  $out.differingRecords = $differ
  $out.seedVersions = (@($seed | ForEach-Object { ($_ -split '\|')[0] } | Sort-Object -Unique) -join ',')
  $out.readBackVersions = (@($rt | ForEach-Object { ($_ -split '\|')[0] } | Sort-Object -Unique) -join ',')
  $out.rewrittenChannels = $rt
  $out | ConvertTo-Json -Depth 6 | Set-Content "C:\vtuner\out\$($job.label).json"
  return
}
# --- 4. open device -------------------------------------------------------
[W]::PostMessage($main, $WM_COMMAND, [IntPtr]802, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 10
$sb = New-Object Text.StringBuilder 512; [W]::GetWindowText($main, $sb, 512) | Out-Null
$out.titleAfterOpen = $sb.ToString()
Shot "C:\vtuner\out\$($job.label)-open.png"

# --- 5. scan dialog -------------------------------------------------------
$dlg = [IntPtr]::Zero
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Date) -lt $deadline -and $dlg -eq [IntPtr]::Zero) {
  [W]::PostMessage($main, $WM_COMMAND, [IntPtr]974, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Seconds 3
  foreach ($w in (Get-ProcWindows $p.Id)) {
    if ($w.cls -eq '#32770' -and $w.vis -and ([W]::GetDlgItem($w.h, 22025)) -ne [IntPtr]::Zero) { $dlg = $w.h; break }
  }
}
if ($dlg -eq [IntPtr]::Zero) {
  $out.error = 'scan dialog never appeared'
  Shot "C:\vtuner\out\$($job.label)-nodlg.png"
  Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force
  $out | ConvertTo-Json -Depth 6 | Set-Content "C:\vtuner\out\$($job.label).json"; return
}
$out.dialog = $dlg.ToString()

$hStart = [W]::GetDlgItem($dlg, 22025)
$hSave  = [W]::GetDlgItem($dlg, 22030)
$hList  = [W]::GetDlgItem($dlg, 22024)
$hProg  = [W]::GetDlgItem($dlg, 22020)

[W]::SendMessageStr([W]::GetDlgItem($dlg, 22021), $WM_SETTEXT, [IntPtr]::Zero, "$($job.freqStart)") | Out-Null
[W]::SendMessageStr([W]::GetDlgItem($dlg, 22023), $WM_SETTEXT, [IntPtr]::Zero, "$($job.freqEnd)")   | Out-Null
[W]::SendMessageStr([W]::GetDlgItem($dlg, 22022), $WM_SETTEXT, [IntPtr]::Zero, "$($job.bandwidth)") | Out-Null
[W]::SendMessageStr([W]::GetDlgItem($dlg, 22026), $WM_SETTEXT, [IntPtr]::Zero, "$($job.symbolRate)")| Out-Null
Start-Sleep -Milliseconds 500
Shot "C:\vtuner\out\$($job.label)-dlg.png"

# --- 6. run the scan, tracing progress -----------------------------------
# The Save button is disabled for exactly the duration of the scan
# (CTunerScanDlg::SetProgress), which is a far more reliable end signal than the
# progress bar - that resets to 0 on completion and so never reads 100.
$sw = [Diagnostics.Stopwatch]::StartNew()
[W]::SendMessageW($hStart, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
$trace = @(); $last = -1; $lastCount = 0
$limit = [int]$job.timeoutSec
$started = $false
while ($sw.Elapsed.TotalSeconds -lt $limit) {
  Start-Sleep -Milliseconds 100
  $pos = [int][W]::SendMessageW($hProg, $PBM_GETPOS, [IntPtr]::Zero, [IntPtr]::Zero)
  $cnt = [int][W]::SendMessageW($hList, $LVM_GETITEMCOUNT, [IntPtr]::Zero, [IntPtr]::Zero)
  if ($pos -ne $last -or $cnt -ne $lastCount) {
    $trace += [pscustomobject]@{ t = [math]::Round($sw.Elapsed.TotalSeconds, 2); pos = $pos; items = $cnt }
    $last = $pos; $lastCount = $cnt
  }
  $saveEnabled = [W]::IsWindowEnabled($hSave)
  if (-not $started) { if (-not $saveEnabled) { $started = $true } }
  elseif ($saveEnabled) { break }
}
$sw.Stop()
$out.completed = $started -and $sw.Elapsed.TotalSeconds -lt $limit
$out.scanSeconds = [math]::Round($sw.Elapsed.TotalSeconds, 2)
$out.progressTrace = $trace
$out.distinctProgress = ($trace | Select-Object -Expand pos -Unique).Count
Start-Sleep -Seconds 2
Shot "C:\vtuner\out\$($job.label)-scan.png"

# --- 7. harvest -----------------------------------------------------------
$out.numbers = Read-ListView $hList $p.Id 0
$out.names   = Read-ListView $hList $p.Id 1
$out.freqs   = Read-ListView $hList $p.Id 2
$out.crypt   = Read-ListView $hList $p.Id 3
$out.records = Read-ListView $hList $p.Id 8

# --- 8. save + quit -------------------------------------------------------
[W]::SendMessageW($hSave, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 3

# --- 8b. the decoded record, AFTER Save ------------------------------------
#
# Placement is load-bearing and was wrong once: /dvb/channels.json renders
# m_DVBChannels, which Save commits -- read before the click it returns a
# perfectly valid { "channels" : [] }, the empty-200 trap this project has
# already documented. After Save and before WM_CLOSE the player is still in
# digital capture, so this is the one window where the endpoint has both a
# tuned player and a committed list.
try {
  # 127.0.0.1, not localhost: the server binds IPv4 0.0.0.0 only and localhost
  # resolves to ::1 first, so the request times out rather than being refused.
  $resp = Invoke-WebRequest -Uri 'http://127.0.0.1:13579/dvb/channels.json' -UseBasicParsing -TimeoutSec 10
  $out.channelsJson = $resp.Content
  try { $out.channelsJsonCount = @(($resp.Content | ConvertFrom-Json).channels).Count } catch { }
} catch {
  $out.channelsJsonError = $_.Exception.Message
}

[W]::PostMessage($main, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
$p.WaitForExit(20000) | Out-Null
if (-not $p.HasExited) { $out.forcedKill = $true; Stop-Process -Id $p.Id -Force }
Start-Sleep -Seconds 2

$saved = @()
(Get-ItemProperty $K -ErrorAction SilentlyContinue).PSObject.Properties |
  Where-Object { $_.Name -match '^\d+$' } | Sort-Object { [int]$_.Name } |
  ForEach-Object { $saved += $_.Value }
$out.savedChannels = $saved
$out.finished = (Get-Date).ToString('o')
$out | ConvertTo-Json -Depth 6 | Set-Content "C:\vtuner\out\$($job.label).json"
