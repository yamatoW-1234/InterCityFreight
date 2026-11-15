#!/usr/bin/env bash
# ============================================================================
#  run.sh — One-command launch: build engine (if needed), set up the Python
#  virtual environment (if needed), and start the Flask backend + dashboard.
# ============================================================================
set -e
cd "$(dirname "$0")/.."

if [ ! -x algorithm_engine/freight_engine ]; then
  echo "==> Engine binary missing — building first…"
  bash scripts/build.sh
fi

if [ ! -d .venv ]; then
  echo "==> Creating Python virtual environment (.venv)…"
  python3 -m venv .venv
  ./.venv/bin/pip install --upgrade pip >/dev/null
  ./.venv/bin/pip install -r requirements.txt
fi

echo "==> Starting Flask backend and dashboard at http://127.0.0.1:5000"
./.venv/bin/python backend/app.py