const fs = require("fs");
const { spawn } = require("child_process");
const { DATA_DIR, ENGINE_CANDIDATES, ENGINE_TIMEOUT_MS } = require("./config");

class EngineError extends Error {
  constructor(message) {
    super(message);
    this.name = "EngineError";
  }
}

function findEngine() {
  for (const candidate of ENGINE_CANDIDATES) {
    try {
      if (fs.statSync(candidate).isFile()) {
        if (process.platform !== "win32") {
          fs.accessSync(candidate, fs.constants.X_OK);
        }
        return candidate;
      }
    } catch (_) {
      // Try next candidate
    }
  }
  return null;
}

function callEngine(requestObj, timeoutMs = ENGINE_TIMEOUT_MS) {
  return new Promise((resolve, reject) => {
    const enginePath = findEngine();
    if (!enginePath) {
      return reject(
        new EngineError(
          "freight_engine binary not found. Build the C engine first (run 'make -C algorithm')."
        )
      );
    }

    const payload = JSON.stringify(requestObj);
    const child = spawn(enginePath, [DATA_DIR], {
      stdio: ["pipe", "pipe", "pipe"],
    });

    let stdout = "";
    let stderr = "";
    let timedOut = false;

    const timer = setTimeout(() => {
      timedOut = true;
      child.kill("SIGKILL");
      reject(new EngineError(`C engine timed out after ${timeoutMs / 1000}s`));
    }, timeoutMs);

    child.stdout.setEncoding("utf8");
    child.stderr.setEncoding("utf8");

    child.stdout.on("data", (chunk) => {
      stdout += chunk;
    });

    child.stderr.on("data", (chunk) => {
      stderr += chunk;
    });

    child.on("error", (err) => {
      clearTimeout(timer);
      reject(new EngineError(`C engine binary could not be executed: ${err.message}`));
    });

    child.on("close", (code) => {
      clearTimeout(timer);
      if (timedOut) return;

      const trimmedOut = stdout.trim();
      const trimmedErr = stderr.trim();

      if (!trimmedOut) {
        return reject(
          new EngineError(
            `engine returned no output (exit code ${code}). stderr: ${trimmedErr}`
          )
        );
      }

      try {
        const parsed = JSON.parse(trimmedOut);
        resolve(parsed);
      } catch (parseErr) {
        reject(
          new EngineError(
            `engine returned invalid JSON: ${parseErr.message}; raw: ${trimmedOut.slice(0, 200)}`
          )
        );
      }
    });

    child.stdin.write(payload);
    child.stdin.end();
  });
}

module.exports = {
  EngineError,
  findEngine,
  callEngine,
};