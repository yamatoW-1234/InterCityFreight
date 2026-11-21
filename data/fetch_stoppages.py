#!/usr/bin/env python3
"""
CargoCompass Stoppage & Facility Fetcher
----------------------------------------
Place this script in `data/` and run:
    python fetch_stoppages.py

Uses HERE Discover API (fast, ~0.15s/query) if HERE_API_KEY is set in ../.env,
or falls back to optimized non-regex OpenStreetMap Overpass queries.
Saves results to `data/stoppages.json`.
"""

import json
import os
import time
import urllib.parse
import urllib.request
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent
OVERPASS_ENDPOINTS = [
    "https://overpass.private.coffee/api/interpreter",
    "https://maps.mail.ru/osm/tools/overpass/api/interpreter",
    "https://overpass-api.de/api/interpreter",
]
USER_AGENT = "CargoCompass-StoppageFetcher/3.0 (student-daa-project)"


def load_dotenv(env_path):
    if not env_path.exists():
        return
    for raw in env_path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip().strip("\"'"))


def fetch_here_places(lat, lon, query_text, here_key, limit=3):
    params = urllib.parse.urlencode({
        "at": f"{lat},{lon}",
        "q": query_text,
        "limit": limit,
        "apiKey": here_key,
    })
    url = f"https://discover.search.hereapi.com/v1/discover?{params}"
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=10) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    out = []
    for item in data.get("items", []):
        pos = item.get("position", {})
        if "lat" in pos and "lng" in pos:
            out.append({
                "name": item.get("title", query_text),
                "lat": round(float(pos["lat"]), 5),
                "lon": round(float(pos["lng"]), 5),
            })
    return out


def fetch_point_here(lat, lon, here_key):
    return {
        "parking": fetch_here_places(lat, lon, "truck parking", here_key),
        "rest_food": fetch_here_places(lat, lon, "highway restaurant dhaba", here_key),
        "repair": fetch_here_places(lat, lon, "truck repair service", here_key),
    }


def fetch_point_overpass(lat, lon, radius_m=5000):
    """Fast Overpass query (exact tag matches, no slow regex scans)."""
    ql = (
        f"[out:json][timeout:15];("
        f'node["amenity"="parking"](around:{radius_m},{lat},{lon});'
        f'node["amenity"="restaurant"](around:{radius_m},{lat},{lon});'
        f'node["shop"="car_repair"](around:{radius_m},{lat},{lon});'
        f");out 25;"
    )
    data = urllib.parse.urlencode({"data": ql}).encode("utf-8")
    last_err = None
    for endpoint in OVERPASS_ENDPOINTS:
        try:
            req = urllib.request.Request(endpoint, data=data, headers={"User-Agent": USER_AGENT}, method="POST")
            with urllib.request.urlopen(req, timeout=18) as resp:
                res = json.loads(resp.read().decode("utf-8"))
            parking, rest_food, repair = [], [], []
            for el in res.get("elements", []):
                tags = el.get("tags", {})
                name = tags.get("name") or tags.get("brand")
                elat, elon = el.get("lat"), el.get("lon")
                if elat is None or elon is None:
                    continue
                if tags.get("amenity") == "parking" and len(parking) < 4:
                    parking.append({"name": name or "Highway Parking", "lat": round(elat, 5), "lon": round(elon, 5)})
                elif tags.get("amenity") == "restaurant" and len(rest_food) < 4:
                    rest_food.append({"name": name or "Highway Restaurant", "lat": round(elat, 5), "lon": round(elon, 5)})
                elif tags.get("shop") == "car_repair" and len(repair) < 4:
                    repair.append({"name": name or "Vehicle Repair", "lat": round(elat, 5), "lon": round(elon, 5)})
            return {"parking": parking, "rest_food": rest_food, "repair": repair}
        except Exception as exc:
            last_err = exc
    raise last_err


def main():
    load_dotenv(DATA_DIR.parent / ".env")
    here_key = os.environ.get("HERE_API_KEY", "").strip()
    mode = "HERE Discover API" if here_key else "Overpass API (fast mode)"
    print(f"Fetching stoppages using: {mode}")

    cities_file = DATA_DIR / "cities.json"
    roads_file = DATA_DIR / "roads.json"
    if not cities_file.exists() or not roads_file.exists():
        raise FileNotFoundError("data/cities.json and data/roads.json are required.")

    cities = json.loads(cities_file.read_text(encoding="utf-8")).get("cities", [])
    roads = json.loads(roads_file.read_text(encoding="utf-8")).get("roads", [])
    city_map = {c["id"]: c for c in cities}

    by_city = {}
    print(f"1) Fetching facilities around {len(cities)} cities...")
    for c in cities:
        cid = str(c["id"])
        try:
            fac = fetch_point_here(c["lat"], c["lon"], here_key) if here_key else fetch_point_overpass(c["lat"], c["lon"])
            by_city[cid] = {"city_id": c["id"], "city_name": c["name"], **fac}
            print(f"   [{c['id']:02d}] {c['name']}: {len(fac['parking'])}P / {len(fac['rest_food'])}F / {len(fac['repair'])}R")
        except Exception as exc:
            print(f"   [WARN] {c['name']}: {exc}")
            by_city[cid] = {"city_id": c["id"], "city_name": c["name"], "parking": [], "rest_food": [], "repair": []}
        time.sleep(0.1 if here_key else 0.4)

    seen_pairs = set()
    corridors = []
    for r in roads:
        a, b = min(r["from"], r["to"]), max(r["from"], r["to"])
        if (a, b) not in seen_pairs:
            seen_pairs.add((a, b))
            corridors.append((a, b, r.get("highway", "Highway")))

    by_corridor = {}
    print(f"\n2) Fetching mid-highway facilities along {len(corridors)} corridors (midpoint)...")
    for u, v, hw in corridors:
        c1, c2 = city_map[u], city_map[v]
        mid_lat = round((c1["lat"] + c2["lat"]) / 2.0, 4)
        mid_lon = round((c1["lon"] + c2["lon"]) / 2.0, 4)
        key = f"{u}-{v}"
        try:
            fac = fetch_point_here(mid_lat, mid_lon, here_key) if here_key else fetch_point_overpass(mid_lat, mid_lon, radius_m=15000)
            by_corridor[key] = {"from": u, "to": v, "highway": hw, **fac}
            print(f"   [{key}] {c1['name']} <-> {c2['name']} ({hw}): {len(fac['parking'])}P / {len(fac['rest_food'])}F / {len(fac['repair'])}R")
        except Exception as exc:
            print(f"   [WARN] {c1['name']}<->{c2['name']}: {exc}")
            by_corridor[key] = {"from": u, "to": v, "highway": hw, "parking": [], "rest_food": [], "repair": []}
        time.sleep(0.1 if here_key else 0.4)

    out_file = DATA_DIR / "stoppages.json"
    out_file.write_text(json.dumps({"by_city": by_city, "by_corridor": by_corridor}, indent=2), encoding="utf-8")
    print(f"\nSaved {out_file}")


if __name__ == "__main__":
    main()