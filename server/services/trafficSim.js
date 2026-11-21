const fs = require('fs');
const path = require('path');

const ROADS_FILE = path.join(__dirname, '..', '..', 'data', 'roads.json');

const congestionMap = new Map(); // key: "from-to" -> multiplier (1.0 - 2.2)
const roadblocks = new Set();    // key: "from-to"
let timer = null;
let lastUpdated = new Date().toISOString();

function edgeKey(from, to) {
  return `${from}-${to}`;
}

function loadUniqueEdgePairs() {
  if (!fs.existsSync(ROADS_FILE)) return [];
  const { roads } = JSON.parse(fs.readFileSync(ROADS_FILE, 'utf8'));
  const seen = new Set();
  const pairs = [];
  for (const r of roads) {
    const k = r.from < r.to ? `${r.from}-${r.to}` : `${r.to}-${r.from}`;
    if (!seen.has(k)) {
      seen.add(k);
      pairs.push([r.from, r.to]);
    }
  }
  return pairs;
}

function tick() {
  const pairs = loadUniqueEdgePairs();
  if (pairs.length === 0) return getState();

  // Gently decay existing congestion toward 1.0
  for (const [k, mult] of congestionMap.entries()) {
    const next = Number((1.0 + (mult - 1.0) * 0.65).toFixed(2));
    if (next <= 1.05) congestionMap.delete(k);
    else congestionMap.set(k, next);
  }

  // Inject fresh simulated traffic on ~15% of corridors
  const sampleCount = Math.max(2, Math.floor(pairs.length * 0.15));
  for (let i = 0; i < sampleCount; i++) {
    const [u, v] = pairs[Math.floor(Math.random() * pairs.length)];
    const mult = Number((1.15 + Math.random() * 1.05).toFixed(2)); // 1.15x to 2.20x
    congestionMap.set(edgeKey(u, v), mult);
    congestionMap.set(edgeKey(v, u), mult);
  }

  lastUpdated = new Date().toISOString();
  return getState();
}

function setRoadblock(from, to, blocked, bidirectional = true) {
  const k1 = edgeKey(from, to);
  const k2 = edgeKey(to, from);
  if (blocked) {
    roadblocks.add(k1);
    if (bidirectional) roadblocks.add(k2);
  } else {
    roadblocks.delete(k1);
    if (bidirectional) roadblocks.delete(k2);
  }
  lastUpdated = new Date().toISOString();
  return getState();
}

function setCongestion(from, to, multiplier = 1.0, bidirectional = true) {
  const mult = Math.max(1.0, Math.min(3.0, Number(multiplier)));
  const k1 = edgeKey(from, to);
  const k2 = edgeKey(to, from);
  if (mult <= 1.01) {
    congestionMap.delete(k1);
    if (bidirectional) congestionMap.delete(k2);
  } else {
    congestionMap.set(k1, mult);
    if (bidirectional) congestionMap.set(k2, mult);
  }
  lastUpdated = new Date().toISOString();
  return getState();
}

function reset() {
  congestionMap.clear();
  roadblocks.clear();
  lastUpdated = new Date().toISOString();
  return getState();
}

function applyToRoads(roads) {
  return roads.map((r) => {
    const k = edgeKey(r.from, r.to);
    return {
      ...r,
      traffic_multiplier: congestionMap.get(k) || 1.0,
      is_blocked: roadblocks.has(k) ? 1 : 0,
    };
  });
}

function getState() {
  const congested_edges = [];
  for (const [k, multiplier] of congestionMap.entries()) {
    const [from, to] = k.split('-').map(Number);
    congested_edges.push({ from, to, multiplier });
  }

  const blocked_edges = [];
  for (const k of roadblocks.values()) {
    const [from, to] = k.split('-').map(Number);
    blocked_edges.push({ from, to });
  }

  return {
    last_updated: lastUpdated,
    congested_edges,
    blocked_edges,
  };
}

function startSimulation(intervalMs = 15000) {
  if (timer) return;
  tick();
  timer = setInterval(tick, intervalMs);
}

function stopSimulation() {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
}

module.exports = {
  startSimulation,
  stopSimulation,
  tick,
  setRoadblock,
  setCongestion,
  reset,
  applyToRoads,
  getState,
};