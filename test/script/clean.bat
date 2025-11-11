@echo off

if not exist "build" mkdir "build"
cl /nologo /EHsc /std:c++20 /O2 /Fobuild\ /Febuild\csb.exe csb.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
build\csb.exe clean
