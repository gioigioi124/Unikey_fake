@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   UniKey Auto-Charset Windows Build Script
echo ===================================================

:: Determine script directory and repository root
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%..\.."
set "ROOT_DIR=%CD%"

echo Repository root: %ROOT_DIR%

:: Detect Compiler
set "CXX="

where g++ >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "CXX=g++"
    goto COMPILER_FOUND
)

where clang++ >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "CXX=clang++"
    goto COMPILER_FOUND
)

if exist "C:\llvm-mingw\llvm-mingw-20260616-ucrt-x86_64\bin\g++.exe" (
    set "CXX=C:\llvm-mingw\llvm-mingw-20260616-ucrt-x86_64\bin\g++.exe"
    set "PATH=C:\llvm-mingw\llvm-mingw-20260616-ucrt-x86_64\bin;%PATH%"
    goto COMPILER_FOUND
)

if exist "C:\Program Files\Git\mingw64\bin\g++.exe" (
    set "CXX=C:\Program Files\Git\mingw64\bin\g++.exe"
    set "PATH=C:\Program Files\Git\mingw64\bin;%PATH%"
    goto COMPILER_FOUND
)

where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "CXX=cl"
    goto COMPILER_MSVC
)

echo [ERROR] No suitable C++ compiler (g++, clang++, or cl) found.
echo Please install MinGW-w64, LLVM-MinGW, or Visual Studio C++.
exit /b 1

:COMPILER_FOUND
echo [INFO] Using Compiler: %CXX%

set "SRCS="
set "SRCS=%SRCS% src/byteio/byteio.cpp src/byteio/prehdr.cpp"
set "SRCS=%SRCS% src/vnconv/charset.cpp src/vnconv/convert.cpp src/vnconv/data.cpp src/vnconv/error.cpp src/vnconv/pattern.cpp"
set "SRCS=%SRCS% src/ukengine/ukengine.cpp src/ukengine/mactab.cpp src/ukengine/inputproc.cpp src/ukengine/usrkeymap.cpp"
set "SRCS=%SRCS% src/ukinterface/unikey.cpp"
set "SRCS=%SRCS% src/win32/config.cpp src/win32/autocs.cpp src/win32/kbhook.cpp src/win32/settings_dlg.cpp src/win32/main.cpp"

set "INCLUDES=-Isrc/ukinterface -Isrc/ukengine -Isrc/vnconv -Isrc/byteio -Isrc/win32"
set "CFLAGS=-O2 -std=c++11 -include src/win32/compat.h -Wno-c++11-narrowing -Wno-writable-strings -Wno-invalid-source-encoding -DUNIKEYHOOK -D_WIN32 -DWIN32"
set "LDFLAGS=-static -mwindows -lpsapi -lshell32 -luser32 -lgdi32 -ladvapi32"

echo [BUILD] Compiling and linking UniKeyAuto.exe...
%CXX% %CFLAGS% %INCLUDES% %SRCS% -o UniKeyAuto.exe %LDFLAGS%

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed!
    exit /b %ERRORLEVEL%
)

goto PACKAGE_OUTPUT

:COMPILER_MSVC
echo [INFO] Using MSVC Compiler: cl.exe
echo [BUILD] Compiling with MSVC...
cl.exe /O2 /EHsc /DUNIKEYHOOK /D_WIN32 /DWIN32 /Isrc\ukinterface /Isrc\ukengine /Isrc\vnconv /Isrc\byteio /Isrc\win32 /FIsrc\win32\compat.h ^
    src\byteio\byteio.cpp src\byteio\prehdr.cpp ^
    src\vnconv\charset.cpp src\vnconv\convert.cpp src\vnconv\data.cpp src\vnconv\error.cpp src\vnconv\pattern.cpp ^
    src\ukengine\ukengine.cpp src\ukengine\mactab.cpp src\ukengine\inputproc.cpp src\ukengine\usrkeymap.cpp ^
    src\ukinterface\unikey.cpp ^
    src\win32\config.cpp src\win32\autocs.cpp src\win32\kbhook.cpp src\win32\settings_dlg.cpp src\win32\main.cpp ^
    /Fe:UniKeyAuto.exe /link /SUBSYSTEM:WINDOWS psapi.lib shell32.lib user32.lib gdi32.lib advapi32.lib

if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSVC Compilation failed!
    exit /b %ERRORLEVEL%
)

:PACKAGE_OUTPUT
echo [SUCCESS] UniKeyAuto.exe built successfully!

:: Create portable distribution directory
echo [PACKAGE] Assembling portable release in 'dist\'...
if not exist "dist" mkdir "dist"

copy /y "UniKeyAuto.exe" "dist\UniKeyAuto.exe" >nul
copy /y "src\win32\app_rules.ini" "dist\app_rules.ini" >nul
copy /y "src\win32\README.txt" "dist\README.txt" >nul

echo [DONE] Portable package created in: %ROOT_DIR%\dist
echo   - dist\UniKeyAuto.exe
echo   - dist\app_rules.ini
echo   - dist\README.txt
echo ===================================================

exit /b 0
