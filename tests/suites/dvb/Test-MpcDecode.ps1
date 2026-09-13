<#
.SYNOPSIS
    Assert what MPC-HC decoded against encoding-matrix.psd1.

.DESCRIPTION
    The sibling of Test-StreamMatrix.ps1, and deliberately the same shape.
    Test-StreamMatrix asserts what we generated; this asserts what MPC-HC saw.
    Both read one matrix, so a disagreement between them is diagnostic rather
    than merely a failure:

        both pass                 the branch under test decodes correctly
        generator passes, this
          fails                   MPC-HC is wrong about a stream we proved
                                  is correct -- a player bug
        generator fails           nothing here means anything; fix the stream
                                  first

    Until now the harness read ToString() records out of a hidden listview
    column in the scan dialog. /dvb/channels.json exists precisely so it does
    not have to, and this is its first consumer.

    THE ENUM MAPPING

    The two vocabularies do not match: the matrix says BDA_MPV, the JSON says
    "MPEG-Video". The mapping lives in BdaRenderMap.ps1, in the harness, never
    in ToJSON -- /dvb/channels.json is a public web interface and its consumers
    should not need MPC-HC's enum spellings. An unmapped name throws there
    rather than returning empty, because an empty string compares equal to
    every field MPC-HC leaves blank.

    READING THE ENDPOINT

    Four things about it, each of which produced a wrong answer before it was
    understood:

      127.0.0.1, not localhost. The server binds IPv4 0.0.0.0 only, and
      localhost resolves to ::1 first, so a request hits the client timeout
      rather than being refused.

      { "channels" : [] } is a valid 200 whenever m_DVBChannels is empty,
      which is the whole of a scan before Save commits. Poll until the array
      is non-empty, never until the first 200.

      It 503s outside PM_DIGITAL_CAPTURE, so it can only be read from a tuned
      player. Reading during the scan run works.

      Settings\DefaultCapture must be 1 or MPC-HC opens the capture options
      page instead of the device, and the harness reports "scan dialog never
      appeared" -- which names the symptom, not the cause.

    PROVENANCE

    A result whose channel list was not cleared is refused. ParsePMT resets
    only fps and chroma; width, height and aspect ratio survive from
    FromString on a channel loaded from the saved list, so a reading taken
    from a re-tuned saved channel is partly memory rather than measurement.

.PARAMETER JsonPath
    A captured /dvb/channels.json to assert offline. Mutually exclusive with
    -FromGuest.

.PARAMETER FromGuest
    Claim a rig and read the endpoint from the tuned player on it.

.EXAMPLE
    .\Test-MpcDecode.ps1 -JsonPath .\results\scan.json
    .\Test-MpcDecode.ps1 -SelfTest
#>
[CmdletBinding(DefaultParameterSetName = 'File')]
param(
    [Parameter(ParameterSetName = 'File', Mandatory)]
    [string] $JsonPath,

    [Parameter(ParameterSetName = 'Guest', Mandatory)]
    [switch] $FromGuest,

    [Parameter(ParameterSetName = 'SelfTest', Mandatory)]
    [switch] $SelfTest,

    # The streams and their declarations are the emulator's; this asserts
    # MPC-HC against them. Default: the pinned emulator submodule.
    [string]   $MatrixPath = (Join-Path $PSScriptRoot '..\..\emulator\tools\encoding-matrix.psd1'),
    [string[]] $EntryId,

    [string] $VMName = '',
    [string] $CredentialPath = '',   # empty: transport/config default,
    [int]    $PollSeconds = 120,
    [switch] $AllowUncleared
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'BdaRenderMap.ps1')

# --- reading the endpoint --------------------------------------------------

