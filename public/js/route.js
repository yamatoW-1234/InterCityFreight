/* ============================================================================
   route.js — Module 2: Route Generator.
   Sends the objective to the backend; the C++ decision controller picks the
   algorithm. This file only renders the returned result.
   ========================================================================== */

const Route = {
    async generate() {
        const body = {
            source: document.getElementById("route-source").value,
            destination: document.getElementById("route-dest").value,
            objective: document.getElementById("route-objective").value,
            budget_limit: parseFloat(document.getElementById("route-budget").value) || 0,
            time_limit_hours: parseFloat(document.getElementById("route-time").value) || 0,
        };
        const btn = document.getElementById("route-go");
        btn.disabled = true; btn.textContent = "Computing…";
        try {
            const res = await API.route(body);
            this.render(res);
        } catch (e) {
            UI.toast(e.message, "error");
        } finally {
            btn.disabled = false; btn.textContent = "Generate Route";
        }
    },

    render(r) {
        const box = document.getElementById("route-result");
        if (r.status !== "ok") {
            UI.toast(r.message || "Route generation failed", "error");
            UI.hide(box);
            return;
        }
        UI.show(box);

        document.getElementById("route-algo").textContent = r.algorithm;
        document.getElementById("route-reason").textContent = r.selection_reason || "";

        const bb = document.getElementById("route-budget-badge");
        const tb = document.getElementById("route-time-badge");
        if (r.budget_limit > 0) {
            bb.textContent = r.within_budget ? "Within budget ✓" : "Over budget ✗";
            bb.className = "badge " + (r.within_budget ? "ok" : "bad");
        } else { bb.textContent = "No budget limit"; bb.className = "badge"; }
        if (r.time_limit_hours > 0) {
            tb.textContent = r.within_time ? "Within time ✓" : "Over time ✗";
            tb.className = "badge " + (r.within_time ? "ok" : "bad");
        } else { tb.textContent = "No time limit"; tb.className = "badge"; }

        document.getElementById("r-distance").textContent = Fmt.km(r.total_distance_km);
        document.getElementById("r-time").textContent = Fmt.hrs(r.total_time_hours);
        document.getElementById("r-toll").textContent = Fmt.inr(r.total_toll);
        document.getElementById("r-fuel").textContent = Fmt.inr(r.total_fuel);
        document.getElementById("r-subsidy").textContent = Fmt.inr(r.total_subsidy);
        document.getElementById("r-cost").textContent = Fmt.inr(r.total_cost);

        const ol = document.getElementById("route-path");
        ol.innerHTML = "";
        (r.path || []).forEach(name => {
            const li = document.createElement("li");
            li.textContent = name;
            ol.appendChild(li);
        });
        document.getElementById("route-exec").textContent =
            `Engine: ${Fmt.ms(r.execution_time_ms)} ms · Expanded vertices: ${Fmt.num(r.expanded_vertices)}` +
            (r.negative_cycle ? " · negative cycle detected" : "");

        const tbody = document.querySelector("#route-edges tbody");
        tbody.innerHTML = "";
        (r.edges || []).forEach(e => {
            const tr = document.createElement("tr");
            tr.innerHTML =
                `<td>${e.from} → ${e.to}</td>` +
                `<td>${e.highway}</td>` +
                `<td>${Fmt.km(e.distance_km)}</td>` +
                `<td>${Fmt.hrs(e.time_hours)}</td>` +
                `<td>${Fmt.inr(e.toll)}</td>` +
                `<td>${Fmt.inr(e.fuel)}</td>` +
                `<td>${Fmt.inr(e.subsidy)}</td>` +
                `<td>${Fmt.inr(e.cost)}</td>`;
            tbody.appendChild(tr);
        });

        Maps.ensure("route-map");
        Maps.clear("route-map");
        if (App.network) Maps.drawRoads("route-map", App.network);
        Maps.drawRoute("route-map", r.path_coords, "#f4f4f6", r.path || []);
        Maps.invalidate("route-map");
    },
};

document.getElementById("route-go").addEventListener("click", () => Route.generate());