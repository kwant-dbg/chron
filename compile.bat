@echo off
setlocal

echo Looking for Visual Studio installation...

REM Find the path to vswhere.exe
set "VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE_PATH%" (
    echo Error: vswhere.exe not found. Please ensure Visual Studio 2017 or later is installed.
    goto :eof
)

REM Find the latest MSBuild path using vswhere
for /f "usebackq tokens=*" %%i in (`"%VSWHERE_PATH%" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"`) do (
    set "MSBUILD_PATH=%%i"
)

if not defined MSBUILD_PATH (
    echo Error: MSBuild.exe could not be found.
    goto :eof
)

echo Found MSBuild at: %MSBUILD_PATH%
echo.
echo Building solution: TemporalPathfinder.sln
echo ======================================================

REM Build the solution. The /p:Configuration=Release flag builds the optimized version.
"%MSBUILD_PATH%" TemporalPathfinder.sln /p:Configuration=Release /p:Platform=x64

echo ======================================================
echo.
echo Build finished.
echo The executable can be found in the 'x64\Release' directory.
echo Run 'x64\Release\TemporalPathfinder.exe' to start the server.

endlocal

