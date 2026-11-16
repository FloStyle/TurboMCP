# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
[CmdletBinding()]
param([Parameter(Mandatory)][string] $Engine)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
& python (Join-Path $PSScriptRoot 'execution_runtime.py') --authorize $repo $Engine
if ($LASTEXITCODE -ne 0) { throw 'Owned execution context required' }

function Assert-OrdinaryItem([string] $Path) {
    $item = Get-Item -LiteralPath $Path -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Packaging path uses a link or junction: $Path"
    }
    return $item
}

function Read-Git([string] $Root, [string[]] $Arguments) {
    $lines = & git -C $Root @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Git package check failed: $($Arguments[0])" }
    return $lines
}

function Read-Json([string] $Path) {
    return (Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json -AsHashtable)
}

function New-StageDirectory([string] $Path) {
    # Without -Force, directory creation refuses an existing destination.
    New-Item -ItemType Directory -Path $Path | Out-Null
    $item = Assert-OrdinaryItem $Path
    if (-not $item.PSIsContainer) { throw "Packaging directory is not a container: $Path" }
}

function Get-PackageFiles([string] $Root) {
    $directory = Assert-OrdinaryItem $Root
    if (-not $directory.PSIsContainer) { throw "Packaging tree is not a directory: $Root" }
    Get-ChildItem -LiteralPath $Root -Force -Recurse | ForEach-Object {
        if ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Packaging tree contains a link or junction: $($_.FullName)"
        }
        if (-not $_.PSIsContainer) { $_ }
    }
}

function Assert-SourceIdentity([string] $Commit) {
    if ((Read-Git $repo @('rev-parse', 'HEAD')) -ne $Commit -or
        @(Read-Git $repo @('status', '--porcelain=v1')).Count) {
        throw 'Packaging requires the exact clean supervisor candidate'
    }
}

function Assert-EngineIdentity([string] $Commit) {
    if ((Read-Git $Engine @('rev-parse', 'HEAD')) -ne $Commit -or
        @(Read-Git $Engine @('status', '--porcelain=v1', '--untracked-files=no')).Count) {
        throw 'Packaging requires the clean pinned engine source'
    }
}

$context = [IO.Path]::GetFullPath($env:TURBOMCP_EXECUTION_CONTEXT)
$run = [IO.Path]::GetDirectoryName($context)
$runId = [IO.Path]::GetFileName($run)
$expectedContext = Join-Path $repo ".local/executions/$runId/context.json"
if ($runId -notmatch '^[0-9a-f]{32}$' -or $context -ne [IO.Path]::GetFullPath($expectedContext)) {
    throw 'Packaging context is outside the owned execution directory'
}
foreach ($path in @((Join-Path $repo '.local'), (Join-Path $repo '.local/executions'), $run, $context)) {
    [void](Assert-OrdinaryItem $path)
}
$reportPath = Join-Path $run 'report.json'
[void](Assert-OrdinaryItem $reportPath)
$report = Read-Json $reportPath
if ($report.request.profile -ne 'package' -or @($report.request.suites).Count -or
    $report.active_command[3] -ne 'Tools/package-plugin.ps1') {
    throw 'An active package profile is required'
}
$commit = $report.request.commit
Assert-SourceIdentity $commit
$pluginTree = Read-Git $repo @('rev-parse', "${commit}:Plugins/TurboMCP")
$lock = Read-Json (Join-Path $repo 'engine.lock.json')
Assert-EngineIdentity $lock.commit
$version = Read-Json (Join-Path $Engine 'Engine/Build/Build.version')
$engineVersion = "$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
if ($engineVersion -ne $lock.version) { throw 'Engine build version does not match the package pin' }

$uat = Join-Path $Engine 'Engine/Build/BatchFiles/RunUAT.bat'
$uatFile = Assert-OrdinaryItem $uat
if ($uatFile.PSIsContainer) { throw 'RunUAT.bat is not a file' }
$artifactRoot = Join-Path $repo 'release/artifacts'
$stage = Join-Path $artifactRoot ".staging-$runId"
& git -C $repo check-ignore --quiet --no-index -- "$stage/source.zip"
if ($LASTEXITCODE -ne 0) { throw 'Packaging staging must be ignored by Git' }
[void](Assert-OrdinaryItem (Join-Path $repo 'release'))
if (-not (Test-Path -LiteralPath $artifactRoot)) { New-StageDirectory $artifactRoot }
[void](Assert-OrdinaryItem $artifactRoot)
New-StageDirectory $stage

