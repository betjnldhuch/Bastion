@echo off
rem Сборка BastionServer.exe компилятором .NET Framework 4 (есть в каждой Windows)
cd /d "%~dp0"
set CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe
"%CSC%" /nologo /codepage:65001 /target:winexe /platform:anycpu /optimize+ ^
  /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll ^
  /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll ^
  /win32icon:..\..\launcher\source\bastion.ico ^
  /resource:..\..\launcher\source\logo-shield.png,logo-shield.png ^
  /out:BastionServer.exe Starter.cs
if errorlevel 1 pause
