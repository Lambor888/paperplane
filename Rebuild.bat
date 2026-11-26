@echo off
rem One-click C++ rebuild for paperPlane:
rem   1. asks the Unreal Editor to close (you still get its "save changes?" prompt)
rem   2. builds paperPlaneEditor Win64 Development
rem   3. reopens the project
rem The MCP server starts by itself via Content\Python\init_unreal.py.
setlocal
set "ENGINE=E:\UE5\UE_5.8\UE_5.8\Engine"
set "PROJECT=%~dp0paperPlane.uproject"

tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
    echo Closing Unreal Editor - answer its save prompt if one appears...
    taskkill /IM UnrealEditor.exe >nul 2>&1
    for /L %%i in (1,1,120) do (
        tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul || goto build
        timeout /t 1 /nobreak >nul
    )
    echo Unreal Editor is still open. Close it manually, then run this script again.
    pause
    exit /b 1
)

:build
echo Building paperPlaneEditor Win64 Development...
call "%ENGINE%\Build\BatchFiles\Build.bat" paperPlaneEditor Win64 Development -Project="%PROJECT%" -WaitMutex
if errorlevel 1 (
    echo.
    echo Build FAILED - see the errors above. The editor was not reopened.
    pause
    exit /b 1
)

echo Build succeeded. Opening the editor...
start "" "%ENGINE%\Binaries\Win64\UnrealEditor.exe" "%PROJECT%"
