window.ItineraryPanel = (() => {
  function fmtHour(hr) {
    const day = Math.floor(hr / 24);
    const h = Math.floor(hr % 24);
    const m = Math.round((hr - Math.floor(hr)) * 60);
    const hh = String(h).padStart(2, '0');
    const mm = String(m).padStart(2, '0');
    return day > 0 ? `Day ${day + 1}, ${hh}:${mm}` : `${hh}:${mm}`;
  }

  function render(result) {
    document.getElementById('mAlgo').textContent = result.algorithm_used || '—';
    document.getElementById('mRuntime').textContent =
      result.runtime_ms !== undefined ? `${result.runtime_ms.toFixed(2)} ms` : '—';
    document.getElementById('mDist').textContent =
      result.total_distance_km !== undefined ? `${result.total_distance_km} km` : '—';

    if (result.mode === 'mst') {
      document.getElementById('mTime').textContent = `${(result.mst_edges || []).length} Links`;
      document.getElementById('mCost').textContent = result.connected ? 'Full MST' : 'Disconnected';
      document.getElementById('itineraryList').innerHTML = (result.mst_edges || [])
        .map(
          (e, i) => `
          <div class="timeline-row">
            <b>${i + 1}. ${e.from_name} ↔ ${e.to_name}</b> via ${e.highway} (${e.distance_km} km)
          </div>`
        )
        .join('');
    } else {
      document.getElementById('mTime').textContent =
        result.total_time_hr !== undefined ? `${result.total_time_hr.toFixed(1)} hrs` : '—';
      document.getElementById('mCost').textContent =
        result.net_cost_inr !== undefined
          ? `₹${Math.round(result.net_cost_inr).toLocaleString()} (Toll ₹${Math.round(result.total_toll_inr || 0)})`
          : '—';

      if (!result.found) {
        document.getElementById('itineraryList').innerHTML =
          '<p class="empty-state">No valid path found under current roadblocks/restrictions.</p>';
      } else {
        document.getElementById('itineraryList').innerHTML = (result.itinerary || [])
          .map((leg, i) => {
            const waitNote =
              leg.wait_hours > 0.05
                ? ` <span style="color:var(--warning)">(Waited ${leg.wait_hours.toFixed(1)}h for open window)</span>`
                : '';
            const breakNote =
              leg.break_hours > 0
                ? ` <span style="color:var(--success)">(+${leg.break_hours}h ${leg.break_type})</span>`
                : '';
            const f = leg.facilities || {};
            const facBits = [];
            if ((f.parking || []).length) facBits.push(`Parking: ${f.parking[0].name}`);
            if ((f.rest_food || []).length) facBits.push(`Food/Rest: ${f.rest_food[0].name}`);
            if ((f.repair || []).length) facBits.push(`Repair: ${f.repair[0].name}`);
            const cf = leg.corridor_facilities || {};
            const hwyBits = [];
            if ((cf.parking || []).length) hwyBits.push(`Hwy Parking: ${cf.parking[0].name}`);
            if ((cf.rest_food || []).length) hwyBits.push(`Hwy Rest/Food: ${cf.rest_food[0].name}`);
            if ((cf.repair || []).length) hwyBits.push(`Hwy Repair: ${cf.repair[0].name}`);
            const facHtml = [
              hwyBits.length ? `<div class="hint">On ${leg.via_highway}: ${hwyBits.join(' · ')}</div>` : '',
              facBits.length ? `<div class="hint">At ${leg.name}: ${facBits.join(' · ')}</div>` : '',
            ].join('');
            return `
            <div class="timeline-row">
              <b>${i + 1}. ${leg.name}</b> — ${fmtHour(leg.arrival_hour)}
              <span class="hint">(${leg.via_highway})</span>${waitNote}${breakNote}
              ${facHtml}
            </div>`;
          })
          .join('');
      }
    }

    const events = result.cargo_events || [];
    const cargoEl = document.getElementById('cargoEventsList');
    if (events.length > 0) {
      cargoEl.innerHTML = events
        .map(
          (ev) => `
        <div class="timeline-row ${ev.action}">
          <b>${ev.action.toUpperCase()}</b> at <b>${ev.city_name}</b>: ${ev.item_name}
          (${ev.weight_kg} kg, ${(ev.fraction * 100).toFixed(0)}%, ₹${Math.round(ev.value_inr).toLocaleString()})
          ${ev.action === 'pickup' ? `→ Drop at <b>${ev.dest_city_name}</b>` : ''}
        </div>`
        )
        .join('');
    } else if (result.knapsack && result.knapsack.allocations && result.knapsack.allocations.length > 0) {
      cargoEl.innerHTML = result.knapsack.allocations
        .map(
          (a) => `
        <div class="timeline-row">
          <b>${a.item_name}</b> (${a.city_name}): ${a.taken_kg}/${a.weight_kg} kg (${(a.fraction * 100).toFixed(0)}%)
        </div>`
        )
        .join('');
    } else {
      cargoEl.innerHTML =
        '<p class="empty-state">Enable Dynamic Cargo on the Route tab to see per-stop pickup &amp; dropoff events.</p>';
    }
  }

  return { render };
})();