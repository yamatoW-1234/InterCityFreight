window.StopManager = (() => {
  let cities = [];
  let stops = [];
  let avoidCities = [];
  let hubs = [];

  function populateSelect(selectEl, defaultId = 0) {
    selectEl.innerHTML = cities
      .map((c) => `<option value="${c.id}">${c.name} (${c.state || ''})</option>`)
      .join('');
    selectEl.value = String(defaultId);
  }

  function renderChips(container, list, onRemove) {
    const cityMap = new Map(cities.map((c) => [c.id, c.name]));
    container.innerHTML = list
      .map(
        (id) =>
          `<span class="chip">${cityMap.get(id) || id} <button data-id="${id}">&times;</button></span>`
      )
      .join('');
    container.querySelectorAll('button').forEach((btn) => {
      btn.addEventListener('click', () => onRemove(Number(btn.dataset.id)));
    });
  }

  function refreshAllChips() {
    renderChips(document.getElementById('stopsChipList'), stops, (id) => {
      stops = stops.filter((x) => x !== id);
      refreshAllChips();
    });
    renderChips(document.getElementById('avoidChipList'), avoidCities, (id) => {
      avoidCities = avoidCities.filter((x) => x !== id);
      refreshAllChips();
    });
    renderChips(document.getElementById('hubsChipList'), hubs, (id) => {
      hubs = hubs.filter((x) => x !== id);
      refreshAllChips();
    });
  }

  function init(cityList) {
    cities = cityList;
    populateSelect(document.getElementById('originSelect'), 0);
    populateSelect(document.getElementById('destSelect'), 14);
    populateSelect(document.getElementById('stopAddSelect'), 3);
    populateSelect(document.getElementById('avoidAddSelect'), 4);
    populateSelect(document.getElementById('hubAddSelect'), 0);

    document.getElementById('btnAddStop').addEventListener('click', () => {
      const id = Number(document.getElementById('stopAddSelect').value);
      if (!stops.includes(id)) stops.push(id);
      refreshAllChips();
    });

    document.getElementById('btnAddAvoid').addEventListener('click', () => {
      const id = Number(document.getElementById('avoidAddSelect').value);
      if (!avoidCities.includes(id)) avoidCities.push(id);
      refreshAllChips();
    });

    document.getElementById('btnAddHub').addEventListener('click', () => {
      const id = Number(document.getElementById('hubAddSelect').value);
      if (!hubs.includes(id)) hubs.push(id);
      refreshAllChips();
    });

    document.querySelectorAll('input[name="optimize_for"]').forEach((r) => {
      r.addEventListener('change', (e) => {
        const isMst = e.target.value === 'mst';
        document.getElementById('pointToPointCard').classList.toggle('hidden', isMst);
        document.getElementById('stopsCard').classList.toggle('hidden', isMst);
        document.getElementById('mstHubsCard').classList.toggle('hidden', !isMst);
      });
    });

    refreshAllChips();
  }

  function getState() {
    const mode = document.querySelector('input[name="optimize_for"]:checked').value;
    return {
      optimize_for: mode,
      origin: Number(document.getElementById('originSelect').value),
      destination: Number(document.getElementById('destSelect').value),
      departure_hour: Number(document.getElementById('departureHour').value),
      fuel_price_inr: Number(document.getElementById('fuelPrice').value),
      respect_time_windows: document.getElementById('respectWindows').checked,
      dynamic_cargo: document.getElementById('dynamicCargoToggle').checked,
      stops: [...stops],
      avoid_cities: [...avoidCities],
      hubs: [...hubs],
    };
  }

  function applyState(req) {
    if (!req) return;
    if (req.optimize_for) {
      const radio = document.querySelector(`input[name="optimize_for"][value="${req.optimize_for}"]`);
      if (radio) {
        radio.checked = true;
        radio.dispatchEvent(new Event('change'));
      }
    }
    if (req.origin !== undefined) document.getElementById('originSelect').value = String(req.origin);
    if (req.destination !== undefined) document.getElementById('destSelect').value = String(req.destination);
    if (req.departure_hour !== undefined) document.getElementById('departureHour').value = req.departure_hour;
    if (req.fuel_price_inr !== undefined) document.getElementById('fuelPrice').value = req.fuel_price_inr;
    if (req.respect_time_windows !== undefined)
      document.getElementById('respectWindows').checked = Boolean(req.respect_time_windows);
    if (req.dynamic_cargo !== undefined)
      document.getElementById('dynamicCargoToggle').checked = Boolean(req.dynamic_cargo);

    stops = Array.isArray(req.stops) ? [...req.stops] : [];
    avoidCities = Array.isArray(req.avoid_cities) ? [...req.avoid_cities] : [];
    hubs = Array.isArray(req.hubs) ? [...req.hubs] : [];
    refreshAllChips();
  }

  return { init, getState, applyState };
})();
