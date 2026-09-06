$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencyPath = Join-Path $projectRoot 'third_party/systemc'
if (-not (Test-Path (Join-Path $dependencyPath 'CMakeLists.txt'))) {
    & git -c http.sslBackend=openssl clone --depth 1 --branch 3.0.1 https://github.com/accellera-official/systemc.git $dependencyPath
    if ($LASTEXITCODE -ne 0) { throw 'SystemC download failed.' }
}
$revision = & git -C $dependencyPath rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $revision -ne '11ad094d282fd5330b27ab57f90f9d231a763da1') {
    throw 'Expected SystemC 3.0.1 commit 11ad094d282fd5330b27ab57f90f9d231a763da1.'
}
Write-Host "SystemC 3.0.1 ready: $revision"
