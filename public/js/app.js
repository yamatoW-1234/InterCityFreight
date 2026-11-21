(() => {
  let cities = [];
  let roads = [];
  let lastRun = null;

  async function refreshNetwork() {
    const [cRes, rRes, tRes] = await Promise.all([
      fetch('/api/cities'),
      fetch('/api/roads'),
      fetch('/api/traffic'),
    ]);
    cities = (await cRes.json()).cities || [];
    roads = (await rRes.json()).roads || [];
    const traffic = await tRes.json();

    const congCount = Math.floor((traffic.congested_edges || []).length / 2);
    const blkCount = Math.floor((traffic.blocked_edges || []).length / 2);
    document.getElementById('trafficSummaryText').textContent =
      `Traffic: ${congCount} congested | ${blkCount} blocked`;

    MapView.renderNetwork(cities, roads);
  }

  async function runOptimize() {
    const btn = document.getElementById('btnOptimize');
    btn.disabled = true;
    btn.textContent = 'Running C Engine...';
    try {
      const state = StopManager.getState();
      const payload = {
        ...state,
        vehicle_id: VehiclePanel.getSelectedVehicleId(),
        cargo_items: state.dynamic_cargo ? CargoPanel.getCargoItems() : [],
      };

      const res = await fetch('/api/optimize', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      const result = await res.json();
      if (!res.ok) throw new Error(result.details || result.error || 'Optimization error');

      lastRun = { request: payload, result };
      MapView.renderResult(result, cities);
      ItineraryPanel.render(result);
    } catch (err) {
      alert(err.message);
    } finally {
      btn.disabled = false;
      btn.textContent = 'Run Engine';
    }
  }

  function setupTabs() {
    document.querySelectorAll('.tab-btn').forEach((btn) => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.tab-btn').forEach((b) => b.classList.remove('active'));
        document.querySelectorAll('.tab-panel').forEach((p) => p.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById(`tab-${btn.dataset.tab}`).classList.add('active');
      });
    });
  }

  function setupThemeToggle() {
    const btn = document.getElementById('btnThemeToggle');
    btn.addEventListener('click', () => {
      const root = document.documentElement;
      const next = root.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
      root.setAttribute('data-theme', next);
      btn.textContent = next === 'dark' ? 'Light Mode' : 'Dark Mode';
      MapView.setTheme(next);
    });
  }

  async function bootstrap() {
    setupTabs();
    setupThemeToggle();

    const workspaceEl = document.querySelector('.workspace');
    const fsBtn = document.getElementById('btnMapFullscreen');
    if (fsBtn && workspaceEl) {
      const toggleFullscreen = (forceState) => {
        const next = forceState !== undefined ? forceState : !workspaceEl.classList.contains('map-fullscreen');
        workspaceEl.classList.toggle('map-fullscreen', next);
        fsBtn.textContent = next ? 'Exit Fullscreen' : 'Fullscreen';
        MapView.invalidateSize();
      };
      fsBtn.addEventListener('click', () => toggleFullscreen());
      document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape' && workspaceEl.classList.contains('map-fullscreen')) {
          toggleFullscreen(false);
        }
      });
    }

    const cfg = await fetch('/api/config').then((r) => r.json()).catch(() => ({}));

    MapView.init(async (from, to, blocked) => {
      await fetch('/api/traffic/roadblock', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ from, to, blocked, bidirectional: true }),
      });
      await refreshNetwork();
    }, cfg.cartoApiKey || '');

    await refreshNetwork();
    StopManager.init(cities);
    await VehiclePanel.init();
    await CargoPanel.init(cities);
    await HistoryMenu.init(
      () => lastRun,
      (plan) => {
        StopManager.applyState(plan.request);
        lastRun = { request: plan.request, result: plan.result };
        MapView.renderResult(plan.result, cities);
        ItineraryPanel.render(plan.result);
      }
    );

    document.getElementById('btnOptimize').addEventListener('click', runOptimize);
    document.getElementById('btnTickTraffic').addEventListener('click', async () => {
      await fetch('/api/traffic/tick', { method: 'POST' });
      await refreshNetwork();
    });
    document.getElementById('btnResetTraffic').addEventListener('click', async () => {
      await fetch('/api/traffic/reset', { method: 'POST' });
      await refreshNetwork();
    });
  }

  window.addEventListener('DOMContentLoaded', bootstrap);
})();