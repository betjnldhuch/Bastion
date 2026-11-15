@echo off
rem Пересобрать лаунчер после правок в Core.cs Ui.cs
chcp 65001 >nul
set CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe
"%CSC%" /nologo /codepage:65001 /target:winexe /platform:x86 /optimize+ /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll /out:..\BastionLauncher.exe Core.cs Ui.cs Installer.cs Updater.cs ClientPack.cs UiPack.cs Secrets.cs
if errorlevel 1 (echo ОШИБКА сборки) else (echo Готово: BastionLauncher.exe пересобран)
pause
