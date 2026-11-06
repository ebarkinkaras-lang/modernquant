@echo off
cd /d "%~dp0"
if not exist "bin\quant_suite.exe" (
    echo [HATA] bin\quant_suite.exe bulunamadi! Once derlemek icin build.bat calistiriniz.
    pause
    exit /b 1
)
"bin\quant_suite.exe" %*

