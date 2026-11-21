#!/usr/bin/env python3
"""
CargoCompass API-Driven Data Fetcher
------------------------------------
Place this script in `data/` and run:
    python fetch_data.py

API Keys (set as environment variables before running):
  - HERE_API_KEY      (Recommended) Used for HERE Routing v8 truck speed limits + toll costs in India.
  - TOLLGURU_API_KEY  (Optional if using HERE) Used for TollGuru India commercial truck FASTag tolls.

Free APIs used (no key required):
  - OpenStreetMap Nominatim API : Geocodes city names -> lat, lon, state
  - OSRM Route API              : Discovers direct highway links, road distance (km), duration, & NH/SH ref tags
  - OpenStreetMap Overpass API  : Queries highway maxspeed tags when HERE_API_KEY is not set
"""

import json
import math
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent

def load_dotenv(env_path):
    """Load key=value pairs from project root .env into os.environ (zero dependencies)."""
    if not env_path.exists():
        return
    for raw in env_path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip().strip("\"'"))

load_dotenv(DATA_DIR.parent / ".env")
HERE_API_KEY = os.environ.get("HERE_API_KEY", "")
TOLLGURU_API_KEY = os.environ.get("TOLLGURU_API_KEY", "")


# Only city names are listed here; lat, lon, and state are fetched from Nominatim API.
CITY_NAMES = [
    # 24 user-provided cities
    "Delhi", "Ludhiana", "Amritsar", "Jaipur", "Agra", "Lucknow",
    "Kanpur", "Varanasi", "Patna", "Kolkata", "Guwahati", "Ahmedabad",
    "Vadodara", "Surat", "Mumbai", "Pune", "Indore", "Bhopal",
    "Nagpur", "Raipur", "Hyderabad", "Visakhapatnam", "Bengaluru", "Chennai",
    # 6 added Indian logistics hubs
    "Chandigarh", "Bhubaneswar", "Ranchi", "Kochi", "Coimbatore", "Vijayawada",
]

USER_AGENT = "CargoCompass-DataFetcher/2.0 (student-daa-project)"


def http_json(url, method="GET", data=None, headers=None, timeout=15):
    hdrs = {"User-Agent": USER_AGENT}
    if headers:
        hdrs.update(headers)
    body = json.dumps(data).encode("utf-8") if data is not None else None
    if body is not None:
        hdrs.setdefault("Content-Type", "application/json")
    req = urllib.request.Request(url, data=body, headers=hdrs, method=method)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def haversine_km(lat1, lon1, lat2, lon2):
    r = 6371.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def fetch_cities_from_nominatim():
    """Fetch lat, lon, and state for every city from OpenStreetMap Nominatim API."""
    cities = []
    print(f"1) Geocoding {len(CITY_NAMES)} cities via Nominatim API...")
    for idx, name in enumerate(CITY_NAMES):
        params = urllib.parse.urlencode({
            "city": name,
            "country": "India",
            "format": "json",
            "addressdetails": 1,
            "limit": 1,
        })
        url = f"https://nominatim.openstreetmap.org/search?{params}"
        res = http_json(url)
        if not res:
            raise RuntimeError(f"Nominatim returned no result for city: {name}")
        top = res[0]
        state = top.get("address", {}).get("state", "Unknown")
        city_obj = {
            "id": idx,
            "name": name,
            "state": state,
            "lat": round(float(top["lat"]), 4),
            "lon": round(float(top["lon"]), 4),
        }
        cities.append(city_obj)
        print(f"   [{idx:02d}] {name} ({state}) -> {city_obj['lat']}, {city_obj['lon']}")
        time.sleep(1.05)  # Nominatim rate limit: max 1 req/sec
    return cities


