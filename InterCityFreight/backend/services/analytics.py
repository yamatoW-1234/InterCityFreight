"""
analytics.py — In-memory + file-backed store of recent route calculations.

Feeds the dashboard's "Recent route calculations" and "Performance summary"
widgets (Module 1). Data originates from real backend responses only.
"""
import json
import time
from collections import deque
from pathlib import Path

STORE = Path(__file__).resolve().parent.parent / "recent_runs.json"


class AnalyticsService:
    def __init__(self, maxlen: int = 50):
        self.maxlen = maxlen
        self._runs = deque(maxlen=maxlen)
        self._load()

    def _load(self):
        try:
            if STORE.exists():
                data = json.loads(STORE.read_text())
                for item in data[: self.maxlen]:
                    self._runs.append(item)
        except Exception:
            pass

    def _save(self):
        try:
            STORE.write_text(json.dumps(list(self._runs), indent=2))
        except Exception:
            pass

    def record(self, entry: dict):
        entry["timestamp"] = time.time()
        self._runs.appendleft(entry)
        self._save()

    def recent(self, limit: int = 10):
        return list(self._runs)[:limit]

    def summary(self) -> dict:
        runs = list(self._runs)
        if not runs:
            return {"total_runs": 0, "avg_execution_ms": 0, "algorithm_usage": {}}
        execs = [r.get("execution_time_ms", 0) or 0 for r in runs]
        algos = {}
        for r in runs:
            a = r.get("algorithm", "unknown")
            algos[a] = algos.get(a, 0) + 1
        return {
            "total_runs": len(runs),
            "avg_execution_ms": sum(execs) / len(execs),
            "algorithm_usage": algos,
        }


analytics = AnalyticsService()