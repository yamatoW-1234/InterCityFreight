const fs = require("fs");
const path = require("path");
const express = require("express");
const cors = require("cors");
const { HOST, PORT, PUBLIC_DIR } = require("./config");
const apiRoutes = require("./routes/apiRoutes");

// Load .env if present (zero extra dependencies)
const envPath = path.join(__dirname, "..", ".env");
if (fs.existsSync(envPath)) {
  fs.readFileSync(envPath, "utf8").split(/\r?\n/).forEach(line => {
    const match = line.match(/^\s*([\w.-]+)\s*=\s*(.*)?\s*$/);
    if (match && !process.env[match[1]]) {
      process.env[match[1]] = (match[2] || "").replace(/^['"]|['"]$/g, "");
    }
  });
}

function createApp() {
  const app = express();

  app.use(cors());
  app.use(express.json());
  app.use("/api", apiRoutes);

  // Unknown API endpoints return JSON 404 instead of index.html (Express 5 syntax)
  app.use("/api/{*splat}", (req, res) => {
    res.status(404).json({ status: "error", message: "not found" });
  });

  // Serve static SPA assets from public/
  app.use(express.static(PUBLIC_DIR));

  // SPA fallback route (Express 5 syntax)
  app.get("/{*splat}", (req, res) => {
    res.sendFile(path.join(PUBLIC_DIR, "index.html"));
  });

  return app;
}

const app = createApp();

if (require.main === module) {
  app.listen(PORT, HOST, () => {
    console.log(`Inter-City Freight server listening on http://${HOST}:${PORT}`);
  });
}

module.exports = { createApp, app };