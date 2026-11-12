/* ============================================================================
   map.js — Leaflet helpers + the Network Map module.
   Connections are SCHEMATIC straight lines between city coordinates, not
   verified road geometry (as labelled in the UI).
   ========================================================================== */

const Maps = {
    _maps: {},
    _groups: {},
    tileUrl: "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',

    ensure(id) {
        if (this._maps[id]) return this._maps[id];
        const el = document.getElementById(id);
        if (!el) return null;
        const map = L.map(id).setView([22.5, 79.5], 5);
        L.tileLayer(this.tileUrl, { attribution: this.attribution, maxZoom: 18 }).addTo(map);
        this._maps[id] = map;
        this._groups[id] = L.featureGroup().addTo(map);
        return map;
    },

    group(id) { return this._groups[id]; },
    clear(id) { const g = this._groups[id]; if (g) g.clearLayers(); },
    invalidate(id) { const m = this._maps[id]; if (m) setTimeout(() => m.invalidateSize(), 60); },
    invalidateAll() { Object.keys(this._maps).forEach(id => this.invalidate(id)); },

    coordsIndex(network) {
        const idx = {};
        (network.cities || []).forEach(c => { idx[c.name] = [c.lat, c.lon]; });
        return idx;
    },

    drawRoads(id, network) {
        const g = this.group(id);
        const idx = this.coordsIndex(network);
        (network.roads || []).forEach(r => {
            const a = idx[r.source], b = idx[r.destination];
            if (!a || !b) return;
            const line = L.polyline([a, b], { color: "#33507e", weight: 2, opacity: 0.7 });
            line.bindPopup("<b>" + r.highway + "</b><br>" + r.source + " ↔ " + r.destination + "<br>" + r.distance_km + " km");
            g.addLayer(line);
        });
    },

    addCityMarkers(id, network, withLabels) {
        const g = this.group(id);
        (network.cities || []).forEach(c => {
            const m = L.circleMarker([c.lat, c.lon], {
                radius: 6, color: "#60a5fa", weight: 2, fillColor: "#0b1220", fillOpacity: 0.9,
            });
            m.bindPopup("<b>" + c.name + "</b>");
            if (withLabels) m.bindTooltip(c.name, { permanent: true, direction: "right", offset: [8, 0] });
            g.addLayer(m);
        });
    },

    /* Draw a single highlighted route from an array of [lat,lon]. */
    drawRoute(id, coords, color) {
        const g = this.group(id);
        if (!coords || coords.length < 1) return;
        const line = L.polyline(coords, { color: color || "#22d3ee", weight: 5, opacity: 0.95 });
        g.addLayer(line);
        g.addLayer(L.circleMarker(coords[0], { radius: 8, color: "#34d399", weight: 3, fillColor: "#34d399", fillOpacity: 1 }).bindPopup("Source"));
        if (coords.length > 1)
            g.addLayer(L.circleMarker(coords[coords.length - 1], { radius: 8, color: "#f87171", weight: 3, fillColor: "#f87171", fillOpacity: 1 }).bindPopup("Destination"));
        for (let i = 1; i < coords.length - 1; ++i)
            g.addLayer(L.circleMarker(coords[i], { radius: 4, color: "#e7eefc", weight: 1, fillColor: "#e7eefc", fillOpacity: 0.8 }));
        try { this._maps[id].fitBounds(L.latLngBounds(coords).pad(0.2)); } catch (e) {}
    },

    /* Draw up to three comparison routes in distinct colours. */
    drawCompare(id, routes) {
        const g = this.group(id);
        if (App.network) this.drawRoads(id, App.network);
        const colors = { fastest: "#22d3ee", cheapest: "#34d399", shortest: "#3b82f6" };
        const all = [];
        routes.forEach(r => {
            if (!r.path_coords || r.path_coords.length < 1) return;
            const line = L.polyline(r.path_coords, { color: colors[r.objective] || "#60a5fa", weight: 4, opacity: 0.85 });
            line.bindPopup("<b>" + r.objective + "</b> (" + r.algorithm + ")<br>" +
                Math.round(r.total_distance_km) + " km · " + Fmt.inr(r.total_cost));
            g.addLayer(line);
            all.push(...r.path_coords);
        });
        if (all.length) { try { this._maps[id].fitBounds(L.latLngBounds(all).pad(0.2)); } catch (e) {} }
    },

    /* Draw a Prim MST backbone (corridors + hub markers). */
    drawMST(id, mst) {
        const g = this.group(id);
        if (App.network) this.drawRoads(id, App.network);
        const boundsPts = [];
        (mst.mst_edges || []).forEach(e => {
            const coords = e.corridor_coords || [];
            if (coords.length >= 2) {
                const line = L.polyline(coords, { color: "#34d399", weight: 4, opacity: 0.95 });
                line.bindPopup("<b>" + e.from + " ↔ " + e.to + "</b><br>" +
                    Math.round(e.distance_km) + " km · " + Fmt.inr(e.cost));
                g.addLayer(line);
                boundsPts.push(...coords);
            }
        });
        const idx = this.coordsIndex(App.network || { cities: [] });
        (mst.hubs || []).forEach(name => {
            const c = idx[name];
            if (c) {
                g.addLayer(L.circleMarker(c, { radius: 9, color: "#fbbf24", weight: 3, fillColor: "#fbbf24", fillOpacity: 1 }).bindPopup("Hub: " + name));
                boundsPts.push(c);
            }
        });
        if (boundsPts.length) { try { this._maps[id].fitBounds(L.latLngBounds(boundsPts).pad(0.2)); } catch (e) {} }
    },
};

/* Network Map module (Module 4). */
const NetworkMap = {
    init() {
        const map = Maps.ensure("network-map");
        if (!map) return;
        this.render();
        Maps.invalidate("network-map");
    },
    render() {
        if (!App.network) return;
        Maps.clear("network-map");
        const showRoads = document.getElementById("map-show-roads").checked;
        const showLabels = document.getElementById("map-show-labels").checked;
        if (showRoads) Maps.drawRoads("network-map", App.network);
        Maps.addCityMarkers("network-map", App.network, showLabels);
        const g = Maps.group("network-map");
        if (g && g.getLayers().length) { try { Maps._maps["network-map"].fitBounds(g.getBounds().pad(0.1)); } catch (e) {} }
    },
};