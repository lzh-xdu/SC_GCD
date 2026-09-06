param(
    [switch]$Run,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [string]$BuildDirectory = 'build',
    [switch]$SkipTests,
    [string]$ToolchainBin = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $projectRoot $BuildDirectory
if ($ToolchainBin) { $env:PATH = "$ToolchainBin;$env:PATH" }
& cmake -S $projectRoot -B $buildPath -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
if (-not $SkipTests) {
    & ctest --test-dir $buildPath --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
if ($Run) {
    Push-Location $buildPath
    try {
        & ./day01_basics.exe
        if ($LASTEXITCODE -ne 0) { throw 'Example failed.' }
    } finally { Pop-Location }
}
