import express from 'express';
import https from 'https';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();

// Serve static files from dist
app.use(express.static(path.join(__dirname, 'dist')));

// Handle SPA routing - use a wildcard pattern that works with newer Express
app.get('/*', (req, res) => {
  res.sendFile(path.join(__dirname, 'dist', 'index.html'));
});

// HTTPS options
const httpsOptions = {
  key: fs.readFileSync(path.join(__dirname, 'localhost+2-key.pem')),
  cert: fs.readFileSync(path.join(__dirname, 'localhost+2.pem'))
};

// Start HTTPS server
const PORT = 3000;
https.createServer(httpsOptions, app).listen(PORT, '0.0.0.0', () => {
  const networkInterfaces = require('os').networkInterfaces();
  const localIP = Object.values(networkInterfaces)
    .flat()
    .find(iface => iface.family === 'IPv4' && !iface.internal)?.address;

  console.log(`🚀 HTTPS Server running on:`);
  console.log(`   Local:   https://localhost:${PORT}`);
  if (localIP) {
    console.log(`   Network: https://${localIP}:${PORT}`);
  }
  console.log('\n📱 PWA Install should now work!\n');
});