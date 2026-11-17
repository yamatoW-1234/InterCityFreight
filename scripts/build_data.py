import json
import math
import time
import urllib.request
import urllib.error
from pathlib import Path

# Top 24 Indian Freight & Industrial Hubs (WGS-84 coordinates)
CITIES = [
    {"id": 0,  "name": "Delhi",         "lat": 28.6139, "lon": 77.2090},
    {"id": 1,  "name": "Ludhiana",      "lat": 30.9010, "lon": 75.8573},
    {"id": 2,  "name": "Amritsar",      "lat": 31.6340, "lon": 74.8723},
    {"id": 3,  "name": "Jaipur",        "lat": 26.9124, "lon": 75.7873},
    {"id": 4,  "name": "Agra",          "lat": 27.1767, "lon": 78.0081},
    {"id": 5,  "name": "Lucknow",       "lat": 26.8467, "lon": 80.9462},
    {"id": 6,  "name": "Kanpur",        "lat": 26.4499, "lon": 80.3319},
    {"id": 7,  "name": "Varanasi",      "lat": 25.3176, "lon": 82.9739},
    {"id": 8,  "name": "Patna",         "lat": 25.5941, "lon": 85.1376},
    {"id": 9,  "name": "Kolkata",       "lat": 22.5726, "lon": 88.3639},
    {"id": 10, "name": "Guwahati",      "lat": 26.1445, "lon": 91.7362},
    {"id": 11, "name": "Ahmedabad",     "lat": 23.0225, "lon": 72.5714},
    {"id": 12, "name": "Vadodara",      "lat": 22.3072, "lon": 73.1812},
    {"id": 13, "name": "Surat",         "lat": 21.1702, "lon": 72.8311},
    {"id": 14, "name": "Mumbai",        "lat": 19.0760, "lon": 72.8777},
    {"id": 15, "name": "Pune",          "lat": 18.5204, "lon": 73.8567},
    {"id": 16, "name": "Indore",        "lat": 22.7196, "lon": 75.8577},
    {"id": 17, "name": "Bhopal",        "lat": 23.2599, "lon": 77.4126},
    {"id": 18, "name": "Nagpur",        "lat": 21.1458, "lon": 79.0882},
    {"id": 19, "name": "Raipur",        "lat": 21.2514, "lon": 81.6296},
    {"id": 20, "name": "Hyderabad",     "lat": 17.3850, "lon": 78.4867},
    {"id": 21, "name": "Visakhapatnam", "lat": 17.6868, "lon": 83.2185},
    {"id": 22, "name": "Bengaluru",     "lat": 12.9716, "lon": 77.5946},
    {"id": 23, "name": "Chennai",       "lat": 13.0827, "lon": 80.2707},
]

