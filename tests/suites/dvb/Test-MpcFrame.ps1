<#
.SYNOPSIS
    Asserts on the picture MPC-HC is actually rendering, by probing known
    colours in captured frames.

.DESCRIPTION
    The rest of the harness reads the channel list, which proves MPC-HC parsed
    the tables. It cannot prove anything rendered. Those are genuinely separate
    failures: a service can be found, named, and reported as playing with the
    right codec and resolution while the picture is black or shows the wrong
    channel entirely. Both have happened here - a black frame behind a status
    bar reading "Playing [MPEG2 720x576]", and a driver serving the previous
    file's content while still reporting a lock.

    This closes that gap. The generated samples are built to make it cheap:
    each channel is a flat colour alternating once per second, and each
    subtitle carries a grid of known colours. So the assertion is a handful of
    pixel reads, not an image comparison and not OCR. Golden images would be
    the wrong tool - colour space conversion, chroma subsampling and renderer
    differences all shift pixels without anything being wrong, and re-blessing
    them would cost more than the bugs they caught.

    Three things are checked:

      Video colour  the frame matches one of the channel's two colours, which
                    proves service -> PID map -> decoder -> renderer connected
      Subtitles     the probe grid matches, which exercises the whole DVB
                    subtitle path: RLE decode, CLUT, region placement,
                    compositing
      Advancement   two frames a second or more apart differ, which catches a
                    stream frozen on its first frame - a real failure mode that
                    a single capture passes happily

    Frames come from MPC-HC's own "Save Image (auto)" command, F5, which writes
    the decoded frame to SnapshotPath with no dialog. That is the rendered
    picture rather than a screen grab, so it does not depend on window position
    or on the window being unobstructed.

.PARAMETER VMName
    Hyper-V guest running MPC-HC. It must already be playing the channel under
    test; this script captures and asserts, it does not tune.

.PARAMETER ExpectedColours
    The channel's two colours as RGB triples. A frame must match one of them.
    Generated channels alternate between the pair once per second, so which one
    depends on when the capture landed - matching either is correct.

.PARAMETER SubtitleProbes
    Probe points from the subtitle-probes.json emitted by New-TestStreams.ps1.
    Optional: omit to skip the subtitle assertion.

.PARAMETER Tolerance
    Permitted sum of absolute RGB differences. The default is deliberately
    loose. The sample colours are maximally separated, so a tolerance this
    large still cannot confuse two of them, while absorbing every colour space
    and subsampling artefact. Measured round-trip error on these samples is
    0 to 4.

.EXAMPLE
    $probes = Get-Content .\subtitle-probes.json -Raw | ConvertFrom-Json
    .\Test-MpcFrame.ps1 -VMName MyTestVM `
        -ExpectedColours @(@{R=255;G=0;B=0}, @{R=0;G=128;B=0}) `
        -SubtitleProbes ($probes | Where-Object Channel -eq 'Test Channel 1').ProbePoints
