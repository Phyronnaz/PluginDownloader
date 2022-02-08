R"(

@echo off
echo #################################################
echo ### Plugin Downloader: Installing %6
echo ### Please close Unreal to proceed to install ###
echo #################################################

echo Unreal PID: %1
echo Will be moving %2 to %3
echo Will be moving %4 to %5

:loop
tasklist | find " %1 " >nul
if not errorlevel 1 (
    timeout /t 1 >nul
    echo Waiting for Unreal to close...
    goto :loop
)

IF [%2] == [] GOTO :nextmove
IF NOT EXIST %2 GOTO :nextmove

echo Moving %2 to %3
:moveloop
robocopy %2 %3 /E /MOVE

REM https://superuser.com/questions/280425/getting-robocopy-to-return-a-proper-exit-code 0 and 1 are success
if errorlevel 0 goto :nextmove
if errorlevel 1 goto :nextmove

echo #################################################################
echo Failed to move directory
echo - Make sure no program is open in that directory or using it
echo - Make sure all Unreal instances are closed
echo It is safe to close this window to cancel the install
timeout /t 5 >nul
goto :moveloop

:nextmove

echo Moving %4 to %5
robocopy %4 %5 /E /MOVE

if errorlevel 0 goto :end
if errorlevel 1 goto :end

echo Failed to move directory, install cancelled
echo You will have to re-download the plugin through Unreal
pause
exit /b %errorlevel%

:end
start RestartEngine.bat
exit

)"