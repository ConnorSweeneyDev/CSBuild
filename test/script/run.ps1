$executable = Get-ChildItem -Path "build" -Filter "test.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $executable)
{
    Write-Host "Executable not found. Please run the build script first."
    exit 1
}
& $executable.FullName