function Get-DvbChannelsJson {
    <#
    .SYNOPSIS
        Read /dvb/channels.json off a tuned player, waiting for it to be real.
    #>
    param($Session, [int] $TimeoutSeconds)

    Invoke-Command -Session $Session -ArgumentList $TimeoutSeconds -ScriptBlock {
        param($timeout)
        # 127.0.0.1: the server binds IPv4 0.0.0.0 only and localhost resolves
        # to ::1 first, which times out rather than being refused.
        $url = 'http://127.0.0.1:13579/dvb/channels.json'
        $deadline = (Get-Date).AddSeconds($timeout)
        $last = ''
        while ((Get-Date) -lt $deadline) {
            try {
                $r = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
                $last = $r.Content
                # An empty array is a valid 200 for the whole of a scan before
                # Save commits, so the first 200 is not the signal.
                $obj = $last | ConvertFrom-Json
                if ($obj.channels -and @($obj.channels).Count -gt 0) { return $last }
            } catch {
                # 503 outside PM_DIGITAL_CAPTURE: not an error, just not tuned yet.
                $last = "request failed: $($_.Exception.Message)"
            }
            Start-Sleep -Seconds 2
        }
        throw "no non-empty channel list within $timeout s. Last response: $last"
    }
}

# --- assertions ------------------------------------------------------------

function Get-JsonField {
    # Under Set-StrictMode -Version Latest, reading a property that is not there
    # throws PropertyNotFoundException. On a capture taken with a binary
    # predating a field -- chroma and pes are both recent -- that error names
    # the property and not the cause, so the caller cannot tell "MPC-HC decoded
    # this wrongly" from "MPC-HC has no such field yet". Returns a sentinel so
    # the difference can be reported.
    param($Object, [string] $Name)
    if ($null -eq $Object) { return $null }
    $prop = $Object.PSObject.Properties[$Name]
    if ($null -eq $prop) { return '<field-absent>' }
    $prop.Value
}

function Compare-Field {
    param([string] $Label, $Expected, $Actual)
    if ($null -eq $Expected) { return }          # nothing declared, nothing to check
    if ("$Actual" -eq '<field-absent>') {
        return ("$Label expected '$Expected' but the capture has no such field at all. The binary that " +
                "produced it predates the field; this is not a decode error.")
    }
    if ("$Expected" -ne "$Actual") {
        "$Label expected '$Expected', decoded '$Actual'"
    }
}

