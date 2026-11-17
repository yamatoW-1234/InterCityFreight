const express = require("express");
const { callEngine, findEngine } = require("../engineBridge");
const { analytics } = require("../services/analytics");

const router = express.Router();

async function runEngine(res, payload, onSuccess = null) {
  try {
    const result = await callEngine(payload);
    if (onSuccess) {
      onSuccess(result);
    }
    return res.json(result);
  } catch (err) {
    return res.status(500).json({
      status: "error",
      message: err.message || "Internal engine error",
    });
  }
}

// ---- Network & meta -------------------------------------------------------
router.get("/network", (req, res) => {
  return runEngine(res, { action: "network" });
});

router.get("/meta", (req, res) => {
  return runEngine(res, { action: "meta" });
});

router.get("/health", (req, res) => {
  const enginePath = findEngine();
  return res.json({
    status: "ok",
    engine_built: enginePath !== null,
    engine_path: enginePath,
  });
});

// ---- Routing --------------------------------------------------------------
router.post("/route", (req, res) => {
  const body = req.body || {};
  const payload = {
    action: "route",
    source: body.source || "",
    destination: body.destination || "",
    objective: body.objective || "fastest",
    budget_limit: Number(body.budget_limit) || 0,
    time_limit_hours: Number(body.time_limit_hours) || 0,
  };

  return runEngine(res, payload, (result) => {
    if (result && result.status === "ok") {
      analytics.record({
        source: payload.source,
        destination: payload.destination,
        objective: payload.objective,
        algorithm: result.algorithm,
        total_distance_km: result.total_distance_km,
        total_time_hours: result.total_time_hours,
        total_cost: result.total_cost,
        execution_time_ms: result.execution_time_ms,
      });
    }
  });
});

router.post("/compare", (req, res) => {
  const body = req.body || {};
  return runEngine(res, {
    action: "compare",
    source: body.source || "",
    destination: body.destination || "",
  });
});

router.post("/mst", (req, res) => {
  const body = req.body || {};
  return runEngine(res, {
    action: "mst",
    hubs: Array.isArray(body.hubs) ? body.hubs : [],
  });
});

// ---- Analysis & visualization ---------------------------------------------
router.post("/benchmark", (req, res) => {
  const body = req.body || {};
  return runEngine(res, {
    action: "benchmark",
    sizes: Array.isArray(body.sizes) ? body.sizes : [10, 25, 50, 100, 200],
  });
});

router.post("/avl-demo", (req, res) => {
  const body = req.body || {};
  return runEngine(res, {
    action: "avl_demo",
    insert: Array.isArray(body.insert) ? body.insert : [],
    delete: Array.isArray(body.delete) ? body.delete : [],
  });
});

router.post("/btree-demo", (req, res) => {
  const body = req.body || {};
  return runEngine(res, {
    action: "btree_demo",
    min_degree: Number(body.min_degree) || 3,
    insert: Array.isArray(body.insert) ? body.insert : [],
  });
});

router.get("/selfcheck", (req, res) => {
  return runEngine(res, { action: "selfcheck" });
});

// ---- Dashboard analytics --------------------------------------------------
router.get("/recent", (req, res) => {
  return res.json({
    status: "ok",
    runs: analytics.recent(10),
  });
});

router.get("/summary", (req, res) => {
  return res.json({
    status: "ok",
    summary: analytics.summary(),
    runs: analytics.recent(10),
  });
});

module.exports = router;