@echo off
setlocal EnableExtensions

rem Modern 64-bit builds (GMP + OpenMP). Uses your flags.
rem Run in MSYS2 MinGW64 (or cmd with 64-bit gcc in PATH and GMP installed).

set "SRC=cc.c"
set "OUTDIR=build-64"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "CC=gcc"
set "COMMON=-Ofast -ffast-math -fomit-frame-pointer -funroll-loops -ftree-vectorize -flto -fopenmp"
set "LIBS=-lgmp"

rem Only arches valid for 64-bit -march on your toolchain
rem Note: x86-64-v2/v3/v4 are NOT valid -mtune values; we force -mtune=generic for them.
set MARCH_LIST=x86-64 x86-64-v2 x86-64-v3 x86-64-v4 nocona k8 k8-sse3 core2 nehalem westmere sandybridge ivybridge core-avx-i haswell core-avx2 broadwell skylake skylake-avx512 cascadelake icelake-client icelake-server tigerlake rocketlake alderlake raptorlake meteorlake sapphirerapids emeraldrapids graniterapids barcelona bdver1 bdver2 bdver3 bdver4 znver1 znver2 znver3 znver4 znver5 btver1 btver2 bonnell silvermont goldmont goldmont-plus tremont gracemont sierraforest

echo [modern-64] Building modern variants with %CC% %SRC%
for %%A in (%MARCH_LIST%) do call :build64 %%A

echo.
echo [modern-64] Also emitting reference builds (native ^& generic)
"%CC%" %COMMON% -march=native -mtune=native "%SRC%" -o "%OUTDIR%\cc-modern-native.exe" %LIBS%
"%CC%" %COMMON% -march=x86-64 -mtune=generic "%SRC%" -o "%OUTDIR%\cc-modern-generic.exe" %LIBS%

rem Optional: RETRO as 64-bit too (no GMP/OpenMP)
echo.
echo [retro-64] Optional RETRO 64-bit variants
for %%A in (x86-64 x86-64-v2 x86-64-v3 x86-64-v4 znver1 znver2 znver3 znver4 znver5 skylake haswell) do call :build64retro %%A

echo.
echo [modern-64] Done. Binaries in %OUTDIR%
goto :eof

:select_tune
rem Sets TUNE to a valid -mtune for the given ARCH
set "ARCH_IN=%~1"
set "TUNE=%ARCH_IN%"
if /I "%ARCH_IN%"=="x86-64"     set "TUNE=generic"
if /I "%ARCH_IN%"=="x86-64-v2"  set "TUNE=generic"
if /I "%ARCH_IN%"=="x86-64-v3"  set "TUNE=generic"
if /I "%ARCH_IN%"=="x86-64-v4"  set "TUNE=generic"
rem everything else tunes to itself (all present in your -mtune list)
exit /b

:build64
set "ARCH=%~1"
call :select_tune %ARCH%
set "OUT=%OUTDIR%\cc-modern-%ARCH%.exe"
echo.
echo [modern-64] %ARCH% march=%ARCH% mtune=%TUNE% -> %OUT%
"%CC%" %COMMON% -march=%ARCH% -mtune=%TUNE% "%SRC%" -o "%OUT%" %LIBS%
if errorlevel 1 (
  echo [modern-64] HARD FAIL %ARCH%
) else (
  echo [modern-64] OK
)
goto :eof

:build64retro
set "ARCH=%~1"
call :select_tune %ARCH%
set "OUT=%OUTDIR%\cc-retro64-%ARCH%.exe"
echo [retro-64] %ARCH% march=%ARCH% mtune=%TUNE% -> %OUT%
"%CC%" -Ofast -ffast-math -fomit-frame-pointer -funroll-loops -ftree-vectorize -flto -DRETRO -march=%ARCH% -mtune=%TUNE% "%SRC%" -o "%OUT%"
if errorlevel 1 (
  echo [retro-64] HARD FAIL %ARCH%
) else (
  echo [retro-64] OK
)
goto :eof
