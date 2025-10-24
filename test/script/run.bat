@echo off

for /r "build" %%f in (test.exe) do (
  if exist "%%f" (
    echo Running: %%f
    "%%f"
    goto :eof
  )
)
echo Executable not found. Please run the build script first.
exit /b 1
