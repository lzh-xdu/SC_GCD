param([switch]$Run)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $projectRoot 'build'
& cmake -S $projectRoot -B $buildPath -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
& ctest --test-dir $buildPath --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
if ($Run) {
    Push-Location $buildPath
    try {
        & ./day01_basics.exe
        if ($LASTEXITCODE -ne 0) { throw 'Example failed.' }
    } finally { Pop-Location }
}
