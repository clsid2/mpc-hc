param(
    [string]$Compiler = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    [string]$RuntimeDirectory = (Join-Path $PSScriptRoot 'outputs\MPC-HC-Ambilight')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$workspace = $PSScriptRoot
$stage = Join-Path $workspace 'work\installer-stage-v6'
if (!(Test-Path -LiteralPath $Compiler)) { throw 'Install Inno Setup 6 or pass -Compiler with the ISCC.exe path.' }
if (Test-Path -LiteralPath $stage) { throw "Stage already exists: $stage. Move it aside before building another installer." }
New-Item -ItemType Directory -Path $stage, (Join-Path $stage 'Source'), (Join-Path $stage 'Validation') | Out-Null
$portable = $RuntimeDirectory
foreach ($directory in @('LAVFilters64','Lang','Shaders','Shaders11','Toolbars')) {
    Copy-Item -LiteralPath (Join-Path $portable $directory) -Destination $stage -Recurse
}
foreach ($file in @('D3DCompiler_47.dll','D3DX9_43.dll','MediaInfo.dll','mpciconlib.dll','COPYING.txt','MPCVR-LICENSE.txt')) {
    Copy-Item -LiteralPath (Join-Path $portable $file) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $workspace 'mpc-hc\bin\mpc-hc_x64\mpc-hc64.exe') -Destination $stage
New-Item -ItemType Directory -Path (Join-Path $stage 'MPCVR') | Out-Null
Copy-Item -LiteralPath (Join-Path $workspace 'VideoRenderer\_bin\Filter_x64\MpcVideoRenderer64.ax') -Destination (Join-Path $stage 'MPCVR')
Copy-Item -LiteralPath (Join-Path $workspace 'installer\README-Ambilight.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $workspace 'installer\mpc-hc64.ini') -Destination $stage
Copy-Item -LiteralPath (Join-Path $workspace 'installer') -Destination (Join-Path $stage 'Source\installer') -Recurse
foreach ($file in @('build-player.cmd','build-ambilight.cmd','build-installer.ps1')) {
    Copy-Item -LiteralPath (Join-Path $workspace $file) -Destination (Join-Path $stage 'Source')
}
foreach ($file in @('renderer-v4-build.log','player-v6-build.log','playback-v4.log')) {
    Copy-Item -LiteralPath (Join-Path $workspace ('work\' + $file)) -Destination (Join-Path $stage 'Validation')
}
function Add-RepoSource($repoPath, $archive, $prefix) {
    foreach ($relative in @(& git -C $repoPath -c core.quotepath=false ls-files --cached --others --exclude-standard)) {
        $sourcePath = Join-Path $repoPath $relative
        if (Test-Path -LiteralPath $sourcePath -PathType Leaf) {
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$sourcePath,($prefix + $relative).Replace('\','/'),[IO.Compression.CompressionLevel]::Optimal)
        }
    }
    foreach ($module in @(& git -C $repoPath ls-files --stage | Where-Object { $_ -match '^160000 ' })) {
        $modulePath = ($module -split "`t",2)[1]
        Add-RepoSource (Join-Path $repoPath $modulePath) $archive ($prefix + $modulePath + '/')
    }
}
$sourceNotes = @('# Build sources', '', 'Extract player-source.zip to mpc-hc and renderer-source.zip to VideoRenderer under one parent directory.',
    'Place the build scripts in that parent. Initialize no further submodules: their sources are included.',
    'Requires VS2022 C++/ATL/MFC, Windows SDK and NASM. Adjust paths in build-player.cmd for your machine.',
    'Run build-player.cmd and build-ambilight.cmd. Runtime assets are provided by this installer.',
    'For installer rebuilding, populate outputs/MPC-HC-Ambilight with those runtime assets and run build-installer.ps1.', '')
foreach ($entry in @(@('VideoRenderer','renderer-source.zip'),@('mpc-hc','player-source.zip'))) {
    $repoPath = Join-Path $workspace $entry[0]
    $sourceNotes += ($entry[0] + ' base commit: ' + (& git -C $repoPath rev-parse HEAD))
    $archive = [IO.Compression.ZipFile]::Open((Join-Path $stage ('Source\' + $entry[1])),[IO.Compression.ZipArchiveMode]::Create)
    try { Add-RepoSource $repoPath $archive '' } finally { $archive.Dispose() }
    Write-Output "Packaged sources: $($entry[0])"
}
$sourceNotes | Set-Content -LiteralPath (Join-Path $stage 'Source\BUILD.md') -Encoding UTF8
& $Compiler (Join-Path $workspace 'installer\Ambilight.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed: $LASTEXITCODE" }
$setupPath = Join-Path $workspace 'outputs\MPC-HC-Ambilight-Setup-x64-v6.exe'
$hash = Get-FileHash -LiteralPath $setupPath -Algorithm SHA256
($hash.Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetFileName($setupPath)) | Set-Content -LiteralPath (Join-Path $workspace 'outputs\SHA256SUMS-installer-v6.txt') -Encoding ASCII
Get-Item -LiteralPath $setupPath | Select-Object Name,Length
Write-Output $hash.Hash
