/* ============================================================================
   visualizer.js — Module 7: Data Structure Visualizer.
   Renders AVL trees and B-Trees (as SVG) from engine-returned structure JSON.
   Each step shows the tree after an insertion/deletion, making rotations and
   node splits visible.
   ========================================================================== */

function svgEl(tag, attrs) {
    const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
    for (const k in attrs) el.setAttribute(k, attrs[k]);
    return el;
}

/* ---- AVL layout + rendering ---- */
function avlPositions(root) {
    const pos = new Map();
    let counter = 0, maxDepth = 0;
    (function walk(node, depth) {
        if (!node) return;
        walk(node.left, depth + 1);
        pos.set(node, { x: counter++, y: depth });
        if (depth > maxDepth) maxDepth = depth;
        walk(node.right, depth + 1);
    })(root, 0);
    return { pos, width: counter, height: maxDepth + 1 };
}

function svgAVL(structure) {
    if (!structure) return null;
    const { pos, width, height } = avlPositions(structure);
    const sx = 46, sy = 54, pad = 30, r = 16;
    const svgW = Math.max(width * sx + pad, 80);
    const svgH = height * sy + pad;
    const svg = svgEl("svg", { width: svgW, height: svgH, viewBox: `0 0 ${svgW} ${svgH}` });

    (function edges(node) {
        if (!node) return;
        const p = pos.get(node);
        [node.left, node.right].forEach(ch => {
            if (ch) {
                const c = pos.get(ch);
                svg.appendChild(svgEl("line", {
                    x1: p.x * sx + pad, y1: p.y * sy + pad,
                    x2: c.x * sx + pad, y2: c.y * sy + pad,
                    stroke: "#33507e", "stroke-width": 2,
                }));
            }
        });
        edges(node.left); edges(node.right);
    })(structure);

    (function nodes(node) {
        if (!node) return;
        const p = pos.get(node);
        const cx = p.x * sx + pad, cy = p.y * sy + pad;
        svg.appendChild(svgEl("circle", { cx, cy, r, fill: "#1b2b45", stroke: "#3b82f6", "stroke-width": 2 }));
        const t = svgEl("text", { x: cx, y: cy + 4, "text-anchor": "middle", fill: "#e7eefc", "font-size": "12" });
        t.textContent = node.key;
        svg.appendChild(t);
        const bf = svgEl("text", { x: cx, y: cy - r - 4, "text-anchor": "middle", fill: "#6f7f9d", "font-size": "9" });
        bf.textContent = "bf " + node.bf;
        svg.appendChild(bf);
        nodes(node.left); nodes(node.right);
    })(structure);

    return svg;
}

/* ---- B-Tree layout + rendering ---- */
function btreeLayout(root) {
    const pos = new Map();
    let maxDepth = 0;
    function widthOf(node) {
        if (node.leaf) return Math.max(node.keys.length, 1);
        let w = 0; node.children.forEach(c => { w += widthOf(c); });
        return Math.max(w, node.keys.length);
    }
    function place(node, depth, leftX) {
        if (depth > maxDepth) maxDepth = depth;
        const w = widthOf(node);
        pos.set(node, { x: leftX + w / 2, y: depth });
        if (!node.leaf) {
            let cx = leftX;
            node.children.forEach(c => { const cw = widthOf(c); place(c, depth + 1, cx); cx += cw; });
        }
    }
    const totalW = place(root, 0, 0);
    return { pos, width: totalW, height: maxDepth + 1 };
}

