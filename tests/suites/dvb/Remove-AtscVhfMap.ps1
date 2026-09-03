<#
.SYNOPSIS
    Undo Add-AtscVhfMap.ps1, leaving the ATSC stream map as provisioned.
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

foreach ($target in $targets) {
  Write-Host "--- $target ---" -ForegroundColor DarkGray
  $ts = Connect-TestGuest -Guest $target -CredentialPath $CredentialPath
  Invoke-Command -Session $ts {
    $base = 'HKLM:\SYSTEM\PSWTuner\ROOT_MEDIA_0001\Device Parameters'
    foreach ($khz in 57000, 69000, 79000, 85000, 177000, 195000, 213000) {
        $key = Join-Path $base $khz
        if (Test-Path $key) { Remove-Item -LiteralPath $key -Recurse -Force; "removed $khz" }
    }
    'stream map now: ' + ((Get-ChildItem $base | Select-Object -ExpandProperty PSChildName) -join ', ')
}
}
