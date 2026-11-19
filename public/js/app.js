/* ============================================================================
   app.js — Application shell: navigation, section switching, global state.
   ========================================================================== */

const UI = {
    _tt: null,
    toast(msg, type = "") {
        const t = document.getElementById("toast");
        if (!t) return;
        t.textContent = msg;
        t.className = "toast show " + type;
        clearTimeout(this._tt);
        this._tt = setTimeout(() => { t.className = "toast"; }, 3800);
    },
    show(el) { if (el) el.classList.remove("hidden"); },
    hide(el) { if (el) el.classList.add("hidden"); },
};

const App = {
    network: null,
    meta: null,
    cities: [],
    activeSection: "dashboard",

    async init() {
        this.bindNav();
        this.bindTabs();
        this.bindMapToggles();
        await this.loadConfig();
        await this.loadNetwork();
        await this.loadMeta();
        await this.checkEngine();
        Dashboard.load();
    },

    async loadConfig() {
        try {
            const cfg = await API.config();
            if (cfg && cfg.carto_api_key) {
                Maps.tileUrl = `https://basemaps.cartocdn.com/rastertiles/dark_all/{z}/{x}/{y}{r}.png?key=${encodeURIComponent(cfg.carto_api_key)}`;
                Maps.attribution = '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> &copy; <a href="https://carto.com/">CARTO</a>';
            }
        } catch (e) { /* falls back to default OpenStreetMap tiles */ }
    },

    bindNav() {
        document.querySelectorAll(".nav-item").forEach(btn => {
            btn.addEventListener("click", () => this.switchSection(btn.dataset.section));
        });
    },

    bindTabs() {
        document.querySelectorAll(".tab").forEach(t => {
            t.addEventListener("click", () => {
                document.querySelectorAll(".tab").forEach(x => x.classList.toggle("active", x === t));
                document.querySelectorAll(".tab-panel").forEach(p =>
                    p.classList.toggle("active", p.id === "tab-" + t.dataset.tab));
            });
        });
    },

    bindMapToggles() {
        const a = document.getElementById("map-show-roads");
        const b = document.getElementById("map-show-labels");
        if (a) a.addEventListener("change", () => NetworkMap.render());
        if (b) b.addEventListener("change", () => NetworkMap.render());
    },

    switchSection(name) {
        this.activeSection = name;
        document.querySelectorAll(".nav-item").forEach(b =>
            b.classList.toggle("active", b.dataset.section === name));
        document.querySelectorAll(".section").forEach(s =>
            s.classList.toggle("active", s.id === "section-" + name));

        if (name === "dashboard") Dashboard.load();
        if (name === "map") NetworkMap.init();
        if (name === "analysis") Analysis.ensureCards();
        Maps.invalidateAll();
    },

    async loadNetwork() {
        try {
            this.network = await API.network();
            this.cities = this.network.cities || [];
            this.populateCitySelects();
            Planner.buildHubPicker(this.cities);
        } catch (e) {
            UI.toast("Failed to load network: " + e.message, "error");
        }
    },

    async loadMeta() {
        try { this.meta = await API.meta(); } catch (e) { this.meta = null; }
    },

    populateCitySelects() {
        ["route-source", "route-dest", "cmp-source", "cmp-dest"].forEach(id => {
            const sel = document.getElementById(id);
            if (!sel) return;
            sel.innerHTML = "";
            this.cities.forEach(c => {
                const o = document.createElement("option");
                o.value = c.name; o.textContent = c.name;
                sel.appendChild(o);
            });
        });
        this.setDefault("route-source", "Delhi");
        this.setDefault("route-dest", "Mumbai");
        this.setDefault("cmp-source", "Delhi");
        this.setDefault("cmp-dest", "Bengaluru");
    },

    setDefault(id, name) {
        const el = document.getElementById(id);
        if (el && [...el.options].some(o => o.value === name)) el.value = name;
    },

    async checkEngine() {
        const el = document.getElementById("engine-status");
        if (!el) return;
        try {
            const h = await API.health();
            el.innerHTML = h.engine_built
                ? '<span class="dot dot-ok"></span> Algorithm engine ready'
                : '<span class="dot dot-err"></span> Engine not built';
        } catch (e) {
            el.innerHTML = '<span class="dot dot-err"></span> Backend offline';
        }
    },
};

document.addEventListener("DOMContentLoaded", () => App.init());