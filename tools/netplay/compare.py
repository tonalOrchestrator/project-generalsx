#!/usr/bin/env python3
"""Compare the logs of a two-machine netplay run: per-minute state CRCs and final player stats."""
import json, re, sys, os

out = sys.argv[1]

def read(name):
    p = os.path.join(out, name)
    return open(p, errors="replace").read() if os.path.exists(p) else ""

def parse(log):
    progress = []
    for m in re.finditer(r"NETPLAY progress (\{.*\})", log):
        try:
            progress.append(json.loads(m.group(1)))
        except ValueError:
            pass
    status = {}
    for m in re.finditer(r"NETPLAY status frame=(\d+) game=(\S+) real=(\d+)s crc=([0-9A-Fa-f]+)", log):
        status[int(m.group(1))] = (m.group(2), m.group(4).upper())
    res = None
    m = re.search(r"NETPLAY_RESULT (\{.*\})", log)
    if m:
        res = json.loads(m.group(1))
    events = re.findall(r"NETPLAY (?:event|error|warning)[^\n]*", log)
    return status, res, events, progress

hs, hr, he, hp = parse(read("host.log"))
js, jr, je, jp = parse(read("join.log"))

print("=== netplay summary ===")
for who, res, ev in (("host (linux)", hr, he), ("join (windows)", jr, je)):
    if res:
        print(f"{who}: frames={res['frames']} game={res['gameSeconds']}s real={res['realSeconds']}s timedOut={res['timedOut']}")
    else:
        print(f"{who}: NO RESULT")
    for e in ev[:5]:
        print("   ", e)

common = sorted(set(hs) & set(js))
bad = [f for f in common if hs[f][1] != js[f][1]]
print(f"state CRC checkpoints: host={len(hs)} join={len(js)} compared={len(common)} mismatched={len(bad)}")
for f in bad[:5]:
    if isinstance(f, int):
        print(f"   frame {f}: host {hs[f][1]} join {js[f][1]}")

if hr:
    print("players (host view):")
    for p in hr["players"]:
        print(f"   {p['name']:<14} {p['type']:<5} {p['template']:<26} alive={p['alive']!s:<5} win={p['victory']!s:<5} "
              f"built={p['unitsBuilt']:>3} lost={p['unitsLost']:>3} killed={p['unitsDestroyed']:>3} earned={p['moneyEarned']}")

KEYS = ("autopilot", "unitsBuilt", "unitsLost", "unitsDestroyed", "buildingsBuilt", "buildingsLost", "buildingsDestroyed", "moneyEarned", "moneySpent")
def strip(snap):
    return [{k: p[k] for k in KEYS + ("template", "alive")} for p in snap["players"]]

if hp:
    print("timeline (host view)  min | per player: built/lost/killed units, money earned")
    for snap in hp:
        row = "  ".join(f"{p['template'].replace('Faction','')[:12]:<12} ap{p.get('autopilot',0)} {p['unitsBuilt']:>3}/{p['unitsLost']:>3}/{p['unitsDestroyed']:>3} bld{p['buildingsBuilt']:>3} ${p['moneyEarned']:>6}" for p in snap["players"])
        print(f"   {snap['gameSeconds']//60:>3} | {row}")
    n = min(len(hp), len(jp))
    pdiff = [i for i in range(n) if strip(hp[i]) != strip(jp[i])]
    print(f"per-minute stat snapshots: host={len(hp)} join={len(jp)} compared={n} differing={len(pdiff)}")
    bad = bad + [("stats@min%d" % (hp[i]['gameSeconds'] // 60)) for i in pdiff]

same = bool(hr and jr and hr["players"] == jr["players"])
print("final stats identical on both machines:", same if (hr and jr) else "n/a")
ok = bool(hr and jr and not bad and common and same)
print("RESULT:", "IN SYNC" if ok else "DESYNC OR INCOMPLETE")
sys.exit(0 if ok else 1)
