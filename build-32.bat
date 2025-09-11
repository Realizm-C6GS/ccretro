@echo off
setlocal EnableExtensions

REM Retro 32-bit builds (no GMP/OpenMP). Uses your flags.
REM Run in MSYS2 MinGW32 (or plain cmd with 32-bit gcc in PATH).

set "SRC=cc.c"
set "OUTDIR=build-32"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "CC=gcc"

REM Your retro flags per target:
REM gcc -Ofast -march=native -mtune=native -ffast-math -fomit-frame-pointer -funroll-loops -ftree-vectorize -flto cc.c -o cc-retro.exe -DRETRO
set "COMMON=-Ofast -ffast-math -fomit-frame-pointer -funroll-loops -ftree-vectorize -flto -DRETRO"

REM Valid 32-bit arches for old PCs
set MARCH_LIST=i386 i486 i586 pentium i686 pentiumpro pentium2 pentium3 pentium3m pentium-m pentium4 prescott

echo [retro-32] Building with %CC% %SRC%
for %%A in (%MARCH_LIST%) do call :build32 %%A
echo.
echo [retro-32] Done. Binaries in %OUTDIR%
goto :eof

:build32
set "ARCH=%~1"
set "OUT=%OUTDIR%\cc-retro-%ARCH%.exe"
echo.
echo [retro-32] %ARCH% -> %OUT%
"%CC%" %COMMON% -march=%ARCH% -mtune=%ARCH% "%SRC%" -o "%OUT%"
if errorlevel 1 (
  echo [retro-32] Failed with -mtune=%ARCH%, retrying with -mtune=generic ...
  "%CC%" %COMMON% -march=%ARCH% -mtune=generic "%SRC%" -o "%OUT%"
  if errorlevel 1 (
    echo [retro-32] HARD FAIL: %ARCH%
  ) else (
    echo [retro-32] OK (fallback generic)
  )
) else (
  echo [retro-32] OK
)
goto :eof
