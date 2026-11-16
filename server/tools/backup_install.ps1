# Bastion Online: test upload + Task Scheduler task "Bastion backup" (daily 05:00,
# or as soon as the PC is on if it was off at that time). Run via backup_install.bat.
$ErrorActionPreference = 'Stop'
$dir = $PSScriptRoot
$ini = Join-Path $dir 'backup_github.ini'
if (-not (Test-Path $ini)) {
    Copy-Item (Join-Path $dir 'backup_github.ini.example') $ini
    Write-Host 'Fill in backup_github.ini (opened in Notepad), save it and run backup_install.bat again.'
    Start-Process notepad.exe $ini
    exit 1
}

Write-Host 'Test upload...'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $dir 'backup_github.ps1')
if ($LASTEXITCODE -ne 0) { Write-Host 'Test upload FAILED, see the message above. The task is not created.'; exit 1 }

$script = Join-Path $dir 'backup_github.ps1'
$a = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$script`"" -WorkingDirectory $dir
$t = New-ScheduledTaskTrigger -Daily -At 05:00
$s = New-ScheduledTaskSettingsSet -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Hours 1) -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 30)
Register-ScheduledTask -TaskName 'Bastion backup' -Action $a -Trigger $t -Settings $s -Description 'Bastion database backup to GitHub' -Force | Out-Null
Write-Host 'Done. Task "Bastion backup" runs every day at 05:00. Log: backup_github.log'
