@echo off
setlocal

set "ENGINE_ROOT=D:\UE_5.5"
set "PROJECT_FILE=%~dp0DemoClient.uproject"
set "SERVER_HOST=127.0.0.1"
set "SERVER_PORT=7777"
set "DISPLAY_NAME=%~1"
set "WIN_X=%~2"
set "WIN_Y=%~3"
set "RES_X=%~4"
set "RES_Y=%~5"
set "CLIENT_LOG=%~dp0Saved\Logs\%DISPLAY_NAME%_DirectClient.log"
set "EXEC_CMDS=t.IdleWhenNotForeground 0; Slate.bAllowThrottling 0; t.MaxFPS 120; r.VSync 0; stat fps; stat unit; stat net"
set "EDITOR_PERF_OVERRIDES=-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False -ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bMonitorEditorPerformance=False -ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bEnableVSync=False"

if "%DISPLAY_NAME%"=="" set "DISPLAY_NAME=Player"
if "%WIN_X%"=="" set "WIN_X=40"
if "%WIN_Y%"=="" set "WIN_Y=40"
if "%RES_X%"=="" set "RES_X=1280"
if "%RES_Y%"=="" set "RES_Y=720"

if not exist "%PROJECT_FILE%" (
	echo DemoClient.uproject not found.
	exit /b 1
)

if not exist "%ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" (
	echo UnrealEditor.exe was not found.
	echo Expected engine root: %ENGINE_ROOT%
	exit /b 1
)

start "" "%ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJECT_FILE%" -game -log ^
 -abslog="%CLIENT_LOG%" ^
 %EDITOR_PERF_OVERRIDES% ^
 -UseDedicatedDemoServer=False ^
 -UseUEDedicatedServerDirectConnect=True ^
 -UEDedicatedServerHost=%SERVER_HOST% ^
 -UEDedicatedServerPort=%SERVER_PORT% ^
 -DisplayName=%DISPLAY_NAME% ^
 -ExecCmds="%EXEC_CMDS%" ^
 -windowed -ResX=%RES_X% -ResY=%RES_Y% -WinX=%WIN_X% -WinY=%WIN_Y%
