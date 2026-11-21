const fs = require('fs');
const path = require('path');

const STOPPAGES_FILE = path.join(__dirname, '..', '..', 'data', 'stoppages.json');

function loadStoppages() {
  if (!fs.existsSync(STOPPAGES_FILE)) return { by_city: {}, by_corridor: {} };
  try {
    const raw = JSON.parse(fs.readFileSync(STOPPAGES_FILE, 'utf8'));
    return {
      by_city: raw.by_city || raw.stoppages || {},
      by_corridor: raw.by_corridor || {},
    };
  } catch (_) {
    return { by_city: {}, by_corridor: {} };
  }
}

function getCorridorFacilities(byCorridor, u, v) {
  const a = Math.min(u, v);
  const b = Math.max(u, v);
  return byCorridor[`${a}-${b}`] || { parking: [], rest_food: [], repair: [] };
}

/**
 * Enriches an engine route result with driver food/rest (+0.5h after ~4.5h driving),
 * overnight sleep + truck parking (+8.0h after ~10h driving), and both city & mid-highway facilities.
 */
function enrichItinerary(result, options = {}) {
  if (!result || result.mode !== 'route' || !result.found || !Array.isArray(result.itinerary)) {
    return result;
  }

  const { by_city, by_corridor } = loadStoppages();
  const maxDriveFoodHr = Number(options.max_drive_before_food_hr || 4.5);
  const foodBreakHr = Number(options.food_break_hr || 0.5);
  const maxDriveSleepHr = Number(options.max_drive_before_sleep_hr || 10.0);
  const sleepBreakHr = Number(options.sleep_break_hr || 8.0);

  let driveSinceFood = 0;
  let driveSinceSleep = 0;
  let extraBreakHours = 0;
  let prevRawArrival = result.itinerary[0]?.arrival_hour || 0;

  const enriched = result.itinerary.map((leg, idx) => {
    const cityFac = by_city[String(leg.city_id)] || {
      parking: [],
      rest_food: [],
      repair: [],
    };

    if (idx === 0) {
      return {
        ...leg,
        break_hours: 0,
        break_type: null,
        facilities: cityFac,
        corridor_facilities: { parking: [], rest_food: [], repair: [] },
      };
    }

    const prevLeg = result.itinerary[idx - 1];
    const corridorFac = getCorridorFacilities(by_corridor, prevLeg.city_id, leg.city_id);

    const legDriveHr = Math.max(0, leg.arrival_hour - prevRawArrival - (leg.wait_hours || 0));
    prevRawArrival = leg.arrival_hour;

    driveSinceFood += legDriveHr;
    driveSinceSleep += legDriveHr;

    let break_hours = 0;
    let break_type = null;
    const isLast = idx === result.itinerary.length - 1;

    if (!isLast || driveSinceSleep >= maxDriveSleepHr) {
      if (driveSinceSleep >= maxDriveSleepHr) {
        break_hours = sleepBreakHr;
        break_type = 'Sleep & Truck Parking';
        driveSinceSleep = 0;
        driveSinceFood = 0;
      } else if (driveSinceFood >= maxDriveFoodHr && !isLast) {
        break_hours = foodBreakHr;
        break_type = 'Rest & Food';
        driveSinceFood = 0;
      }
    }

    const adjustedArrival = Number((leg.arrival_hour + extraBreakHours).toFixed(2));
    extraBreakHours += break_hours;

    return {
      ...leg,
      arrival_hour: adjustedArrival,
      break_hours,
      break_type,
      facilities: cityFac,
      corridor_facilities: corridorFac,
    };
  });

  return {
    ...result,
    total_time_hr: Number((result.total_time_hr + extraBreakHours).toFixed(2)),
    total_break_hr: Number(extraBreakHours.toFixed(2)),
    itinerary: enriched,
  };
}

module.exports = { enrichItinerary };