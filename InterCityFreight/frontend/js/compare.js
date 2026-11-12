/* ============================================================================
   compare.js — Module 3: Route Comparison.
   Renders fastest vs cheapest vs shortest from a single backend call.
   ========================================================================== */

const Compare = {
    async run() {
        const body = {
            source: document.getElementById("cmp-source").value,
            destination: document.getElementById("cmp-dest").value,
        };
        const btn = document.getElementById("cmp-go");
        btn.disabled = true; btn.textContent = "Comparing…";
        try {
            const res = await API.compare(body);
            this.render(res);
        } catch (e) {
            UI.toast(e.message, "error");
        } finally {
            btn.disabled = false; btn.textContent = "Compare Routes";
        }
    },

    render(res) {
        const box = document.getElementById("cmp-result");
        if (res.status !== "ok") {
            UI.toast(res.message || "Comparison failed", "error");
            UI.hide(box);
            return;
        }
        UI.show(box);

        const routes = res.routes || [];
        const tbody = document.querySelector("#cmp-table tbody");
        tbody.innerHTML = "";
        routes.forEach(r => {
            const ok = r.status === "ok";
            const tr = document.createElement("tr");
            tr.innerHTML =
                `<td>${r.objective}${ok ? "" : " (failed)"}</td>` +
                `<td>${r.algorithm || "–"}</td>` +
                `<td>${ok ? Fmt.km(r.total_distance_km) : "–"}</td>` +
                `<td>${ok ? Fmt.hrs(r.total_time_hours) : "–"}</td>` +
                `<td>${ok ? Fmt.inr(r.total_cost) : "–"}</td>` +
                `<td>${ok ? Fmt.ms(r.execution_time_ms) : "–"}</td>` +
                `<td>${ok ? Fmt.num(r.expanded_vertices) : "–"}</td>`;
            tbody.appendChild(tr);
        });

        const okRoutes = routes.filter(r => r.status === "ok");
        Charts.renderComparison("cmp-chart", okRoutes);
        Maps.ensure("cmp-map");
        Maps.clear("cmp-map");
        Maps.drawCompare("cmp-map", okRoutes);
        Maps.invalidate("cmp-map");
    },
};

document.getElementById("cmp-go").addEventListener("click", () => Compare.run());