# Collatz correctness regression tests

Run from the repository root:

```sh
python3 tests/run_tests.py
```

Requires Python 3.8+, a GCC-compatible C compiler, GMP development headers and
libraries, and OpenMP. The tests use only the CPU. `CC` and `CFLAGS` can override
the compiler and build flags, including any nonstandard GMP include/library paths.
Builds, logs, and RETRO cache configuration go into a temporary directory and are
removed after the run.

The C harness includes the production source with its CLI entry point renamed,
so it exercises the actual static helpers. An independent GMP reference counts
one transition at a time. Coverage includes:

- Known lengths: 1 -> 0, 2 -> 1, 3 -> 7, 7 -> 16, 27 -> 111.
- All inputs 1 through 10,000, and 10,000 odd inputs of the form `i * 2^128 + 1`.
- The 64-bit odd-step overflow boundary, immediate and delayed GMP handoffs,
  and values around `2^64`. The helper's partial count and returned state are
  checked against a reference that stops before an overflowing transition.
- Powers of two, `27 * 2^p`, and `2^p +/- 1`, for p = 128, 256, 512, 1024,
  covering GMP odd/even operations and return to the 64-bit path.
- Both the `__uint128_t` path and the portable overflow-check path (compiled
  with `-U__SIZEOF_INT128__`).
- One-thread and four-thread OpenMP execution, with actual team sizes checked.

The Python harness also runs the production CLI and checks every logged record
and the final maximum against a straightforward Python integer reference:

- Full sweep of exponents 1 through 12 (inputs 2 through 8191).
- 37 samples per range for exponents 1 through 12.
- 37 arbitrary-precision samples in the exponent-128 range.
- RETRO full sweep of exponents 1 through 12, for cross-mode agreement.

Each modern CLI case runs with both one and four threads in both compiler paths.
For ties, any starting value attaining the correct maximum is accepted because
OpenMP scheduling can change which tied record is reported.

## Validation on October 28, 2026

Environment: Linux x86-64, GCC 13.3.0, GMP 6.3.0, OpenMP/libgomp.

Before the fix, the C harness failed with 20,006 failing cases, including
3 -> 5 (expected 7), 7 -> 11 (expected 16), and 27 -> 70 (expected 111).
The immediate-overflow input 6148914691236517205 returned 64 instead of 65,
demonstrating the GMP odd-step omission independently of the 64-bit odd branch.

After the fix, the complete suite passed with both flag sets:

```sh
CFLAGS='-O3 -Wall -Wextra' python3 tests/run_tests.py
CFLAGS='-Ofast -ffast-math -fomit-frame-pointer -funroll-loops -ftree-vectorize -flto -march=x86-64 -mtune=generic -Wall -Wextra' python3 tests/run_tests.py
```

Local GMP include/library search paths were also supplied through `CFLAGS`.
All builds completed without warnings. Both thread counts and both 64-bit paths
produced the expected CLI maxima: 261 (full sweep), 173 (small sampled ranges),
and 1137 (exponent-128 samples). RETRO also returned 261 for the full sweep.
`git diff --check` passed.

The second flag set matches the optimization flags used by `build-64.bat` for
its generic modern build. Windows/DJGPP binaries and the complete CPU target
matrix were not built in this Linux environment. There is no existing Makefile
or correctness suite in the inspected checkout; the batch scripts are its
available build entry points.
