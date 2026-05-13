@echo off
setlocal

call "%~dp0Package_UEDedicatedServer_Win64.bat"
if not "%errorlevel%"=="0" (
	exit /b %errorlevel%
)

call "%~dp0Package_DemoClient_Win64.bat"
if not "%errorlevel%"=="0" (
	exit /b %errorlevel%
)

echo.
echo Packaged local test build is ready.
echo Start server with: Run_PackagedUEDedicatedServer_Local.bat
echo Start clients with:
echo   Run_PackagedDemoClient_UEDirect_Player1.bat
echo   Run_PackagedDemoClient_UEDirect_Player2.bat
exit /b 0
