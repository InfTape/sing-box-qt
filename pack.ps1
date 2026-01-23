$ErrorActionPreference = "Stop"

# Paths
$qtRoot = $env:QT_ROOT
if ([string]::IsNullOrWhiteSpace($qtRoot)) {
    $qtRoot = "C:\Qt\6.10.1\msvc2022_64"
}
$windeployqt = Join-Path $qtRoot "bin\windeployqt.exe"

$cmake = $env:CMAKE_BIN
if ([string]::IsNullOrWhiteSpace($cmake)) {
    $cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
}
if (Test-Path $cmake) {
    $cmakeCmd = $cmake
} else {
    $cmakeCmd = "cmake"
}

$buildDir = "build"
$distDir = "dist"
$exeName = "sing-box-qt.exe"
$exePath = Join-Path $buildDir "Release\$exeName"
$zipName = "sing-box-qt-portable.zip"
$desktopPath = [Environment]::GetFolderPath('Desktop')
$desktopExePath = Join-Path $desktopPath $exeName

if (!(Test-Path $windeployqt)) {
    throw "windeployqt not found: $windeployqt. Set QT_ROOT or update script path."
}
if (!(Get-Command $cmakeCmd -ErrorAction SilentlyContinue)) {
    throw "cmake not found. Set CMAKE_BIN or install CMake and add it to PATH."
}

# Configure if needed
if (!(Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    Write-Host "Configuring project..." -ForegroundColor Cyan
    & $cmakeCmd -S . -B $buildDir -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="$qtRoot"
}

# Build
Write-Host "Building (Release)..." -ForegroundColor Cyan
& $cmakeCmd --build $buildDir --config Release

if (!(Test-Path $exePath)) {
    throw "Release exe not found: $exePath"
}

# Prepare dist
if (Test-Path $distDir) { Remove-Item $distDir -Recurse -Force }
New-Item -ItemType Directory $distDir | Out-Null
Copy-Item $exePath $distDir

# Deploy Qt runtime
Write-Host "Deploying Qt runtime..." -ForegroundColor Cyan
& $windeployqt --release --compiler-runtime (Join-Path $distDir $exeName)

# Optional: bundle kernel if you want (currently app downloads kernel itself)
# Copy-Item "C:\path\to\sing-box.exe" $distDir

# Copy exe to desktop for convenience
try {
    Copy-Item $exePath $desktopExePath -Force
    Write-Host "Copied to desktop: $desktopExePath" -ForegroundColor Cyan
} catch {
    Write-Warning "复制到桌面失败：$($_.Exception.Message)"
}

# Zip
if (Test-Path $zipName) { Remove-Item $zipName -Force }
Compress-Archive -Path "$distDir\*" -DestinationPath $zipName -Force

Write-Host "Portable zip created: $zipName" -ForegroundColor Green
