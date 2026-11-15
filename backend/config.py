"""
config.py — Central configuration for the Flask backend.
Reads deployment overrides from environment variables so the same code runs
locally and on Render (which injects $PORT and expects 0.0.0.0).
"""
import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = PROJECT_ROOT / "data"
FRONTEND_DIR = PROJECT_ROOT / "frontend"

ENGINE_TIMEOUT = int(os.environ.get("ENGINE_TIMEOUT", "60"))
HOST = os.environ.get("HOST", "0.0.0.0")
PORT = int(os.environ.get("PORT", "5000"))
DEBUG = os.environ.get("FLASK_DEBUG", "0") == "1"