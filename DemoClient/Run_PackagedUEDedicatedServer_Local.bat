@echo off
setlocal

set "ARCHIVE_ROOT=%~dp0Packaged\WindowsServer"
set "STAGED_ROOT=%~dp0Saved\StagedBuilds\WindowsServer"
set "SERVER_BOOTSTRAP=%ARCHIVE_ROOT%\DemoClientServer.exe"
set "SERVER_ROOT=%ARCHIVE_ROOT%"
set "MAP_PATH=/Game/UtopianCity/Maps/UtopianCityDemoMap"
set "SERVER_PORT=7777"

if not exist "%SERVER_BOOTSTRAP%" (
	set "SERVER_BOOTSTRAP=%STAGED_ROOT%\DemoClientServer.exe"
	set "SERVER_ROOT=%STAGED_ROOT%"
)

if not exist "%SERVER_BOOTSTRAP%" (
	echo Packaged DemoClientServer.exe not found.
	echo Checked:
	echo   %ARCHIVE_ROOT%\DemoClientServer.exe
	echo   %STAGED_ROOT%\DemoClientServer.exe
	echo Run Package_UEDedicatedServer_Win64.bat first.
	exit /b 1
)

pushd "%SERVER_ROOT%"
"%SERVER_BOOTSTRAP%" %MAP_PATH% -log -port=%SERVER_PORT%
popd
