window.MapView = (() => {
  let map;
  let baseLayer;
  let roadsLayer;
  let citiesLayer;
  let activeLayer;
  let onRoadClickCb = null;
  let savedKey = '';
  let cachedRoads = [];

  function tileUrl(theme) {
    const style = theme === 'light' ? 'light_all' : 'dark_all';
    const qs = savedKey ? `?key=${encodeURIComponent(savedKey)}` : '';
    return `https://basemaps.cartocdn.com/rastertiles/${style}/{z}/{x}/{y}.png${qs}`;
  }

  function init(onRoadClick, cartoApiKey = '') {
    onRoadClickCb = onRoadClick;
    savedKey = cartoApiKey;
    map = L.map('map').setView([22.5, 79.0], 5);

    // Dedicated high-zIndex pane so active green route/MST always stays above network roads
    map.createPane('activeRoutePane');
    map.getPane('activeRoutePane').style.zIndex = 450;

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

  function buildRoadTooltip(c1, c2, r) {
    const hw = r ? r.highway : 'Highway';
    const dist = r ? `${r.distance_km} km` : '';
    const spd = r ? `${r.speed_limit_kmh} km/h` : '';
    const traf = r ? `Traffic: ${r.traffic_multiplier || 1}x` : '';
    const status = r && r.is_blocked ? 'BLOCKED (Click to unblock)' : 'Click to simulate roadblock';
    return `${c1.name} ↔ ${c2.name} (${hw})<br>${[dist, spd, traf].filter(Boolean).join(' | ')}<br><b>${status}</b>`;
  }

  function findRoad(u, v) {
    return cachedRoads.find((r) => (r.from === u && r.to === v) || (r.from === v && r.to === u));
  }

  function renderNetwork(cities, roads) {
    cachedRoads = roads || [];
    roadsLayer.clearLayers();
    citiesLayer.clearLayers();

    const cityMap = new Map(cities.map((c) => [c.id, c]));
    const drawnPairs = new Set();

    for (const r of cachedRoads) {
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

      line.bindTooltip(buildRoadTooltip(c1, c2, r));
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
          { color: '#22c55e', weight: 4.5, opacity: 0.95, pane: 'activeRoutePane' }
        )
          .bindTooltip(`${e.from_name} ↔ ${e.to_name} (${e.highway || 'MST'} | ${e.distance_km} km)`)
          .addTo(activeLayer);
      }
      return;
    }

    if (Array.isArray(result.itinerary) && result.itinerary.length > 1) {
      const coords = result.itinerary.map((step) => [step.lat, step.lon]);

      for (let i = 1; i < result.itinerary.length; i++) {
        const prev = result.itinerary[i - 1];
        const curr = result.itinerary[i];
        const r = findRoad(prev.city_id, curr.city_id);
        const line = L.polyline(
          [
            [prev.lat, prev.lon],
            [curr.lat, curr.lon],
          ],
          { color: '#22c55e', weight: 5, opacity: 0.95, pane: 'activeRoutePane' }
        );
        line.bindTooltip(buildRoadTooltip(prev, curr, r || { highway: curr.via_highway }));
        if (r) {
          line.on('click', () => {
            if (onRoadClickCb) onRoadClickCb(r.from, r.to, !r.is_blocked);
          });
        }
        line.addTo(activeLayer);
      }

      result.itinerary.forEach((step, idx) => {
        const isEnd = idx === 0 || idx === result.itinerary.length - 1;
        L.circleMarker([step.lat, step.lon], {
          radius: isEnd ? 8 : 6,
          fillColor: idx === 0 ? '#22c55e' : idx === result.itinerary.length - 1 ? '#ef4444' : '#facc15',
          color: '#ffffff',
          weight: 2,
          fillOpacity: 1,
          pane: 'activeRoutePane',
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