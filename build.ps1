Set-Location $PSScriptRoot

Write-Host "========================================================" -ForegroundColor Yellow
Write-Host "  ModernQuantSuite Windows Derleme Scripti (PowerShell) " -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Yellow

$compiler = $null
$mingwBin = $null

if (Test-Path "C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin\g++.exe") {
    $compiler = "C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin\g++.exe"
    $mingwBin = "C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin"
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
    $compiler = "g++"
}

if (-not $compiler) {
    Write-Host "[HATA] g++ derleyicisi bulunamadı!" -ForegroundColor Red
    exit 1
}

if ($mingwBin) {
    $env:PATH = "$mingwBin;$env:PATH"
}

if (-not (Test-Path "bin")) {
    New-Item -ItemType Directory -Path "bin" | Out-Null
}

if (Test-Path "bin\quant_suite.prev.exe") {
    Remove-Item "bin\quant_suite.prev.exe" -Force -ErrorAction SilentlyContinue
}
if (Test-Path "bin\quant_suite.exe") {
    Remove-Item "bin\quant_suite.exe" -Force -ErrorAction SilentlyContinue
    if (Test-Path "bin\quant_suite.exe") {
        Move-Item "bin\quant_suite.exe" "bin\quant_suite.prev.exe" -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "[*] Modern C++20 quant_suite derleniyor..." -ForegroundColor Cyan
& $compiler -std=c++20 -O3 -DBOOST_JSON_NO_LIB -static-libgcc -static-libstdc++ `
    -I include `
    -I deps\boost_1_84_0 `
    -I deps\eigen-3.4.0 `
    -I deps\curl-8.23.0_8-win64-mingw\include `
    src\main.cpp `
    -L deps\curl-8.23.0_8-win64-mingw\lib `
    -lcurl -lws2_32 -lcrypt32 -lwldap32 `
    -o bin\quant_suite.exe

if ($LASTEXITCODE -ne 0) {
    Write-Host "[HATA] Derleme başarısız oldu!" -ForegroundColor Red
    exit 1
}

if (Test-Path "deps\curl-8.23.0_8-win64-mingw\bin\libcurl-x64.dll") {
    Copy-Item "deps\curl-8.23.0_8-win64-mingw\bin\libcurl-x64.dll" -Destination "bin\" -Force -ErrorAction SilentlyContinue
}
if ($mingwBin -and (Test-Path "$mingwBin\libwinpthread-1.dll")) {
    Copy-Item "$mingwBin\libwinpthread-1.dll" -Destination "bin\" -Force -ErrorAction SilentlyContinue
}

Write-Host "[OK] Derleme başarıyla tamamlandı: bin\quant_suite.exe" -ForegroundColor Green
Write-Host "[*] Çalıştırmak için .\run.ps1 veya .\run.bat kullanabilirsiniz." -ForegroundColor Cyan

