/* ============================================================================
   dashboard.js — Module 1: Main Dashboard.
   Shows network stats, algorithm list, performance summary and recent runs.
   ========================================================================== */

const Dashboard = {
    async load() {
        this.renderNetworkStats();
        await Promise.all([this.renderAlgos(), this.renderSummary()]);
    },

    renderNetworkStats() {
        const n = App.network;
        if (!n) return;
        document.getElementById("stat-cities").textContent = Fmt.num(n.num_cities);
        document.getElementById("stat-roads").textContent = Fmt.num(n.num_roads);
        document.getElementById("stat-algos").textContent = "4";
        document.getElementById("stat-negedge").textContent = n.has_negative_financial_edge ? "Yes" : "No";
    },

    async renderAlgos() {
        const box = document.getElementById("dash-algo-list");
        if (!box) return;
        box.innerHTML = "";
        let meta = App.meta;
        if (!meta) { try { meta = await API.meta(); } catch (e) { meta = null; } }
        if (!meta || !meta.algorithms) {
            box.innerHTML = '<div class="muted">Algorithm metadata unavailable.</div>';
            return;
        }
        meta.algorithms.forEach(a => {
            const div = document.createElement("div");
            div.className = "algo-item";
            div.innerHTML = `<b>${a.name}</b><span>${a.purpose}</span>`;
            box.appendChild(div);
        });
    },

    async renderSummary() {
        const box = document.getElementById("dash-summary");
        if (!box) return;
        try {
            const s = await API.summary();
            const sum = s.summary || {};
            box.innerHTML = "";
            const rows = [
                ["Total route runs", Fmt.num(sum.total_runs)],
                ["Avg engine time", Fmt.ms(sum.avg_execution_ms) + " ms"],
            ];
            const usage = sum.algorithm_usage || {};
            Object.keys(usage).forEach(k => rows.push([k + " runs", Fmt.num(usage[k])]));
            if (rows.length === 2) rows.push(["Algorithm usage", "none yet"]);
            rows.forEach(([k, v]) => {
                const d = document.createElement("div");
                d.className = "kv";
                d.innerHTML = `<span>${k}</span><span>${v}</span>`;
                box.appendChild(d);
            });
            this.renderRecent(s.runs || []);
        } catch (e) {
            box.innerHTML = '<div class="muted">No performance data yet.</div>';
            this.renderRecent([]);
        }
    },

    renderRecent(runs) {
        const tbody = document.querySelector("#recent-table tbody");
        if (!tbody) return;
        tbody.innerHTML = "";
        if (!runs.length) {
            tbody.innerHTML = '<tr><td colspan="7" class="muted">No route calculations yet.</td></tr>';
            return;
        }
        runs.forEach(r => {
            const tr = document.createElement("tr");
            const t = r.timestamp ? new Date(r.timestamp * 1000).toLocaleTimeString() : "–";
            tr.innerHTML =
                `<td>${t}</td>` +
                `<td>${r.source || "?"} → ${r.destination || "?"}</td>` +
                `<td>${r.objective || "–"}</td>` +
                `<td>${r.algorithm || "–"}</td>` +
                `<td>${Fmt.km(r.total_distance_km)}</td>` +
                `<td>${Fmt.inr(r.total_cost)}</td>` +
                `<td>${Fmt.ms(r.execution_time_ms)}</td>`;
            tbody.appendChild(tr);
        });
    },
};