function Test-ChannelAgainstEntry {
    <#
    .SYNOPSIS
        Every problem with one channel, or nothing when it decoded as declared.
    #>
    param($Channel, $Entry, $Defaults)

    $problems = @()

    $problems += Compare-Field 'name' $Entry.ServiceName $Channel.name
    foreach ($pair in @(
        @{ L = 'tsid'; E = 'TransportStreamId' },
        @{ L = 'onid'; E = 'OriginalNetworkId' })) {
        $want = if ($Entry.ContainsKey($pair.E)) { $Entry[$pair.E] } elseif ($Defaults.ContainsKey($pair.E)) { $Defaults[$pair.E] } else { $null }
        $problems += Compare-Field $pair.L $want $Channel.$($pair.L)
    }

    $expect = $Entry.Expect

    # --- video -------------------------------------------------------------
    $video = Get-JsonField $Channel 'video'
    $videoStream = @($expect.Streams | Where-Object { $_.Role -eq 'video' })
    if ($videoStream.Count) {
        $problems += Compare-Field 'video.type' (ConvertTo-BdaWireValue -Field Bda -EnumName $videoStream[0].Bda) (Get-JsonField $video 'type')
    }
    if ($expect.ContainsKey('Video') -and $expect.Video) {
        $v = $expect.Video
        foreach ($f in 'Width', 'Height') {
            if ($v.ContainsKey($f)) { $problems += Compare-Field "video.$($f.ToLower())" $v[$f] (Get-JsonField $video $f.ToLower()) }
        }
        foreach ($pair in @(
            @{ K = 'Fps';         J = 'fps' },
            @{ K = 'Chroma';      J = 'chroma' },
            @{ K = 'AspectRatio'; J = 'aspectRatio' })) {
            if (-not $v.ContainsKey($pair.K)) { continue }
            if ($null -eq $v[$pair.K]) { continue }
            # Empty string is a real expected value, not a missing one -- which
            # is why the absent-field sentinel has to be distinct from it.
            $want = ConvertTo-BdaWireValue -Field $pair.K -EnumName $v[$pair.K]
            $problems += Compare-Field "video.$($pair.J)" $want (Get-JsonField $video $pair.J)
        }
    }

    # --- audio and subtitles ------------------------------------------------
    #
    # Matching CONSUMES. An earlier version searched the decoded list afresh for
    # every declaration and removed nothing, so two declarations sharing a type
    # and language both matched the same decoded stream and neither reported a
    # problem. That is not hypothetical: a real capture carries C8 with two
    # 'fre' DVB-Subtitle streams on pids 140 and 141, and CSTAR likewise. Declare
    # two, have MPC-HC decode one, and the dropped one passed silently -- the
    # count check could not save it either, since one decoded is not more than
    # two declared.
    #
    # Consuming also gives invention detection for free, and properly. Comparing
    # counts could only say "more arrived than expected"; it could not notice
    # that declaring A and B while decoding A and C balances. Whatever is left in
    # the pool after every declaration has taken its match is, by construction,
    # something MPC-HC produced that nothing asked for.
    foreach ($role in 'audio', 'subtitle') {
        $wanted  = @($expect.Streams | Where-Object { $_.Role -eq $role })
        $jsonKey = if ($role -eq 'audio') { 'audio' } else { 'subtitles' }
        $pool    = [System.Collections.ArrayList]@(@($Channel.$jsonKey))

        foreach ($w in $wanted) {
            # BDA_UNKNOWN is an absence assertion: ConvertToDVBType rejected the
            # stream and AddStreamInfo was never called, so it must NOT appear.
            # It consumes nothing -- there is nothing to consume -- and it must
            # not count towards the declared streams either, or it would raise
            # the bar an invented stream has to clear.
            if ($w.Bda -eq 'BDA_UNKNOWN') {
                $present = @($pool | Where-Object { (Get-JsonField $_ 'pes') -eq $w.StreamType })
                if ($present.Count) {
                    $problems += ("$role stream_type 0x{0:X2} was expected to be dropped entirely but appears " +
                                  "as pes {1}. That means this build classifies it -- see the entry's " +
                                  "GapDiscriminator.") -f $w.StreamType, (Get-JsonField $present[0] 'pes')
                    # Consume it, or it is also reported as an invention below.
                    $pool.Remove($present[0])
                }
                continue
            }

            $wantType = ConvertTo-BdaWireValue -Field Bda -EnumName $w.Bda
            $wantLang = if ($w.ContainsKey('Language')) { $w.Language } else { $null }

            # Prefer a candidate whose pes also matches, so that when several
            # streams share a type and language the one actually described is
            # taken rather than an arbitrary sibling.
            $candidates = @($pool | Where-Object {
                (Get-JsonField $_ 'type') -eq $wantType -and
                (-not $wantLang -or (Get-JsonField $_ 'language') -eq $wantLang) })

            if (-not $candidates.Count) {
                $langNote = if ($wantLang) { " in '$wantLang'" } else { '' }
                $problems += "no $role stream decoded as '$wantType'$langNote (declared stream_type 0x{0:X2})" -f $w.StreamType
                continue
            }

            $chosen = $null
            if ($w.ContainsKey('StreamType')) {
                $chosen = @($candidates | Where-Object { (Get-JsonField $_ 'pes') -eq $w.StreamType }) | Select-Object -First 1
            }
            if (-not $chosen) {
                $chosen = $candidates[0]
                # pes carries the PMT stream_type after any overriding
                # descriptor, and is the only thing separating branches that
                # share a BDA type.
                if ($w.ContainsKey('StreamType')) {
                    $problems += ("$role '$wantType' decoded with pes {0}, declared stream_type 0x{1:X2} ({1}). " +
                                  "A descriptor may have overridden it.") -f (Get-JsonField $chosen 'pes'), $w.StreamType
                }
            }
            $pool.Remove($chosen)
        }

        # Whatever is left was decoded and never declared. A stream MPC-HC
        # invents is as wrong as one it drops, and now it is named rather than
        # inferred from a count.
        foreach ($extra in $pool) {
            $problems += ("undeclared $role stream decoded: type '{0}' pes {1} language '{2}' on pid {3}") -f
                (Get-JsonField $extra 'type'), (Get-JsonField $extra 'pes'),
                (Get-JsonField $extra 'language'), (Get-JsonField $extra 'pid')
        }
    }

    $problems
}

