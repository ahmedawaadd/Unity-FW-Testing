# Host-based Unity test build for Windows / PowerShell (MSYS2 mingw64 gcc).
# Usage:  .\build.ps1

$ErrorActionPreference = "Stop"

$sources = @("unity/unity.c")
$sources += Get-ChildItem -Path "src", "test" -Filter *.c -ErrorAction SilentlyContinue |
            ForEach-Object { $_.FullName }

& gcc -DUNIT_TEST -Iunity -Isrc -Wall -Wextra -g $sources -o test_runner.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& .\test_runner.exe
exit $LASTEXITCODE
