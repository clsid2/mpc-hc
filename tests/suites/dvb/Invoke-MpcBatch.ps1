<#
.SYNOPSIS
    Run a sequence of MPC-HC scans, and the rig changes between them, from one
    plan file.

.DESCRIPTION
    A plan is a JSON array. Each element is either a job for Invoke-MpcScan or a
    rig action taken between jobs:

        { "hostAction": "vhf-add" }      map ATSC VHF channels onto real captures
        { "hostAction": "vhf-remove" }   put the stream map back

    Comparisons only mean something when both runs see the same rig, so a batch
    is the unit to think in: build the "before" binary and the "after" binary,
    run them back to back, and change the rig only where the plan says so.

    Each result is written to -OutDir as <label>.json.

.EXAMPLE
    .\Invoke-MpcBatch.ps1 -Plan .\plans\atsc-channel-plan.json -OutDir .\results
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $Plan,
    [string] $OutDir = (Join-Path $PSScriptRoot 'results'),
    # Empty claims one guest for the whole plan; see the claim block below.
    [string] $VMName = '',
    [string] $CredentialPath = '',   # empty: transport/config default,

    # Passed through to Invoke-MpcScan. A batch is the case most likely to meet
    # contention, because it is the longest thing anyone runs, so a caller
    # wants to be able to say "wait" rather than lose the whole plan to one
    # busy moment. Default stays 0 to match the single-scan behaviour.
    [string] $LockOwner = 'dvbEmulator',
    [ValidateRange(0, 3600)] [int] $LockWaitSeconds = 0
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$jobs = Get-Content $Plan -Raw | ConvertFrom-Json

# Claim once for the whole plan rather than per job. The jobs in a plan are
# compared against each other, so a guest swapped underneath them mid-plan
# invalidates the comparison rather than one result. This also settles the
# lock-scope question the per-job rig lock raised: the claim is what provides
# exclusivity now, and it has no stale timeout to trip on a long plan.
. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigClaim.ps1')
$claim = Enter-RigClaim -VMName $VMName
$guest = $claim.Guest
Write-Host "rig: $guest" -ForegroundColor DarkGray

try {
$clearSession = Connect-TestGuest -Guest $guest -CredentialPath $CredentialPath
try { Clear-InheritedRigLock -Session $clearSession -Claim $claim } finally { Remove-PSSession $clearSession }

foreach ($entry in $jobs) {
    if ($entry.hostAction) {
        Write-Host "--- $($entry.hostAction) ---" -ForegroundColor DarkCyan
        switch ($entry.hostAction) {
            'vhf-add'    { & "$PSScriptRoot\Add-AtscVhfMap.ps1"    -VMName $guest -CredentialPath $CredentialPath }
            'vhf-remove' { & "$PSScriptRoot\Remove-AtscVhfMap.ps1" -VMName $guest -CredentialPath $CredentialPath }
            default      { throw "unknown hostAction '$($entry.hostAction)'" }
        }
        continue
    }

    Write-Host "=== $($entry.label) ===" -ForegroundColor Cyan
    $job = @{}
    $entry.PSObject.Properties | Where-Object { $_.Name -ne 'waitSec' } | ForEach-Object { $job[$_.Name] = $_.Value }
    $waitSec = if ($entry.waitSec) { [int]$entry.waitSec } else { 900 }

    # -VMName pins the child to the guest this plan claimed, so it reuses the
    # claim instead of taking one of its own.
    $result = & "$PSScriptRoot\Invoke-MpcScan.ps1" -Job $job -VMName $guest -CredentialPath $CredentialPath -WaitSec $waitSec `
                  -LockOwner $LockOwner -LockWaitSeconds $LockWaitSeconds
    $result | Set-Content (Join-Path $OutDir "$($entry.label).json")

    $parsed = $result | ConvertFrom-Json
    $found  = @($parsed.numbers | Where-Object { $_ }).Count
    Write-Host ("    {0} channels in {1}s" -f $found, $parsed.scanSeconds)
}
}
finally {
    # One claim for the plan, released once the plan is done or has thrown.
    Exit-RigClaim -Claim $claim
}