# --- self test -------------------------------------------------------------

if ($SelfTest) {
    # Honest about what this proves. Building the expected JSON from the matrix
    # and checking it passes is close to circular -- it shows the comparison is
    # self-consistent and little else. The mutations are the part that matters:
    # they prove each assertion can fail, which is the only reason to trust a
    # validator that has never seen a real decode.
    $matrix   = Import-PowerShellDataFile $MatrixPath
    $defaults = $matrix.Defaults
    $entry    = $matrix.Entries | Where-Object { $_.Id -eq 'mpv-video-stream-descriptor-422' }
    if (-not $entry) { throw 'self-test needs entry mpv-video-stream-descriptor-422' }

    $good = [pscustomobject]@{
        name  = $entry.ServiceName
        sid   = $entry.ServiceId
        tsid  = $defaults.TransportStreamId
        onid  = $defaults.OriginalNetworkId
        video = [pscustomobject]@{
            type = 'MPEG-Video'; width = 704; height = 576
            fps = '50'; aspectRatio = ''; chroma = '4:2:2'
        }
        audio     = @([pscustomobject]@{ type = 'MPEG-Audio'; language = 'eng'; pes = 3; pid = 257; default = $true })
        subtitles = @()
    }

    $cases = @(
        @{ Name = 'a correct decode passes'; Mutate = { param($c) $c }; Expect = 0 }
        @{ Name = 'wrong chroma fails';       Mutate = { param($c) $c.video.chroma = '4:2:0'; $c }; Expect = 1 }
        @{ Name = 'wrong fps fails';          Mutate = { param($c) $c.video.fps = '25'; $c };      Expect = 1 }
        @{ Name = 'empty chroma fails';       Mutate = { param($c) $c.video.chroma = ''; $c };     Expect = 1 }
        @{ Name = 'wrong video type fails';   Mutate = { param($c) $c.video.type = 'H264'; $c };   Expect = 1 }
        @{ Name = 'wrong service name fails'; Mutate = { param($c) $c.name = 'nope'; $c };         Expect = 1 }
        @{ Name = 'missing audio fails';      Mutate = { param($c) $c.audio = @(); $c };           Expect = 1 }
        @{ Name = 'wrong audio language fails'; Mutate = { param($c) $c.audio[0].language = 'fra'; $c }; Expect = 1 }
        @{ Name = 'wrong audio pes fails';    Mutate = { param($c) $c.audio[0].pes = 4; $c };      Expect = 1 }
        @{ Name = 'an invented extra audio fails'; Mutate = { param($c)
              $c.audio = @($c.audio[0], [pscustomobject]@{ type='AC3'; language='deu'; pes=129; pid=258; default=$false }); $c }; Expect = 1 }
    )

    # The single-channel fixture above has one audio and no subtitles, so it
    # cannot reach multi-stream matching at all -- which is exactly why three
    # real defects survived it. These use a synthetic entry with duplicate
    # languages, the shape a real capture actually has: C8 carries two 'fre'
    # DVB-Subtitle streams, and CSTAR likewise.
    $dupEntry = @{
        Id = 'synthetic-dup'; ServiceName = 'DUP'; ServiceId = 0x0999
        Expect = @{ Streams = @(
            @{ Role = 'video';    StreamType = 0x02; Bda = 'BDA_MPV';      Pes = 'VIDEO_STREAM_MPEG2' }
            @{ Role = 'subtitle'; StreamType = 0x06; Bda = 'BDA_SUBTITLE'; Language = 'fre' }
            @{ Role = 'subtitle'; StreamType = 0x06; Bda = 'BDA_SUBTITLE'; Language = 'fre' }
        )}
    }
    $dupGood = [pscustomobject]@{
        name = 'DUP'; sid = 0x0999; tsid = $defaults.TransportStreamId; onid = $defaults.OriginalNetworkId
        video = [pscustomobject]@{ type = 'MPEG-Video' }
        audio = @()
        subtitles = @(
            [pscustomobject]@{ type='DVB-Subtitle'; language='fre'; pes=6; pid=140; default=$false },
            [pscustomobject]@{ type='DVB-Subtitle'; language='fre'; pes=6; pid=141; default=$false })
    }

    # An entry declaring a stream that must be dropped, plus one that must not.
    $gapEntry = @{
        Id = 'synthetic-gap'; ServiceName = 'GAP'; ServiceId = 0x0998
        Expect = @{ Streams = @(
            @{ Role = 'video'; StreamType = 0x1B; Bda = 'BDA_H264'; Pes = 'VIDEO_STREAM_H264' }
            @{ Role = 'audio'; StreamType = 0x87; Bda = 'BDA_UNKNOWN'; Pes = 'INVALID'; Language = 'eng' }
        )}
    }
    $gapGood = [pscustomobject]@{
        name = 'GAP'; sid = 0x0998; tsid = $defaults.TransportStreamId; onid = $defaults.OriginalNetworkId
        video = [pscustomobject]@{ type = 'H264' }
        audio = @(); subtitles = @()
    }

    $multiCases = @(
        @{ Name = 'two same-language subtitles, both present, passes'
           Entry = $dupEntry; Good = $dupGood; Mutate = { param($c) $c }; Expect = 0 }
        @{ Name = 'two declared, one decoded -- the dropped one must fail'
           Entry = $dupEntry; Good = $dupGood
           Mutate = { param($c) $c.subtitles = @($c.subtitles[0]); $c }; Expect = 1 }
        @{ Name = 'declare A and B, decode A and C -- both the miss and the invention'
           Entry = $dupEntry; Good = $dupGood
           Mutate = { param($c)
               $c.subtitles = @($c.subtitles[0], [pscustomobject]@{ type='DVB-Subtitle'; language='nld'; pes=6; pid=142; default=$false })
               $c }; Expect = 2 }
        @{ Name = 'a dropped stream stays dropped, passes'
           Entry = $gapEntry; Good = $gapGood; Mutate = { param($c) $c }; Expect = 0 }
        @{ Name = 'a stream declared dropped that appears must fail'
           Entry = $gapEntry; Good = $gapGood
           Mutate = { param($c) $c.audio = @([pscustomobject]@{ type='EAC3'; language='eng'; pes=135; pid=258; default=$true }); $c }
           Expect = 1 }
        @{ Name = 'an absence assertion must not mask an invented stream'
           Entry = $gapEntry; Good = $gapGood
           Mutate = { param($c) $c.audio = @([pscustomobject]@{ type='AC3'; language='deu'; pes=129; pid=259; default=$true }); $c }
           Expect = 1 }
        @{ Name = 'a capture with no chroma field says so, not "wrong value"'
           Entry = $entry; Good = $good
           Mutate = { param($c) $c.video.PSObject.Properties.Remove('chroma'); $c }
           Expect = 1; Match = 'no such field' }
    )

    $failed = 0
    foreach ($case in ($cases + $multiCases)) {
        $source   = if ($case.ContainsKey('Good'))  { $case.Good }  else { $good }
        $target   = if ($case.ContainsKey('Entry')) { $case.Entry } else { $entry }
        $copy     = $source | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        $subject  = & $case.Mutate $copy
        $problems = @(Test-ChannelAgainstEntry -Channel $subject -Entry $target -Defaults $defaults)
        $ok = if ($case.Expect -eq 0) { $problems.Count -eq 0 } else { $problems.Count -ge $case.Expect }
        if ($ok -and $case.ContainsKey('Match')) {
            $ok = [bool]@($problems | Where-Object { $_ -like "*$($case.Match)*" }).Count
        }
        if ($ok) { Write-Host ("  PASS  " + $case.Name) -ForegroundColor Green }
        else {
            Write-Host ("  FAIL  {0} -- {1} problem(s): {2}" -f $case.Name, $problems.Count, ($problems -join '; ')) -ForegroundColor Red
            $failed++
        }
    }
    if ($failed) { throw "$failed self-test case(s) failed; this validator does not do what it claims" }
    Write-Host 'self-test passed' -ForegroundColor Green
    return
}

