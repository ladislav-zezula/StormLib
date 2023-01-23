:: Build file for Visual Studio 2008 and 2017
@echo off

:: Save the values of INCLUDE, LIB and PATH
set SAVE_INCLUDE=%INCLUDE%
set SAVE_LIB=%LIB%
set SAVE_PATH=%PATH%
set LIB_NAME=StormLib

:: Determine where the program files are, both for 64-bit and 32-bit Windows
if exist "%ProgramFiles%"      set PROGRAM_FILES_X64=%ProgramFiles%
if exist "%ProgramFiles%"      set PROGRAM_FILES_DIR=%ProgramFiles%
if exist "%ProgramFiles(x86)%" set PROGRAM_FILES_DIR=%ProgramFiles(x86)%

:: Determine the installed version of Visual Studio (Prioritize Enterprise over Professional)
if exist "%PROGRAM_FILES_DIR%\Microsoft Visual Studio 9.0\VC\vcvarsall.bat"                               set VCVARS_2008=%PROGRAM_FILES_DIR%\Microsoft Visual Studio 9.0\VC\vcvarsall.bat
if exist "%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat"    set VCVARS_20xx=%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" set VCVARS_20xx=%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"   set VCVARS_20xx=%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"    set VCVARS_20xx=%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" set VCVARS_20xx=%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"   set VCVARS_20xx=%PROGRAM_FILES_DIR%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_X64%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"    set VCVARS_20xx=%PROGRAM_FILES_X64%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_X64%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" set VCVARS_20xx=%PROGRAM_FILES_X64%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat
if exist "%PROGRAM_FILES_X64%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"   set VCVARS_20xx=%PROGRAM_FILES_X64%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat

:: Build all libraries using Visual Studio 2008 and 2019
if not "x%VCVARS_2008%" == "x" call :BuildLibs "%VCVARS_2008%" x86 %LIB_NAME%_vs08.sln \vs2008
if not "x%VCVARS_2008%" == "x" call :BuildLibs "%VCVARS_2008%" x64 %LIB_NAME%_vs08.sln \vs2008
if not "x%VCVARS_20xx%" == "x" call :BuildLibs "%VCVARS_20xx%" x86 %LIB_NAME%_vs22.sln
if not "x%VCVARS_20xx%" == "x" call :BuildLibs "%VCVARS_20xx%" x64 %LIB_NAME%_vs22.sln
goto:eof

::-----------------------------------------------------------------------------
:: Build all 8 configurations of the library
::
:: Parameters:
::
::   %1     Full path to the VCVARS.BAT file
::   %2     Target build platform (x86 or x64)
::   %3     Plain name of the .sln solution file ("StormLib_vs##.sln")
::   %4     Subdirectory for the target folder of the library ("\vs2008" or "")
::

:BuildLibs
if not exist %1 goto:eof
call %1 %2
if "%2" == "x86" set SLN_TRG=Win32
if "%2" == "x86" set LIB_TRG=lib32%4
if "%2" == "x64" set SLN_TRG=x64
if "%2" == "x64" set LIB_TRG=lib64%4

call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% DebugAD
call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% DebugAS
call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% DebugUD
call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% DebugUS
call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% ReleaseAD
call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% ReleaseAS
call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% ReleaseUD
call :BuildAndCopyLib %3 %SLN_TRG% %LIB_TRG% ReleaseUS

:: Restore environment variables to the old level
set INCLUDE=%SAVE_INCLUDE%
set LIB=%SAVE_LIB%
set PATH=%SAVE_PATH%
set VSINSTALLDIR=
set VCINSTALLDIR=
set DevEnvDir=
goto:eof

::-----------------------------------------------------------------------------
:: Build and update a particular subvariant of the library
::
:: Parameters:
::
::   %1     Plain name of the .sln solution file ("StormLib_vs##.sln")
::   %2     Target build platform ("Win32" or "x64")
::   %3     Target directory for the library ("lib32", "lib32\vs2008", "lib64" or "lib64\vs2008")
::   %4     Subvariant of the library ("DebugAD", "ReleaseUS", ...)
::

:BuildAndCopyLib
if not exist %1 goto:eof
devenv.com %1 /project "%LIB_NAME%" /rebuild "%4|%2"
if not exist ..\aaa goto:eof
xcopy.exe /Y /D .\src\StormLib.h ..\aaa\inc >nul
xcopy.exe /Y /D .\src\StormPort.h ..\aaa\inc >nul
xcopy.exe /Y /D .\bin\StormLib\%2\%4\*.lib ..\aaa\%3 >nul
goto:eof
