const express = require('express');
const fs = require('fs');
const path = require('path');

const cBridge = require('../services/cBridge');
const trafficSim = require('../services/trafficSim');
const planStore = require('../services/planStore');

const router = express.Router();
const DATA_DIR = path.join(__dirname, '..', '..', 'data');

function readJson(filename) {
  const filePath = path.join(DATA_DIR, filename);
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

// GET /api/cities
router.get('/cities', (_req, res) => {
  try {
    const data = readJson('cities.json');
    res.json(data);
  } catch (err) {
    res.status(500).json({ error: 'Failed to load cities.json', details: err.message });
  }
});

// GET /api/roads (merged with live simulated traffic & roadblocks)
router.get('/roads', (_req, res) => {
  try {
    const { roads } = readJson('roads.json');
    const liveRoads = trafficSim.applyToRoads(roads);
    res.json({ roads: liveRoads });
  } catch (err) {
    res.status(500).json({ error: 'Failed to load roads.json', details: err.message });
  }
});

// POST /api/optimize (invokes compiled C freight_engine)
router.post('/optimize', async (req, res) => {
  try {
    const { cities } = readJson('cities.json');
    const { roads } = readJson('roads.json');
    const { vehicles } = readJson('vehicles.json');

    const body = req.body || {};
    const liveRoads = trafficSim.applyToRoads(roads);

    // Resolve vehicle by ID or accept custom inline vehicle object
    let selectedVehicle = vehicles[0];
    if (body.vehicle && typeof body.vehicle === 'object') {
      selectedVehicle = body.vehicle;
    } else if (body.vehicle_id) {
      const match = vehicles.find((v) => v.id === body.vehicle_id);
      if (match) selectedVehicle = match;
    }

    const enginePayload = {
      cities,
      roads: liveRoads,
      vehicle: selectedVehicle,
      origin: Number(body.origin ?? 0),
      destination: Number(body.destination ?? 1),
      stops: Array.isArray(body.stops) ? body.stops.map(Number) : [],
      avoid_cities: Array.isArray(body.avoid_cities) ? body.avoid_cities.map(Number) : [],
      hubs: Array.isArray(body.hubs) ? body.hubs.map(Number) : [],
      departure_hour: Number(body.departure_hour ?? 8.0),
      fuel_price_inr: Number(body.fuel_price_inr ?? 95.0),
      optimize_for: body.optimize_for || 'fastest',
      respect_time_windows: body.respect_time_windows !== undefined ? Number(Boolean(body.respect_time_windows)) : 1,
      dynamic_cargo: body.dynamic_cargo ? 1 : 0,
      cargo_items: Array.isArray(body.cargo_items)
        ? body.cargo_items
        : body.dynamic_cargo && fs.existsSync(path.join(DATA_DIR, "cargo.json"))
          ? readJson("cargo.json").cargo_items || []
          : [],
    };

    const result = await cBridge.runEngine(enginePayload);
    res.json(result);
  } catch (err) {
    res.status(500).json({ error: 'Optimization failed', details: err.message });
  }
});

// Past routes & hub MST plans (Feature #5)
router.get('/plans', (_req, res) => {
  try {
    res.json({ plans: planStore.getAllPlans() });
  } catch (err) {
    res.status(500).json({ error: 'Failed to read saved plans', details: err.message });
  }
});

router.post('/plans', (req, res) => {
  try {
    const saved = planStore.savePlan(req.body);
    res.status(201).json(saved);
  } catch (err) {
    res.status(400).json({ error: 'Failed to save plan', details: err.message });
  }
});

router.delete('/plans/:id', (req, res) => {
  try {
    const deleted = planStore.deletePlan(req.params.id);
    if (!deleted) return res.status(404).json({ error: 'Plan not found' });
    res.json({ deleted: true, id: req.params.id });
  } catch (err) {
    res.status(500).json({ error: 'Failed to delete plan', details: err.message });
  }
});

module.exports = router;