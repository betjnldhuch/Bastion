# Собрать client-pack.json для лаунчера.
#   1. Сложи файлы в папку pack\ так же, как они лежат в папке GTA:
#        pack\models\hud.txd
#        pack\models\gta3.img\radar00.txd   (папка с именем .img = записи внутри архива)
#        pack\models\gta3.img\colt45.txd\colt45icon.tex   (папка .txd = заплатка одной текстуры в этой записи)
#   2. Запусти:  powershell -ExecutionPolicy Bypass -File make-client-pack.ps1 -Version 1
#   3. Залей client-pack.json и папку pack\ в папку launcher/ на сервере раздачи.
param([int]$Version = 1, [string]$Pack = "pack", [string]$Out = "client-pack.json")
$root = (Resolve-Path $Pack).Path.TrimEnd('\') + '\'
$files = @()
Get-ChildItem $root -Recurse -File | Sort-Object FullName | ForEach-Object {
    $rel = $_.FullName.Substring($root.Length).Replace('\', '/')
    $entry = [ordered]@{}
    $dir = Split-Path $rel -Parent
    $up = if ($dir) { Split-Path $dir -Parent } else { '' }
    if ($dir -and $dir.ToLower().EndsWith('.txd') -and $up -and $up.ToLower().EndsWith('.img')) {
        $entry.path = $up.Replace('\', '/')
        $entry.name = Split-Path $dir -Leaf
        $entry.texture = [IO.Path]::GetFileNameWithoutExtension($_.Name)
    } elseif ($dir -and $dir.ToLower().EndsWith('.img')) {
        $entry.path = $dir.Replace('\', '/')
        $entry.name = $_.Name
    } else {
        $entry.path = $rel
    }
    $entry.url = "pack/" + ((($rel -split '/') | ForEach-Object { [uri]::EscapeDataString($_) }) -join '/')
    $entry.size = $_.Length
    $entry.sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
    $files += $entry
}
$json = [ordered]@{ version = $Version; product = "Bastion PC client pack"; files = $files } | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText((Join-Path (Get-Location) $Out), $json, (New-Object Text.UTF8Encoding $false))
Write-Host "Готово: $Out, файлов: $($files.Count), версия $Version"
