$ErrorActionPreference = "Stop"

$CmakeExe = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$QtPath   = "C:\Qt\6.10.1\msvc2022_64"
$BuildDir = Join-Path $PSScriptRoot "build"
$CtestExe = Join-Path (Split-Path $CmakeExe -Parent) "ctest.exe"

Write-Host "Configuring unit tests..." -ForegroundColor Cyan
& $CmakeExe -S $PSScriptRoot -B $BuildDir -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="$QtPath"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuilding (Release)..." -ForegroundColor Cyan
    & $CmakeExe --build $BuildDir --config Release

    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nRunning unit tests with CTest..." -ForegroundColor Cyan
        Push-Location $BuildDir
        try {
            & $CtestExe -C Release --output-on-failure
        }
        finally {
            Pop-Location
        }
    } else {
        Write-Host "`nBuild FAILED" -ForegroundColor Red
    }
} else {
    Write-Host "`nCMake configure FAILED" -ForegroundColor Red
}
