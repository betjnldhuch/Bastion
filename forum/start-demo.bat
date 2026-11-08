@echo off
chcp 65001 >nul
title Bastion Forum DEMO
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 forum.py --demo %*
) else (
  python forum.py --demo %*
)
if errorlevel 1 (
  echo.
  echo Python не найден или форум остановился с ошибкой.
  echo Установи Python 3 с https://www.python.org/downloads/ и при установке
  echo поставь галочку "Add Python to PATH".
)
pause
