"""
test_engine.py — Backend <-> C++ engine integration tests.

Runs the compiled freight_engine binary directly with JSON requests and checks
the JSON responses. Also performs a STATIC frontend consistency check (actual
visual rendering must be verified manually in a browser).

Run after building the engine (tests/run_tests.sh does this), or:
    python3 tests/test_engine.py
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENGINE = ROOT / "algorithm_engine" / "freight_engine"
DATA = ROOT / "data"

passed = 0
failed = 0


def check(cond, msg):
    global passed, failed
    if cond:
        passed += 1
        print("  [PASS]", msg)
    else:
        failed += 1
        print("  [FAIL]", msg)


def call(req):
    proc = subprocess.run(
        [str(ENGINE), str(DATA)],
        input=json.dumps(req), capture_output=True, text=True, timeout=60,
    )
    if not proc.stdout.strip():
        raise RuntimeError("engine returned no output; stderr=" + proc.stderr)
    return json.loads(proc.stdout)


def main():
    if not ENGINE.is_file():
        print("ERROR: engine binary not found at", ENGINE)
        print("Build it first (scripts/build.sh or tests/run_tests.sh).")
        sys.exit(1)

    print("=== Backend <-> C++ integration tests ===")

    net = call({"action": "network"})
    check(net.get("status") == "ok", "network action returns ok")
    check(net.get("num_cities", 0) >= 15, "dataset has >= 15 cities")

    r = call({"action": "route", "source": "Delhi", "destination": "Mumbai", "objective": "fastest"})
    check(r.get("status") == "ok", "fastest route Delhi->Mumbai ok")
    check(r.get("algorithm") == "A* Search", "fastest objective selects A*")
    check(len(r.get("path", [])) >= 2, "route returns a path")

    c = call({"action": "route", "source": "Delhi", "destination": "Mumbai", "objective": "cheapest"})
    check(c.get("status") == "ok", "cheapest route ok")
    check(c.get("algorithm") == "Bellman-Ford", "cheapest selects Bellman-Ford (negative edge present)")

    s = call({"action": "route", "source": "Delhi", "destination": "Mumbai", "objective": "shortest"})
    check(s.get("status") == "ok", "shortest route ok")
    check(s.get("algorithm") == "Dijkstra", "shortest selects Dijkstra")

    cmp = call({"action": "compare", "source": "Delhi", "destination": "Mumbai"})
    check(cmp.get("status") == "ok" and len(cmp.get("routes", [])) == 3, "compare returns 3 routes")

    m = call({"action": "mst", "hubs": ["Delhi", "Mumbai", "Bengaluru", "Kolkata"]})
    check(m.get("status") == "ok", "MST over 4 hubs ok")
    check(len(m.get("mst_edges", [])) == 3, "MST has V-1 edges")

    bad = call({"action": "route", "source": "Atlantis", "destination": "Mumbai", "objective": "fastest"})
    check(bad.get("status") == "error", "unknown source city rejected")

    sc = call({"action": "selfcheck"})
    check(sc.get("avl_self_check") is True and sc.get("btree_self_check") is True,
          "AVL + B-Tree self-checks pass")

    # ---- Static frontend consistency (not a rendering test) ----
    idx = ROOT / "frontend" / "index.html"
    check(idx.is_file(), "frontend/index.html exists")
    if idx.is_file():
        html = idx.read_text()
        for js in ["api.js", "charts.js", "map.js", "dashboard.js", "route.js",
                   "compare.js", "planner.js", "analysis.js", "visualizer.js", "app.js"]:
            check(("js/" + js) in html, f"index.html references js/{js}")
    print("NOTE: visual frontend rendering requires manual verification in a browser.")

    print(f"\n=== Integration: {passed} passed, {failed} failed ===")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()