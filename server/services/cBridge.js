const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const BIN_DIR = path.join(__dirname, '..', '..', 'bin');

function resolveEngineBinary() {
  const candidates =
    process.platform === 'win32'
      ? [path.join(BIN_DIR, 'freight_engine.exe'), path.join(BIN_DIR, 'freight_engine')]
      : [path.join(BIN_DIR, 'freight_engine'), path.join(BIN_DIR, 'freight_engine.exe')];

  for (const binPath of candidates) {
    if (fs.existsSync(binPath)) return binPath;
  }
  return candidates[0];
}

function runEngine(payload) {
  return new Promise((resolve, reject) => {
    const binPath = resolveEngineBinary();
    if (!fs.existsSync(binPath)) {
      return reject(
        new Error(`C engine binary not found at ${binPath}. Run 'make' or compile engine/src/*.c first.`)
      );
    }

    const child = spawn(binPath, [], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (chunk) => {
      stdout += chunk.toString();
    });

    child.stderr.on('data', (chunk) => {
      stderr += chunk.toString();
    });

    child.on('error', (err) => {
      reject(new Error(`Failed to spawn C engine: ${err.message}`));
    });

    child.on('close', (code) => {
      if (code !== 0) {
        return reject(new Error(`C engine exited with code ${code}: ${stderr || stdout}`));
      }
      try {
        const parsed = JSON.parse(stdout);
        resolve(parsed);
      } catch (err) {
        reject(new Error(`Invalid JSON from C engine: ${err.message}\nRaw output: ${stdout.slice(0, 300)}`));
      }
    });

    child.stdin.write(JSON.stringify(payload));
    child.stdin.end();
  });
}

module.exports = { runEngine };