# Sparse National Highway Backbone Corridors:
# (source, destination, primary_nh_ref, is_expressway, fwd_rebate_ratio, rev_rebate_ratio)
# Rebate ratios reflect DPIIT North-East Transport Subsidy (50%) & State Export Port Rebates (25%).
CORRIDORS = [
    ("Delhi", "Ludhiana", "NH44", False, 0.0, 0.0),
    ("Ludhiana", "Amritsar", "NH3", False, 0.0, 0.0),
    ("Delhi", "Jaipur", "NH48", False, 0.0, 0.0),
    ("Delhi", "Agra", "NH19", True, 0.0, 0.0),
    ("Agra", "Jaipur", "NH21", False, 0.0, 0.0),
    ("Agra", "Lucknow", "NE2", True, 0.0, 0.0),
    ("Agra", "Kanpur", "NH19", False, 0.0, 0.0),
    ("Kanpur", "Lucknow", "NH27", False, 0.0, 0.0),
    ("Kanpur", "Varanasi", "NH19", False, 0.0, 0.0),
    ("Lucknow", "Varanasi", "NH731", False, 0.0, 0.0),
    ("Varanasi", "Patna", "NH19", False, 0.0, 0.0),
    ("Patna", "Kolkata", "NH19", False, 0.0, 0.0),
    ("Kolkata", "Guwahati", "NH12/NH27", False, 0.50, 0.0),     # DPIIT NE Freight Subsidy (50%)
    ("Jaipur", "Ahmedabad", "NH48", False, 0.0, 0.0),
    ("Ahmedabad", "Vadodara", "NE1", True, 0.25, 0.0),          # Port-bound export corridor rebate
    ("Vadodara", "Surat", "NH48", False, 0.25, 0.0),
    ("Surat", "Mumbai", "NH48", False, 0.25, 0.0),
    ("Ahmedabad", "Indore", "NH47", False, 0.0, 0.0),
    ("Vadodara", "Indore", "NH47", False, 0.0, 0.0),
    ("Indore", "Bhopal", "NH52", False, 0.0, 0.0),
    ("Agra", "Bhopal", "NH46", False, 0.0, 0.0),
    ("Bhopal", "Nagpur", "NH47", False, 0.0, 0.0),
    ("Mumbai", "Pune", "NH48", True, 0.0, 0.0),
    ("Mumbai", "Nagpur", "NH161", True, 0.0, 0.0),
    ("Pune", "Nagpur", "NH361", False, 0.0, 0.0),
    ("Pune", "Hyderabad", "NH65", False, 0.0, 0.0),
    ("Pune", "Bengaluru", "NH48", False, 0.0, 0.0),
    ("Nagpur", "Hyderabad", "NH44", False, 0.0, 0.0),
    ("Nagpur", "Raipur", "NH53", False, 0.0, 0.0),
    ("Raipur", "Kolkata", "NH53/NH16", False, 0.0, 0.0),
    ("Raipur", "Visakhapatnam", "NH26/NH16", False, 0.25, 0.0), # Port-bound export corridor rebate
    ("Kolkata", "Visakhapatnam", "NH16", False, 0.0, 0.0),
    ("Visakhapatnam", "Chennai", "NH16", False, 0.0, 0.0),
    ("Hyderabad", "Visakhapatnam", "NH65/NH16", False, 0.25, 0.0),
    ("Hyderabad", "Bengaluru", "NH44", False, 0.0, 0.0),
    ("Hyderabad", "Chennai", "NH16", False, 0.0, 0.0),
    ("Bengaluru", "Chennai", "NH48", False, 0.25, 0.0),         # Port-bound export corridor rebate
]

OSRM_BASE_URL = "http://router.project-osrm.org/route/v1/driving"
TRUCK_SPEED_FACTOR = 0.78      # Heavy freight truck average vs. OSRM light-vehicle speed
NH_TOLL_RATE_INR_PER_KM = 2.15 # NHAI standard 2/3-axle commercial truck rate
EXP_TOLL_RATE_INR_PER_KM = 2.80
FUEL_PRICE_INR = 95.0
TRUCK_MILEAGE_KMPL = 6.5       # Realistic heavy commercial truck mileage (km/l)


