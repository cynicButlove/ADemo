@echo off
setlocal

set "ENGINE_ROOT=D:\UnrealEngine-5.5"
set "PROJECT_ROOT=%~dp0"
set "PROJECT_FILE=%PROJECT_ROOT%DemoClient.uproject"
set "ARCHIVE_ROOT=%PROJECT_ROOT%Packaged\WindowsClient"
set "COOK_ROOT=%PROJECT_ROOT%Saved\Cooked\Windows"
set "STAGE_ROOT=%PROJECT_ROOT%Saved\StagedBuilds\Windows"
set "INTERMEDIATE_STAGE_ROOT=%PROJECT_ROOT%Intermediate\Staging"

if not exist "%ENGINE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" (
	echo RunUAT.bat not found under %ENGINE_ROOT%.
	exit /b 1
)

if not exist "%PROJECT_FILE%" (
	echo DemoClient.uproject not found.
	exit /b 1
)

if exist "%COOK_ROOT%" (
	echo Removing stale cooked client data...
	rmdir /s /q "%COOK_ROOT%"
)

if exist "%STAGE_ROOT%" (
	echo Removing stale staged client data...
	rmdir /s /q "%STAGE_ROOT%"
)

if exist "%INTERMEDIATE_STAGE_ROOT%" (
	echo Removing stale intermediate staging data...
	rmdir /s /q "%INTERMEDIATE_STAGE_ROOT%"
)

if exist "%ARCHIVE_ROOT%" (
	echo Removing stale archived client build...
	rmdir /s /q "%ARCHIVE_ROOT%"
)

echo Packaging DemoClient Win64...
call "%ENGINE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
 -project="%PROJECT_FILE%" ^
 -noP4 ^
 -build ^
 -nocompileeditor ^
 -cook ^
 -stage ^
 -pak ^
 -iostore ^
 -archive ^
 -archivedirectory="%ARCHIVE_ROOT%" ^
 -platform=Win64 ^
 -target=DemoClient ^
 -clientconfig=Development ^
 -prereqs ^
 -utf8output

if not "%errorlevel%"=="0" (
	echo.
	echo DemoClient packaging failed.
	echo Expected staged exe: %STAGE_ROOT%\DemoClient.exe
	echo Expected archived exe: %ARCHIVE_ROOT%\DemoClient.exe
	pause
	exit /b %errorlevel%
)

echo.
echo DemoClient packaging finished successfully.
echo Staged exe: %STAGE_ROOT%\DemoClient.exe
echo Archived exe: %ARCHIVE_ROOT%\DemoClient.exe
exit /b 0
