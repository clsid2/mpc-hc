<#
.SYNOPSIS
    Install the guest-side harness and the scheduled task that runs it.

.DESCRIPTION
    Copies harness.ps1 into the guest and registers a scheduled task that runs
    it with an interactive token as the logged-on user.

    The scheduled task is not a convenience. MPC-HC has to run on the
    interactive desktop for its windows to exist somewhere the harness can find
    them: window stations are per-session, so a process started from a
    PowerShell Direct session (session 0) cannot enumerate or message the
    windows of an application on the console session. Running the harness itself
    through an interactive-token task puts the driver and the application in the
    same session.

    The task wraps the harness in a transcript, because a scheduled task that
    fails leaves nothing behind but an exit code.

    Idempotent: re-run it to update harness.ps1 in the guest.

.PARAMETER GuestUser
    The account logged on at the guest console, as DOMAIN\user. Defaults to the
    owner of the interactive session.
#>
[CmdletBinding()]
param(
    # Empty means the whole pool. The harness is part of what a guest IS, so
    # installing it into one clone and not the others makes them diverge.
    [string[]] $VMName = @(),
    [string] $CredentialPath = '',   # empty: transport/config default,
    [string] $GuestUser
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '..\..\emulator\tools\RigClaim.ps1')
$targets = if ($VMName) { $VMName }
           elseif ((Get-TestBedConfig).BrokerPath) { Get-FanOutTargets }
           else { @((Get-TestBedConfig).Guest) }   # no pool: the one bench
Write-Host ("installing to: " + ($targets -join ', ')) -ForegroundColor DarkCyan

# Keep the caller's value, if any. Auto-detection assigns $GuestUser inside the
# loop, and without this the second guest would silently inherit the first
# guest's console user -- registering an interactive task for an account that
# may not be logged on there, which fails at run time rather than here.
$guestUserParam = $GuestUser

foreach ($target in $targets) {
Write-Host "--- $target ---" -ForegroundColor DarkGray
$GuestUser = $guestUserParam
$session = Connect-TestGuest -Guest $target -CredentialPath $CredentialPath
try {
    Invoke-Command -Session $session {
        New-Item -ItemType Directory -Force -Path C:\vtuner, C:\vtuner\out | Out-Null
    }
    Copy-Item "$PSScriptRoot\harness.ps1" -Destination C:\vtuner\harness.ps1 -ToSession $session -Force

    if (-not $GuestUser) {
        $GuestUser = Invoke-Command -Session $session {
            # the console session's owner is the account whose desktop MPC-HC must appear on
            $explorer = Get-CimInstance Win32_Process -Filter "Name='explorer.exe'" | Select-Object -First 1
            if (-not $explorer) { throw 'nobody is logged on at the guest console; log in before installing' }
            $owner = Invoke-CimMethod -InputObject $explorer -MethodName GetOwner
            "$($owner.Domain)\$($owner.User)"
        }
    }

    Invoke-Command -Session $session -ArgumentList $GuestUser {
        param($guestUser)
        $command = '-NoProfile -ExecutionPolicy Bypass -Command "& { ' +
                   'Start-Transcript -Path C:\vtuner\out\harness.log -Force; ' +
                   'try { & C:\vtuner\harness.ps1 } ' +
                   'catch { $_ | Out-String | Write-Host; $_.ScriptStackTrace | Write-Host } ' +
                   'finally { Stop-Transcript } }"'
        Unregister-ScheduledTask -TaskName MpcHarness -Confirm:$false -ErrorAction SilentlyContinue
        Register-ScheduledTask -TaskName MpcHarness `
            -Action    (New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $command) `
            -Principal (New-ScheduledTaskPrincipal -UserId $guestUser -LogonType Interactive -RunLevel Highest) `
            -Settings  (New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 2)) |
            Select-Object TaskName, State
    }
} finally {
    $session | Remove-PSSession
}
}
