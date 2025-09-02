@echo off
setlocal

:: --- Configuration ---
:: Set to 1 to skip the most time-consuming test: set SKIP_EXTREME=1
if not defined SKIP_EXTREME (set "SKIP_EXTREME=0")

:: Binaries are expected in .\build\modern\
set "BIN_MT=.\build\modern\ccretro.exe"
set "BIN_ST=.\build\modern\ccretro-1t.exe"

:: --- Main Script ---

:: Check that binaries exist
if not exist "%BIN_MT%" (
    echo Error: Binary not found at %BIN_MT%
    echo Build it first with: make arch=modern
    goto :eof
)
if not exist "%BIN_ST%" (
    echo Error: Binary not found at %BIN_ST%
    echo Build it first with: make arch=modern
    goto :eof
)

:: Function-like routine to run a benchmark and print the time
:run_test
    set "description=%~1"
    set "command_to_run=%~2"
    
    <nul set /p "=Running test: %description%: "

    :: Use PowerShell for accurate timing and capture the output (in total seconds)
    for /f %%t in ('powershell -Command "$Cmd = { %command_to_run% > $null }; $Result = Measure-Command -Expression $Cmd; Write-Host $Result.TotalSeconds"') do (
        set "runtime=%%t"
    )
    
    echo %runtime%s
    goto :eof

echo Running Collatz Conjecture benchmark:

:: --- Define and run benchmarks ---
call :run_test "2^64 to 2^256, 10k steps (MT)"   "%BIN_MT% -start 64 -end 256 -stepsize 10000"
call :run_test "2^64 to 2^256, 10k steps (ST)"   "%BIN_ST% -start 64 -end 256 -stepsize 10000"
call :run_test "2^8192 to 2^8193, 10k steps (MT)" "%BIN_MT% -start 8192 -end 8192 -stepsize 10000"
call :run_test "2^8192 to 2^8193, 10k steps (ST)" "%BIN_ST% -start 8192 -end 8192 -stepsize 10000"

if "%SKIP_EXTREME%"=="1" (
    echo Skipping extreme test: 2^5e6 to 2^5e6+1, 1 step (ST)
) else (
    call :run_test "2^5e6 to 2^5e6+1, 1 step (ST)" "%BIN_ST% -start 5000000 -end 5000000 -stepsize 1"
)

echo.
pause
