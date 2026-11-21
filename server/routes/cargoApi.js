const express = require('express');
const fs = require('fs');
const path = require('path');
const cBridge = require('../services/cBridge');

const router = express.Router();
const DATA_DIR = path.join(__dirname, '..', '..', 'data');
const CARGO_FILE = path.join(DATA_DIR, 'cargo.json');

const DEFAULT_CARGO = [
  { id: 'c1', type: 'Electronics', name: 'Semiconductors & PCBs', src_city_id: 0, dest_city_id: 22, weight_kg: 4500, value_inr: 450000 },
  { id: 'c2', type: 'Construction', name: 'Marble & Granite Slabs', src_city_id: 3, dest_city_id: 14, weight_kg: 12000, value_inr: 180000 },
  { id: 'c3', type: 'Pharma', name: 'Cold-Pack Vaccines', src_city_id: 11, dest_city_id: 20, weight_kg: 3200, value_inr: 384000 },
  { id: 'c4', type: 'Automotive', name: 'EV Battery Packs', src_city_id: 23, dest_city_id: 15, weight_kg: 7500, value_inr: 525000 },
  { id: 'c5', type: 'Machinery', name: 'CNC Lathe Parts', src_city_id: 1, dest_city_id: 14, weight_kg: 11000, value_inr: 330000 },
  { id: 'c6', type: 'Textiles', name: 'Jute & Cotton Bales', src_city_id: 9, dest_city_id: 0, weight_kg: 8000, value_inr: 120000 },
];

function readCargoStore() {
  if (!fs.existsSync(CARGO_FILE)) {
    fs.writeFileSync(CARGO_FILE, JSON.stringify({ cargo_items: DEFAULT_CARGO }, null, 2) + '\n', 'utf8');
  }
  return JSON.parse(fs.readFileSync(CARGO_FILE, 'utf8'));
}

function writeCargoStore(data) {
  fs.writeFileSync(CARGO_FILE, JSON.stringify(data, null, 2) + '\n', 'utf8');
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
    res.status(500).json({ error: 'Failed to load cargo.json', details: err.message });
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