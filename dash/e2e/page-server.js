// Tiny static server for the dashboard E2E tests: serves the production page
// (index.html with the webpack bundle inlined, like the firmware does), and
// nothing else. The API and WebSocket are intercepted by Playwright routes.
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 8321;

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');
const bundle = fs.readFileSync(path.join(__dirname, '..', 'bundle.js'), 'utf8');
const page = html.replace('%%%SCRIPT%%%', () => bundle);

http.createServer((req, res) => {
    if (req.url === '/') {
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(page);
    } else {
        res.writeHead(404);
        res.end();
    }
}).listen(PORT, () => console.log(`page-server on :${PORT}`));
