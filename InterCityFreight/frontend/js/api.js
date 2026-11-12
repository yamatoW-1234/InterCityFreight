/* ============================================================================
   api.js — Thin fetch wrapper over the Flask REST API + shared formatters.
   All data shown in the UI comes from these backend responses.
   ========================================================================== */

const API = {
    base: "/api",

    async request(path, options = {}) {
        const res = await fetch(this.base + path, options);
        let data = null;
        try { data = await res.json(); } catch (e) { data = null; }
        if (!res.ok) {
            const msg = (data && data.message) ? data.message : ("HTTP " + res.status);
            throw new Error(msg);
        }
        return data;
    },

    get(path) { return this.request(path); },
    post(path, body) {
        return this.request(path, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body || {}),
        });
    },

    // ---- named endpoints ----
    network()    { return this.get("/network"); },
    meta()       { return this.get("/meta"); },
    health()     { return this.get("/health"); },
    summary()    { return this.get("/summary"); },
    recent()     { return this.get("/recent"); },
    selfcheck()  { return this.get("/selfcheck"); },
    route(body)  { return this.post("/route", body); },
    compare(b)   { return this.post("/compare", b); },
    mst(body)    { return this.post("/mst", body); },
    benchmark(b) { return this.post("/benchmark", b); },
    avlDemo(b)   { return this.post("/avl-demo", b); },
    btreeDemo(b) { return this.post("/btree-demo", b); },
};

/* Number / currency formatting helpers used across all modules. */
const Fmt = {
    num(x, digits = 0) {
        if (x === null || x === undefined || isNaN(x)) return "–";
        return Number(x).toLocaleString("en-IN", {
            maximumFractionDigits: digits, minimumFractionDigits: 0,
        });
    },
    km(x)  { return (x === null || x === undefined || isNaN(x)) ? "–" : Number(x).toFixed(1); },
    inr(x) { return (x === null || x === undefined || isNaN(x)) ? "–" : "₹" + Math.round(x).toLocaleString("en-IN"); },
    hrs(x) { return (x === null || x === undefined || isNaN(x)) ? "–" : Number(x).toFixed(1); },
    ms(x)  { return (x === null || x === undefined || isNaN(x)) ? "–" : Number(x).toFixed(2); },
};