def route_passes_near_other_city(coords, c1, c2, all_cities, threshold_km=35.0):
    """Return True if an OSRM route passes close to a third hub city (so it's not a direct link)."""
    # Sample every few geometry points for fast proximity check
    step = max(1, len(coords) // 25)
    sampled = coords[::step]
    for other in all_cities:
        if other["id"] in (c1["id"], c2["id"]):
            continue
        for lon, lat in sampled:
            if haversine_km(lat, lon, other["lat"], other["lon"]) < threshold_km:
                return True
    return False


def fetch_osrm_direct_link(c1, c2, all_cities):
    """Query OSRM for road distance, duration, highway refs, and verify no intermediate hub."""
    url = (
        f"https://router.project-osrm.org/route/v1/driving/"
        f"{c1['lon']},{c1['lat']};{c2['lon']},{c2['lat']}"
        f"?overview=simplified&geometries=geojson&steps=true"
    )
    data = http_json(url, timeout=15)
    if data.get("code") != "Ok" or not data.get("routes"):
        return None

    route = data["routes"][0]
    coords = route["geometry"]["coordinates"]
    if route_passes_near_other_city(coords, c1, c2, all_cities):
        return None

    dist_km = round(route["distance"] / 1000.0, 1)
    dur_hrs = round(route["duration"] / 3600.0, 2)

    # Extract unique highway refs (e.g. NH 44, NH 48) from OSRM step metadata
    refs = []
    for leg in route.get("legs", []):
        for st in leg.get("steps", []):
            ref = st.get("ref")
            if ref:
                for token in ref.split(";"):
                    token = token.strip()
                    if token and token not in refs and ("NH" in token or "NE" in token or "AH" in token or "SH" in token):
                        refs.append(token)
    highway_name = "/".join(refs[:3]) if refs else "Highway"
    avg_speed = round(min(80.0, dist_km / max(dur_hrs, 0.1)))
    return {
        "distance_km": dist_km,
        "duration_hrs": dur_hrs,
        "highway": highway_name,
        "speed_limit_kmh": avg_speed,
    }


def fetch_here_truck_data(c1, c2, here_key):
    """Query HERE Routing v8 for commercial truck speed limits and toll costs in INR."""
    params = urllib.parse.urlencode({
        "transportMode": "truck",
        "origin": f"{c1['lat']},{c1['lon']}",
        "destination": f"{c2['lat']},{c2['lon']}",
        "return": "polyline,summary,tolls",
        "spans": "maxSpeed",
        "currency": "INR",
        "apiKey": here_key,
    })
    url = f"https://router.hereapi.com/v8/routes?{params}"
    res = http_json(url, timeout=15)
    routes = res.get("routes", [])
    if not routes:
        return None, None

    section = routes[0]["sections"][0]
    # Toll total: deduplicate by fare id and pick ONE fare per toll booth (preferring transponder/FASTag)
    toll_total = 0.0
    seen_fare_ids = set()
    for toll in section.get("tolls", []):
        fares = [f for f in toll.get("fares", []) if f.get("id") not in seen_fare_ids]
        if not fares:
            continue
        transponder_fares = [f for f in fares if "transponder" in f.get("paymentMethods", [])]
        chosen = min(
            transponder_fares or fares,
            key=lambda f: float(f.get("price", {}).get("value", 0.0)),
        )
        if chosen.get("id"):
            seen_fare_ids.add(chosen["id"])
        toll_total += float(chosen.get("price", {}).get("value", 0.0))

    # Median/max truck speed limit from spans (m/s -> km/h)
    speeds = [
        (s.get("maxSpeed") or s.get("speedLimit")) * 3.6
        for s in section.get("spans", [])
        if (s.get("maxSpeed") or s.get("speedLimit") or 0) > 0
    ]
    speed_kmh = round(min(80.0, sum(speeds) / len(speeds))) if speeds else None
    return round(toll_total), speed_kmh


def fetch_tollguru_toll(c1, c2, tollguru_key):
    """Query TollGuru API for 5-axle commercial truck FASTag toll in India."""
    url = "https://apis.tollguru.com/toll/v2/origin-destination-waypoints"
    payload = {
        "from": {"lat": c1["lat"], "lng": c1["lon"]},
        "to": {"lat": c2["lat"], "lng": c2["lon"]},
        "vehicle": {"type": "5AxlesTruck"},
        "country": "IND",
    }
    res = http_json(url, method="POST", data=payload, headers={"x-api-key": tollguru_key}, timeout=15)
    tag = res.get("routes", [{}])[0].get("costs", {}).get("tag")
    return round(float(tag)) if tag is not None else None


def discover_and_fetch_roads(cities, here_key, tollguru_key):
    """Discover direct highway edges using k-nearest hub pairs + OSRM + HERE/TollGuru."""
    roads = []
    seen_pairs = set()
    k_neighbors = 5

    print(f"\n2) Discovering direct highway links via OSRM (k={k_neighbors} nearest hubs per city)...")
    for c1 in cities:
        # Sort other cities by straight-line distance and test nearest k_neighbors
        neighbors = sorted(
            [c for c in cities if c["id"] != c1["id"]],
            key=lambda c: haversine_km(c1["lat"], c1["lon"], c["lat"], c["lon"]),
        )[:k_neighbors]

        for c2 in neighbors:
            pair = tuple(sorted((c1["id"], c2["id"])))
            if pair in seen_pairs:
                continue
            seen_pairs.add(pair)

            try:
                link = fetch_osrm_direct_link(c1, c2, cities)
            except Exception as exc:
                print(f"   [WARN] OSRM error {c1['name']}-{c2['name']}: {exc}")
                link = None

            if not link:
                continue

            toll_inr = None
            speed_kmh = link["speed_limit_kmh"]

            if here_key:
                try:
                    here_toll, here_speed = fetch_here_truck_data(c1, c2, here_key)
                    if here_toll is not None:
                        toll_inr = here_toll
                    if here_speed is not None:
                        speed_kmh = here_speed
                except Exception as exc:
                    print(f"   [WARN] HERE error {c1['name']}-{c2['name']}: {exc}")

            if toll_inr is None and tollguru_key:
                try:
                    toll_inr = fetch_tollguru_toll(c1, c2, tollguru_key)
                except Exception as exc:
                    print(f"   [WARN] TollGuru error {c1['name']}-{c2['name']}: {exc}")

            print(
                f"   [EDGE] {c1['name']} <-> {c2['name']} | {link['highway']} | "
                f"{link['distance_km']} km | {speed_kmh} km/h | Toll: {toll_inr if toll_inr is not None else 'N/A (needs API key)'}"
            )

            for src, dst in ((c1["id"], c2["id"]), (c2["id"], c1["id"])):
                roads.append({
                    "from": src,
                    "to": dst,
                    "highway": link["highway"],
                    "distance_km": link["distance_km"],
                    "speed_limit_kmh": speed_kmh,
                    "toll_inr": toll_inr,
                    "subsidy_inr": 0,  # No public API exists for Indian state freight subsidies
                    "open_hour": 0,
                    "close_hour": 24,
                })
            time.sleep(0.3)

    return roads


def main():
    here_key = (HERE_API_KEY or os.environ.get("HERE_API_KEY", "")).strip()
    tollguru_key = (TOLLGURU_API_KEY or os.environ.get("TOLLGURU_API_KEY", "")).strip()

    if not here_key and not tollguru_key:
        print(
            "NOTE: Neither HERE_API_KEY nor TOLLGURU_API_KEY is set.\n"
            "      Cities, direct highway links, distances, and speeds will be fetched from free OSM/OSRM APIs,\n"
            "      but `toll_inr` will be null until you provide HERE_API_KEY or TOLLGURU_API_KEY.\n",
            file=sys.stderr,
        )

    cities_file = DATA_DIR / "cities.json"
    if cities_file.exists():
        cities = json.loads(cities_file.read_text(encoding="utf-8")).get("cities", [])
        print(f"1) Loaded {len(cities)} cities from existing cities.json")
    else:
        cities = fetch_cities_from_nominatim()
    roads = discover_and_fetch_roads(cities, here_key, tollguru_key)

    (DATA_DIR / "cities.json").write_text(json.dumps({"cities": cities}, indent=2), encoding="utf-8")
    (DATA_DIR / "roads.json").write_text(json.dumps({"roads": roads}, indent=2), encoding="utf-8")

    print("\nSaved:")
    print(f"  - {DATA_DIR / 'cities.json'} ({len(cities)} cities)")
    print(f"  - {DATA_DIR / 'roads.json'} ({len(roads)} directed edges)")


if __name__ == "__main__":
    main()