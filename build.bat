@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================================
echo   ModernQuantSuite Windows Derleme Scripti
echo ========================================================

set "COMPILER="
if exist "C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin\g++.exe" (
    set "COMPILER=C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin\g++.exe"
    set "MINGW_BIN=C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin"
) else (
    where g++ >nul 2>&1
    if !errorlevel! equ 0 (
        set "COMPILER=g++"
    )
)

if "%COMPILER%"=="" (
    echo [HATA] g++ derleyicisi bulunamadi!
    pause
    exit /b 1
)

if not "%MINGW_BIN%"=="" (
    set "PATH=%MINGW_BIN%;%PATH%"
)

if not exist bin mkdir bin

echo [*] Modern C++20 quant_suite derleniyor...
"%COMPILER%" -std=c++20 -O3 -DBOOST_JSON_NO_LIB -static-libgcc -static-libstdc++ ^
    -I include ^
    -I deps\boost_1_84_0 ^
    -I deps\eigen-3.4.0 ^
    -I deps\curl-8.23.0_8-win64-mingw\include ^
    src\main.cpp ^
    -L deps\curl-8.23.0_8-win64-mingw\lib ^
    -lcurl -lws2_32 -lcrypt32 -lwldap32 ^
    -o bin\quant_suite.exe

if !errorlevel! neq 0 (
    echo [HATA] Derleme basarisiz oldu!
    pause
    exit /b 1
)

if exist deps\curl-8.23.0_8-win64-mingw\bin\libcurl-x64.dll (
    copy /y deps\curl-8.23.0_8-win64-mingw\bin\libcurl-x64.dll bin\ >nul
)
if exist "%MINGW_BIN%\libwinpthread-1.dll" (
    copy /y "%MINGW_BIN%\libwinpthread-1.dll" bin\ >nul
)

echo [OK] Derleme basariyla tamamlandi: bin\quant_suite.exe
echo [*] Calistirmak icin run.bat veya bin\quant_suite.exe kullanabilirsiniz.

