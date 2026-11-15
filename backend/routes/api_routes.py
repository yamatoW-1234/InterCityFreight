"""
api_routes.py — REST API blueprint. Every endpoint delegates the real work
to the C++ engine via engine_bridge; Python performs no algorithmic work.
"""
from flask import Blueprint, request, jsonify

from engine_bridge import call_engine, EngineError, find_engine
from services.analytics import analytics

api_bp = Blueprint("api", __name__)


def _run(payload: dict):
    try:
        return call_engine(payload), None
    except EngineError as exc:
        return None, str(exc)


def _err(message: str):
    return jsonify({"status": "error", "message": message}), 500


# ---- Network & meta -------------------------------------------------------
@api_bp.get("/network")
def network():
    res, err = _run({"action": "network"})
    if err:
        return _err(err)
    return jsonify(res)


@api_bp.get("/meta")
def meta():
    res, err = _run({"action": "meta"})
    if err:
        return _err(err)
    return jsonify(res)


@api_bp.get("/health")
def health():
    engine = find_engine()
    return jsonify({
        "status": "ok",
        "engine_built": engine is not None,
        "engine_path": str(engine) if engine else None,
    })


# ---- Routing --------------------------------------------------------------
@api_bp.post("/route")
def route():
    body = request.get_json(silent=True) or {}
    payload = {
        "action": "route",
        "source": body.get("source", ""),
        "destination": body.get("destination", ""),
        "objective": body.get("objective", "fastest"),
        "budget_limit": body.get("budget_limit", 0),
        "time_limit_hours": body.get("time_limit_hours", 0),
    }
    res, err = _run(payload)
    if err:
        return _err(err)
    if res.get("status") == "ok":
        analytics.record({
            "source": payload["source"],
            "destination": payload["destination"],
            "objective": payload["objective"],
            "algorithm": res.get("algorithm"),
            "total_distance_km": res.get("total_distance_km"),
            "total_time_hours": res.get("total_time_hours"),
            "total_cost": res.get("total_cost"),
            "execution_time_ms": res.get("execution_time_ms"),
        })
    return jsonify(res)


@api_bp.post("/compare")
def compare():
    body = request.get_json(silent=True) or {}
    payload = {
        "action": "compare",
        "source": body.get("source", ""),
        "destination": body.get("destination", ""),
    }
    res, err = _run(payload)
    if err:
        return _err(err)
    return jsonify(res)


@api_bp.post("/mst")
def mst():
    body = request.get_json(silent=True) or {}
    payload = {"action": "mst", "hubs": body.get("hubs", [])}
    res, err = _run(payload)
    if err:
        return _err(err)
    return jsonify(res)


# ---- Analysis & visualization --------------------------------------------
@api_bp.post("/benchmark")
def benchmark():
    body = request.get_json(silent=True) or {}
    payload = {"action": "benchmark", "sizes": body.get("sizes", [10, 25, 50, 100, 200])}
    res, err = _run(payload)
    if err:
        return _err(err)
    return jsonify(res)


@api_bp.post("/avl-demo")
def avl_demo():
    body = request.get_json(silent=True) or {}
    payload = {
        "action": "avl_demo",
        "insert": body.get("insert", []),
        "delete": body.get("delete", []),
    }
    res, err = _run(payload)
    if err:
        return _err(err)
    return jsonify(res)


@api_bp.post("/btree-demo")
def btree_demo():
    body = request.get_json(silent=True) or {}
    payload = {
        "action": "btree_demo",
        "min_degree": body.get("min_degree", 3),
        "insert": body.get("insert", []),
    }
    res, err = _run(payload)
    if err:
        return _err(err)
    return jsonify(res)


@api_bp.get("/selfcheck")
def selfcheck():
    res, err = _run({"action": "selfcheck"})
    if err:
        return _err(err)
    return jsonify(res)


# ---- Dashboard analytics --------------------------------------------------
@api_bp.get("/recent")
def recent():
    return jsonify({"status": "ok", "runs": analytics.recent(10)})


@api_bp.get("/summary")
def summary():
    return jsonify({
        "status": "ok",
        "summary": analytics.summary(),
        "runs": analytics.recent(10),
    })