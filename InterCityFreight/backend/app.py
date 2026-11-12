"""
app.py — Flask application factory + static frontend server.

Run from the project root or the backend directory:
    python backend/app.py
"""
import sys
from pathlib import Path

# Ensure the backend package directory is importable regardless of cwd.
BACKEND_DIR = Path(__file__).resolve().parent
if str(BACKEND_DIR) not in sys.path:
    sys.path.insert(0, str(BACKEND_DIR))

from flask import Flask, send_from_directory, jsonify   # noqa: E402
from config import FRONTEND_DIR, HOST, PORT, DEBUG      # noqa: E402
from routes import api_bp                               # noqa: E402


def create_app() -> Flask:
    app = Flask(__name__, static_folder=None)
    app.register_blueprint(api_bp, url_prefix="/api")

    @app.route("/")
    def index():
        return send_from_directory(FRONTEND_DIR, "index.html")

    @app.route("/<path:path>")
    def frontend(path):
        if path.startswith("api/"):
            return jsonify({"status": "error", "message": "not found"}), 404
        return send_from_directory(FRONTEND_DIR, path)

    return app


app = create_app()


if __name__ == "__main__":
    app.run(host=HOST, port=PORT, debug=DEBUG)