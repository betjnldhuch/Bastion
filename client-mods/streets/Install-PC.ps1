# Offline Mod Loader installation from the extracted street pack.
param([string]$GameDirectory, [string]$OutputDirectory,
      [string]$ModName = 'Bastion-Unity-Station', [int]$DictionaryCount = 4)
$ErrorActionPreference = 'Stop'
if (!$GameDirectory) { $GameDirectory = (Read-Host 'GTA San Andreas folder').Trim().Trim('"') }
$archive = Join-Path $GameDirectory 'models/gta3.img'
if (!$OutputDirectory) { $OutputDirectory = Join-Path (Join-Path $GameDirectory 'modloader') $ModName }
$patchRoot = Join-Path $PSScriptRoot 'pack/models/gta3.img'
$folders = @(Get-ChildItem -LiteralPath $patchRoot -Directory)
if ($DictionaryCount -lt 1 -or $folders.Count -ne $DictionaryCount) { throw 'Extract the entire ZIP first.' }

$reader = [IO.BinaryReader]::new([IO.File]::OpenRead($archive))
$dictionaryFiles = @{}
try {
    if ([Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -ne 'VER2') { throw 'Expected an IMG v2 archive.' }
    $count = $reader.ReadUInt32()
    if ((8 + 32L * $count) -gt $reader.BaseStream.Length) { throw 'Invalid IMG directory.' }
    $locations = @{}
    for ($i = 0; $i -lt $count; $i++) {
        $offset = $reader.ReadUInt32()
        $size = $reader.ReadUInt16()
        $allocated = $reader.ReadUInt16()
        if (!$size) { $size = $allocated }
        $name = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(24)).TrimEnd([char]0)
        $locations[$name] = @((2048L * $offset), (2048L * $size))
    }
    foreach ($folder in $folders) {
        $name = $folder.Name
        if (!$locations.ContainsKey($name)) { throw "Dictionary missing: $name" }
        $place = $locations[$name]
        $reader.BaseStream.Position = $place[0]
        $bytes = $reader.ReadBytes([int]$place[1])
        if ([BitConverter]::ToUInt32($bytes, 0) -ne 22) { throw "Invalid TXD: $name" }
        $end = 12L + [BitConverter]::ToUInt32($bytes, 4)
        if ($end -gt $bytes.Length) { throw "Truncated TXD: $name" }
        $patches = @{}
        foreach ($file in Get-ChildItem -LiteralPath $folder.FullName -File -Filter '*.tex') {
            $patches[$file.BaseName] = [IO.File]::ReadAllBytes($file.FullName)
        }
        $stream = [IO.MemoryStream]::new()
        try {
            $stream.Write($bytes, 0, 12)
            for ($pos = 12; $pos -lt $end;) {
                $kind = [BitConverter]::ToUInt32($bytes, $pos)
                $length = 12L + [BitConverter]::ToUInt32($bytes, $pos + 4)
                if ($pos + $length -gt $end) { throw "Invalid chunk in $name" }
                $texture = ''
                if ($kind -eq 21 -and $length -ge 64) {
                    $texture = [Text.Encoding]::ASCII.GetString($bytes, $pos + 32, 32).TrimEnd([char]0)
                }
                if ($patches.ContainsKey($texture)) {
                    $patch = $patches[$texture]
                    $stream.Write($patch, 0, $patch.Length)
                    $patches.Remove($texture)
                } else {
                    $stream.Write($bytes, $pos, $length)
                }
                $pos += $length
            }
            if ($patches.Count) { throw "Missing texture in ${name}: $($patches.Keys -join ', ')" }
            $result = $stream.ToArray()
            [BitConverter]::GetBytes([uint32]($result.Length - 12)).CopyTo($result, 4)
            $dictionaryFiles[$name] = $result
        } finally { $stream.Dispose() }
    }
} finally { $reader.Dispose() }

[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
foreach ($name in $dictionaryFiles.Keys) {
    $destination = Join-Path $OutputDirectory $name
    $temporary = Join-Path $OutputDirectory ([guid]::NewGuid().ToString() + '.tmp')
    [IO.File]::WriteAllBytes($temporary, $dictionaryFiles[$name])
    if ([IO.File]::Exists($destination)) {
        [IO.File]::Replace($temporary, $destination, [System.Management.Automation.Language.NullString]::Value)
    } else { [IO.File]::Move($temporary, $destination) }
}
Write-Host "Installed $($dictionaryFiles.Count) dictionaries in $OutputDirectory"
