if (-not $Env:VSCMD_ARG_HOST_ARCH -or -not $Env:VSINSTALLDIR -or -not $Env:VCToolsVersion -or -not $Env:WindowsSDKVersion)
{
  Write-Host "MSVC environment variables not set. Please run from an environment with MSVC build tools available."
  exit 1
}

if (!(Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }

cl /nologo /EHsc /std:c++20 /Fobuild\ /c csb.cpp
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
link /NOLOGO /MACHINE:$Env:PROCESSOR_ARCHITECTURE /OUT:build\csb.exe build\csb.obj
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

.\build\csb.exe
