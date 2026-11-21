window.HistoryMenu = (() => {
  let plans = [];
  let onLoadCb = null;
  let getCurrentPayloadCb = null;

  function render() {
    const el = document.getElementById('savedPlansList');
    if (plans.length === 0) {
      el.innerHTML = '<p class="empty-state">No saved routes or hub plans yet.</p>';
      return;
    }
    el.innerHTML = plans
      .map(
        (p) => `
      <div class="list-item">
        <div>
          <strong>${p.title}</strong><br>
          <span class="hint">${p.mode.toUpperCase()} | ${p.algorithm_used || ''}</span>
        </div>
        <div style="display:flex; gap:4px;">
          <button class="btn btn-sm" data-load="${p.id}">Load</button>
          <button class="btn btn-sm btn-danger-outline" data-del="${p.id}">&times;</button>
        </div>
      </div>`
      )
      .join('');

    el.querySelectorAll('[data-load]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const plan = plans.find((x) => x.id === btn.dataset.load);
        if (plan && onLoadCb) onLoadCb(plan);
      });
    });

    el.querySelectorAll('[data-del]').forEach((btn) => {
      btn.addEventListener('click', async () => {
        await fetch(`/api/plans/${btn.dataset.del}`, { method: 'DELETE' });
        await refresh();
      });
    });
  }

  async function refresh() {
    const res = await fetch('/api/plans');
    const data = await res.json();
    plans = data.plans || [];
    render();
  }

  async function init(getCurrentPayload, onLoad) {
    getCurrentPayloadCb = getCurrentPayload;
    onLoadCb = onLoad;
    await refresh();

    document.getElementById('btnSavePlan').addEventListener('click', async () => {
      const current = getCurrentPayloadCb ? getCurrentPayloadCb() : null;
      if (!current || !current.result) {
        alert('Run the engine first to generate a route or MST before saving.');
        return;
      }
      const title =
        document.getElementById('planTitleInput').value.trim() ||
        `Plan (${new Date().toLocaleTimeString()})`;

      await fetch('/api/plans', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          title,
          mode: current.result.mode || 'route',
          algorithm_used: current.result.algorithm_used,
          request: current.request,
          result: current.result,
        }),
      });
      document.getElementById('planTitleInput').value = '';
      await refresh();
    });
  }

  return { init, refresh };
})();
