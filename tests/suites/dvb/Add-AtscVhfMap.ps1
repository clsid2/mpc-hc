<#
.SYNOPSIS
    Map ATSC VHF channels onto captures already in the guest's stream library.

.DESCRIPTION
    The capture library is all UHF, so a VHF sweep has nothing to lock onto and
    can only be observed rather than tested. Mapping existing captures onto VHF
    frequencies turns it into a pass or a fail.

    Channels 5 and 6 (79000 and 85000) are the pair that matters. The ATSC plan
    puts a 10 MHz step between channels 4 and 5, so a uniform 6 MHz sweep begun
    at 57000 lands on 81000 and 87000 - both outside the driver's +/-1 MHz
    probe. Only a sweep that follows the channel plan can reach them.

    Channel 3 (63000) is deliberately left unmapped as a control: a plan sweep
    and a uniform sweep both visit it, and both must find nothing there.

    Each frequency gets a different capture, so the services that turn up
    identify which frequency was actually visited. Without that, a sweep that
    lands on the wrong frequency looks the same as one that lands right.

    Undo with Remove-AtscVhfMap.ps1.
#>
[CmdletBinding()]
param(
    # Empty means the whole pool. Changing the stream map changes what a guest
    # IS, so it has to reach all three or they stop being interchangeable.
    # Naming guests explicitly overrides that, for the case where you know
    # better than the default.
    [string[]] $VMName = @(),
    [string] $CredentialPath = ''   # empty: transport/config default
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigClaim.ps1')
$targets = if ($VMName) { $VMName }
           elseif ((Get-TestBedConfig).BrokerPath) { Get-FanOutTargets }
           else { @((Get-TestBedConfig).Guest) }
Write-Host ("fanning out to: " + ($targets -join ', ')) -ForegroundColor DarkCyan

# channel -> frequency in kHz -> capture
$map = @(
    @{ ch =  2; khz =  57000; ts = 'C:\ts\473.ts' }        # virtual channels 10.x
    @{ ch =  4; khz =  69000; ts = 'C:\ts\atsc-515.ts' }   # virtual channel  50.1
    @{ ch =  5; khz =  79000; ts = 'C:\ts\473.ts' }        # decisive
    @{ ch =  6; khz =  85000; ts = 'C:\ts\atsc-515.ts' }   # decisive
    @{ ch =  7; khz = 177000; ts = 'C:\ts\atsc-605.ts' }   # virtual channels 9.x
    @{ ch = 10; khz = 195000; ts = 'C:\ts\473.ts' }
    @{ ch = 13; khz = 213000; ts = 'C:\ts\atsc-515.ts' }
)

foreach ($target in $targets) {
  Write-Host "--- $target ---" -ForegroundColor DarkGray
  $ts = Connect-TestGuest -Guest $target -CredentialPath $CredentialPath
  Invoke-Command -Session $ts -ArgumentList (, $map) {
    param($map)
    $base = 'HKLM:\SYSTEM\PSWTuner\ROOT_MEDIA_0001\Device Parameters'
    foreach ($entry in $map) {
        $key = Join-Path $base $entry.khz
        New-Item -Path $key -Force | Out-Null
        Set-ItemProperty $key -Name StreamLocation -Value ('\??\' + $entry.ts) -Type String
        Set-ItemProperty $key -Name FriendlyName   -Value ('vhf-ch' + $entry.ch) -Type String
        Set-ItemProperty $key -Name SignalLocked   -Value 1   -Type DWord
        Set-ItemProperty $key -Name SignalPresent  -Value 1   -Type DWord
        Set-ItemProperty $key -Name SignalQuality  -Value 100 -Type DWord
        Set-ItemProperty $key -Name SignalStrength -Value 63  -Type DWord
        "mapped channel $($entry.ch) at $($entry.khz) kHz -> $($entry.ts)"
    }
}
}
