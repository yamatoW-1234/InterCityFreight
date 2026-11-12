"""
engine_bridge.py
================
Subprocess bridge between the Flask backend and the compiled C++ engine.

The C++ executable `freight_engine` is the ONLY place where graph algorithms
run. Python's role is to locate the binary, pipe a JSON request to stdin, and
read the JSON result from stdout.
"""
import json
import os
import subprocess
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = PROJECT_ROOT / "data"

ENGINE_CANDIDATES = [
    PROJECT_ROOT / "algorithm_engine" / "freight_engine",
    PROJECT_ROOT / "algorithm_engine" / "build" / "freight_engine",
]

# Benchmarks can take a while on shared/free-tier CPUs; override with ENGINE_TIMEOUT.
DEFAULT_TIMEOUT = int(os.environ.get("ENGINE_TIMEOUT", "60"))


class EngineError(Exception):
    """Raised when the C++ engine cannot be reached or fails."""


def find_engine():
    """Return the path to the compiled engine binary, or None if not built."""
    for candidate in ENGINE_CANDIDATES:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def call_engine(request_obj: dict, timeout: int = DEFAULT_TIMEOUT) -> dict:
    """Run the C++ engine with a JSON request and return the parsed JSON result."""
    engine = find_engine()
    if engine is None:
        raise EngineError(
            "freight_engine binary not found. Build the C++ engine first "
            "(run scripts/build.sh or the CMake commands in README.md)."
        )

    payload = json.dumps(request_obj)
    try:
        proc = subprocess.run(
            [str(engine), str(DATA_DIR)],
            input=payload,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        raise EngineError(f"C++ engine timed out after {timeout}s")
    except FileNotFoundError:
        raise EngineError("C++ engine binary could not be executed (permissions?)")

    stdout = proc.stdout.strip()
    stderr = proc.stderr.strip()

    if not stdout:
        raise EngineError(
            f"engine returned no output (exit code {proc.returncode}). stderr: {stderr}"
        )

    try:
        return json.loads(stdout)
    except json.JSONDecodeError as exc:
        raise EngineError(f"engine returned invalid JSON: {exc}; raw: {stdout[:200]}")