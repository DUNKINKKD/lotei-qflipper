@echo off
rem ============================================================================
rem  Full build (qmake + jom/nmake). Run once, or after a .pro change.
rem  Paths are overridable via env vars; defaults match a standard aqt install.
rem    set QT_DIR=...      set VS_VCVARS=...      set JOM=...
rem  NOTE: keep Qt on a DIFFERENT drive than the source, or qmake makes broken
rem  relative paths and the build fails in dfu.
rem ============================================================================
if "%VS_VCVARS%"=="" set "VS_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if "%QT_DIR%"==""    set "QT_DIR=C:\Qt\6.4.2\msvc2019_64"

call "%VS_VCVARS%" >nul
set "PATH=%QT_DIR%\bin;%PATH%"

if "%JOM%"=="" if exist "C:\Qt\jom\jom.exe" set "JOM=C:\Qt\jom\jom.exe"
set "MAKE=nmake"
if not "%JOM%"=="" set "MAKE=%JOM%"

cd /d "%~dp0"
if not exist build mkdir build
cd build
echo === qmake ===
qmake ..\qFlipper.pro -spec win32-msvc "CONFIG+=qtquickcompiler" || exit /b 2
echo === %MAKE% qmake_all ===
"%MAKE%" qmake_all || exit /b 3
echo === %MAKE% build ===
"%MAKE%" || exit /b 4
echo BUILD_OK
