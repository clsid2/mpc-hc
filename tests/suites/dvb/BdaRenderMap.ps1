<#
.SYNOPSIS
    Map MPC-HC's BDA enum names onto the strings /dvb/channels.json spells them
    with.

.DESCRIPTION
    encoding-matrix.psd1 declares expectations as wire value plus enum name --
    ChromaFormat 2 alongside Chroma 'BDA_Chroma_4_2_2' -- and never as the JSON
    spelling. The matrix describes the stream and MPC-HC's classification of
    it; how the web interface renders that classification is a separate fact
    that lives here.

    The point of the split is that renaming a JSON key or relabelling a value
    is then a one-line change in this file rather than a migration across every
    matrix entry. It has already earned that once: BDA_MPV was relabelled from
    "MPEG2" to "MPEG-Video", and because the matrix says BDA_MPV, nothing in it
    had to move.

    PINNED TO mpc-hc 5dd8e312f8 (branch dvb-json-api, adipose fork). Read from
    DVBChannel.cpp at that commit: StreamTypeName, FpsName, AspectRatioName and
    ChromaName. If the branch relabels a value, this file is what changes, and
    a mismatch here produces a failure that has nothing to do with the streams
    under test -- which is the same ambiguity the tstables gate exists to
    remove, arriving from the other side.

    Empty string is a real expected value, not a missing one. BDA_Chroma_NONE
    and BDA_AR_NULL both render "", and the MPEG_1_only control asserts exactly
    that: if an absent chroma is later rendered as "none" or its key dropped,
    that is a visible change to a public web interface and should break a test.
#>

Set-StrictMode -Version Latest

# DVBChannel.cpp @ 5dd8e312f8, StreamTypeName
$script:BdaStreamTypeName = @{
    'BDA_MPV'      = 'MPEG-Video'   # not "MPEG2": ConvertToDVBType folds MPEG1 and MPEG2 into BDA_MPV
    'BDA_H264'     = 'H264'
    'BDA_HEVC'     = 'HEVC'
    'BDA_MPA'      = 'MPEG-Audio'
    'BDA_AC3'      = 'AC3'
    'BDA_EAC3'     = 'EAC3'
    'BDA_ADTS'     = 'AAC-ADTS'
    'BDA_LATM'     = 'AAC-LATM'
    'BDA_SUBTITLE' = 'DVB-Subtitle'
}

# DVBChannel.cpp @ 5dd8e312f8, FpsName
$script:BdaFpsName = @{
    'BDA_FPS_NONE'   = ''
    'BDA_FPS_23_976' = '23.976'
    'BDA_FPS_24_0'   = '24'
    'BDA_FPS_25_0'   = '25'
    'BDA_FPS_29_97'  = '29.97'
    'BDA_FPS_30_0'   = '30'
    'BDA_FPS_50_0'   = '50'
    'BDA_FPS_59_94'  = '59.94'
    'BDA_FPS_60_0'   = '60'
}

# DVBChannel.cpp @ 5dd8e312f8, ChromaName
$script:BdaChromaName = @{
    'BDA_Chroma_NONE'  = ''
    'BDA_Chroma_4_2_0' = '4:2:0'
    'BDA_Chroma_4_2_2' = '4:2:2'
    'BDA_Chroma_4_4_4' = '4:4:4'
}

# DVBChannel.cpp @ 5dd8e312f8, AspectRatioName
$script:BdaAspectRatioName = @{
    'BDA_AR_NULL'   = ''
    'BDA_AR_1'      = '1:1'
    'BDA_AR_3_4'    = '4:3'
    'BDA_AR_9_16'   = '16:9'
    'BDA_AR_1_2_21' = '2.21:1'
}