#>
[CmdletBinding()]
param(
    # Empty claims one from the pool; naming a guest bypasses the broker.
    [string]        $VMName = '',
    [string]        $CredentialPath = '',   # empty: transport/config default,

    [Parameter(Mandatory)]
    [object[]]      $ExpectedColours,

    [object[]]      $SubtitleProbes,

    [ValidateRange(0, 765)]
    [int]           $Tolerance = 96,

    [ValidateRange(500, 10000)]
    [int]           $AdvanceDelayMs = 1200,

    [string]        $OutDir = (Join-Path $PSScriptRoot 'frames'),
    [string]        $GuestSnapshotDir = 'C:\vtuner\frames',

    # Matrix streams are a single flat colour with no overlay counter -- static
    # by design -- so the advancement check would fail on a perfectly healthy
    # stream. This skips it, recorded as skipped rather than silently passed.
    # The channel-plan streams alternate and must NOT set this.
    [switch]        $NoAdvance,

    # Recorded in the rig lock so a blocked session knows who to ask.
    [string]        $LockOwner = 'dvbEmulator',
    [ValidateRange(0, 3600)] [int] $LockWaitSeconds = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigClaim.ps1')
$claim = Enter-RigClaim -VMName $VMName
$session = Connect-TestGuest -Guest $claim.Guest -CredentialPath $CredentialPath

# Before any capture: confirm the guest is the one we claimed.
Assert-RigIdentity -Session $session -Expected $claim.Guest | Out-Null

# See RigClaim.ps1: every clone carries whatever lock was live when the image
# was taken, and on a guest we hold exclusively it cannot be anyone's.
Clear-InheritedRigLock -Session $session -Claim $claim

# More than one session drives this guest. Capturing frames while another is
# swapping the binary or cycling a tuner device produces plausible output
# measured against the wrong thing, which is worse than an error.
. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigLock.ps1')
$rigLock = Enter-RigLock -Session $session -Owner $LockOwner -Purpose 'frame capture' -WaitSeconds $LockWaitSeconds

try {
    # --- Configure and capture -------------------------------------------------
    #
    # PNG rather than JPEG: this is a colour assertion, and JPEG's chroma
    # handling would add error to the very thing being measured.
    # SnapshotSubtitles must be on or the subtitle grid is absent from the
    # capture and its probes fail for the wrong reason.

    # Wrapped: a remote call returning one item yields a bare string, and .Count
    # on that throws under StrictMode rather than reporting 1.
    $consoleUser = (Get-TestBedConfig).GuestConsoleUser
    $captured = @(Invoke-Command -Session $session -ArgumentList $GuestSnapshotDir, $AdvanceDelayMs, $consoleUser -ScriptBlock {
        param($SnapDir, $DelayMs, $ConsoleUser)

        $sid = (New-Object System.Security.Principal.NTAccount($ConsoleUser)).Translate(
                   [System.Security.Principal.SecurityIdentifier]).Value
        $rs = "Registry::HKEY_USERS\$sid\Software\MPC-HC\MPC-HC\Settings"
        New-Item -Path $rs -Force | Out-Null
        Set-ItemProperty -Path $rs -Name 'SnapshotPath'      -Value $SnapDir -Type String
        Set-ItemProperty -Path $rs -Name 'SnapshotExt'       -Value '.png'   -Type String
        Set-ItemProperty -Path $rs -Name 'SnapshotSubtitles' -Value 1        -Type DWord

        New-Item -ItemType Directory -Force $SnapDir | Out-Null

        # MPC-HC reads SnapshotPath at startup, so setting it above only takes
        # effect for a later launch. Rather than require a restart, note the
        # time and afterwards collect whatever appeared in either the configured
        # directory or the default Pictures folder.
        # Every user's Pictures, not this session's. MPC-HC runs as the console
        # user under a scheduled task, while this code runs over PowerShell
        # Direct as whoever the harness authenticated as - so $env:USERPROFILE
        # here is the wrong profile entirely, and its Pictures folder is empty.
        $candidates = @($SnapDir)
        $candidates += @(Get-ChildItem 'C:\Users' -Directory -ErrorAction SilentlyContinue |
                         ForEach-Object { Join-Path $_.FullName 'Pictures' } |
                         Where-Object { Test-Path $_ })
        $candidates = $candidates | Where-Object { $_ } | Select-Object -Unique
        $cutoff = (Get-Date).AddSeconds(-2)

        # Posts ID_FILE_SAVE_IMAGE_AUTO (807) straight to the main window, the
        # same approach harness.ps1 uses for the scan. A posted command needs
        # no focus, so nothing can intercept it: an earlier version sent F5
        # through SendKeys and was at the mercy of whatever window happened to
        # be foreground, which on this guest included MPC-HC's own first-run
        # prompt and a Windows OneDrive popup.
        #
        # It still runs from a scheduled task with an interactive token,
        # because window messages do not cross sessions and a PowerShell Direct
        # session lands in session 0 where MPC-HC's window does not exist.
        $driver = @'
param([int]$DelayMs)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
}
"@
$p = Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { exit 2 }
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { exit 3 }
$WM_COMMAND = 0x0111
$ID_FILE_SAVE_IMAGE_AUTO = 807
[void][Win]::PostMessage($h, $WM_COMMAND, [IntPtr]$ID_FILE_SAVE_IMAGE_AUTO, [IntPtr]::Zero)
Start-Sleep -Milliseconds $DelayMs
[void][Win]::PostMessage($h, $WM_COMMAND, [IntPtr]$ID_FILE_SAVE_IMAGE_AUTO, [IntPtr]::Zero)
Start-Sleep -Milliseconds 800
'@
        Set-Content 'C:\vtuner\frame-driver.ps1' -Value $driver -Encoding ASCII
        & schtasks.exe /create /tn VTunerFrame /f /sc once /st 00:00 /ru $ConsoleUser /it `
            /tr "powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\vtuner\frame-driver.ps1 -DelayMs $DelayMs" 2>&1 | Out-Null
        & schtasks.exe /run /tn VTunerFrame 2>&1 | Out-Null

        # Wait for two files rather than sleeping a fixed time.
        $deadline = (Get-Date).AddSeconds(40)
        do {
            Start-Sleep -Milliseconds 500
            # Both extensions: SnapshotExt is also only read at startup, so a
            # running instance keeps writing JPEG whatever we set. That is
            # tolerable here - the sample colours are far enough apart that
            # JPEG's chroma handling cannot move one into another's range.
            $files = @(Get-ChildItem $candidates -Include *.png, *.jpg -Recurse -ErrorAction SilentlyContinue |
                       Where-Object { $_.LastWriteTime -gt $cutoff })
        } while ($files.Count -lt 2 -and (Get-Date) -lt $deadline)

        # Return the bytes rather than paths. MPC-HC names snapshots with
        # square brackets, which are wildcard syntax to PowerShell, and neither
        # -Path nor -LiteralPath survives Copy-Item -FromSession intact.
        @($files | Sort-Object LastWriteTime | Select-Object -First 2 | ForEach-Object {
            [pscustomobject]@{
                Name    = $_.Name
                Base64  = [Convert]::ToBase64String([IO.File]::ReadAllBytes($_.FullName))
            }
        })
    })

    if ($captured.Count -lt 2) {
        # Template first: -f binds tighter than +, so applying it to a
        # concatenation formats only the last literal.
        $msg = "MPC-HC produced {0} frame(s), expected 2. Is a channel selected and rendering, " +
               "and is someone logged on at the guest console? F5 is Save Image (auto), which " +
               "writes nothing unless a video is actually on screen -- opening the device without " +
               "selecting a channel maps no PIDs and renders black."
        throw ($msg -f $captured.Count)
    }

    New-Item -ItemType Directory -Force $OutDir | Out-Null
    $local = @()
    foreach ($f in $captured) {
        $dest = Join-Path $OutDir $f.Name
        [IO.File]::WriteAllBytes($dest, [Convert]::FromBase64String($f.Base64))
        $local += $dest
    }
} finally {
    if ($session -and $session.State -eq 'Opened') {
        Exit-RigLock -Session $session -Lock $rigLock
        Remove-PSSession $session
    }
    Exit-RigClaim -Claim $claim
}

# --- Assertions ------------------------------------------------------------

function Get-ColourDistance {
    param($Pixel, $Expected)
    [Math]::Abs($Pixel.R - $Expected.R) +
    [Math]::Abs($Pixel.G - $Expected.G) +
    [Math]::Abs($Pixel.B - $Expected.B)
}

$results = [System.Collections.Generic.List[object]]::new()
$img = [System.Drawing.Bitmap]::FromFile($local[0])
$img2 = [System.Drawing.Bitmap]::FromFile($local[1])

try {
    # Video colour. Sampled from the upper area, well clear of the identifying
    # overlay across the middle and the subtitle band at the bottom.
    $sx = [int]($img.Width / 2)
    $sy = [int]($img.Height * 0.15)
    $px = $img.GetPixel($sx, $sy)
    $best = ($ExpectedColours | ForEach-Object { Get-ColourDistance -Pixel $px -Expected $_ } |
             Measure-Object -Minimum).Minimum
    $results.Add([pscustomobject]@{
        Check    = 'video-colour'
        Detail   = "($sx,$sy) = $($px.R),$($px.G),$($px.B)"
        Distance = $best
        Pass     = ($best -le $Tolerance)
    })

    # Advancement. Sampled across the overlay band in the middle of the frame,
    # not the flat colour above it. The colour alternates on a two-second cycle,
    # so two captures an even number of seconds apart land on the same colour
    # and a flat-area comparison reports no change on a perfectly healthy
    # stream. The overlay carries a per-second counter, whose glyphs differ
    # between any two distinct seconds regardless of the colour phase.
    if ($NoAdvance) {
        $results.Add([pscustomobject]@{
            Check = 'advancement'; Detail = 'skipped: source is static by design (-NoAdvance)'
            Distance = 0; Pass = $true })
    } else {
    $changed = 0
    $ay = [int]($img.Height / 2)
    for ($i = 1; $i -le 40; $i++) {
        $ax = [int]($img.Width * $i / 41)
        if ((Get-ColourDistance -Pixel $img.GetPixel($ax, $ay) -Expected $img2.GetPixel($ax, $ay)) -gt 40) { $changed++ }
    }
    $results.Add([pscustomobject]@{
        Check    = 'advancement'
        Detail   = "$changed of 40 points across the overlay band differ between frames ~${AdvanceDelayMs}ms apart"
        Distance = $changed
        Pass     = ($changed -gt 0)
    })
    }

    # Subtitle grid.
    if ($SubtitleProbes) {
        foreach ($p in $SubtitleProbes) {
            if ($p.X -ge $img.Width -or $p.Y -ge $img.Height) {
                $results.Add([pscustomobject]@{
                    Check = "subtitle-$($p.Colour)"; Detail = "probe ($($p.X),$($p.Y)) outside the $($img.Width)x$($img.Height) frame"
                    Distance = -1; Pass = $false
                })
                continue
            }
            $sp = $img.GetPixel($p.X, $p.Y)
            $d = Get-ColourDistance -Pixel $sp -Expected $p.Expected
            $results.Add([pscustomobject]@{
                Check    = "subtitle-$($p.Colour)"
                Detail   = "($($p.X),$($p.Y)) = $($sp.R),$($sp.G),$($sp.B)"
                Distance = $d
                Pass     = ($d -le $Tolerance)
            })
        }
    }
} finally {
    $img.Dispose(); $img2.Dispose()
}

$results | Format-Table -AutoSize @{L='Check';E={$_.Check}},
                                  @{L='Dist';E={$_.Distance}},
                                  @{L='Result';E={if ($_.Pass) {'PASS'} else {'FAIL'}}},
                                  @{L='Detail';E={$_.Detail}} |
    Out-String -Width 200 | Write-Host

$failed = @($results | Where-Object { -not $_.Pass })
Write-Host ("{0}/{1} checks passed" -f ($results.Count - $failed.Count), $results.Count) -ForegroundColor $(if ($failed) { 'Red' } else { 'Green' })
Write-Host "Frames: $($local -join ', ')" -ForegroundColor DarkGray

[pscustomobject]@{
    Frames = $local
    Checks = $results
    Passed = ($failed.Count -eq 0)
}

if ($failed.Count -gt 0) { exit 1 }
