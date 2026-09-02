<#
.SYNOPSIS
    Run one MPC-HC tuner scan in the test guest and return the result as JSON.

.DESCRIPTION
    Drives MPC-HC's tuner scan dialog through window messages and returns what
    the scan actually produced: the channel list's raw CBDAChannel records, a
    timestamped progress trace, and the channel list persisted on exit.

    The work runs inside the guest, because window messages do not cross session
    boundaries: a PowerShell Direct session lands in session 0 and cannot see
    the windows of an application on the interactive desktop. Install-MpcHarness
    registers a scheduled task with an interactive token for that reason; this
    script writes the job description, starts that task and waits for its result
    file.

    The tuner device is cycled before every run. The virtual tuner serves
    correctly for the first MPC-HC session after it starts and then stops
    locking, with no outward sign - see the README.

.PARAMETER Job
    Hashtable describing the run. Recognised keys:
      label             name for the result file (required)
      exe               binary to copy over C:\mpc-hc\mpc-hc64.exe before running
      tuner             BDATuner moniker; also selects which device to cycle
      freqStart/freqEnd sweep bounds in kHz
      bandwidth         in kHz; symbolRate in symbols per second
      ignoreEncrypted   0/1, the "Ignore encrypted channels" option
      clearChannels     drop any saved channel list first
      seedChannels      array of CBDAChannel records to plant before launching
      readChannelsOnly  load the seeded list and report it back, do not scan
      timeoutSec        give up on the scan after this long

.EXAMPLE
    .\Invoke-MpcScan.ps1 -Job @{
        label      = 'atsc-uhf'
        exe        = 'C:\vtuner\bin\mpc-hc64.exe'
        tuner      = $atscMoniker
        freqStart  = 473000; freqEnd = 695000
        bandwidth  = 6000;   symbolRate = 0
        ignoreEncrypted = 0; clearChannels = $true
        timeoutSec = 400
    }
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][hashtable] $Job,

    # Empty means "claim one from the pool", which is the normal path. Naming a
    # guest bypasses the broker: an escape hatch for driving a specific rig by
    # hand, not something a batch or a test should do.
    [string] $VMName = '',
    [string] $CredentialPath = '',   # empty: transport/config default,
    [int]    $WaitSec = 900,

    # Same names and defaults as Test-MpcFrame.ps1, so a caller driving both
    # uses one owner and one wait policy rather than two conventions.
    [string] $LockOwner = 'dvbEmulator',
    [ValidateRange(0, 3600)] [int] $LockWaitSeconds = 0,

    # Host path to the binary this run is meant to measure. The pool hands out
    # whichever guest is free and every clone carries its own copy of whatever
    # was deployed when the image was taken, so anything already in the guest
    # describes some earlier run rather than this one. Copying it in as part of
    # the run is what makes the result attributable to a known build.
    [string] $HostBinary
)

$ErrorActionPreference = 'Stop'
$label = $Job.label
if (-not $label) { throw 'Job needs a label' }

# Claim before anything else. A session that scans without claiming is
# invisible to the broker, so the pool can hand the same guest to someone else
# and both runs proceed looking entirely normal.
. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigClaim.ps1')
$claim = Enter-RigClaim -VMName $VMName
$guest = $claim.Guest
Write-Verbose "guest: $guest"

$session = Connect-TestGuest -Guest $guest -CredentialPath $CredentialPath

# Confirm we landed where we think we did, before anything is measured or
# changed. The clones share a computer name, so this is the only thing that can
# tell them apart -- see Assert-RigIdentity.
Assert-RigIdentity -Session $session -Expected $guest | Out-Null

# The guest runs its own copy of harness.ps1, installed separately, and a fix
# to it is inert until that copy is replaced. That is not hypothetical: the
# DefaultCapture stamping added in 706daf2 sat on the host for hours while
# every run used a guest copy predating it, and the symptom was
# "scan dialog never appeared" -- which names neither the stale harness nor
# the missing registry value.
#
# Checked rather than deployed here. The harness is part of what a guest IS,
# so by the split the manager settled it fans out to the whole pool rather
# than being installed onto whichever guest this run happens to hold. Failing
# with the remedy named is the honest half of that.
# Compared on normalised content, not raw bytes. git restores this file with
# CRLF while the tooling that edits it writes LF, so a byte hash reports a
# difference between two identical scripts. A check that cannot tell changed
# content from changed line endings cries wolf, and a check that cries wolf
# gets bypassed -- which is exactly how the rig lock came to be released while
# its run was still alive. A guard nobody believes is worse than no guard.
function Get-ScriptFingerprint {
    param([string] $Text)
    $norm = ($Text -replace "`r`n", "`n").TrimEnd()
    $sha  = [System.Security.Cryptography.SHA256]::Create()
    try {
        ($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($norm)) |
            ForEach-Object { $_.ToString('x2') }) -join ''
    } finally { $sha.Dispose() }
}

