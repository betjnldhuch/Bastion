@echo off
rem Bastion Online: copy database backups to another disk or a cloud folder.
rem
rem 1. Put this file into the server folder (next to omp-server.exe).
rem 2. Change DST below to a folder on ANOTHER disk or inside your cloud folder
rem    (Yandex Disk, Google Drive, OneDrive).
rem 3. Windows Task Scheduler: create a task that runs this file once a day.
rem
rem The server itself writes scriptfiles\rp_backup_YYYY-MM-DD.db once a day.

set "SRC=%~dp0scriptfiles"
set "DST=D:\BastionBackups"

if not exist "%DST%" mkdir "%DST%"
robocopy "%SRC%" "%DST%" rp_backup_*.db /XO /R:1 /W:1 /NJH /NJS /NP

rem Delete copies older than 60 days in the destination folder
forfiles /P "%DST%" /M rp_backup_*.db /D -60 /C "cmd /c del @path" 2>nul
exit /b 0
