$CurrentDir = (Get-Location).Path
$CompileCommand =
@{
  directory = $CurrentDir
  file = "$CurrentDir\csb.hpp"
  command = "clang++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow-all -Wundef -Wdeprecated -Wtype-limits -Wcast-qual -Wcast-align -Wfloat-equal -Wunreachable-code-aggressive -Wformat=2"
}
@($CompileCommand) | ConvertTo-Json -Depth 3 -AsArray | Out-File -FilePath "compile_commands.json" -Encoding UTF8
