const Charts = {
    _instances: {},

    destroy(id) {
        if (this._instances[id]) {
            this._instances[id].destroy();
            delete this._instances[id];
        }
    },

    create(id, config) {
        if (typeof Chart === "undefined") {
            UI.toast("Chart library failed to load — check network connection", "error");
            return;
        }
        this.destroy(id);
        const el = document.getElementById(id);
        if (!el) return;
        if (el.parentElement) {
            el.parentElement.style.position = "relative";
            el.parentElement.style.height = "280px";
        }
        Chart.defaults.color = "#9e9ea9";
        Chart.defaults.borderColor = "#27272e";
        this._instances[id] = new Chart(el.getContext("2d"), config);
    },

    renderComparison(id, routes) {
        const list = Array.isArray(routes) ? routes : [];
        const labels = list.map(r => `${r.objective} (${r.algorithm})`);
        this.create(id, {
            type: "bar",
            data: {
                labels,
                datasets: [
                    {
                        label: "Distance (km)",
                        data: list.map(r => Math.round(r.total_distance_km || 0)),
                        backgroundColor: "#f4f4f6",
                        borderRadius: 4,
                        yAxisID: "y",
                    },
                    {
                        label: "Time (hrs)",
                        data: list.map(r => +(r.total_time_hours || 0).toFixed(1)),
                        backgroundColor: "#a1a1aa",
                        borderRadius: 4,
                        yAxisID: "y1",
                    },
                    {
                        label: "Cost (₹)",
                        data: list.map(r => Math.round(r.total_cost || 0)),
                        backgroundColor: "#52525b",
                        borderRadius: 4,
                        yAxisID: "y2",
                    },
                ],
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: { labels: { color: "#e4e4e7", boxWidth: 12 } },
                },
                scales: {
                    x: { grid: { color: "#1b1b20" } },
                    y:  { position: "left",  title: { display: true, text: "km" }, grid: { color: "#1b1b20" } },
                    y1: { position: "right", title: { display: true, text: "hrs" }, grid: { drawOnChartArea: false } },
                    y2: { display: false },
                },
            },
        });
    },

    renderBenchmark(id, rows) {
        const list = Array.isArray(rows) ? rows : [];
        const labels = list.map(r => `${r.vertices} vertices / ${r.edges} edges`);
        this.create(id, {
            type: "line",
            data: {
                labels,
                datasets: [
                    {
                        label: "Dijkstra (ms)",
                        data: list.map(r => r.dijkstra_ms),
                        borderColor: "#f4f4f6",
                        backgroundColor: "#f4f4f6",
                        tension: 0.25,
                    },
                    {
                        label: "A* Search (ms)",
                        data: list.map(r => r.astar_ms),
                        borderColor: "#a1a1aa",
                        backgroundColor: "#a1a1aa",
                        borderDash: [6, 4],
                        tension: 0.25,
                    },
                ],
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: { labels: { color: "#e4e4e7", boxWidth: 12 } },
                },
                scales: {
                    x: { grid: { color: "#1b1b20" } },
                    y: { title: { display: true, text: "Execution Time (ms)" }, grid: { color: "#1b1b20" } },
                },
            },
        });
    },
};