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
$coreManagerName = "sing-box-core-manager.exe"
$zipName = "sing-box-qt-portable.zip"
$desktopPath = [Environment]::GetFolderPath('Desktop')
$desktopExePath = Join-Path $desktopPath $exeName
$desktopZipPath = Join-Path $desktopPath $zipName

if (!(Test-Path $windeployqt)) {
    throw "windeployqt not found: $windeployqt. Set QT_ROOT or update script path."
}
if (!(Get-Command $cmakeCmd -ErrorAction SilentlyContinue)) {
    throw "cmake not found. Set CMAKE_BIN or install CMake and add it to PATH."
}

# Always configure to avoid stale/broken cache and fail fast on CI
Write-Host "Configuring project..." -ForegroundColor Cyan
$configureArgs = @(
    "-S", ".",
    "-B", $buildDir,
    "-DCMAKE_PREFIX_PATH=$qtRoot",
    "-DCMAKE_BUILD_TYPE=Release"
)
& $cmakeCmd @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed"
}

# Build
Write-Host "Building (Release)..." -ForegroundColor Cyan
$Jobs = [Environment]::ProcessorCount
& $cmakeCmd --build $buildDir --config Release --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed"
}

$binaryDir = @(
    (Join-Path $buildDir "Release"),
    $buildDir
) | Where-Object {
    (Test-Path (Join-Path $_ $exeName)) -and
    (Test-Path (Join-Path $_ $coreManagerName))
} | Select-Object -First 1

if ([string]::IsNullOrWhiteSpace($binaryDir)) {
    throw "Release executables not found under: $buildDir\Release or $buildDir"
}
$exePath = Join-Path $binaryDir $exeName
$coreManagerPath = Join-Path $binaryDir $coreManagerName

# Prepare dist
if (Test-Path $distDir) { Remove-Item $distDir -Recurse -Force }
New-Item -ItemType Directory $distDir | Out-Null
Copy-Item $exePath $distDir
Copy-Item $coreManagerPath $distDir
Set-Content -Path (Join-Path $distDir "portable.flag") -Value "1" -Encoding Ascii

# Deploy Qt runtime
Write-Host "Deploying Qt runtime..." -ForegroundColor Cyan
& $windeployqt --release --no-compiler-runtime (Join-Path $distDir $exeName)

# Optional: bundle kernel if you want (currently app downloads kernel itself)
# Copy-Item "C:\path\to\sing-box.exe" $distDir

# Copy exe to desktop for convenience
try {
    Copy-Item $exePath $desktopExePath -Force
    Write-Host "Copied to desktop: $desktopExePath" -ForegroundColor Cyan
} catch {
    Write-Warning "Failed to copy to desktop: $($_.Exception.Message)"
}

# Zip
if (Test-Path $zipName) { Remove-Item $zipName -Force }
Compress-Archive -Path "$distDir\*" -DestinationPath $zipName -Force

Write-Host "Portable zip created: $zipName" -ForegroundColor Green

# Copy zip to desktop
try {
    Copy-Item $zipName $desktopZipPath -Force
    Write-Host "Copied zip to desktop: $desktopZipPath" -ForegroundColor Cyan
} catch {
    Write-Warning "Failed to copy zip to desktop: $($_.Exception.Message)"
}