def haversine_km(c1: dict, c2: dict) -> float:
    r = 6371.0
    lat1, lon1 = math.radians(c1["lat"]), math.radians(c1["lon"])
    lat2, lon2 = math.radians(c2["lat"]), math.radians(c2["lon"])
    dlat, dlon = lat2 - lat1, lon2 - lon1
    a = math.sin(dlat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def fetch_osrm_segment(c1: dict, c2: dict) -> tuple[float, float]:
    """Fetches real driving distance (km) and truck-adjusted speed (km/h) from OSRM."""
    url = f"{OSRM_BASE_URL}/{c1['lon']},{c1['lat']};{c2['lon']},{c2['lat']}?overview=false"
    req = urllib.request.Request(url, headers={"User-Agent": "InterCityFreight-DAA-Project/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
            if payload.get("code") == "Ok" and payload.get("routes"):
                route = payload["routes"][0]
                dist_km = round(route["distance"] / 1000.0, 1)
                car_hours = route["duration"] / 3600.0
                truck_speed = round((dist_km / car_hours) * TRUCK_SPEED_FACTOR, 1)
                truck_speed = max(45.0, min(85.0, truck_speed))
                return dist_km, truck_speed
    except (urllib.error.URLError, TimeoutError, KeyError, ZeroDivisionError) as exc:
        print(f"  [WARN] OSRM fallback for {c1['name']} -> {c2['name']}: {exc}")

    # Fallback using road detour factor (1.22x Haversine) if offline
    est_dist = round(haversine_km(c1, c2) * 1.22, 1)
    return est_dist, 65.0


def main() -> None:
    root_dir = Path(__file__).resolve().parent.parent
    data_dir = root_dir / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    city_map = {c["name"]: c for c in CITIES}
    roads_output = []

    print(f"Querying OSRM for {len(CORRIDORS)} national highway corridors...")
    for src_name, dst_name, nh_ref, is_exp, fwd_ratio, rev_ratio in CORRIDORS:
        c1, c2 = city_map[src_name], city_map[dst_name]
        dist_km, avg_speed = fetch_osrm_segment(c1, c2)

        toll_rate = EXP_TOLL_RATE_INR_PER_KM if is_exp else NH_TOLL_RATE_INR_PER_KM
        toll_inr = round(dist_km * toll_rate)

        # Base operating cost (fuel + toll) used to compute policy rebate amount
        base_fuel_cost = (dist_km / TRUCK_MILEAGE_KMPL) * FUEL_PRICE_INR
        base_operating_cost = base_fuel_cost + toll_inr

        subsidy_fwd = round(base_operating_cost * fwd_ratio)
        subsidy_rev = round(base_operating_cost * rev_ratio)

        roads_output.append({
            "source": src_name,
            "destination": dst_name,
            "highway": nh_ref,
            "distance_km": dist_km,
            "avg_speed_kmph": avg_speed,
            "toll": toll_inr,
            "subsidy_forward": subsidy_fwd,
            "subsidy_reverse": subsidy_rev,
        })
        print(f"  {src_name:14s} <-> {dst_name:14s} | {nh_ref:10s} | {dist_km:6.1f} km | {avg_speed:4.1f} km/h | Toll: INR {toll_inr}")
        time.sleep(0.25)  # Respect public OSRM rate limits

    cities_payload = {
        "_meta": {
            "description": "Top 24 Indian commercial freight & industrial hub cities.",
            "source": "OpenStreetMap / WGS-84 city coordinates",
            "coordinate_reference": "WGS-84 (latitude, longitude in decimal degrees)"
        },
        "cities": CITIES
    }

    roads_payload = {
        "_meta": {
            "description": "Inter-city National Highway & Expressway freight network.",
            "sources": [
                "OpenStreetMap / Project OSRM Routing API (driving distances & speeds)",
                "NHAI Commercial Axle Toll Schedule (approx. INR 2.15/km NH, INR 2.80/km Expressway)",
                "DPIIT North-East Transport Subsidy (50%) & State Export Port Rebate Policies (25%)"
            ],
            "units": {
                "distance_km": "kilometres (OSRM driving distance)",
                "avg_speed_kmph": "kilometres per hour (commercial freight truck average)",
                "toll": "Indian Rupees (estimated commercial FASTag toll)",
                "subsidy_forward": "Rupees rebate applied source->destination only",
                "subsidy_reverse": "Rupees rebate applied destination->source only"
            }
        },
        "roads": roads_output
    }

    config_payload = {
        "_meta": {
            "description": "Configurable freight vehicle and heuristic assumptions.",
            "sources": [
                "Indian retail diesel benchmark (~INR 95.0/L)",
                "Heavy commercial freight truck average mileage (6.5 km/L)"
            ]
        },
        "fuel_price_per_litre": FUEL_PRICE_INR,
        "vehicle_mileage_kmpl": TRUCK_MILEAGE_KMPL,
        "currency": "INR",
        "max_speed_override_kmph": 85.0
    }

    (data_dir / "cities.json").write_text(json.dumps(cities_payload, indent=2), encoding="utf-8")
    (data_dir / "roads.json").write_text(json.dumps(roads_payload, indent=2), encoding="utf-8")
    (data_dir / "config.json").write_text(json.dumps(config_payload, indent=2), encoding="utf-8")
    print(f"\nSuccessfully generated cities.json ({len(CITIES)} cities), roads.json ({len(roads_output)} corridors), and config.json in {data_dir}")


if __name__ == "__main__":
    main()