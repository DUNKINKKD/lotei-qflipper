@echo off
rem ============================================================================
rem  Incremental build (jom only, ~1 min). Run a full build_pink.bat first.
rem  Paths are overridable via env vars; defaults match a standard aqt install.
rem    set QT_DIR=...      set VS_VCVARS=...      set JOM=...
rem ============================================================================
if "%VS_VCVARS%"=="" set "VS_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if "%QT_DIR%"==""    set "QT_DIR=C:\Qt\6.4.2\msvc2019_64"
if "%JOM%"==""       set "JOM=C:\Qt\jom\jom.exe"

call "%VS_VCVARS%" >nul
set "PATH=%QT_DIR%\bin;%PATH%"
cd /d "%~dp0build"
"%JOM%" || exit /b 4
echo BUILD_OK
