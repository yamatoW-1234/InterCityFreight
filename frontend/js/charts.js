/* ============================================================================
   charts.js — Chart.js helpers for comparison + benchmark visualizations.
   ========================================================================== */

const Charts = {
    _instances: {},

    destroy(id) {
        if (this._instances[id]) { this._instances[id].destroy(); delete this._instances[id]; }
    },

    create(id, config) {
        this.destroy(id);
        const el = document.getElementById(id);
        if (!el) return;
        Chart.defaults.color = "#9fb0cc";
        Chart.defaults.borderColor = "#24365a";
        this._instances[id] = new Chart(el.getContext("2d"), config);
    },

    /* Comparison of fastest / cheapest / shortest. `routes` = array of RouteResult JSON. */
    renderComparison(canvasId, routes) {
        const labels = routes.map(r => r.objective + "\n(" + r.algorithm + ")");
        this.create(canvasId, {
            type: "bar",
            data: {
                labels,
                datasets: [
                    { label: "Distance (km)", data: routes.map(r => r.total_distance_km), backgroundColor: "#3b82f6", yAxisID: "y" },
                    { label: "Time (hrs)",    data: routes.map(r => r.total_time_hours),  backgroundColor: "#22d3ee", yAxisID: "y1" },
                    { label: "Cost (₹)",      data: routes.map(r => r.total_cost),        backgroundColor: "#34d399", yAxisID: "y2" },
                ],
            },
            options: {
                responsive: true,
                scales: {
                    y:  { position: "left",  title: { display: true, text: "km" } },
                    y1: { position: "right", title: { display: true, text: "hrs" }, grid: { drawOnChartArea: false } },
                    y2: { display: false },
                },
                plugins: { legend: { display: true } },
            },
        });
    },

    /* Benchmark: execution time (ms) vs graph size (V) for each algorithm. */
    renderBenchmark(canvasId, rows) {
        const labels = rows.map(r => r.vertices);
        this.create(canvasId, {
            type: "line",
            data: {
                labels,
                datasets: [
                    { label: "Dijkstra",     data: rows.map(r => r.dijkstra_ms),     borderColor: "#3b82f6", backgroundColor: "#3b82f6", tension: 0.2 },
                    { label: "A*",           data: rows.map(r => r.astar_ms),        borderColor: "#22d3ee", backgroundColor: "#22d3ee", tension: 0.2 },
                    { label: "Bellman-Ford", data: rows.map(r => r.bellman_ford_ms), borderColor: "#fbbf24", backgroundColor: "#fbbf24", tension: 0.2 },
                    { label: "Prim",         data: rows.map(r => r.prim_ms),         borderColor: "#a78bfa", backgroundColor: "#a78bfa", tension: 0.2 },
                ],
            },
            options: {
                responsive: true,
                scales: {
                    x: { title: { display: true, text: "Vertices (V)" } },
                    y: { title: { display: true, text: "Execution time (ms)" }, beginAtZero: true },
                },
                plugins: { legend: { display: true } },
            },
        });
    },
};