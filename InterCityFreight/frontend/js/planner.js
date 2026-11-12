/* ============================================================================
   planner.js — Module 5: Multi-Hub Planner (Prim's MST).
   ========================================================================== */

const Planner = {
    selected: new Set(),

    buildHubPicker(cities) {
        const box = document.getElementById("hub-picker");
        if (!box) return;
        box.innerHTML = "";
        this.selected.clear();
        cities.forEach(c => {
            const chip = document.createElement("div");
            chip.className = "hub-chip";
            chip.textContent = c.name;
            chip.addEventListener("click", () => {
                if (this.selected.has(c.name)) {
                    this.selected.delete(c.name);
                    chip.classList.remove("selected");
                } else {
                    this.selected.add(c.name);
                    chip.classList.add("selected");
                }
            });
            box.appendChild(chip);
        });
    },

    async run() {
        const hubs = [...this.selected];
        if (hubs.length < 2) { UI.toast("Select at least two hubs", "error"); return; }
        const btn = document.getElementById("mst-go");
        btn.disabled = true; btn.textContent = "Computing…";
        try {
            const res = await API.mst({ hubs });
            this.render(res);
        } catch (e) {
            UI.toast(e.message, "error");
        } finally {
            btn.disabled = false; btn.textContent = "Run Prim's MST";
        }
    },

    render(m) {
        const box = document.getElementById("mst-result");
        if (m.status !== "ok") {
            UI.toast(m.message || "MST planning failed", "error");
            UI.hide(box);
            return;
        }
        UI.show(box);
        document.getElementById("mst-hubs").textContent = Fmt.num((m.hubs || []).length);
        document.getElementById("mst-distance").textContent = Fmt.km(m.total_distance_km);
        document.getElementById("mst-cost").textContent = Fmt.inr(m.total_cost);
        document.getElementById("mst-exec").textContent = Fmt.ms(m.execution_time_ms);

        const tbody = document.querySelector("#mst-edges tbody");
        tbody.innerHTML = "";
        (m.mst_edges || []).forEach(e => {
            const tr = document.createElement("tr");
            const corridor = (e.corridor_path || []).join(" → ");
            tr.innerHTML =
                `<td>${e.from} ↔ ${e.to}</td>` +
                `<td class="small">${corridor}</td>` +
                `<td>${Fmt.km(e.distance_km)}</td>` +
                `<td>${Fmt.inr(e.cost)}</td>`;
            tbody.appendChild(tr);
        });

        Maps.ensure("mst-map");
        Maps.clear("mst-map");
        Maps.drawMST("mst-map", m);
        Maps.invalidate("mst-map");
    },

    clear() {
        this.selected.clear();
        document.querySelectorAll(".hub-chip").forEach(c => c.classList.remove("selected"));
        UI.hide(document.getElementById("mst-result"));
    },
};

document.getElementById("mst-go").addEventListener("click", () => Planner.run());
document.getElementById("mst-clear").addEventListener("click", () => Planner.clear());