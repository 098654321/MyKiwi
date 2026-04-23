#!/usr/bin/env python3
"""Generate ILP MPS by C++ binary and solve with HiGHS."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
from typing import Sequence


def run_cmd(cmd: Sequence[str]) -> None:
    proc = subprocess.run(cmd, text=True, capture_output=True)
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed: {' '.join(cmd)}\n"
            f"stdout:\n{proc.stdout}\n"
            f"stderr:\n{proc.stderr}"
        )
    if proc.stdout.strip():
        print(proc.stdout.strip())


def solve_with_highs(mps_path: pathlib.Path, time_limit: float | None, show_log: bool) -> None:
    try:
        from highspy import Highs
    except Exception as exc:  # pragma: no cover
        raise RuntimeError(
            "failed to import highspy. install it with `pip install highspy`"
        ) from exc

    highs = Highs()
    highs.setOptionValue("output_flag", show_log)
    if time_limit is not None:
        highs.setOptionValue("time_limit", float(time_limit))

    read_status = highs.readModel(str(mps_path))
    if str(read_status).lower().find("error") >= 0:
        raise RuntimeError(f"HiGHS failed to read MPS: {mps_path}")

    run_status = highs.run()
    if str(run_status).lower().find("error") >= 0:
        raise RuntimeError("HiGHS run failed")

    model_status = highs.getModelStatus()
    obj = highs.getObjectiveValue()
    solution = highs.getSolution()

    values = list(solution.col_value)
    nonzeros = [(idx, val) for idx, val in enumerate(values) if abs(val) > 1e-9]

    print(f"Model status: {model_status}")
    print(f"Objective value: {obj}")
    print(f"Nonzero variables: {len(nonzeros)}")
    for idx, val in nonzeros[:20]:
        print(f"  x[{idx}] = {val}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate and solve TOB ILP with HiGHS")
    parser.add_argument("config_path", help="path to PR_tool config folder/file")
    parser.add_argument(
        "--binary",
        default="output/test_ILP",
        help="path to compiled test_ILP executable (default: output/test_ILP)",
    )
    parser.add_argument(
        "--mps-path",
        default="output/test_ILP_model.mps",
        help="output MPS path (default: output/test_ILP_model.mps)",
    )
    parser.add_argument(
        "--time-limit",
        type=float,
        default=None,
        help="HiGHS time limit in seconds",
    )
    parser.add_argument(
        "--show-log",
        action="store_true",
        help="show HiGHS internal solving logs",
    )
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parents[2]
    binary = (repo_root / args.binary).resolve() if not pathlib.Path(args.binary).is_absolute() else pathlib.Path(args.binary)
    mps_path = (repo_root / args.mps_path).resolve() if not pathlib.Path(args.mps_path).is_absolute() else pathlib.Path(args.mps_path)
    config_path = (repo_root / args.config_path).resolve() if not pathlib.Path(args.config_path).is_absolute() else pathlib.Path(args.config_path)

    if not binary.exists():
        raise RuntimeError(f"binary not found: {binary}")

    mps_path.parent.mkdir(parents=True, exist_ok=True)
    run_cmd([str(binary), str(config_path), str(mps_path)])
    print(f"MPS generated at: {mps_path}")

    solve_with_highs(mps_path, args.time_limit, args.show_log)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        raise SystemExit(1)
