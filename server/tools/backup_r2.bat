@echo off
rem Bastion Online: upload the newest database backup to Cloudflare R2.
rem
rem 1. Put this file into the server folder (next to omp-server.exe).
rem 2. Fill in WORKER_URL and BACKUP_KEY below (see cloud/download-worker/README.md).
rem 3. Windows Task Scheduler: run this file once a day.
rem
rem Keep this file private: BACKUP_KEY must not be published or sent to anyone.

set "WORKER_URL=https://bastion-download.YOUR-NAME.workers.dev"
set "BACKUP_KEY=PUT-YOUR-BACKUP-KEY-HERE"

set "SRC=%~dp0scriptfiles"
set "NEWEST="
for /f "delims=" %%F in ('dir /b /o-n "%SRC%\rp_backup_*.db" 2^>nul') do (
    if not defined NEWEST set "NEWEST=%%F"
)
if not defined NEWEST (
    echo No backup files in %SRC%. Start the server once, it creates them itself.
    exit /b 1
)

echo Uploading %NEWEST% ...
curl.exe --fail --silent --show-error -T "%SRC%\%NEWEST%" -H "X-Bastion-Backup-Key: %BACKUP_KEY%" "%WORKER_URL%/backups/%NEWEST%"
if errorlevel 1 (
    echo Upload FAILED. Check WORKER_URL, BACKUP_KEY and the internet connection.
    exit /b 1
)
exit /b 0
