const express = require('express');
const fs = require('fs');
const path = require('path');
const cBridge = require('../services/cBridge');

const router = express.Router();
const DATA_DIR = path.join(__dirname, '..', '..', 'data');
const CARGOS_FILE = path.join(DATA_DIR, 'cargos.json');

function readCargoStore() {
  if (!fs.existsSync(CARGOS_FILE)) {
    return { cargo_items: [] };
  }
  return JSON.parse(fs.readFileSync(CARGOS_FILE, 'utf8'));
}

function writeCargoStore(data) {
  fs.writeFileSync(CARGOS_FILE, JSON.stringify(data, null, 2) + '\n', 'utf8');
}

// GET /api/cargo - list all warehouse cargos (optionally filter by ?src_city_id=X)
router.get('/', (req, res) => {
  try {
    const data = readCargoStore();
    if (req.query.src_city_id !== undefined) {
      const srcId = Number(req.query.src_city_id);
      return res.json({ cargo_items: data.cargo_items.filter((c) => c.src_city_id === srcId) });
    }
    res.json(data);
  } catch (err) {
    res.status(500).json({ error: 'Failed to load cargos.json', details: err.message });
  }
});

// POST /api/cargo - create a custom cargo item (type, name, src_city_id, dest_city_id, weight_kg, value_inr)
router.post('/', (req, res) => {
  try {
    const b = req.body || {};
    if (
      !b.name ||
      b.src_city_id === undefined ||
      b.dest_city_id === undefined ||
      !b.weight_kg ||
      !b.value_inr
    ) {
      return res.status(400).json({
        error: 'type, name, src_city_id, dest_city_id, weight_kg, and value_inr are required',
      });
    }

    const newCargo = {
      id: b.id || `cargo_${Date.now()}`,
      type: String(b.type || 'General Freight').trim(),
      name: String(b.name).trim(),
      src_city_id: Number(b.src_city_id),
      dest_city_id: Number(b.dest_city_id),
      weight_kg: Number(b.weight_kg),
      value_inr: Number(b.value_inr),
    };

    const data = readCargoStore();
    data.cargo_items.push(newCargo);
    writeCargoStore(data);
    res.status(201).json(newCargo);
  } catch (err) {
    res.status(500).json({ error: 'Failed to create custom cargo', details: err.message });
  }
});

// DELETE /api/cargo/:id - remove a cargo item
router.delete('/:id', (req, res) => {
  try {
    const data = readCargoStore();
    const before = data.cargo_items.length;
    data.cargo_items = data.cargo_items.filter((c) => c.id !== req.params.id);
    if (data.cargo_items.length === before) {
      return res.status(404).json({ error: 'Cargo item not found' });
    }
    writeCargoStore(data);
    res.json({ deleted: true, id: req.params.id });
  } catch (err) {
    res.status(500).json({ error: 'Failed to delete cargo item', details: err.message });
  }
});

// POST /api/cargo/optimize - Partial Knapsack evaluation at a given stop/capacity
router.post('/optimize', async (req, res) => {
  try {
    const { cities } = JSON.parse(fs.readFileSync(path.join(DATA_DIR, 'cities.json'), 'utf8'));
    const { vehicles } = JSON.parse(fs.readFileSync(path.join(DATA_DIR, 'vehicles.json'), 'utf8'));

    const body = req.body || {};
    let selectedVehicle = vehicles[0];
    if (body.vehicle && typeof body.vehicle === 'object') {
      selectedVehicle = body.vehicle;
    } else if (body.vehicle_id) {
      const match = vehicles.find((v) => v.id === body.vehicle_id);
      if (match) selectedVehicle = match;
    }

    const result = await cBridge.runEngine({
      cities,
      roads: [],
      vehicle: selectedVehicle,
      origin: 0,
      destination: 0,
      cargo_items: Array.isArray(body.cargo_items) ? body.cargo_items : [],
    });

    res.json({
      runtime_ms: result.runtime_ms,
      knapsack: result.knapsack,
    });
  } catch (err) {
    res.status(500).json({ error: 'Cargo optimization failed', details: err.message });
  }
});

module.exports = router;