# --- gather ----------------------------------------------------------------

$matrix   = Import-PowerShellDataFile $MatrixPath
$defaults = $matrix.Defaults

Test-BdaRenderMap -MatrixPath $MatrixPath

if ($FromGuest) {
    . (Join-Path $PSScriptRoot '..\..\emulator\tools\RigClaim.ps1')
    $claim = Enter-RigClaim -VMName $VMName
    try {
        $session = Connect-TestGuest -Guest $claim.Guest -CredentialPath $CredentialPath
        try {
            Assert-RigIdentity -Session $session -Expected $claim.Guest | Out-Null
            Write-Host "reading /dvb/channels.json from $($claim.Guest) ..." -ForegroundColor Cyan
            $raw = Get-DvbChannelsJson -Session $session -TimeoutSeconds $PollSeconds
        } finally { Remove-PSSession $session }
    } finally { Exit-RigClaim -Claim $claim }
}
else {
    $raw     = Get-Content -LiteralPath $JsonPath -Raw
    $wrapper = $raw | ConvertFrom-Json
    $names   = $wrapper.PSObject.Properties.Name

    # Two shapes arrive here. A harness result carries the endpoint body in
    # channelsJson alongside its provenance; a bare capture is the endpoint body
    # itself and carries none.
    if ($names -contains 'channelsJson') {
        if (-not $wrapper.channelsJson) {
            $why = if ($names -contains 'channelsJsonError') { $wrapper.channelsJsonError } else { 'no reason recorded' }
            throw "the harness result has no captured channel list: $why"
        }
        $raw = $wrapper.channelsJson
    }

    # Provenance is three states, not two. An earlier version treated a missing
    # channelsCleared as consent, which passed a bare capture silently -- and a
    # bare capture is exactly the case where nobody would think to pass
    # -AllowUncleared, so the least provenanced input got the least scrutiny.
    # dvb-json-api caught that; it is the same defect as an unmapped enum
    # comparing equal to a blank field.
    if (-not $AllowUncleared) {
        if ($names -notcontains 'channelsCleared') {
            throw ("this capture carries no provenance, so whether the channel list was cleared before the " +
                   "scan cannot be established. ParsePMT resets only fps and chroma; width, height and aspect " +
                   "ratio survive from the saved list, so an uncleared run is partly memory rather than " +
                   "measurement. Assert against a harness result, or pass -AllowUncleared if you know how it " +
                   "was produced.")
        }
        if (-not $wrapper.channelsCleared) {
            throw ("this result did not clear the channel list first, so width, height and aspect ratio may " +
                   "have survived from the saved list rather than being decoded. Rerun with clearChannels, or " +
                   "pass -AllowUncleared if you mean it.")
        }
    }
}

