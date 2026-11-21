window.CargoPanel = (() => {
  let cities = [];
  let cargoItems = [];

  function cityName(id) {
    const c = cities.find((x) => x.id === id);
    return c ? c.name : `City #${id}`;
  }

  function renderList() {
    const el = document.getElementById('cargoList');
    if (cargoItems.length === 0) {
      el.innerHTML = '<p class="empty-state">No warehouse cargo items yet.</p>';
      return;
    }
    el.innerHTML = cargoItems
      .map(
        (c) => `
      <div class="list-item">
        <div>
          <strong>${c.name}</strong> <span class="hint">(${c.type})</span><br>
          <span class="hint">${cityName(c.src_city_id)} → ${cityName(c.dest_city_id)} | ${c.weight_kg} kg | ₹${Number(c.value_inr).toLocaleString()}</span>
        </div>
        <button class="btn btn-sm btn-danger-outline" data-id="${c.id}">&times;</button>
      </div>`
      )
      .join('');

    el.querySelectorAll('button').forEach((btn) => {
      btn.addEventListener('click', async () => {
        await fetch(`/api/cargo/${btn.dataset.id}`, { method: 'DELETE' });
        await refresh();
      });
    });
  }

  async function refresh() {
    const res = await fetch('/api/cargo');
    const data = await res.json();
    cargoItems = data.cargo_items || [];
    renderList();
  }

  async function init(cityList) {
    cities = cityList;
    const opts = cities.map((c) => `<option value="${c.id}">${c.name}</option>`).join('');
    document.getElementById('cargoSrcSelect').innerHTML = opts;
    document.getElementById('cargoDestSelect').innerHTML = opts;
    if (cities.length > 1) document.getElementById('cargoDestSelect').value = String(cities[1].id);

    await refresh();

    document.getElementById('btnAddCargo').addEventListener('click', async () => {
      const name = document.getElementById('cargoName').value.trim();
      const type = document.getElementById('cargoType').value.trim() || 'General Freight';
      const src_city_id = Number(document.getElementById('cargoSrcSelect').value);
      const dest_city_id = Number(document.getElementById('cargoDestSelect').value);
      const weight_kg = Number(document.getElementById('cargoWeight').value);
      const value_inr = Number(document.getElementById('cargoValue').value);

      if (!name || !weight_kg || !value_inr) {
        alert('Please enter cargo name, weight, and value.');
        return;
      }

      await fetch('/api/cargo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type, name, src_city_id, dest_city_id, weight_kg, value_inr }),
      });
      document.getElementById('cargoName').value = '';
      await refresh();
    });
  }

  function getCargoItems() {
    return cargoItems;
  }

  return { init, getCargoItems };
})();
