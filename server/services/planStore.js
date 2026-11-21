const fs = require('fs');
const path = require('path');

const PLANS_FILE = path.join(__dirname, '..', '..', 'data', 'saved_plans.json');

function readStore() {
  if (!fs.existsSync(PLANS_FILE)) {
    fs.writeFileSync(PLANS_FILE, JSON.stringify({ plans: [] }, null, 2) + '\n', 'utf8');
  }
  return JSON.parse(fs.readFileSync(PLANS_FILE, 'utf8'));
}

function writeStore(data) {
  fs.writeFileSync(PLANS_FILE, JSON.stringify(data, null, 2) + '\n', 'utf8');
}

function getAllPlans() {
  const { plans } = readStore();
  return plans;
}

function savePlan(planInput) {
  if (!planInput || typeof planInput !== 'object') {
    throw new Error('Plan payload is required');
  }
  const store = readStore();
  const newPlan = {
    id: `plan_${Date.now()}`,
    title: String(planInput.title || 'Untitled Freight Plan').trim(),
    mode: planInput.mode || 'route', // 'route' or 'mst'
    algorithm_used: planInput.algorithm_used || '',
    request: planInput.request || {},
    result: planInput.result || {},
    created_at: new Date().toISOString(),
  };

  store.plans.unshift(newPlan);
  writeStore(store);
  return newPlan;
}

function deletePlan(id) {
  const store = readStore();
  const before = store.plans.length;
  store.plans = store.plans.filter((p) => p.id !== id);
  if (store.plans.length === before) return false;
  writeStore(store);
  return true;
}

module.exports = {
  getAllPlans,
  savePlan,
  deletePlan,
};