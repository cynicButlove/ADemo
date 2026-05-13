@echo off
setlocal

if "%VSCMD_VER%"=="" (
  echo Please run this script from a Visual Studio Developer Command Prompt.
  exit /b 1
)

if not exist build mkdir build

cl /std:c++17 /EHsc /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /nologo /Febuild\DemoServer.exe src\DemoServerMain.cpp ws2_32.lib
