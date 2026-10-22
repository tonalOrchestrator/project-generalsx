#!/usr/bin/env python3
"""Compare two autopilot_scenarios.py runs (their summary.json files).

usage: compare_summaries.py <before-dir> <after-dir>   (each a logs/autopilot-scenarios/<timestamp> directory)

Prints one line per scenario that differs (earned, spent, buildings, lost, units, alive) and a totals line.
Exit code 0 when every scenario is identical (a pure refactor must give this), 1 otherwise.
The runner uses the INSTALLED binary: copy the build to ~/GeneralsX/GeneralsZH/ before each run or the comparison proves nothing.
"""
import json
import sys
from pathlib import Path

KEYS = ("earned", "spent", "buildings", "lost", "units", "alive")


def load(d):
    rows = json.loads((Path(d) / "summary.json").read_text())
    return {r["name"]: r for r in rows}


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    a, b = load(sys.argv[1]), load(sys.argv[2])
    differing = 0
    for name in sorted(set(a) | set(b)):
        if name not in a or name not in b:
            print(f"{name}: only in {'before' if name in a else 'after'}")
            differing += 1
            continue
        diff = {k: (a[name].get(k), b[name].get(k)) for k in KEYS if a[name].get(k) != b[name].get(k)}
        if diff:
            differing += 1
            print(f"{name}: " + ", ".join(f"{k} {x} -> {y}" for k, (x, y) in diff.items()))
    common = [n for n in a if n in b]
    tot = lambda rows, k: sum(rows[n].get(k) or 0 for n in common)
    print(f"{len(common)} scenarios, {differing} differ; alive {tot(a, 'alive')} -> {tot(b, 'alive')}, "
          f"earned {tot(a, 'earned')} -> {tot(b, 'earned')}, units {tot(a, 'units')} -> {tot(b, 'units')}")
    return 0 if differing == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
