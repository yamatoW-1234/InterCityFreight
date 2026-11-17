/* ============================================================================
   map.js — Leaflet helpers + the Network Map module.
   Connections are SCHEMATIC straight lines between city coordinates, not
   verified road geometry (as labelled in the UI).
   ========================================================================== */

const Maps = {
    _maps: {},
    _groups: {},
    tileUrl: "https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png",
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> &copy; <a href="https://carto.com/">CARTO</a>',

    ensure(id) {
        if (typeof L === "undefined") {
            UI.toast("Map library failed to load — check internet connection", "error");
            return null;
        }
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
            const line = L.polyline([a, b], { color: "#3f3f46", weight: 2, opacity: 0.75 });
            line.bindPopup("<b>" + r.highway + "</b><br>" + r.source + " ↔ " + r.destination + "<br>" + r.distance_km + " km");
            g.addLayer(line);
        });
    },

    addCityMarkers(id, network, withLabels) {
        const g = this.group(id);
        (network.cities || []).forEach(c => {
            const m = L.circleMarker([c.lat, c.lon], {
                radius: 5.5, color: "#f4f4f6", weight: 2, fillColor: "#09090b", fillOpacity: 0.95,
            });
            m.bindPopup("<b>" + c.name + "</b>");
            if (withLabels) m.bindTooltip(c.name, { permanent: true, direction: "right", offset: [8, 0] });
            g.addLayer(m);
        });
    },

    /* Draw a single highlighted route from an array of [lat,lon]. */
    /* Draw a single highlighted route from an array of [lat,lon]. */
    drawRoute(id, coords, color, names = []) {
        const g = this.group(id);
        if (!coords || coords.length < 1) return;

        // Subtle outer glow halo + crisp main corridor line
        g.addLayer(L.polyline(coords, { color: "#ffffff", weight: 9, opacity: 0.14 }));
        g.addLayer(L.polyline(coords, { color: color || "#f4f4f6", weight: 4, opacity: 0.95 }));

        // Intermediate waypoints
        for (let i = 1; i < coords.length - 1; ++i) {
            const label = names[i] ? `Stop ${i}: ${names[i]}` : `Waypoint ${i}`;
            g.addLayer(L.circleMarker(coords[i], {
                radius: 4.5, color: "#09090b", weight: 1.5, fillColor: "#a1a1aa", fillOpacity: 1
            }).bindTooltip(label, { direction: "top", offset: [0, -4] }));
        }

        // Source (dark slate core with silver ring)
        const srcLabel = names[0] ? `Source: ${names[0]}` : "Source";
        g.addLayer(L.circleMarker(coords[0], {
            radius: 7.5, color: "#a1a1aa", weight: 2.5, fillColor: "#27272a", fillOpacity: 1
        }).bindTooltip(srcLabel, { direction: "top", offset: [0, -6] }));

        // Destination (bright moonlight white with dark core ring)
        if (coords.length > 1) {
            const last = coords.length - 1;
            const dstLabel = names[last] ? `Destination: ${names[last]}` : "Destination";
            g.addLayer(L.circleMarker(coords[last], {
                radius: 8, color: "#09090b", weight: 2.5, fillColor: "#ffffff", fillOpacity: 1
            }).bindTooltip(dstLabel, { direction: "top", offset: [0, -6] }));
        }

        try { this._maps[id].fitBounds(L.latLngBounds(coords).pad(0.22)); } catch (e) {}
    },

    /* Draw up to three comparison routes in distinct monochrome styles. */
    drawCompare(id, routes) {
        const g = this.group(id);
        if (App.network) this.drawRoads(id, App.network);
        const colors = { fastest: "#0284c7", cheapest: "#16a34a", shortest: "#ea580c" };
        const all = [];
        routes.forEach(r => {
            if (!r.path_coords || r.path_coords.length < 1) return;
            const c = colors[r.objective] || "#0284c7";
            g.addLayer(L.polyline(r.path_coords, { color: "#ffffff", weight: 7, opacity: 0.8 }));
            const line = L.polyline(r.path_coords, { color: c, weight: 4.5, opacity: 0.95 });
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
                const line = L.polyline(coords, { color: "#09090b", weight: 4.5, opacity: 0.95 });
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
                g.addLayer(L.circleMarker(c, { radius: 8.5, color: "#09090b", weight: 2.5, fillColor: "#f59e0b", fillOpacity: 1 }).bindPopup("Hub: " + name));
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