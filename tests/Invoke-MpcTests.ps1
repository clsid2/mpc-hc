<#
.SYNOPSIS
    Run the MPC-HC test suites against the build from this repository.

.DESCRIPTION
    The orchestrator. Discovers every suite under suites\ (a suite is a
    directory with an Invoke-Suite.ps1), probes each for readiness, claims a
    test rig once, runs every runnable suite against it, and aggregates the
    results. A suite that is not ready is reported and skipped, never
    silently dropped.

    The suite contract -- what suites\<name>\Invoke-Suite.ps1 implements:

      -Probe        Print a readiness object and do nothing else:
                      @{ Suite; Description; Ready; Reason }
                    Probing is cheap and touches no rig.

      (run)         Parameters -VMName (the claimed guest; empty on a
                    single-target bench), -OutDir (artefact directory,
                    created by the orchestrator), -PlayerBinary (host path
                    of the build under test; optional). Returns
                      @{ Suite; Passed; Failed; Skipped; Notes }
                    and writes whatever evidence it produces to -OutDir.
                    A throw is an infrastructure failure, distinct from
                    test failures.

    The player under test is this repository's own build: by default
    bin\mpc-hc_x64\mpc-hc64.exe if present, else whatever is already
    deployed on the rig (with a warning, since that binary's provenance is
    the rig's history rather than this checkout).

.PARAMETER Suite
    Run only these suites. Default: every suite that probes ready.

.PARAMETER List
    Probe and list the suites, then exit without running anything.

.PARAMETER VMName
    Pin the run to a named guest instead of claiming one from the pool.
    An escape hatch for driving a specific rig by hand.

.EXAMPLE
    .\Invoke-MpcTests.ps1 -List
    .\Invoke-MpcTests.ps1
    .\Invoke-MpcTests.ps1 -Suite dvb
#>
[CmdletBinding()]
param(
    [string[]] $Suite,
    [switch]   $List,
    [string]   $OutDir,
    [string]   $VMName = '',
    [string]   $PlayerBinary
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$TestsRoot = $PSScriptRoot
$RepoRoot  = Split-Path $TestsRoot -Parent

# --- discover ---------------------------------------------------------------

$suiteScripts = Get-ChildItem (Join-Path $TestsRoot 'suites') -Directory |
                ForEach-Object { Join-Path $_.FullName 'Invoke-Suite.ps1' } |
                Where-Object { Test-Path $_ }
if (-not $suiteScripts) { throw "No suites found under $TestsRoot\suites." }

$probes = foreach ($s in $suiteScripts) { & $s -Probe }

if ($Suite) {
    $unknown = $Suite | Where-Object { $_ -notin $probes.Suite }
    if ($unknown) { throw "Unknown suite(s): $($unknown -join ', '). Available: $($probes.Suite -join ', ')" }
    $probes = $probes | Where-Object { $_.Suite -in $Suite }
}

if ($List) {
    $probes | ForEach-Object {
        $state = if ($_.Ready) { 'ready' } else { "not ready -- $($_.Reason)" }
        Write-Host ("  {0,-12} {1}" -f $_.Suite, $state) -ForegroundColor ($(if ($_.Ready) { 'Green' } else { 'Yellow' }))
        Write-Host ("  {0,-12} {1}" -f '', $_.Description) -ForegroundColor DarkGray
    }
    return
}

$runnable = @($probes | Where-Object Ready)
$skipped  = @($probes | Where-Object { -not $_.Ready })
foreach ($p in $skipped) { Write-Host "SKIP $($p.Suite): $($p.Reason)" -ForegroundColor Yellow }
if (-not $runnable) { Write-Warning 'No suite is ready to run.'; exit 1 }

# --- the build under test ---------------------------------------------------

if (-not $PlayerBinary) {
    $candidate = Join-Path $RepoRoot 'bin\mpc-hc_x64\mpc-hc64.exe'
    if (Test-Path $candidate) { $PlayerBinary = $candidate }
}
if ($PlayerBinary) {
    Write-Host "Build under test: $PlayerBinary" -ForegroundColor Cyan
} else {
    Write-Warning 'No built player found (bin\mpc-hc_x64\mpc-hc64.exe); suites will run whatever is already deployed on the rig.'
}

if (-not $OutDir) { $OutDir = Join-Path $TestsRoot ("results\{0:yyyyMMdd-HHmmss}" -f (Get-Date)) }
New-Item -ItemType Directory -Force $OutDir | Out-Null

# --- claim once, run everything ---------------------------------------------

. (Join-Path $TestsRoot 'emulator\tools\RigClaim.ps1')
$claim = Enter-RigClaim -VMName $VMName
Write-Host "rig: $($claim.Guest)" -ForegroundColor DarkGray

$results = [System.Collections.Generic.List[object]]::new()
try {
    foreach ($p in $runnable) {
        Write-Host "`n===== suite: $($p.Suite) =====" -ForegroundColor Cyan
        $suiteOut = Join-Path $OutDir $p.Suite
        New-Item -ItemType Directory -Force $suiteOut | Out-Null
        # A suite that throws is one suite's infrastructure failure, not the
        # run's: record it, keep the other suites running, and still write the
        # summary. A test framework whose own crash swallows every other
        # result is worse than useless.
        try {
            $entry = & (Join-Path $TestsRoot "suites\$($p.Suite)\Invoke-Suite.ps1") `
                         -VMName $claim.Guest -OutDir $suiteOut -PlayerBinary $PlayerBinary
        } catch {
            Write-Host "  suite errored: $($_.Exception.Message)" -ForegroundColor Red
            $entry = [pscustomobject]@{ Suite = $p.Suite; Passed = 0; Failed = 1; Skipped = 0; Notes = @("suite error: $($_.Exception.Message)") }
        }
        $results.Add($entry)
    }
} finally {
    Exit-RigClaim -Claim $claim
}

# --- summarise --------------------------------------------------------------

$summary = [pscustomobject]@{
    when    = (Get-Date).ToString('o')
    build   = $PlayerBinary
    commit  = (git -C $RepoRoot rev-parse HEAD 2>$null)
    rig     = $claim.Guest
    suites  = $results
    skipped = $skipped | ForEach-Object { @{ suite = $_.Suite; reason = $_.Reason } }
}
$summary | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutDir 'summary.json')

Write-Host ''
$failed = 0
foreach ($r in $results) {
    $failed += $r.Failed
    $colour = if ($r.Failed) { 'Red' } else { 'Green' }
    Write-Host ("  {0,-12} passed {1}  failed {2}  skipped {3}" -f $r.Suite, $r.Passed, $r.Failed, $r.Skipped) -ForegroundColor $colour
}
Write-Host "`nResults in $OutDir" -ForegroundColor DarkGray
exit $failed