$harnessLocal = Get-ScriptFingerprint (Get-Content (Join-Path $PSScriptRoot 'harness.ps1') -Raw)
$harnessGuest = Invoke-Command -Session $session -ScriptBlock {
    if (-not (Test-Path 'C:\vtuner\harness.ps1')) { return $null }
    $norm = ((Get-Content 'C:\vtuner\harness.ps1' -Raw) -replace "`r`n", "`n").TrimEnd()
    $sha  = [System.Security.Cryptography.SHA256]::Create()
    try { ($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($norm)) | ForEach-Object { $_.ToString('x2') }) -join '' }
    finally { $sha.Dispose() }
}
if (-not $harnessGuest) {
    throw "the guest has no C:\vtuner\harness.ps1. Run Install-MpcHarness.ps1, which fans out to the pool."
}
if ($harnessGuest -ne $harnessLocal) {
    throw ("the guest is running a different harness.ps1 from this checkout (guest {0}, local {1}). " +
           "Any fix made here is inert until it is deployed, and the failure it produces will describe " +
           "a symptom rather than the staleness. Run Install-MpcHarness.ps1, which fans out to the " +
           "whole pool because the harness is part of what a guest is." -f
           $harnessGuest.Substring(0,8), $harnessLocal.Substring(0,8))
}

# The clone was imaged from a running guest, so a lock file that was live at
# that moment is now baked into every copy. On a guest the broker has just
# given us exclusively, such a lock cannot be anyone's, and left alone it would
# make the first scan on a fresh rig wait out the stale timeout for a session
# that was never there.
Clear-InheritedRigLock -Session $session -Claim $claim

# Every scan in this project goes through this script, which makes this the
# lock's load-bearing call site. Until now only Test-MpcFrame.ps1 took it, so
# the lock was acquired and honoured correctly all day while the path that
# swaps the binary, stamps the shared profile and cycles the tuner device took
# nothing at all. Concurrent sessions contaminated each other and every
# resulting failure completed or reported plausibly instead of erroring.
#
# Acquire before anything touches guest state -- which means before the device
# cycle below, not after it.
. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigLock.ps1')
$rigLock = Enter-RigLock -Session $session -Owner $LockOwner -Purpose "scan: $label" -WaitSeconds $LockWaitSeconds

