/* ============================================================================
   analysis.js — Module 6: Algorithm Analysis.
   Complexity figures and algorithm metadata come from /api/meta (C++ engine).
   Benchmark timings come from /api/benchmark (measured on synthetic graphs).
   Pseudocode below is standard academic reference text, not measured data.
   ========================================================================== */

const PSEUDOCODE = {
    "A* Search":
`openSet ← {source};  g[source] ← 0
while openSet is not empty:
    u ← node with lowest f = g + h
    if u = goal: return reconstruct_path(u)
    move u to closedSet
    for each neighbour v of u:
        tentative ← g[u] + travel_time(u, v)
        if tentative < g[v]:
            g[v] ← tentative
            f[v] ← tentative + haversine(v, goal) / max_speed
            pred[v] ← u;  insert/decrease-key v in openSet`,

    "Dijkstra":
`dist[source] ← 0;  PQ ← {source}
while PQ is not empty:
    u ← extract-min(PQ)
    if u already settled: continue
    settle u
    for each neighbour v of u:
        if dist[u] + w(u, v) < dist[v]:
            dist[v] ← dist[u] + w(u, v)
            pred[v] ← u
            decrease-key(v) in PQ`,

    "Bellman-Ford":
`dist[source] ← 0
repeat |V| - 1 times:
    for each edge (u, v):
        if dist[u] + w(u, v) < dist[v]:
            dist[v] ← dist[u] + w(u, v)
            pred[v] ← u
for each edge (u, v):              # negative-cycle check
    if dist[u] + w(u, v) < dist[v]:
        report reachable negative cycle`,

    "Prim's MST":
`key[root] ← 0;  PQ ← all hubs
while PQ is not empty:
    u ← extract-min(PQ);  add u to MST
    for each hub v adjacent to u, v not in MST:
        if w(u, v) < key[v]:
            key[v] ← w(u, v)
            parent[v] ← u
            decrease-key(v) in PQ`,
};

const PRE_STYLE = "background:var(--bg-soft);padding:12px;border-radius:8px;" +
                  "overflow-x:auto;font-family:var(--mono);white-space:pre;" +
                  "color:var(--text-dim);font-size:12px;line-height:1.4;";

const Analysis = {
    cardsBuilt: false,

    ensureCards() {
        if (this.cardsBuilt) return;
        this.cardsBuilt = true;
        this.buildCards();
    },

    async buildCards() {
        const box = document.getElementById("algo-cards");
        if (!box) return;
        box.innerHTML = "";

        let meta = App.meta;
        if (!meta) { try { meta = await API.meta(); } catch (e) { meta = null; } }
        if (!meta || !meta.algorithms) {
            box.innerHTML = '<div class="muted">Algorithm metadata unavailable.</div>';
            return;
        }

        // Graph metrics card (real network V / E from the engine).
        const gCard = document.createElement("div");
        gCard.className = "card";
        const n = App.network || {};
        gCard.innerHTML =
            `<div class="card-title">Live Network Metrics</div>` +
            `<div class="kv-list">` +
            `<div class="kv"><span>Vertices (cities)</span><span>${Fmt.num(n.num_cities)}</span></div>` +
            `<div class="kv"><span>Undirected roads</span><span>${Fmt.num(n.num_roads)}</span></div>` +
            `<div class="kv"><span>Directed edges</span><span>${Fmt.num(n.num_directed_edges)}</span></div>` +
            `<div class="kv"><span>Negative financial edge</span><span>${n.has_negative_financial_edge ? "Yes" : "No"}</span></div>` +
            `</div>` +
            `<p class="muted small" style="margin-top:10px">Theoretical complexity (below) is separate from experimental benchmark timings.</p>`;
        box.appendChild(gCard);

        meta.algorithms.forEach(a => {
            const card = document.createElement("div");
            card.className = "card";
            card.innerHTML =
                `<div class="card-title">${a.name}</div>` +
                `<p class="small muted" style="margin:0 0 10px">${a.purpose}</p>` +
                `<div class="kv-list">` +
                `<div class="kv"><span>Edge weight</span><span>${a.weight}</span></div>` +
                `<div class="kv"><span>Heuristic</span><span>${a.heuristic}</span></div>` +
                `<div class="kv"><span>Best / Avg / Worst</span><span>${a.time_best} / ${a.time_avg} / ${a.time_worst}</span></div>` +
                `<div class="kv"><span>Space</span><span>${a.space}</span></div>` +
                `<div class="kv"><span>Data structure</span><span>${a.data_structure}</span></div>` +
                `</div>` +
                `<pre style="${PRE_STYLE}">${PSEUDOCODE[a.name] || ""}</pre>`;
            box.appendChild(card);
        });
    },

    async runBenchmark() {
        const btn = document.getElementById("bench-go");
        const status = document.getElementById("bench-status");
        btn.disabled = true;
        status.textContent = "Running benchmark on synthetic graphs…";
        try {
            const res = await API.benchmark({ sizes: [10, 25, 50, 100, 200, 400] });
            const rows = res.results || [];
            Charts.renderBenchmark("bench-chart", rows);
            const tbody = document.querySelector("#bench-table tbody");
            tbody.innerHTML = "";
            rows.forEach(r => {
                const tr = document.createElement("tr");
                tr.innerHTML =
                    `<td>${Fmt.num(r.vertices)}</td>` +
                    `<td>${Fmt.num(r.edges)}</td>` +
                    `<td>${Fmt.ms(r.dijkstra_ms)}</td>` +
                    `<td>${Fmt.ms(r.astar_ms)}</td>` +
                    `<td>${Fmt.ms(r.bellman_ford_ms)}</td>` +
                    `<td>${Fmt.ms(r.prim_ms)}</td>`;
                tbody.appendChild(tr);
            });
            status.textContent = "Benchmark complete (" + rows.length + " graph sizes).";
        } catch (e) {
            status.textContent = "";
            UI.toast(e.message, "error");
        } finally {
            btn.disabled = false;
        }
    },
};

document.getElementById("bench-go").addEventListener("click", () => Analysis.runBenchmark());