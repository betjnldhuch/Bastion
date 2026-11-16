# Bastion Online: download and unpack a backup from GitHub.
#
#   powershell -ExecutionPolicy Bypass -File backup_restore.ps1            -> newest backup
#   powershell -ExecutionPolicy Bypass -File backup_restore.ps1 -List      -> list backups
#   powershell -ExecutionPolicy Bypass -File backup_restore.ps1 -Name bastion_2026-11-15_05-00.zip.enc
#   powershell -ExecutionPolicy Bypass -File backup_restore.ps1 -File D:\some.zip.enc   (already downloaded)
#
# Unpacks into the folder "restore_<name>" next to this script. It does NOT touch rp.db:
# stop the server and copy restore_...\scriptfiles\rp.db into scriptfiles yourself.
# Then delete scriptfiles\rp.db-wal and scriptfiles\rp.db-shm if they are there (old WAL files).

param([string]$Name = '', [string]$File = '', [switch]$List,
      [string]$Config = (Join-Path $PSScriptRoot 'backup_github.ini'))
. (Join-Path $PSScriptRoot 'backup_common.ps1')

$cfg = Read-BackupConfig $Config
if ($File) {
    $bytes = [IO.File]::ReadAllBytes($File)
    $Name = Split-Path $File -Leaf
} else {
    $release = Invoke-GitHub $cfg GET '/releases/tags/db-backups'
    $assets = @(Get-BackupAssets $cfg $release)
    if ($List) { $assets | ForEach-Object { '{0}  {1,8} KB' -f $_.name, [math]::Round($_.size / 1KB) }; exit 0 }
    $asset = if ($Name) { $assets | Where-Object { $_.name -eq $Name } | Select-Object -First 1 } else { $assets | Select-Object -First 1 }
    if (-not $asset) { throw "Backup '$Name' not found. Use -List." }
    $Name = $asset.name
    $tmpFile = [IO.Path]::GetTempFileName()
    $h = @{ Authorization = "Bearer $($cfg.TOKEN)"; Accept = 'application/octet-stream'; 'User-Agent' = 'bastion-backup' }
    Invoke-WebRequest -Uri $asset.url -Headers $h -OutFile $tmpFile -UseBasicParsing
    $bytes = [IO.File]::ReadAllBytes($tmpFile)
    Remove-Item $tmpFile -Force
}

if ($Name.EndsWith('.enc')) {
    if (-not $cfg.PASSWORD) { throw 'The backup is encrypted: set PASSWORD in backup_github.ini.' }
    $bytes = Unprotect-BackupBytes $bytes $cfg.PASSWORD
}
$base = $Name -replace '\.zip(\.enc)?$', ''
$out = Join-Path $PSScriptRoot "restore_$base"
$zip = Join-Path ([IO.Path]::GetTempPath()) "$base.zip"
[IO.File]::WriteAllBytes($zip, $bytes)
Expand-Archive -Path $zip -DestinationPath $out -Force
Remove-Item $zip -Force
Write-Host "Unpacked to $out"
Write-Host "To restore: stop the server, copy $out\scriptfiles\rp.db to scriptfiles\rp.db, delete scriptfiles\rp.db-wal and rp.db-shm if present, start the server."
