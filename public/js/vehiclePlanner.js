window.VehiclePanel = (() => {
  let vehicles = [];

  function renderPreview() {
    const id = document.getElementById('vehicleSelect').value;
    const v = vehicles.find((item) => item.id === id) || vehicles[0];
    if (!v) return;
    document.getElementById('vehicleSpecPreview').innerHTML = `
      <div class="spec-item"><span>Type</span>${v.type}</div>
      <div class="spec-item"><span>Payload Cap</span>${v.cargo_capacity_kg.toLocaleString()} kg</div>
      <div class="spec-item"><span>Engine / Power</span>${v.engine_cc} cc (${v.horsepower} HP)</div>
      <div class="spec-item"><span>Mileage / Tank</span>${v.mileage_kmpl} km/L (${v.fuel_capacity_l} L)</div>
      <div class="spec-item"><span>Axle / Toll Mult</span>Class ${v.toll_axle_class} (${v.toll_multiplier}x)</div>
      <div class="spec-item"><span>Speed Cap</span>${v.max_speed_kmh} km/h</div>
    `;
  }

  async function loadVehicles(selectId = null) {
    const res = await fetch('/api/vehicles');
    const data = await res.json();
    vehicles = data.vehicles || [];
    const sel = document.getElementById('vehicleSelect');
    sel.innerHTML = vehicles
      .map((v) => `<option value="${v.id}">${v.name} (${(v.cargo_capacity_kg / 1000).toFixed(1)}T)</option>`)
      .join('');
    if (selectId && vehicles.some((v) => v.id === selectId)) {
      sel.value = selectId;
    }
    renderPreview();
  }

  async function init() {
    await loadVehicles();
    document.getElementById('vehicleSelect').addEventListener('change', renderPreview);

    document.getElementById('btnCreateTruck').addEventListener('click', async () => {
      const name = document.getElementById('customTruckName').value.trim();
      const engine_cc = Number(document.getElementById('customEngineCc').value);
      const cargo_capacity_kg = Number(document.getElementById('customCargoKg').value);
      const fuelVal = document.getElementById('customFuelL').value;
      if (!name || !engine_cc || !cargo_capacity_kg) {
        alert('Enter truck name, engine cc, and cargo capacity.');
        return;
      }

      const res = await fetch('/api/vehicles', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          name,
          engine_cc,
          cargo_capacity_kg,
          fuel_capacity_l: fuelVal ? Number(fuelVal) : undefined,
        }),
      });
      if (!res.ok) return alert('Failed to save custom truck.');
      const created = await res.json();
      document.getElementById('customTruckName').value = '';
      await loadVehicles(created.id);
    });
  }

  function getSelectedVehicleId() {
    return document.getElementById('vehicleSelect').value;
  }

  return { init, getSelectedVehicleId };
})();
