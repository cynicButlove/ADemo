@echo off
setlocal

set "ARCHIVE_ROOT=%~dp0Packaged\WindowsClient"
set "STAGED_ROOT=%~dp0Saved\StagedBuilds\Windows"
set "CLIENT_BOOTSTRAP=%ARCHIVE_ROOT%\DemoClient.exe"
set "CLIENT_ROOT=%ARCHIVE_ROOT%"
set "SERVER_HOST=127.0.0.1"
set "SERVER_PORT=7777"
set "DISPLAY_NAME=%~1"
set "WIN_X=%~2"
set "WIN_Y=%~3"
set "RES_X=%~4"
set "RES_Y=%~5"
set "CLIENT_LOG=%~dp0Saved\Logs\%DISPLAY_NAME%_PackagedClient.log"
set "EXEC_CMDS=t.IdleWhenNotForeground 0; t.MaxFPS 120; r.VSync 0; stat fps; stat unit; stat net"

if "%DISPLAY_NAME%"=="" set "DISPLAY_NAME=Player"
if "%WIN_X%"=="" set "WIN_X=40"
if "%WIN_Y%"=="" set "WIN_Y=40"
if "%RES_X%"=="" set "RES_X=1280"
if "%RES_Y%"=="" set "RES_Y=720"

if not exist "%CLIENT_BOOTSTRAP%" (
	set "CLIENT_BOOTSTRAP=%STAGED_ROOT%\DemoClient.exe"
	set "CLIENT_ROOT=%STAGED_ROOT%"
)

if not exist "%CLIENT_BOOTSTRAP%" (
	echo Packaged DemoClient.exe not found.
	echo Checked:
	echo   %ARCHIVE_ROOT%\DemoClient.exe
	echo   %STAGED_ROOT%\DemoClient.exe
	echo Run Package_DemoClient_Win64.bat first.
	exit /b 1
)

pushd "%CLIENT_ROOT%"
start "" "%CLIENT_BOOTSTRAP%" -log ^
 -abslog="%CLIENT_LOG%" ^
 -UseDedicatedDemoServer=False ^
 -UseUEDedicatedServerDirectConnect=True ^
 -UEDedicatedServerHost=%SERVER_HOST% ^
 -UEDedicatedServerPort=%SERVER_PORT% ^
 -DisplayName=%DISPLAY_NAME% ^
 -ExecCmds="%EXEC_CMDS%" ^
 -windowed -ResX=%RES_X% -ResY=%RES_Y% -WinX=%WIN_X% -WinY=%WIN_Y%
popd

exit /b 0
