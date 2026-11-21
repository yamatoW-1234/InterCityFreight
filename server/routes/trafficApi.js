const express = require('express');
const trafficSim = require('../services/trafficSim');

const router = express.Router();

// GET /api/traffic - current traffic multipliers & active roadblocks
router.get('/', (_req, res) => {
  res.json(trafficSim.getState());
});

// POST /api/traffic/tick - trigger a simulated traffic/incident refresh
router.post('/tick', (_req, res) => {
  const state = trafficSim.tick();
  res.json(state);
});

// POST /api/traffic/roadblock - toggle or set roadblock on edge (from <-> to)
router.post('/roadblock', (req, res) => {
  const { from, to, blocked, bidirectional = true } = req.body || {};
  if (from === undefined || to === undefined) {
    return res.status(400).json({ error: '`from` and `to` city IDs are required' });
  }
  const state = trafficSim.setRoadblock(Number(from), Number(to), Boolean(blocked), Boolean(bidirectional));
  res.json(state);
});

// POST /api/traffic/congestion - set custom traffic multiplier on an edge
router.post('/congestion', (req, res) => {
  const { from, to, multiplier = 1.0 } = req.body || {};
  if (from === undefined || to === undefined) {
    return res.status(400).json({ error: '`from` and `to` city IDs are required' });
  }
  const state = trafficSim.setCongestion(Number(from), Number(to), Number(multiplier));
  res.json(state);
});

// POST /api/traffic/reset - clear all roadblocks and reset traffic to 1.0x
router.post('/reset', (_req, res) => {
  res.json(trafficSim.reset());
});

module.exports = router;