$ErrorActionPreference = "Stop"

$CmakeExe = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$QtPath   = "C:\Qt\6.10.1\msvc2022_64"
$QtBin    = Join-Path $QtPath "bin"
$BuildDir = Join-Path $PSScriptRoot "build"

Write-Host "Configuring fluorescent button test..." -ForegroundColor Cyan
& $CmakeExe -S $PSScriptRoot -B $BuildDir -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="$QtPath"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuilding (Release)..." -ForegroundColor Cyan
    & $CmakeExe --build $BuildDir --config Release

    if ($LASTEXITCODE -eq 0) {
        $exePath = Join-Path $BuildDir "Release/FluorescentButtonTest.exe"

        # 准备 Qt 运行环境，拷贝依赖 + 平台插件
        $env:PATH = "$QtBin;$env:PATH"
        & (Join-Path $QtBin "windeployqt.exe") --no-compiler-runtime $exePath | Out-Null

        Write-Host "`nBuild SUCCESS, launching UI..." -ForegroundColor Green
        & $exePath
    } else {
        Write-Host "`nBuild FAILED" -ForegroundColor Red
    }
} else {
    Write-Host "`nCMake configure FAILED" -ForegroundColor Red
}
