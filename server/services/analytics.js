const fs = require("fs");
const path = require("path");

const STORE_PATH = path.resolve(__dirname, "..", "recent_runs.json");

class AnalyticsService {
  constructor(maxlen = 50) {
    this.maxlen = maxlen;
    this.runs = [];
    this._load();
  }

  _load() {
    try {
      if (fs.existsSync(STORE_PATH)) {
        const raw = fs.readFileSync(STORE_PATH, "utf8");
        const data = JSON.parse(raw);
        if (Array.isArray(data)) {
          this.runs = data.slice(0, this.maxlen);
        }
      }
    } catch (_) {
      // Ignore corrupt/missing history file
    }
  }

  _save() {
    try {
      fs.writeFileSync(STORE_PATH, JSON.stringify(this.runs, null, 2), "utf8");
    } catch (_) {
      // Ignore write failures
    }
  }

  record(entry) {
    const recordWithTs = {
      ...entry,
      timestamp: Date.now() / 1000,
    };
    this.runs.unshift(recordWithTs);
    if (this.runs.length > this.maxlen) {
      this.runs.length = this.maxlen;
    }
    this._save();
  }

  recent(limit = 10) {
    return this.runs.slice(0, limit);
  }

  summary() {
    if (this.runs.length === 0) {
      return {
        total_runs: 0,
        avg_execution_ms: 0,
        algorithm_usage: {},
      };
    }

    let totalExecMs = 0;
    const algos = {};

    for (const run of this.runs) {
      totalExecMs += Number(run.execution_time_ms) || 0;
      const algo = run.algorithm || "unknown";
      algos[algo] = (algos[algo] || 0) + 1;
    }

    return {
      total_runs: this.runs.length,
      avg_execution_ms: totalExecMs / this.runs.length,
      algorithm_usage: algos,
    };
  }
}

const analytics = new AnalyticsService(50);

module.exports = {
  AnalyticsService,
  analytics,
};