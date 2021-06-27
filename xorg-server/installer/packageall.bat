@echo off

if "%1"=="nox86" goto skipx86

if exist vcxsrv.*.installer.exe del vcxsrv.*.installer.exe

copy "%VCToolsRedistDir%\x86\Microsoft.VC142.CRT\msvcp140.dll"
copy "%VCToolsRedistDir%\x86\Microsoft.VC142.CRT\vcruntime140.dll"
copy "%VCToolsRedistDir%\debug_nonredist\x86\Microsoft.VC142.DebugCRT\msvcp140d.dll"
copy "%VCToolsRedistDir%\debug_nonredist\x86\Microsoft.VC142.DebugCRT\vcruntime140d.dll"

if exist ..\obj\servrelease\vcxsrv.exe "makensis.exe" vcxsrv.nsi
if exist ..\obj\servdebug\vcxsrv.exe "makensis.exe" vcxsrv-debug.nsi

:skipx86
if "%1"=="nox64" goto skipx64

if exist vcxsrv-64.*.installer.exe del vcxsrv-64.*.installer.exe

copy "%VCToolsRedistDir%\x64\Microsoft.VC142.CRT\msvcp140.dll"
copy "%VCToolsRedistDir%\x64\Microsoft.VC142.CRT\vcruntime140.dll"
copy "%VCToolsRedistDir%\x64\Microsoft.VC142.CRT\vcruntime140_1.dll"
copy "%VCToolsRedistDir%\debug_nonredist\x64\Microsoft.VC142.DebugCRT\msvcp140d.dll"
copy "%VCToolsRedistDir%\debug_nonredist\x64\Microsoft.VC142.DebugCRT\vcruntime140d.dll"
copy "%VCToolsRedistDir%\debug_nonredist\x64\Microsoft.VC142.DebugCRT\vcruntime140_1d.dll"

if exist ..\obj64\servrelease\vcxsrv.exe "makensis.exe" vcxsrv-64.nsi
if exist ..\obj64\servdebug\vcxsrv.exe "makensis.exe" vcxsrv-64-debug.nsi


del vcruntime140_1.dll
del vcruntime140_1d.dll

:skipx64

del vcruntime140.dll
del vcruntime140d.dll
del msvcp140.dll
del msvcp140d.dll