# Not a rendering: a sentinel for "this stream never reaches the JSON".
# ParsePMT calls AddStreamInfo only when ConvertToDVBType returns something
# other than BDA_UNKNOWN (Mpeg2SectionData.cpp:597-599), so such a stream is
# absent from audio[] and subtitles[] entirely. atsc-h264-eac3-gap asserts
# exactly that for ATSC E-AC-3 0x87. It must be checked as an absence, never
# rendered -- mapping it to '' would make it compare equal to every blank
# field, which is this project's recurring defect in yet another costume.
$script:BdaAbsent = 'BDA_UNKNOWN'

$script:BdaMaps = @{
    Fps         = $script:BdaFpsName
    Chroma      = $script:BdaChromaName
    AspectRatio = $script:BdaAspectRatioName
    Type        = $script:BdaStreamTypeName
    Bda         = $script:BdaStreamTypeName
}

function ConvertTo-BdaWireValue {
    <#
    .SYNOPSIS
        The JSON string MPC-HC renders for a BDA enum name.
    .DESCRIPTION
        Throws on an unknown name rather than returning empty. An unmapped enum
        would otherwise compare equal to every field MPC-HC leaves blank, which
        is a test that passes because it does not know the answer -- the same
        defect this project has now found five times in a different costume.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][ValidateSet('Fps', 'Chroma', 'AspectRatio', 'Type', 'Bda')][string] $Field,
        [Parameter(Mandatory)][AllowEmptyString()][string] $EnumName
    )

    if ($EnumName -eq $script:BdaAbsent) {
        throw ("'{0}' has no rendering by design: the stream is dropped before it reaches the JSON. " +
               "Assert that the stream is absent from audio[]/subtitles[], not that a field equals " +
               "something.") -f $EnumName
    }

    $map = $script:BdaMaps[$Field]
    if (-not $map.ContainsKey($EnumName)) {
        throw ("no rendering known for '{0}' as {1}. Either the matrix names an enum MPC-HC does not have, " +
               "or DVBChannel.cpp gained a value since this map was pinned to 5dd8e312f8. Do not guess: an " +
               "unmapped name silently compares equal to every blank field.") -f $EnumName, $Field
    }
    $map[$EnumName]
}

function Test-BdaRenderMap {
    <#
    .SYNOPSIS
        Every enum name the matrix uses must have a rendering.
    .DESCRIPTION
        Run before asserting anything. A matrix entry naming an enum this map
        does not know would fail at assertion time with a message about the
        stream, when the fault is here.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $MatrixPath)

    $matrix  = Import-PowerShellDataFile $MatrixPath
    $missing = @()

    foreach ($entry in $matrix.Entries) {
        if (-not $entry.ContainsKey('Expect') -or -not $entry.Expect) { continue }

        if ($entry.Expect.ContainsKey('Video') -and $entry.Expect.Video) {
            foreach ($field in 'Fps', 'Chroma', 'AspectRatio') {
                if (-not $entry.Expect.Video.ContainsKey($field)) { continue }
                $name = $entry.Expect.Video[$field]
                if ($null -eq $name) { continue }
                if (-not $script:BdaMaps[$field].ContainsKey($name)) {
                    $missing += "{0}: Video.{1} = '{2}'" -f $entry.Id, $field, $name
                }
            }
        }

        if ($entry.Expect.ContainsKey('Streams') -and $entry.Expect.Streams) {
            foreach ($stream in $entry.Expect.Streams) {
                if (-not $stream.ContainsKey('Bda')) { continue }
                if ($stream.Bda -eq $script:BdaAbsent) { continue }   # absence assertion, not a rendering
                if (-not $script:BdaStreamTypeName.ContainsKey($stream.Bda)) {
                    $missing += "{0}: Streams[].Bda = '{1}'" -f $entry.Id, $stream.Bda
                }
            }
        }
    }

    if ($missing.Count) {
        throw ("{0} matrix value(s) have no rendering in BdaRenderMap.ps1:`n  {1}" -f
               $missing.Count, ($missing -join "`n  "))
    }
    Write-Verbose 'every matrix enum name has a rendering'
}
