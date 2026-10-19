@echo off
rem Builds the mod against the Geode v6 SDK (GD 2.2091).
rem By default it looks for a TOOLS folder next to this project containing
rem geode-sdk-v6, geode-bindings and geode-cli. Set TOOLS, GEODE_SDK,
rem GEODE_BINDINGS_REPO_PATH or VCVARS yourself to use other locations.
setlocal
if not defined TOOLS set TOOLS=%~dp0..\TOOLS
if not defined GEODE_SDK set GEODE_SDK=%TOOLS%\geode-sdk-v6
if not defined GEODE_BINDINGS_REPO_PATH set GEODE_BINDINGS_REPO_PATH=%TOOLS%\geode-bindings
set PATH=%TOOLS%\geode-cli;%PATH%

if not defined VCVARS (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat
)
if not defined VCVARS (
    echo Couldn't find Visual Studio Build Tools. Set VCVARS to your vcvars64.bat.
    exit /b 1
)

call "%VCVARS%" >nul 2>&1 || exit /b 1
rem Prefer the CMake/Ninja bundled with VS: they use the Windows certificate
rem store for dependency downloads.
set PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cd /d "%~dp0"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGEODE_DONT_USE_CCACHE=ON || exit /b 1
cmake --build build %*
