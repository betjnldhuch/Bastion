# Bastion Online: upload database backups to a PRIVATE GitHub repository (release "db-backups").
#
# Put next to omp-server.exe together with backup_common.ps1 and backup_github.ini.
# What goes into the archive: the newest scriptfiles\rp_backup_*.db (the server makes it
# itself every day, or with /backup), config.json and small files from scriptfiles.
# Encrypted with PASSWORD from backup_github.ini (if it is set). Keeps the KEEP newest copies.
# Log: backup_github.log next to this script.

param([string]$Config = (Join-Path $PSScriptRoot 'backup_github.ini'))
. (Join-Path $PSScriptRoot 'backup_common.ps1')

$server = $PSScriptRoot
$log = Join-Path $server 'backup_github.log'
function Log([string]$m) { $s = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $m"; Write-Host $s; Add-Content -Path $log -Value $s -Encoding UTF8 }

$tmp = Join-Path ([IO.Path]::GetTempPath()) ("bastion_backup_" + [guid]::NewGuid().ToString('N'))
try {
    $cfg = Read-BackupConfig $Config
    $sf = Join-Path $server 'scriptfiles'

    $db = Get-ChildItem -Path $sf -Filter 'rp_backup_*.db' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $db) { throw "No scriptfiles\rp_backup_*.db. Start the server once or type /backup in game." }
    if ($db.LastWriteTime -lt (Get-Date).AddDays(-2)) { Log "WARNING: newest copy $($db.Name) is older than 2 days. Is the server running?" }

    New-Item -ItemType Directory -Path (Join-Path $tmp 'pack\scriptfiles') -Force | Out-Null
    Copy-Item $db.FullName (Join-Path $tmp 'pack\scriptfiles\rp.db')
    foreach ($f in 'config.json', 'server.cfg', 'bans.json') {
        $p = Join-Path $server $f
        if (Test-Path $p) { Copy-Item $p (Join-Path $tmp 'pack') }
    }
    Get-ChildItem -Path $sf -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -notin '.db', '.db-journal', '.db-wal', '.db-shm' -and $_.Length -lt 5MB } |
        ForEach-Object { Copy-Item $_.FullName (Join-Path $tmp 'pack\scriptfiles') }

    $zip = Join-Path $tmp 'backup.zip'
    Compress-Archive -Path (Join-Path $tmp 'pack\*') -DestinationPath $zip -CompressionLevel Optimal
    $bytes = [IO.File]::ReadAllBytes($zip)
    $name = "bastion_$(Get-Date -Format 'yyyy-MM-dd_HH-mm').zip"
    if ($cfg.PASSWORD) { $bytes = Protect-BackupBytes $bytes $cfg.PASSWORD; $name += '.enc' }
    else { Log 'WARNING: PASSWORD is empty, the archive is not encrypted.' }

    # Find or create the release that holds the backups
    $release = $null
    try { $release = Invoke-GitHub $cfg GET '/releases/tags/db-backups' } catch { if ($_.Exception.Response.StatusCode.value__ -ne 404) { throw } }
    if (-not $release) {
        $repo = Invoke-GitHub $cfg GET ''
        if (-not $repo.private) { throw "Repository $($cfg.REPO) is PUBLIC. Backups go only to a private repository." }
        try {
            $release = Invoke-GitHub $cfg POST '/releases' @{ tag_name = 'db-backups'; name = 'Database backups'; body = 'Automatic backups of the Bastion database. Restore with backup_restore.ps1.' }
        } catch {
            # An empty repository has no commit to tag: add a README first
            $readme = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes("# Bastion backups`n`nPrivate. Backups are in Releases -> db-backups.`n"))
            Invoke-GitHub $cfg PUT '/contents/README.md' @{ message = 'init'; content = $readme } | Out-Null
            $release = Invoke-GitHub $cfg POST '/releases' @{ tag_name = 'db-backups'; name = 'Database backups'; body = 'Automatic backups of the Bastion database. Restore with backup_restore.ps1.' }
        }
    } elseif (-not (Invoke-GitHub $cfg GET '').private) {
        throw "Repository $($cfg.REPO) is PUBLIC. Backups go only to a private repository."
    }

    $old = Get-BackupAssets $cfg $release | Where-Object { $_.name -eq $name }
    foreach ($a in $old) { Invoke-GitHub $cfg DELETE "/releases/assets/$($a.id)" | Out-Null }

    $up = "https://uploads.github.com/repos/$($cfg.REPO)/releases/$($release.id)/assets?name=$name"
    $h = @{ Authorization = "Bearer $($cfg.TOKEN)"; Accept = 'application/vnd.github+json'; 'User-Agent' = 'bastion-backup' }
    $file = Join-Path $tmp $name
    [IO.File]::WriteAllBytes($file, $bytes)
    Invoke-RestMethod -Method POST -Uri $up -Headers $h -ContentType 'application/octet-stream' -InFile $file | Out-Null
    Log "OK: uploaded $name ($([math]::Round($bytes.Length / 1KB)) KB, from $($db.Name))"

    $keep = [int]$cfg.KEEP
    if ($keep -lt 1) { $keep = 1 }
    $extra = @(Get-BackupAssets $cfg $release) | Select-Object -Skip $keep
    foreach ($a in $extra) {
        Invoke-GitHub $cfg DELETE "/releases/assets/$($a.id)" | Out-Null
        Log "deleted old $($a.name)"
    }
    exit 0
} catch {
    Log "FAILED: $($_.Exception.Message)"
    exit 1
} finally {
    if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue }
}
