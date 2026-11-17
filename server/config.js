const path = require("path");

const PROJECT_ROOT = path.resolve(__dirname, "..");
const DATA_DIR = path.join(PROJECT_ROOT, "data");
const PUBLIC_DIR = path.join(PROJECT_ROOT, "public");
const BIN_DIR = path.join(PROJECT_ROOT, "bin");

const ENGINE_BINARY =
  process.platform === "win32" ? "freight_engine.exe" : "freight_engine";

const ENGINE_CANDIDATES = [
  path.join(BIN_DIR, ENGINE_BINARY),
  path.join(PROJECT_ROOT, "algorithm", "build", ENGINE_BINARY),
];

const ENGINE_TIMEOUT_MS = parseInt(process.env.ENGINE_TIMEOUT || "60", 10) * 1000;
const HOST = process.env.HOST || "0.0.0.0";
const PORT = parseInt(process.env.PORT || "5000", 10);

module.exports = {
  PROJECT_ROOT,
  DATA_DIR,
  PUBLIC_DIR,
  BIN_DIR,
  ENGINE_CANDIDATES,
  ENGINE_TIMEOUT_MS,
  HOST,
  PORT,
};