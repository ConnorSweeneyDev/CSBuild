@echo off
setlocal

if not exist build mkdir build
set "CURRENT_DIR=%CD%"
(
echo [
echo   {
echo     "directory": "%CURRENT_DIR:\=\\%",
echo     "file": "%CURRENT_DIR:\=\\%\\csb.hpp",
echo     "command": "clang++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow-all -Wundef -Wdeprecated -Wtype-limits -Wcast-qual -Wcast-align -Wfloat-equal -Wunreachable-code-aggressive -Wformat=2 -D_WIN32 -c \"%CURRENT_DIR:\=\\%\\csb.hpp\" -o \"build\\csb.o\""
echo   }
echo ]
) > build\compile_commands.json

set "CLANG_VERSION=22.1.8"
set "CLANG_ARCHITECTURE=x86_64"
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "CLANG_ARCHITECTURE=aarch64"
set "CURRENT_VERSION="
if exist build\clang\clang.exe for /f "tokens=3" %%V in ('build\clang\clang.exe --version ^| findstr /C:"clang version"') do set "CURRENT_VERSION=%%V"
if "%CURRENT_VERSION%"=="%CLANG_VERSION%" goto :build
if exist build\clang rmdir /S /Q build\clang
set "ARCHIVE=clang+llvm-%CLANG_VERSION%-%CLANG_ARCHITECTURE%-pc-windows-msvc"
set "URL=https://github.com/llvm/llvm-project/releases/download/llvmorg-%CLANG_VERSION%/%ARCHIVE%.tar.xz"
echo Downloading archive at '%URL%'...
curl -f -L -C - -o build\clang.tar.xz "%URL%"
if errorlevel 1 (
  echo Failed to download clang archive.
  exit /b 1
)
echo Extracting archive to 'build\clang'...
tar -xf build\clang.tar.xz -C build
if errorlevel 1 (
  echo Failed to extract clang archive.
  exit /b 1
)
del build\clang.tar.xz
if not exist "build\%ARCHIVE%" (
  echo Failed to find build\%ARCHIVE%.
  exit /b 1
)
if not exist build\clang mkdir build\clang
move /Y "build\%ARCHIVE%\bin\*" build\clang >nul
if errorlevel 1 (
  echo Failed to move clang binaries.
  exit /b 1
)
rmdir /S /Q "build\%ARCHIVE%"

:build

<nul set /p "=Formatting csb.hpp... "
powershell -NoProfile -Command ^
  "$utf8 = New-Object System.Text.UTF8Encoding($false);" ^
  "$content = [IO.File]::ReadAllText('csb.hpp', $utf8);" ^
  "$match = [regex]::Match($content, '(?m)^// CSB ');" ^
  "if (-not $match.Success) { exit 1 };" ^
  "[IO.File]::WriteAllText('build\formatting.hpp', $content.Substring($match.Index), $utf8)"
if errorlevel 1 (
  echo Failed to extract the CSB section of csb.hpp.
  exit /b 1
)
build\clang\clang-format.exe -i "build\formatting.hpp"
if errorlevel 1 (
  echo Failed to format csb.hpp.
  exit /b 1
)
powershell -NoProfile -Command ^
  "$utf8 = New-Object System.Text.UTF8Encoding($false);" ^
  "$content = [IO.File]::ReadAllText('csb.hpp', $utf8);" ^
  "$match = [regex]::Match($content, '(?m)^// CSB ');" ^
  "if (-not $match.Success) { exit 1 };" ^
  "$formatted = [IO.File]::ReadAllText('build\formatting.hpp', $utf8);" ^
  "[IO.File]::WriteAllText('csb.hpp', $content.Substring(0, $match.Index) + $formatted, $utf8)"
if errorlevel 1 (
  echo Failed to write the formatted section back to csb.hpp.
  exit /b 1
)
del build\formatting.hpp
echo done.

endlocal
