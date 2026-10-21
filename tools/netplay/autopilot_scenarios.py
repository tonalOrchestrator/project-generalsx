#!/usr/bin/env python3
"""Autopilot scenario matrix: one human on Autopilot vs one Easy AI, headless, one machine.

Runs each (faction, level) scenario through the LAN harness (-lanHumans 1) and checks that the autopilot does what it
should, so a regression shows up as a failed row instead of a debugging session.

    tools/netplay/autopilot_scenarios.py                       # full matrix
    tools/netplay/autopilot_scenarios.py --factions FactionGLA --levels 4 --minutes 3
    tools/netplay/autopilot_scenarios.py --list

Checks per scenario: the player is alive, the run finished, few failed placements, the supply center went up, income
started, and the base grew. Logs and summary.json go to logs/autopilot-scenarios/<timestamp>/.
Exit status is the number of failed scenarios.
"""
import argparse, json, os, re, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME_DIR = Path(os.environ.get("LINUX_GAME_DIR", Path.home() / "GeneralsX/GeneralsZH"))

FACTIONS = [
    "FactionAmerica", "FactionAmericaAirForceGeneral", "FactionAmericaLaserGeneral", "FactionAmericaSuperWeaponGeneral",
    "FactionChina", "FactionChinaInfantryGeneral", "FactionChinaNukeGeneral", "FactionChinaTankGeneral",
    "FactionGLA", "FactionGLADemolitionGeneral", "FactionGLAStealthGeneral", "FactionGLAToxinGeneral",
]
LEVEL_NAMES = {1: "easy", 2: "normal", 3: "hard", 4: "wallet", 5: "explore", 0: "custom"}


def run_scenario(faction, level, args, outdir):
    name = f"{faction}-{LEVEL_NAMES[level]}"
    log = outdir / f"{name}.log"
    cmd = [
        "./GeneralsXZH", "-headless", "-lanhost", args.map, "-lanHumans", "1", "-lanSlots", args.enemy,
        "-lanSeed", str(args.seed), "-lanMaxMinutes", str(args.minutes),
        "-lanHostTemplate", faction, "-lanAutopilotSchedule", args.schedule or f"5:{level}",
    ] + ([] if args.realtime else ["-lanFast"])
    env = dict(os.environ, LD_LIBRARY_PATH=str(GAME_DIR))
    started = time.time()
    try:
        with open(log, "w") as fh:
            subprocess.run(cmd, cwd=GAME_DIR, env=env, stdout=fh, stderr=subprocess.STDOUT,
                           timeout=args.minutes * 60 * (2 if args.realtime else 1) + 240)
    except subprocess.TimeoutExpired:
        pass
    return name, log, time.time() - started


def analyse(log, level, minutes, faction):
    text = Path(log).read_text(errors="replace")
    res = {"finished": False, "alive": False, "failed_places": text.count("-> FAILED"),
           "supply_placed": False, "first_income_s": None, "earned": 0, "spent": 0, "buildings": 0, "lost": 0, "units": 0, "enemy_killed": 0,
           "expansions": text.count("autopilot supply expansion:"), "template": "?"}
    for line in text.splitlines():
        if line.startswith("NETPLAY progress ") or line.startswith("NETPLAY_RESULT "):
            try:
                d = json.loads(line[line.index("{"):])
            except ValueError:
                continue
            p = next((q for q in d["players"] if q["type"] == "human"), None)
            if not p:
                continue
            res["template"] = p["template"]
            res["alive"] = p["alive"] and not p["defeated"]
            res["earned"], res["spent"] = p["moneyEarned"], p["moneySpent"]
            res["buildings"], res["units"] = p["buildingsBuilt"], p["unitsBuilt"]
            res["lost"], res["enemy_killed"] = p["buildingsLost"], p["unitsDestroyed"]
            if p["moneyEarned"] > 0 and res["first_income_s"] is None:
                res["first_income_s"] = d["gameSeconds"]
            if line.startswith("NETPLAY_RESULT "):
                res["finished"] = True
    res["supply_placed"] = bool(re.search(r"place \S*(Supply|Stash|DropZone|Dropzone)\S* priority=1 -> placed", text))
    problems = []
    if res["template"] != faction:
        problems.append(f"ran as {res['template']}, not {faction} (bad template name?)")
    if not res["finished"]:
        problems.append("did not finish")
    if not res["alive"]:
        problems.append("player not alive")
    if res["failed_places"] > 2:
        problems.append(f"{res['failed_places']} failed placements")
    if level not in (0, 4) and not res["supply_placed"] and res["earned"] == 0:
        problems.append("no supply center and no income")
    if res["earned"] < 500 and minutes >= 4:
        problems.append("income under $500")
    if res["buildings"] < 3:
        problems.append("base did not grow")
    res["problems"] = problems
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--factions", nargs="*", default=FACTIONS)
    ap.add_argument("--levels", nargs="*", type=int, default=[2, 4])
    ap.add_argument("--map", default="Dark Night")
    ap.add_argument("--minutes", type=int, default=4)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--schedule", help='custom autopilot schedule "sec:level,..." instead of one level (e.g. a stress test); counts as level 0')
    ap.add_argument("--enemy", default="E", help="enemy AI slot spec: E, M or H (optionally E:FactionChina), default E")
    ap.add_argument("--realtime", action="store_true", help="do not use -lanFast (run at game speed)")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    scenarios = [(f, 0) for f in args.factions] if args.schedule else [(f, l) for f in args.factions for l in args.levels]
    if args.list:
        for f, l in scenarios:
            print(f"{f}-{LEVEL_NAMES[l]}")
        return 0

    outdir = ROOT / "logs" / "autopilot-scenarios" / time.strftime("%Y%m%d-%H%M%S")
    outdir.mkdir(parents=True, exist_ok=True)
    rows, failed = [], 0
    print(f"{len(scenarios)} scenarios, {args.minutes} game-minutes each, logs in {outdir}")
    for faction, level in scenarios:
        name, log, secs = run_scenario(faction, level, args, outdir)
        r = analyse(log, level, args.minutes, faction)
        r.update(name=name, level=level, faction=faction, real_seconds=round(secs))
        rows.append(r)
        status = "PASS" if not r["problems"] else "FAIL"
        failed += status == "FAIL"
        print(f"{status}  {name:34s} earned=${r['earned']:<6} first_income={r['first_income_s']}s "
              f"buildings={r['buildings']} lost={r['lost']} expansions={r['expansions']} failed_places={r['failed_places']}"
              + ("  <- " + "; ".join(r["problems"]) if r["problems"] else ""), flush=True)
        (outdir / "summary.json").write_text(json.dumps(rows, indent=1))
    print(f"\n{len(rows) - failed}/{len(rows)} passed")
    return failed


if __name__ == "__main__":
    sys.exit(main())
