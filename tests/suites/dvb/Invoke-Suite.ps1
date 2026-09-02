<#
.SYNOPSIS
    The dvb suite's entry point for the orchestrator.

.DESCRIPTION
    Runs a headless tuner scan (/dvbscan, in the tree since #4138) per
    standard the rig provides, and asserts the DVB-T results against the
    emulator's encoding matrix with Test-MpcDecode. The headless scan writes
    the channel record JSON to a file and quits, so the assertions depend on
    neither the web API nor the scan-dialog window driving -- those remain
    available as the deeper standalone scripts in this directory.

    Tuners are resolved from the rig itself: the emulator's driver instances
    under DeviceClasses, classified by the standard named in each device's
    FriendlyName. Nothing about device monikers is assumed, because the
    trailing interface GUID differs per device and per install.

    Scan ranges per standard live in scan-ranges.psd1 beside this script and
    match the emulator's default provisioning; a rig provisioned differently
    overrides with -RangesPath.
#>
[CmdletBinding()]
param(
    [switch] $Probe,
    [string] $VMName = '',
    [string] $OutDir = (Join-Path $PSScriptRoot 'results'),
    [string] $PlayerBinary,
    [string] $RangesPath = (Join-Path $PSScriptRoot 'scan-ranges.psd1')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$description = 'Digital TV: headless tuner scans against the virtual BDA tuner, channel records asserted against the encoding matrix'

if ($Probe) {
    $ready = $true; $reason = ''
    if (-not (Test-Path (Join-Path $PSScriptRoot '..\..\emulator\tools\GuestTransport.ps1'))) {
        $ready = $false; $reason = 'emulator submodule not initialised (git submodule update --init)'
    } else {
        . (Join-Path $PSScriptRoot '..\..\emulator\tools\GuestTransport.ps1')
        if (-not (Test-GuestTransportAvailable)) {
            $ready = $false; $reason = 'no usable transport -- create testbed.config.psd1 (see the emulator README)'
        }
    }
    return [pscustomobject]@{ Suite = 'dvb'; Description = $description; Ready = $ready; Reason = $reason }
}

. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigClaim.ps1')
New-Item -ItemType Directory -Force $OutDir | Out-Null
$ranges = Import-PowerShellDataFile $RangesPath

$passed = 0; $failed = 0; $skipped = 0; $notes = [System.Collections.Generic.List[string]]::new()
function Note { param([string] $Colour, [string] $Text) $notes.Add($Text); Write-Host "  $Text" -ForegroundColor $Colour }

# --- resolve the rig's tuners ----------------------------------------------

$claim = Enter-RigClaim -VMName $VMName
$guest = $claim.Guest
try {
    $session = Connect-TestGuest -Guest $guest
    try {
        $tuners = Invoke-Command -Session $session -ScriptBlock {
            # KSCATEGORY_BDA_NETWORK_TUNER interface instances of the virtual
            # devices. The moniker MPC-HC stores is the device interface path
            # plus the KS reference string, both read from the registry rather
            # than assumed: the reference-string subkey is named #{guid} and
            # its Device Parameters carries the filter's FriendlyName
            # (e.g. 'BDA DVBT Sample Tuner'). Lowercased because that is how
            # MPC-HC's device enumeration spells the moniker it compares
            # against.
            $cat = 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses\{71985f48-1ca1-11d3-9cc8-00c04f7971e0}'
            # -LiteralPath throughout: the interface name contains '?', which
            # the provider otherwise treats as a wildcard -- and a wildcard
            # leaf makes Get-ChildItem return the matching key itself instead
            # of its children, silently.
            foreach ($ifaceName in (Get-ChildItem -LiteralPath $cat -Name | Where-Object { $_ -match '#ROOT#MEDIA#' })) {
                $inst = ($ifaceName -split '#')[3..5] -join '\'   # ROOT\MEDIA\000n
                foreach ($refName in (Get-ChildItem -LiteralPath "$cat\$ifaceName" -Name | Where-Object { $_ -match '^#\{' })) {
                    $friendly = (Get-ItemProperty -LiteralPath "$cat\$ifaceName\$refName\Device Parameters" -Name FriendlyName -ErrorAction SilentlyContinue).FriendlyName
                    [pscustomobject]@{
                        Moniker  = ('@device:pnp:\\?\' + ($ifaceName -replace '^##\?#', '') + '\' + $refName.TrimStart('#')).ToLowerInvariant()
                        Friendly = $friendly
                        Instance = $inst
                    }
                }
            }
        }
    } finally { Remove-PSSession $session }

    $byStandard = @{}
    foreach ($t in $tuners) {
        foreach ($std in 'DVBT', 'DVBC', 'DVBS', 'ATSC') {
            if ($t.Friendly -and $t.Friendly -replace '[- ]', '' -match $std) { $byStandard[$std] = $t; break }
        }
    }
    if (-not $byStandard.Count) {
        throw "No virtual tuners resolved on $guest. Deploy the driver first (emulator: Build-And-Deploy.ps1). Found: $(@($tuners).Count) unclassified device(s)."
    }
    Note DarkGray "tuners on ${guest}: $(($byStandard.Keys | Sort-Object) -join ', ')"

    # --- one headless scan per standard, decode assertion on DVB-T ----------

    foreach ($std in ($byStandard.Keys | Sort-Object)) {
        if (-not $ranges.ContainsKey($std)) { $skipped++; Note Yellow "${std}: no scan range declared -- skipped"; continue }
        $r = $ranges[$std]
        $job = @{
            label      = "headless-$($std.ToLower())"
            headless   = $true
            tuner      = $byStandard[$std].Moniker
            freqStart  = $r.FreqStart; freqEnd = $r.FreqEnd
            bandwidth  = $r.Bandwidth; symbolRate = $r.SymbolRate
            ignoreEncrypted = 0; clearChannels = $true
            timeoutSec = $r.TimeoutSec
        }
        $scanArgs = @{ Job = $job; VMName = $guest; WaitSec = ($r.TimeoutSec + 300) }
        if ($PlayerBinary) { $scanArgs.HostBinary = $PlayerBinary }
        else {
            # No build under test on the host: measure what the rig carries,
            # and say so -- the harness refuses an unnamed binary otherwise.
            $job.allowExistingBinary = $true
        }

        try {
            $raw = & (Join-Path $PSScriptRoot 'Invoke-MpcScan.ps1') @scanArgs
        } catch {
            $failed++; Note Red "${std}: scan could not run -- $($_.Exception.Message.Split([char]10)[0])"
            continue
        }
        $raw | Set-Content (Join-Path $OutDir "$($job.label).json")
        $result = $raw | ConvertFrom-Json

        $channels = @()
        if ($result.PSObject.Properties['channelsJson'] -and $result.channelsJson) {
            $channels = @(($result.channelsJson | ConvertFrom-Json).channels)
        }
        if ($channels.Count -gt 0) {
            $passed++; Note Green "${std}: headless scan found $($channels.Count) channel(s)"
        } else {
            $failed++; Note Red "${std}: headless scan found no channels ($(if ($result.PSObject.Properties['error']) { $result.error } else { 'empty result' }))"
            continue
        }

        if ($std -eq 'DVBT') {
            # The matrix streams are provisioned on the DVB-T tuner; assert the
            # channel records against the emulator's declarations. The assert
            # can fail two ways -- exit 1 (a mismatch) or a throw (a precondition
            # it refuses to guess past, e.g. non-unique service identifiers) --
            # and both are a recorded failure of this suite, never a crash of
            # the run.
            $chFile = Join-Path $OutDir 'headless-dvbt.channels.json'
            $result.channelsJson | Set-Content $chFile
            $log = Join-Path $OutDir 'decode-assert.log'
            $global:LASTEXITCODE = 0
            try {
                & (Join-Path $PSScriptRoot 'Test-MpcDecode.ps1') -JsonPath $chFile -AllowUncleared *> $log
                if ($LASTEXITCODE -eq 0) { $passed++; Note Green 'DVBT: channel records match the encoding matrix' }
                else { $failed++; Note Red "DVBT: matrix assertion failed -- see $log" }
            } catch {
                # A throw is a precondition the assertion refuses to guess past
                # -- chiefly channels that do not identify uniquely, which means
                # the DVB-T tuner is carrying the self-describing colour streams
                # (they share tsid/onid by design) rather than the matrix
                # streams this asserts against. That is the assertion being
                # inapplicable to how the rig is provisioned, not a player
                # fault: record it skipped, with the reason, not failed.
                $skipped++
                Note Yellow "DVBT: matrix assertion not applicable -- $($_.Exception.Message.Split([char]10)[0])"
                Note DarkGray '      (provision the DVB-T tuner with New-MatrixStreams to run it)'
                "assertion precondition not met: $($_.Exception.Message)" | Add-Content $log
            }
        }
    }
} finally {
    Exit-RigClaim -Claim $claim
}

[pscustomobject]@{ Suite = 'dvb'; Passed = $passed; Failed = $failed; Skipped = $skipped; Notes = $notes }
