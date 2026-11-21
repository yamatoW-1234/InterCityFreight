window.MapView = (() => {
  let map;
  let baseLayer;
  let roadsLayer;
  let citiesLayer;
  let activeLayer;
  let onRoadClickCb = null;
  let savedKey = '';

  function tileUrl(theme) {
    const style = theme === 'light' ? 'light_all' : 'dark_all';
    const qs = savedKey ? `?key=${encodeURIComponent(savedKey)}` : '';
    return `https://basemaps.cartocdn.com/rastertiles/${style}/{z}/{x}/{y}.png${qs}`;
  }

  function init(onRoadClick, cartoApiKey = '') {
    onRoadClickCb = onRoadClick;
    savedKey = cartoApiKey;
    map = L.map('map').setView([22.5, 79.0], 5);

    const initialTheme = document.documentElement.getAttribute('data-theme') || 'dark';
    baseLayer = L.tileLayer(tileUrl(initialTheme), {
      attribution: '&copy; OpenStreetMap &copy; CARTO',
      maxZoom: 18,
    }).addTo(map);

    roadsLayer = L.layerGroup().addTo(map);
    activeLayer = L.layerGroup().addTo(map);
    citiesLayer = L.layerGroup().addTo(map);
  }

  function setTheme(theme) {
    if (!map || !baseLayer) return;
    map.removeLayer(baseLayer);
    baseLayer = L.tileLayer(tileUrl(theme), {
      attribution: '&copy; OpenStreetMap &copy; CARTO',
      maxZoom: 18,
    }).addTo(map);
    baseLayer.bringToBack();
  }

  function renderNetwork(cities, roads) {
    roadsLayer.clearLayers();
    citiesLayer.clearLayers();

    const cityMap = new Map(cities.map((c) => [c.id, c]));
    const drawnPairs = new Set();

    for (const r of roads) {
      const key = r.from < r.to ? `${r.from}-${r.to}` : `${r.to}-${r.from}`;
      if (drawnPairs.has(key)) continue;
      drawnPairs.add(key);

      const c1 = cityMap.get(r.from);
      const c2 = cityMap.get(r.to);
      if (!c1 || !c2) continue;

      let color = '#3b82f6';
      let dashArray = null;
      let weight = 2.5;
      let opacity = 0.55;

      if (r.is_blocked) {
        color = '#ef4444';
        dashArray = '6,6';
        weight = 3.5;
        opacity = 0.9;
      } else if ((r.traffic_multiplier || 1) >= 1.45) {
        color = '#f59e0b';
        weight = 3;
        opacity = 0.8;
      }

      const line = L.polyline(
        [
          [c1.lat, c1.lon],
          [c2.lat, c2.lon],
        ],
        { color, weight, opacity, dashArray }
      );

      line.bindTooltip(
        `${c1.name} ↔ ${c2.name} (${r.highway})<br>` +
          `${r.distance_km} km | ${r.speed_limit_kmh} km/h | Traffic: ${r.traffic_multiplier || 1}x<br>` +
          `<b>${r.is_blocked ? 'BLOCKED (Click to unblock)' : 'Click to simulate roadblock'}</b>`
      );

      line.on('click', () => {
        if (onRoadClickCb) onRoadClickCb(r.from, r.to, !r.is_blocked);
      });

      line.addTo(roadsLayer);
    }

    for (const c of cities) {
      const marker = L.circleMarker([c.lat, c.lon], {
        radius: 5,
        fillColor: '#38bdf8',
        color: '#0f172a',
        weight: 1.5,
        fillOpacity: 0.95,
      });
      marker.bindTooltip(`${c.name} (${c.state || ''})`, { direction: 'top' });
      marker.addTo(citiesLayer);
    }
  }

  function renderResult(result, cities) {
    activeLayer.clearLayers();
    if (!result) return;

    const cityMap = new Map(cities.map((c) => [c.id, c]));

    if (result.mode === 'mst' && Array.isArray(result.mst_edges)) {
      for (const e of result.mst_edges) {
        const c1 = cityMap.get(e.from);
        const c2 = cityMap.get(e.to);
        if (!c1 || !c2) continue;
        L.polyline(
          [
            [c1.lat, c1.lon],
            [c2.lat, c2.lon],
          ],
          { color: '#22c55e', weight: 4.5, opacity: 0.95 }
        )
          .bindTooltip(`${e.from_name} ↔ ${e.to_name} (${e.distance_km} km)`)
          .addTo(activeLayer);
      }
      return;
    }

    if (Array.isArray(result.itinerary) && result.itinerary.length > 1) {
      const coords = result.itinerary.map((step) => [step.lat, step.lon]);
      L.polyline(coords, { color: '#22c55e', weight: 5, opacity: 0.95 }).addTo(activeLayer);

      result.itinerary.forEach((step, idx) => {
        const isEnd = idx === 0 || idx === result.itinerary.length - 1;
        L.circleMarker([step.lat, step.lon], {
          radius: isEnd ? 8 : 6,
          fillColor: idx === 0 ? '#22c55e' : idx === result.itinerary.length - 1 ? '#ef4444' : '#facc15',
          color: '#ffffff',
          weight: 2,
          fillOpacity: 1,
        })
          .bindTooltip(`${idx + 1}. ${step.name}`)
          .addTo(activeLayer);
      });

      map.fitBounds(L.latLngBounds(coords), { padding: [40, 40] });
    }
  }

  function invalidateSize() {
    if (map) setTimeout(() => map.invalidateSize(), 50);
  }

  return { init, setTheme, renderNetwork, renderResult, invalidateSize };
})();