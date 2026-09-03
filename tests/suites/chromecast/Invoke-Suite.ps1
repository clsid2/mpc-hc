<#
.SYNOPSIS
    The chromecast suite's entry point for the orchestrator.

.DESCRIPTION
    Scaffold. The tests this suite will run are specified in README.md,
    keyed to the Cast to Device work in clsid2/mpc-hc#4128; they land once
    that PR's surfaces are in the build under test. Until then the suite
    probes not-ready and the orchestrator skips it.
#>
[CmdletBinding()]
param(
    [switch] $Probe,
    [string] $VMName = '',
    [string] $OutDir,
    [string] $PlayerBinary
)

Set-StrictMode -Version Latest

$description = 'Cast sender behaviour against the cast-mock receiver (scaffold; see README.md)'

if ($Probe) {
    return [pscustomobject]@{
        Suite       = 'chromecast'
        Description = $description
        Ready       = $false
        Reason      = 'scaffold -- tests land when the build carries PR #4128 (Cast to Device)'
    }
}

throw 'The chromecast suite has no tests yet; see README.md for what they will assert.'