$decoded = ($raw | ConvertFrom-Json).channels
if (-not $decoded) { throw 'the channel list is empty; nothing to assert' }

# The join key has to identify one service, and nothing guarantees it does.
# dvb-json-api found a real capture where it did not: the pre-existing muxes
# were all built without an explicit transport stream id or service ids, so
# ffmpeg defaulted them alike and (65281, 256, 1) appeared on three separate
# frequencies. Ten channels, seven distinct triplets. A join against that
# matches the wrong service and passes, which is worse than failing.
#
# Checked rather than assumed, for the same reason the cleared-channel-list
# check exists: the harness should not depend on a property it can verify in
# one pass. Generating unique ids is the fix; this is the guard that the fix
# actually held.
$byKey = $decoded | Group-Object { "{0}/{1}/{2}" -f $_.onid, $_.tsid, $_.sid }
$collisions = @($byKey | Where-Object { $_.Count -gt 1 })
if ($collisions.Count) {
    $detail = ($collisions | ForEach-Object {
        "  (onid/tsid/sid) {0} x{1}: {2}" -f $_.Name, $_.Count, (($_.Group | ForEach-Object { "'$($_.name)' at $($_.frequency)" }) -join ', ')
    }) -join "`n"
    throw ("the decoded channel list does not identify its services uniquely, so a join would match the " +
           "wrong one and report a pass:`n$detail`n" +
           "Regenerate the streams: each mux needs its own transport stream id and each service its own " +
           "service id within it.")
}

