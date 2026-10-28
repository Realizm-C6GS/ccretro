#!/usr/bin/env python3
"""CPU-only regression suite; requires GCC-compatible C compiler, GMP and OpenMP."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CC = shlex.split(os.environ.get("CC", "gcc"))
FLAGS = shlex.split(os.environ.get("CFLAGS", "-O3 -Wall -Wextra"))


def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, timeout=120, **kwargs)


def reference(n):
    steps = 0
    while n > 1:
        n = 3 * n + 1 if n & 1 else n // 2
        steps += 1
    return steps


def check_cli(binary, work, threads, start, end, samples=None):
    args = [str(binary), "-start", str(start), "-end", str(end), "-log"]
    args += ["-nostep"] if samples is None else ["-stepsize", str(samples)]
    env = dict(os.environ, OMP_NUM_THREADS=str(threads), OMP_DYNAMIC="FALSE",
               OMP_THREAD_LIMIT=str(threads))
    log = work / "cc.log"
    log.unlink(missing_ok=True)
    result = run(args, cwd=work, env=env, capture_output=True)
    records = [tuple(map(int, row.split(","))) for row in log.read_text().splitlines()]
    candidates = {}
    for p in range(start, end + 1):
        base, limit = 2**p, 2**(p + 1)
        step = 1 if samples is None else (limit - base + samples - 1) // samples
        seeds = range(base, limit) if samples is None else (
            base + j * step for j in range(samples) if base + j * step < limit
        )
        candidates.update((n, reference(n)) for n in seeds)
    expected = max(candidates.values())
    for n, length in records:
        assert candidates.get(n) == length, (binary, threads, n, length)
    # Parallel scheduling can choose any seed tied for the maximum.
    assert records[-1][1] == expected, (binary, threads, records[-1], expected)
    match = re.search(r"Longest length=(\d+)", result.stdout)
    assert match and int(match[1]) == expected, result.stdout
    return expected


def main():
    with tempfile.TemporaryDirectory(prefix="ccretro-tests-") as directory:
        work = Path(directory)
        for label, extra in (("int128", []), ("portable", ["-U__SIZEOF_INT128__"])):
            helper = work / ("helpers-" + label)
            binary = work / ("modern-" + label)
            for source, output in ((ROOT / "tests/test_collatz.c", helper),
                                   (ROOT / "cc.c", binary)):
                run(CC + FLAGS + extra + ["-fopenmp", str(source), "-o", str(output), "-lgmp"])
            for threads in (1, 4):
                env = dict(os.environ, OMP_NUM_THREADS=str(threads), OMP_DYNAMIC="FALSE",
                           OMP_THREAD_LIMIT=str(threads))
                result = run([str(helper)], env=env, capture_output=True)
                assert f"({threads} threads)" in result.stdout, result.stdout
                print(result.stdout, end="", flush=True)
                for start, end, samples in ((1, 12, None), (1, 12, 37), (128, 128, 37)):
                    length = check_cli(binary, work, threads, start, end, samples)
                    print(f"PASS: {label} CLI threads={threads}, exponents={start}..{end}, "
                          f"samples={samples}, maximum={length}", flush=True)
        retro = work / "retro"
        run(CC + FLAGS + ["-DRETRO", str(ROOT / "cc.c"), "-o", str(retro)])
        (work / "train.cfg").write_text("4096\n")
        length = check_cli(retro, work, 1, 1, 12)
        print(f"PASS: RETRO full sweep agrees with reference, maximum={length}")
    print("All Collatz regression tests passed.")


if __name__ == "__main__":
    main()
