@echo off
rem Bastion Online: set up automatic database backups to a private GitHub repository.
rem Put all backup_* files from server/tools next to omp-server.exe and run this file.
rem First run creates backup_github.ini: fill it in and run this file again.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0backup_install.ps1"
pause
