@echo off
chcp 65001 >nul
title Bastion Bots
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 bot.py %*
) else (
  python bot.py %*
)
if errorlevel 1 (
  echo.
  echo Python не найден или бот остановился с ошибкой.
  echo Установи Python 3 с https://www.python.org/downloads/ и при установке
  echo поставь галочку "Add Python to PATH".
)
pause
