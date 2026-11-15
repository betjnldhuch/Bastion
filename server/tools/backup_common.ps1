# Bastion Online: shared helpers for backup_github.ps1 and backup_restore.ps1.
# Encryption: AES-256-CBC, key from the password via PBKDF2-SHA256, HMAC-SHA256 check.
# File format: "BKP1" | salt(16) | iv(16) | ciphertext | hmac(32)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Read-BackupConfig([string]$Path) {
    if (-not (Test-Path $Path)) { throw "Config $Path not found. Copy backup_github.ini.example to backup_github.ini and fill it in." }
    $cfg = @{ KEEP = '30'; PASSWORD = '' }
    foreach ($line in Get-Content -Path $Path -Encoding UTF8) {
        $t = $line.Trim()
        if ($t -eq '' -or $t.StartsWith('#') -or $t.StartsWith(';')) { continue }
        $i = $t.IndexOf('=')
        if ($i -lt 1) { continue }
        $cfg[$t.Substring(0, $i).Trim().ToUpper()] = $t.Substring($i + 1).Trim()
    }
    foreach ($k in 'TOKEN', 'REPO') {
        if (-not $cfg[$k] -or $cfg[$k] -like '*PUT-*') { throw "Fill in $k in $Path" }
    }
    return $cfg
}

function Get-BackupKeys([string]$Password, [byte[]]$Salt) {
    $kdf = [System.Security.Cryptography.Rfc2898DeriveBytes]::new($Password, $Salt, 200000, [System.Security.Cryptography.HashAlgorithmName]::SHA256)
    $bytes = $kdf.GetBytes(64)
    $kdf.Dispose()
    return , $bytes
}

function Protect-BackupBytes([byte[]]$Data, [string]$Password) {
    $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    $salt = New-Object byte[] 16; $rng.GetBytes($salt)
    $iv = New-Object byte[] 16; $rng.GetBytes($iv)
    $keys = Get-BackupKeys $Password $salt
    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.Key = [byte[]]$keys[0..31]; $aes.IV = $iv
    $aes.Mode = 'CBC'; $aes.Padding = 'PKCS7'
    $ct = $aes.CreateEncryptor().TransformFinalBlock($Data, 0, $Data.Length)
    $aes.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $magic = [Text.Encoding]::ASCII.GetBytes('BKP1')
    $ms.Write($magic, 0, 4); $ms.Write($salt, 0, 16); $ms.Write($iv, 0, 16); $ms.Write($ct, 0, $ct.Length)
    $hmac = [System.Security.Cryptography.HMACSHA256]::new([byte[]]$keys[32..63])
    $tag = $hmac.ComputeHash($ms.ToArray())
    $ms.Write($tag, 0, 32)
    return , $ms.ToArray()
}

function Unprotect-BackupBytes([byte[]]$Data, [string]$Password) {
    if ($Data.Length -lt 84 -or [Text.Encoding]::ASCII.GetString($Data, 0, 4) -ne 'BKP1') { throw 'Not an encrypted Bastion backup.' }
    $salt = [byte[]]$Data[4..19]
    $iv = [byte[]]$Data[20..35]
    $keys = Get-BackupKeys $Password $salt
    $hmac = [System.Security.Cryptography.HMACSHA256]::new([byte[]]$keys[32..63])
    $tag = $hmac.ComputeHash($Data, 0, $Data.Length - 32)
    $diff = 0
    for ($i = 0; $i -lt 32; $i++) { $diff = $diff -bor ($tag[$i] -bxor $Data[$Data.Length - 32 + $i]) }
    if ($diff -ne 0) { throw 'Wrong password or damaged file.' }
    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.Key = [byte[]]$keys[0..31]; $aes.IV = $iv
    $aes.Mode = 'CBC'; $aes.Padding = 'PKCS7'
    $pt = $aes.CreateDecryptor().TransformFinalBlock($Data, 36, $Data.Length - 36 - 32)
    $aes.Dispose()
    return , $pt
}

function Invoke-GitHub([hashtable]$Cfg, [string]$Method, [string]$Url, $Body = $null) {
    $h = @{ Authorization = "Bearer $($Cfg.TOKEN)"; Accept = 'application/vnd.github+json'; 'X-GitHub-Api-Version' = '2022-11-28'; 'User-Agent' = 'bastion-backup' }
    if (-not $Url.StartsWith('https://')) { $Url = "https://api.github.com/repos/$($Cfg.REPO)$Url" }
    if ($null -ne $Body) {
        return Invoke-RestMethod -Method $Method -Uri $Url -Headers $h -ContentType 'application/json' -Body ([Text.Encoding]::UTF8.GetBytes(($Body | ConvertTo-Json -Compress)))
    }
    return Invoke-RestMethod -Method $Method -Uri $Url -Headers $h
}

function Get-BackupAssets([hashtable]$Cfg, $Release) {
    $all = @()
    for ($page = 1; ; $page++) {
        $batch = @(Invoke-GitHub $Cfg GET "/releases/$($Release.id)/assets?per_page=100&page=$page")
        $all += $batch
        if ($batch.Count -lt 100) { break }
    }
    return $all | Where-Object { $_.name -like 'bastion_*' } | Sort-Object created_at -Descending
}