# --- assert ----------------------------------------------------------------

$entries = $matrix.Entries
if ($EntryId) { $entries = $entries | Where-Object { $EntryId -contains $_.Id } }

$results = @()
foreach ($entry in $entries) {
    # Joined on the full DVB triplet, not frequency: the matrix deliberately
    # does not record where a stream is tuned, and (onid, tsid, sid) identifies
    # the service whichever mux it ends up in.
    #
    # It must be the same key the collision guard checked. Joining on sid alone
    # relied on a weaker property than the guard establishes -- two services
    # sharing a sid across different transport streams would clear the guard and
    # still collide here. Latent while the matrix shares one TransportStreamId,
    # live the moment an entry declares its own.
    $wantTsid = if ($entry.ContainsKey('TransportStreamId')) { $entry.TransportStreamId } else { $defaults.TransportStreamId }
    $wantOnid = if ($entry.ContainsKey('OriginalNetworkId')) { $entry.OriginalNetworkId } else { $defaults.OriginalNetworkId }
    $channel = @($decoded | Where-Object {
        $_.sid -eq $entry.ServiceId -and $_.tsid -eq $wantTsid -and $_.onid -eq $wantOnid })

    if (-not $channel.Count) {
        $results += [pscustomobject]@{ Id = $entry.Id; State = 'ABSENT'
            Problems = @("no channel decoded with onid/tsid/sid $wantOnid/$wantTsid/$($entry.ServiceId)") }
        continue
    }
    if ($channel.Count -gt 1) {
        # The guard should have caught this; belt and braces, because a join
        # silently taking [0] is how the wrong service gets asserted against.
        $results += [pscustomobject]@{ Id = $entry.Id; State = 'FAIL'
            Problems = @("$($channel.Count) decoded channels share onid/tsid/sid $wantOnid/$wantTsid/$($entry.ServiceId); refusing to guess which one this entry describes") }
        continue
    }
    $problems = @(Test-ChannelAgainstEntry -Channel $channel[0] -Entry $entry -Defaults $defaults)
    $results += [pscustomobject]@{
        Id = $entry.Id
        State = $(if ($problems.Count) { 'FAIL' } else { 'PASS' })
        Problems = $problems
    }
}

$unmatched = @($decoded | Where-Object { $sid = $_.sid; -not ($entries | Where-Object { $_.ServiceId -eq $sid }) })

foreach ($r in $results) {
    $colour = switch ($r.State) { 'PASS' { 'Green' } 'FAIL' { 'Red' } default { 'Yellow' } }
    Write-Host ("{0,-42} {1}" -f $r.Id, $r.State) -ForegroundColor $colour
    foreach ($p in $r.Problems) { Write-Host "    $p" }
}

if ($unmatched.Count) {
    Write-Host ''
    Write-Host "$($unmatched.Count) decoded channel(s) matched no matrix entry:" -ForegroundColor Yellow
    foreach ($u in $unmatched) { Write-Host ("    sid {0}  '{1}'" -f $u.sid, $u.name) }
}

$passed = @($results | Where-Object { $_.State -eq 'PASS' }).Count
Write-Host ''
Write-Host ("{0} of {1} entries decoded as declared." -f $passed, $results.Count)
if ($passed -ne $results.Count) { exit 1 }
