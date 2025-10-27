@echo off

if "%VSCMD_ARG_HOST_ARCH%"=="" goto :missing_env
if "%VSINSTALLDIR%"=="" goto :missing_env
if "%VCToolsVersion%"=="" goto :missing_env
if "%WindowsSDKVersion%"=="" goto :missing_env
goto :env_ok

:missing_env
echo MSVC environment variables not set. Please run from an environment with MSVC build tools available.
exit /b 1

:env_ok
if not exist "build" mkdir "build"
cl /nologo /EHsc /std:c++20 /O2 /Fobuild\ /c csb.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
link /NOLOGO /MACHINE:%PROCESSOR_ARCHITECTURE% /OUT:build\csb.exe build\csb.obj
if %errorlevel% neq 0 exit /b %errorlevel%
build\csb.exe
