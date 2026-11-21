const express = require('express');
const fs = require('fs');
const path = require('path');

const router = express.Router();
const VEHICLES_FILE = path.join(__dirname, '..', '..', 'data', 'vehicles.json');

function readVehicles() {
  return JSON.parse(fs.readFileSync(VEHICLES_FILE, 'utf8'));
}

function writeVehicles(data) {
  fs.writeFileSync(VEHICLES_FILE, JSON.stringify(data, null, 2) + '\n', 'utf8');
}

function deriveTruckSpecs(cargoCapacityKg, engineCc) {
  let toll_axle_class = 2;
  let toll_multiplier = 1.0;
  let type = 'Intermediate Commercial Truck (4x2)';
  let defaultFuelL = 180;

  if (cargoCapacityKg > 32000) {
    toll_axle_class = 5;
    toll_multiplier = 1.9;
    type = 'Heavy Tractor-Trailer (6x4)';
    defaultFuelL = 450;
  } else if (cargoCapacityKg > 22000) {
    toll_axle_class = 4;
    toll_multiplier = 1.55;
    type = 'Multi-Axle Rigid Haulage (10x2)';
    defaultFuelL = 375;
  } else if (cargoCapacityKg > 12000) {
    toll_axle_class = 3;
    toll_multiplier = 1.15;
    type = 'Heavy Rigid Truck (6x2)';
    defaultFuelL = 320;
  }

  const horsepower = Math.round(engineCc * 0.041);
  const mileage_kmpl = Number(
    Math.max(2.0, Math.min(7.5, 7.6 - cargoCapacityKg / 8500 - engineCc / 9000)).toFixed(2)
  );

  return {
    type,
    horsepower,
    mileage_kmpl,
    toll_axle_class,
    toll_multiplier,
    defaultFuelL,
    max_speed_kmh: 80, // MoRTH statutory max for commercial freight vehicles
  };
}

// GET /api/vehicles - list all fleet trucks
router.get('/', (_req, res) => {
  try {
    res.json(readVehicles());
  } catch (err) {
    res.status(500).json({ error: 'Failed to read vehicles.json', details: err.message });
  }
});

// POST /api/vehicles - create custom truck (user gives name, engine_cc, cargo_capacity_kg, optional fuel_capacity_l)
router.post('/', (req, res) => {
  try {
    const b = req.body || {};
    if (!b.name || !b.cargo_capacity_kg || !b.engine_cc) {
      return res.status(400).json({
        error: 'name, engine_cc, and cargo_capacity_kg are required',
      });
    }

    const cargo_capacity_kg = Number(b.cargo_capacity_kg);
    const engine_cc = Number(b.engine_cc);
    const derived = deriveTruckSpecs(cargo_capacity_kg, engine_cc);

    const newVehicle = {
      id: `custom_${Date.now()}`,
      name: String(b.name).trim(),
      type: derived.type,
      engine_cc,
      horsepower: derived.horsepower,
      fuel_capacity_l: Number(b.fuel_capacity_l || derived.defaultFuelL),
      mileage_kmpl: derived.mileage_kmpl,
      cargo_capacity_kg,
      max_speed_kmh: derived.max_speed_kmh,
      toll_axle_class: derived.toll_axle_class,
      toll_multiplier: derived.toll_multiplier,
      is_custom: true,
    };

    const data = readVehicles();
    data.vehicles.push(newVehicle);
    writeVehicles(data);
    res.status(201).json(newVehicle);
  } catch (err) {
    res.status(500).json({ error: 'Failed to save custom vehicle', details: err.message });
  }
});

// DELETE /api/vehicles/:id - remove a custom truck
router.delete('/:id', (req, res) => {
  try {
    const data = readVehicles();
    const target = data.vehicles.find((v) => v.id === req.params.id);
    if (!target) return res.status(404).json({ error: 'Vehicle not found' });
    if (!target.is_custom) return res.status(403).json({ error: 'Cannot delete preset vehicles' });

    data.vehicles = data.vehicles.filter((v) => v.id !== req.params.id);
    writeVehicles(data);
    res.json({ deleted: true, id: req.params.id });
  } catch (err) {
    res.status(500).json({ error: 'Failed to delete vehicle', details: err.message });
  }
});

module.exports = router;