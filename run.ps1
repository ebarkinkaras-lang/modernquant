Set-Location $PSScriptRoot
if (-not (Test-Path "bin\quant_suite.exe")) {
    Write-Host "[HATA] bin\quant_suite.exe bulunamadı! Önce derlemek için .\build.bat çalıştırınız." -ForegroundColor Red
    exit 1
}
& ".\bin\quant_suite.exe" @args