try {
    # Every variant is the same driver installed again under its own device
    # instance; the moniker says which one this job is aimed at. Read the
    # instance number out of it rather than listing the known ones, so adding a
    # fifth tuner does not need a change here.
    $instance = if ($Job.tuner -match 'media#(\d{4})') { "ROOT\MEDIA\$($Matches[1])" } else { 'ROOT\MEDIA\0000' }

    # Deploy what this run is meant to measure. Done inside the lock and before
    # the player check, so nothing can be holding the target when we write it.
    if ($HostBinary) {
        if (-not (Test-Path $HostBinary)) { throw "-HostBinary '$HostBinary' does not exist" }
        $hostHash = (Get-FileHash $HostBinary).Hash
        Invoke-Command -Session $session { New-Item -ItemType Directory -Force C:\vtuner\bin | Out-Null }
        Copy-Item -Path $HostBinary -Destination 'C:\vtuner\bin\mpc-hc64.exe' -ToSession $session -Force

        # A copy that silently truncated would be measured as though it were
        # the build under test, so confirm the bytes landed rather than assume.
        $guestHash = Invoke-Command -Session $session { (Get-FileHash 'C:\vtuner\bin\mpc-hc64.exe').Hash }
        if ($guestHash -ne $hostHash) {
            throw "deploy verify failed: host $hostHash, guest $guestHash"
        }
        $Job.exe = 'C:\vtuner\bin\mpc-hc64.exe'
        Write-Verbose "deployed $HostBinary ($($hostHash.Substring(0,8))) to $guest"
    }

    # After any mutation of $Job above, never before it.
    $json = $Job | ConvertTo-Json -Depth 6 -Compress

    Invoke-Command -Session $session -ArgumentList $instance {
        param($instance)

        # Killing the process and the image lock being released are not the
        # same instant, and the deploy in harness.ps1 needs the second one.
        # Unguarded here, a player left behind by an earlier run fails that
        # copy inside the guest, where the only record is the guest-side
        # transcript -- so the caller sees a run that simply did not work.
        # Test the condition the deploy actually needs, that the image can be
        # opened for writing, and fail here in the caller's own output.
        Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

        $target = 'C:\mpc-hc\mpc-hc64.exe'
        if (Test-Path $target) {
            $free = $false
            for ($i = 0; $i -lt 30 -and -not $free; $i++) {
                try {
                    $fs = [System.IO.File]::Open($target, [System.IO.FileMode]::Open,
                                                 [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
                    $fs.Dispose()
                    $free = $true
                } catch {
                    Start-Sleep -Milliseconds 500
                }
            }
            if (-not $free) {
                $holder = (Get-Process mpc-hc64 -ErrorAction SilentlyContinue |
                           ForEach-Object { "pid $($_.Id) started $($_.StartTime.ToString('HH:mm:ss'))" }) -join ', '
                throw ("cannot deploy: $target is still held after 15s" +
                       "$(if ($holder) { " by $holder" }). Holding the rig lock means this is not a " +
                       'live session, so suspect a crashed run that left its player behind.')
            }
        }

        Start-Sleep -Seconds 2
        Disable-PnpDevice -InstanceId $instance -Confirm:$false
        Start-Sleep -Seconds 3
        Enable-PnpDevice -InstanceId $instance -Confirm:$false
        Start-Sleep -Seconds 6
        $device = Get-PnpDevice -InstanceId $instance
        if ($device.Status -ne 'OK') { throw "device $instance is $($device.Status) after cycling" }
    }

    Invoke-Command -Session $session -ArgumentList $json, $label {
        param($json, $label)
        Set-Content C:\vtuner\job.json -Value $json -Encoding UTF8
        Remove-Item "C:\vtuner\out\$label.json" -ErrorAction SilentlyContinue
        Start-ScheduledTask -TaskName MpcHarness
    }

    $deadline = (Get-Date).AddSeconds($WaitSec)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 10
        $state = Invoke-Command -Session $session -ArgumentList $label {
            param($label)
            [pscustomobject]@{
                task = (Get-ScheduledTask -TaskName MpcHarness).State
                done = Test-Path "C:\vtuner\out\$label.json"
            }
        }
        # Prove we are still alive while we wait, so nobody has to judge whether
        # this lock is abandoned.
        Update-RigLock -Session $session -Lock $rigLock
        if ($state.done -and $state.task -eq 'Ready') { break }
    }

    $raw = Invoke-Command -Session $session -ArgumentList $label {
        param($label)
        if (Test-Path "C:\vtuner\out\$label.json") {
            Get-Content "C:\vtuner\out\$label.json" -Raw
        } else {
            throw "no result for $label; task is $((Get-ScheduledTask -TaskName MpcHarness).State)"
        }
    }

    # Stamp the guest into the result. The clones share a computer name, so
    # nothing inside the guest can say which one you were on -- and the pool is
    # only homogeneous until someone provisions a single clone. A new driver or
    # an edited stream map reaches one guest and not the other two, and from
    # then on which rig you drew changes the answer. Recording it here is what
    # makes that visible in a result rather than in a week of confusion.
    try {
        $obj = $raw | ConvertFrom-Json
        $obj | Add-Member -NotePropertyName rigGuest -NotePropertyValue $guest -Force
        if ($HostBinary) {
            $obj | Add-Member -NotePropertyName deployedFrom -NotePropertyValue $HostBinary -Force
            $obj | Add-Member -NotePropertyName deployedHash -NotePropertyValue $hostHash -Force
        }
        $obj | ConvertTo-Json -Depth 8
    } catch {
        # An unparseable result is still the caller's to see; do not lose it
        # trying to annotate it.
        Write-Warning "could not stamp rigGuest onto the result: $($_.Exception.Message)"
        $raw
    }
} finally {
    if ($session -and $session.State -eq 'Opened') {
        Exit-RigLock -Session $session -Lock $rigLock
    }
    $session | Remove-PSSession
    # A claim held past the work is a guest nobody else can have.
    Exit-RigClaim -Claim $claim
}