$archive = Join-Path $stage 'source.zip'
$inputRoot = Join-Path $stage 'source'
Read-Git $repo @('archive', '--format=zip', "--output=$archive", $commit, '--', 'Plugins/TurboMCP')
Expand-Archive -LiteralPath $archive -DestinationPath $inputRoot
$inputPlugin = Join-Path $inputRoot 'Plugins/TurboMCP'
$tracked = @{}
foreach ($line in @(Read-Git $repo @('-c', 'core.quotePath=false', 'ls-tree', '-r', $pluginTree))) {
    if ($line -match '^(100644|100755) blob ([a-f0-9]{40})\t(.+)$') {
        $tracked.Add($Matches[3], $Matches[2])
    } else {
        throw 'The staged plugin contains a non-ordinary or unrecognized Git entry'
    }
}
$inputFiles = @(Get-PackageFiles $inputPlugin)
if ($inputFiles.Count -ne $tracked.Count) { throw 'Staged plugin file count differs from Git' }
foreach ($file in $inputFiles) {
    $relative = [IO.Path]::GetRelativePath($inputPlugin, $file.FullName).Replace('\', '/')
    $blob = Read-Git $repo @('hash-object', '--no-filters', '--', $file.FullName)
    if ($tracked[$relative] -ne $blob) { throw "Staged plugin bytes differ from Git: $relative" }
}
$descriptorPath = Join-Path $inputPlugin 'TurboMCP.uplugin'
$sourceDescriptor = Read-Json $descriptorPath
$pluginVersion = $sourceDescriptor.VersionName
if ($pluginVersion -notmatch '^[A-Za-z0-9._+-]{1,64}$') { throw 'Plugin version is not usable in an artifact filename' }
$label = "TurboMCP-$pluginVersion-Win64-UE$engineVersion-$($commit.Substring(0, 12))-$runId"
$destination = Join-Path $artifactRoot $label
if (Get-ChildItem -LiteralPath $artifactRoot -Force | Where-Object Name -eq $label) {
    throw 'Package delivery destination already exists'
}

$buildRoot = Join-Path $stage 'build'
New-StageDirectory $buildRoot
$builtPlugin = Join-Path $buildRoot 'TurboMCP'
if (@(Get-ChildItem -LiteralPath $buildRoot -Force).Count) { throw 'Package output must start absent' }
$platformArguments = @('-HostPlatforms=Win64', '-TargetPlatforms=Win64')
$buildArguments = @('BuildPlugin', "-Plugin=$descriptorPath", "-Package=$builtPlugin") + $platformArguments
Write-Output ('BuildPlugin command: ' + (ConvertTo-Json -InputObject (@($uat) + $buildArguments) -Compress))
& $uat @buildArguments
if ($LASTEXITCODE -ne 0) { throw "Win64 BuildPlugin failed with exit $LASTEXITCODE" }
Assert-SourceIdentity $commit
Assert-EngineIdentity $lock.commit
foreach ($path in @($artifactRoot, $stage, $inputRoot, (Join-Path $inputRoot 'Plugins'), $inputPlugin, $buildRoot)) {
    [void](Assert-OrdinaryItem $path)
}
[void]@(Get-PackageFiles $builtPlugin)

# Stock BuildPlugin filters can omit these plugin-root files. Use the exact staged copies.
foreach ($name in @('LICENSE', 'NOTICES.json')) {
    $source = (Assert-OrdinaryItem (Join-Path $inputPlugin $name)).FullName
    if ((Read-Git $repo @('hash-object', '--no-filters', '--', $source)) -ne $tracked[$name]) {
        throw "Staged notice differs from Git: $name"
    }
    $target = Join-Path $builtPlugin $name
    if (-not (Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $source -Destination $target }
    [void](Assert-OrdinaryItem $target)
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw "Packaged notice differs from Git: $name"
    }
}

$binaryPath = 'Binaries/Win64/UnrealEditor-TurboMCP.dll'
foreach ($name in @('TurboMCP.uplugin', $binaryPath, 'Binaries/Win64/UnrealEditor.modules',
                    'Content/Traps/EngineToolTraps.json', 'Resources/Icon128.png')) {
    $item = Assert-OrdinaryItem (Join-Path $builtPlugin $name)
    if ($item.PSIsContainer -or $item.Length -le 0) { throw "Missing packaged file: $name" }
}
$builtDescriptor = Read-Json (Join-Path $builtPlugin 'TurboMCP.uplugin')
$moduleReceipt = Read-Json (Join-Path $builtPlugin 'Binaries/Win64/UnrealEditor.modules')
if ($builtDescriptor.Installed -ne $true -or $builtDescriptor.VersionName -ne $pluginVersion -or
    $moduleReceipt.Modules.TurboMCP -ne 'UnrealEditor-TurboMCP.dll') {
    throw 'Package descriptor or module identity did not match the build'
}

$observedChanges = [ordered]@{}
foreach ($key in @((@($sourceDescriptor.Keys) + @($builtDescriptor.Keys)) | Sort-Object -Unique)) {
    $old = $sourceDescriptor[$key]
    $new = $builtDescriptor[$key]
    if ((ConvertTo-Json -InputObject $old -Depth 30 -Compress) -cne
        (ConvertTo-Json -InputObject $new -Depth 30 -Compress)) {
        $observedChanges.Add($key, @{source = $old; packaged = $new})
    }
}
$files = [ordered]@{}
foreach ($file in @(Get-PackageFiles $builtPlugin | Sort-Object FullName)) {
    $relative = [IO.Path]::GetRelativePath($builtPlugin, $file.FullName).Replace('\', '/')
    $files.Add("TurboMCP/$relative", @{
        bytes = $file.Length
        sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    })
}

$delivery = Join-Path $stage 'delivery'
New-StageDirectory $delivery
$zipPath = Join-Path $delivery "$label.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($builtPlugin, $zipPath, [IO.Compression.CompressionLevel]::Optimal, $true)
$zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
try {
    foreach ($entry in $zip.Entries) {
        if (-not $entry.Name) { continue }
        $path = $entry.FullName.Replace('\', '/')
        $expected = $files[$path]
        if (-not $expected -or -not $seen.Add($path) -or $entry.Length -ne $expected.bytes) {
            throw "Unexpected ZIP entry: $path"
        }
        $stream = $entry.Open()
        try {
            if ((Get-FileHash -InputStream $stream).Hash.ToLowerInvariant() -ne $expected.sha256) {
                throw "ZIP entry hash mismatch: $path"
            }
        } finally { $stream.Dispose() }
    }
    if ($seen.Count -ne $files.Count) { throw 'ZIP is missing packaged files' }
} finally { $zip.Dispose() }

$zipHash = (Get-FileHash -LiteralPath $zipPath).Hash.ToLowerInvariant()
$manifest = [ordered]@{
    schema = 1
    release_scope = 'development-preview'
    engine_distribution = 'source-build'
    source_commit = $commit
    plugin_tree = $pluginTree
    engine_version = $engineVersion
    engine_commit = $lock.commit
    platform = 'Win64'
    installed_consumer_acceptance = 'pending'
    build_command = @('Engine/Build/BatchFiles/RunUAT.bat', 'BuildPlugin',
                      '-Plugin=<staged-plugin>/TurboMCP.uplugin', '-Package=<staged-output>/TurboMCP') + $platformArguments
    descriptor_changes = $observedChanges
    module_build_id = $moduleReceipt.BuildId
    dll_sha256 = $files["TurboMCP/$binaryPath"].sha256
    archive = "$label.zip"
    archive_sha256 = $zipHash
    files = $files
}
$manifest | ConvertTo-Json -Depth 35 | Set-Content -LiteralPath (Join-Path $delivery 'manifest.json') -Encoding UTF8
# A same-volume directory move exposes both verified files and refuses an existing destination.
$artifactBoundary = (Assert-OrdinaryItem $artifactRoot).FullName
$stagePath = (Assert-OrdinaryItem $stage).FullName
$deliveryPath = (Assert-OrdinaryItem $delivery).FullName
$destinationPath = [IO.Path]::GetFullPath($destination)
if ([IO.Path]::GetDirectoryName($destinationPath) -ne $artifactBoundary -or
    [IO.Path]::GetDirectoryName($stagePath) -ne $artifactBoundary -or
    [IO.Path]::GetDirectoryName($deliveryPath) -ne $stagePath) {
    throw 'Package delivery leaves the owned artifact directory'
}
[IO.Directory]::Move($deliveryPath, $destinationPath)
Write-Output "PASS: development preview $destination"
Write-Output "SHA-256: $zipHash"
