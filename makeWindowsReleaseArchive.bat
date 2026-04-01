@echo off
REM makeWindowsReleaseArchive.bat - Create a Windows release ZIP archive

setlocal enabledelayedexpansion

set OSName=Windows
set releaseDirName=TournamentTrivia_%OSName%

REM Extract version from doorset.h
for /f "tokens=3 delims= " %%a in ('findstr DOOR_VERSION source\trivia\doorset.h') do set version=%%~a
REM Remove quotes from version
set version=%version:"=%
REM Version without dot for filename
set versionWithoutDot=%version:.=%

REM Clean up any previous release directory
if exist "%releaseDirName%" rmdir /s /q "%releaseDirName%"

REM Create release directory
mkdir "%releaseDirName%"

REM Copy release files, excluding .exe files
for %%f in (release\*) do (
    if /i not "%%~xf"==".exe" (
        copy /y "%%f" "%releaseDirName%\" >nul
    )
)

REM Copy built executables from VS build output
copy /y vs\bin\Release\trivsrv.exe "%releaseDirName%\" >nul
copy /y vs\bin\Release\trivsync.exe "%releaseDirName%\" >nul
copy /y vs\bin\Release\triv32.exe "%releaseDirName%\" >nul

REM Create zip file (unless --no-zip is passed, e.g. for CI artifact uploads)
if /i "%~1"=="--no-zip" (
    echo Release directory prepared: %releaseDirName%
) else (
    set zipName=TournamentTrivia_%versionWithoutDot%_%OSName%.zip
    if exist "%zipName%" del "%zipName%"
    tar -a -cf "%zipName%" "%releaseDirName%"
    rmdir /s /q "%releaseDirName%"
    echo.
    echo Release archive created: %zipName%
)
