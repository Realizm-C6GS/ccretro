# CCRetro — Collatz Conjecture benchmark (GMP/OpenMP + DJGPP)

CCRetro is a Collatz Conjecture benchmark that uses GMP for big integers and supports:
- Modern native builds (multi-threaded via OpenMP, plus a single-thread build)
- Retro DOS builds via DJGPP (single-thread)

It scans powers-of-two ranges, sampling points with configurable step counts, and reports the longest sequence encountered along with total runtime. Command-line options:
- -start <power>    starting power-of-two exponent
- -end <power>      ending power-of-two exponent (inclusive)
- -stepsize <N>     number of sample steps in the range
These are supported by both binaries (multi and single) and the program prints a final line: “Total runtime: <seconds> seconds” which we use in the runner script [ccretro.c] [ccretro-nosmp.c].

## Repo layout

- ccretro.c       — multi-threaded build (OpenMP, modern targets)
- ccretro-1t.c    — single-thread build (used for DOS and also modern single-thread)
- scripts/runbench.sh — runs standard benchmarks and prints a clean summary
- Makefile        — portable build with two main modes:
  - make arch=modern  → build native ccretro and ccretro-1t
  - make arch=dos     → build many DOS .exe variants via DJGPP

## Build instructions

Prereqs (native):
- A C compiler (gcc/clang)
- GMP dev headers and libs (Ubuntu: `sudo apt-get install libgmp-dev`; macOS: `brew install gmp`)
- OpenMP (libgomp) for the multi-threaded binary (installed with gcc on most distros)

Build (modern):
- `make arch=modern`
  - Produces:
    - `build/modern/ccretro`   (OpenMP)
    - `build/modern/ccretro-1t` (single threaded)

Optionally try static linking on Linux (may require static libgomp/libgmp):
- `make arch=modern STATIC=1`

Build (DOS / DJGPP):
- Ensure a DJGPP cross-compiler and a GMP build for that toolchain
- Common triplet: `i586-pc-msdosdjgpp-gcc`
- Example:
  - `make arch=dos CC=i586-pc-msdosdjgpp-gcc`
  - If your headers/libs are in non-default paths:
    - `make arch=dos CC=i586-pc-msdosdjgpp-gcc DOS_CPPFLAGS="-I/usr/local/djgpp/include" DOS_LDFLAGS="-L/usr/local/djgpp/lib -static"`

This will emit a matrix of `.exe` files for classic x86 CPUs (i386, i486, i686, pentium2, pentium3, pentium4+SSE2, prescott+SSE3, k6, k6-2, k6-3, athlon-tbird, athlon-xp+3DNow!, athlon64, nehalem+SSE4.2) — mirroring typical DJGPP/GCC `-march` support and your previous mapping.

## Usage

Examples:
- Modern MT (OpenMP):
  - `./build/modern/ccretro -start 64 -end 256 -stepsize 10000`
- Modern 1-thread:
  - `./build/modern/ccretro-1t -start 8192 -end 8192 -stepsize 10000`
- DOS (example on real DOS or emulator):
  - `ccp3.exe -start 64 -end 64 -stepsize 10000`

Output ends with:
