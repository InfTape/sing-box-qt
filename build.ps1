$ErrorActionPreference = "Stop"

# Configuration
$CvPath = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
# $QtPath = "C:\Qt\6.10.1\msvc2022_64" 
# Use environment variable or default path if you prefer, 
# but previous script had hardcoded path which worked.
$QtPath = "C:\Qt\6.10.1\msvc2022_64"

Write-Host "Configuring project..." -ForegroundColor Cyan
& $CvPath -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="$QtPath"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuilding project (Release)..." -ForegroundColor Cyan
    & $CvPath --build build --config Release
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nBuild SUCCESS!" -ForegroundColor Green
        Write-Host "Executable: .\build\Release\sing-box-qt.exe" -ForegroundColor Green
        Write-Host "Executable: .\build\Release\sing-box-core-manager.exe" -ForegroundColor Green
        
        # Try to run
        Write-Host "`nLaunching..."
        Start-Process ".\build\Release\sing-box-qt.exe"
    }
    else {
        Write-Host "`nBuild FAILED" -ForegroundColor Red
    }
}
else {
    Write-Host "`nCMake Configuration FAILED" -ForegroundColor Red
}
