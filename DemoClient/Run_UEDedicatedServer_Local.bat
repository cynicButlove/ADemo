@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
set "SERVER_EXE=%PROJECT_ROOT%Binaries\Win64\DemoClientServer.exe"
set "MAP_PATH=/Game/UtopianCity/Maps/UtopianCityDemoMap"
set "SERVER_PORT=7777"

if not exist "%SERVER_EXE%" (
	echo DemoClientServer.exe not found.
	echo Build the DemoClientServer target with a source-built Unreal Engine first.
	exit /b 1
)

"%SERVER_EXE%" %MAP_PATH% -log -port=%SERVER_PORT%
