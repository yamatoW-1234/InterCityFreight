const path = require('path');
try { process.loadEnvFile(path.join(__dirname, '..', '.env')); } catch (_) {}
const express = require('express');
const cors = require('cors');

const routeApi = require('./routes/routeApi');
const trafficApi = require('./routes/trafficApi');
const vehicleApi = require('./routes/vehicleApi');
const cargoApi = require('./routes/cargoApi');
const trafficSim = require('./services/trafficSim');

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json({ limit: '2mb' }));

// Serve frontend static files from public/
const publicDir = path.join(__dirname, '..', 'public');
app.use(express.static(publicDir));

// API routes
app.get('/api/config', (_req, res) => {
  res.json({ cartoApiKey: process.env.CARTO_API_KEY || '' });
});

app.get('/api/health', (_req, res) => {
  res.json({ status: 'ok', app: 'CargoCompass', timestamp: new Date().toISOString() });
});

app.use('/api', routeApi);
app.use('/api/traffic', trafficApi);
app.use('/api/vehicles', vehicleApi);
app.use('/api/cargo', cargoApi);

// Fallback to index.html for root/SPA navigation
app.use((_req, res) => {
  res.sendFile(path.join(publicDir, 'index.html'));
});

app.listen(PORT, () => {
  console.log(`CargoCompass server running at http://localhost:${PORT} (CARTO key: ${process.env.CARTO_API_KEY ? 'loaded' : 'missing'})`);
  trafficSim.startSimulation();
});