function svgBTree(structure) {
    if (!structure) return null;
    const { pos, width, height } = btreeLayout(structure);
    const unitX = 36, rowH = 66, pad = 24, keyW = 30, boxH = 26;
    const svgW = Math.max(width * unitX + pad * 2, 90);
    const svgH = height * rowH + pad * 2;
    const svg = svgEl("svg", { width: svgW, height: svgH, viewBox: `0 0 ${svgW} ${svgH}` });

    (function edges(node) {
        if (!node || node.leaf) return;
        const p = pos.get(node);
        node.children.forEach(c => {
            const cp = pos.get(c);
            svg.appendChild(svgEl("line", {
                x1: p.x * unitX + pad, y1: p.y * rowH + pad + boxH,
                x2: cp.x * unitX + pad, y2: cp.y * rowH + pad,
                stroke: "#33507e", "stroke-width": 2,
            }));
        });
        node.children.forEach(edges);
    })(structure);

    (function nodes(node) {
        if (!node) return;
        const p = pos.get(node);
        const n = node.keys.length;
        const startX = p.x * unitX + pad - (n * keyW) / 2;
        const y = p.y * rowH + pad;
        node.keys.forEach((k, i) => {
            svg.appendChild(svgEl("rect", {
                x: startX + i * keyW, y, width: keyW, height: boxH,
                fill: "#1b2b45", stroke: "#22d3ee", "stroke-width": 1.5, rx: 4,
            }));
            const t = svgEl("text", {
                x: startX + i * keyW + keyW / 2, y: y + boxH / 2 + 4,
                "text-anchor": "middle", fill: "#e7eefc", "font-size": "11",
            });
            t.textContent = k;
            svg.appendChild(t);
        });
        if (!node.leaf) node.children.forEach(nodes);
    })(structure);

    return svg;
}

/* ---- Module ---- */
const Visualizer = {
    parseCSV(str) {
        return (str || "").split(",").map(s => parseInt(s.trim(), 10)).filter(x => !isNaN(x));
    },

    async runAVL() {
        const insert = this.parseCSV(document.getElementById("avl-insert").value);
        const del = this.parseCSV(document.getElementById("avl-delete").value);
        try {
            const res = await API.avlDemo({ insert, "delete": del });
            this.renderAVL(res);
        } catch (e) { UI.toast(e.message, "error"); }
    },

    renderAVL(res) {
        const canvas = document.getElementById("avl-canvas");
        canvas.innerHTML = "";
        (res.steps || []).forEach((s, i) => {
            const wrap = document.createElement("div");
            wrap.style.cssText = "display:inline-block;margin:6px;vertical-align:top;";
            const cap = document.createElement("div");
            cap.className = "muted small"; cap.textContent = "Step " + (i + 1); cap.style.textAlign = "center";
            wrap.appendChild(cap);
            const svg = svgAVL(s);
            if (svg) wrap.appendChild(svg);
            canvas.appendChild(wrap);
        });
        document.getElementById("avl-stats").innerHTML =
            `<div class="kv"><span>Size</span><span>${res.size}</span></div>` +
            `<div class="kv"><span>Height</span><span>${res.height}</span></div>` +
            `<div class="kv"><span>AVL invariants valid</span><span>${res.valid ? "Yes" : "No"}</span></div>`;
        const ino = document.getElementById("avl-inorder");
        ino.innerHTML = "";
        (res.inorder || []).forEach(v => {
            const s = document.createElement("span"); s.className = "node-pill"; s.textContent = v; ino.appendChild(s);
        });
    },

    async runBTree() {
        const degree = parseInt(document.getElementById("bt-degree").value, 10) || 3;
        const insert = this.parseCSV(document.getElementById("bt-insert").value);
        try {
            const res = await API.btreeDemo({ min_degree: degree, insert });
            this.renderBTree(res);
        } catch (e) { UI.toast(e.message, "error"); }
    },

    renderBTree(res) {
        const canvas = document.getElementById("bt-canvas");
        canvas.innerHTML = "";
        (res.steps || []).forEach((s, i) => {
            const wrap = document.createElement("div");
            wrap.style.margin = "10px 0";
            const cap = document.createElement("div");
            cap.className = "muted small"; cap.textContent = "After insertion " + (i + 1);
            wrap.appendChild(cap);
            const svg = svgBTree(s);
            if (svg) wrap.appendChild(svg);
            canvas.appendChild(wrap);   // FIXED: single append (was duplicated)
        });
        document.getElementById("bt-stats").innerHTML =
            `<div class="kv"><span>Minimum degree t</span><span>${res.min_degree}</span></div>` +
            `<div class="kv"><span>Keys</span><span>${res.size}</span></div>` +
            `<div class="kv"><span>Height</span><span>${res.height}</span></div>`;
        const ino = document.getElementById("bt-inorder");
        ino.innerHTML = "";
        (res.inorder || []).forEach(v => {
            const s = document.createElement("span"); s.className = "node-pill"; s.textContent = v; ino.appendChild(s);
        });
    },
};

document.getElementById("avl-go").addEventListener("click", () => Visualizer.runAVL());
document.getElementById("bt-go").addEventListener("click", () => Visualizer.runBTree());